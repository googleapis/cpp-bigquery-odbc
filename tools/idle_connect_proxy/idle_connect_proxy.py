#!/usr/bin/env python3
# Copyright 2026 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""HTTP CONNECT proxy for simulating network middlebox idle timeouts and resets."""

import argparse
import asyncio
import logging
import os
import socket
import struct
import sys
import tempfile
import time

LOG = logging.getLogger("idle-proxy")

ARGS = None
TUNNELS = set()
SESSION_HAD_DROP = False
STATS = {
    "tunnels": 0,
    "kills": 0,
    "server_drops": 0,
    "ping_floods": 0,
    "reconnects": 0,
    "storage_tunnels": 0,
}

READ_CHUNK = 64 * 1024
DEFAULT_KILLS_FILE = os.path.join(tempfile.gettempdir(), "idle_connect_proxy.kills")
DEFAULT_STATS_FILE = os.path.join(tempfile.gettempdir(), "idle_connect_proxy.stats")


class Tunnel:
    _next_id = 0

    def __init__(self, target, peer):
        Tunnel._next_id += 1
        self.id = Tunnel._next_id
        self.target = target
        self.peer = peer
        self.opened = time.monotonic()
        self.last_activity = self.opened
        self.last_down_activity = self.opened
        self.up_bytes = 0
        self.down_bytes = 0
        self.state = "open"
        self.kill_reason = None
        self.monitored = True
        self.client_writer = None
        self.server_writer = None
        self.window_start = self.opened
        self.window_packets = 0

    def __str__(self):
        return f"#{self.id} {self.target}"

    def age(self):
        return time.monotonic() - self.opened

    def idle(self):
        return time.monotonic() - self.last_activity


def reset_writer(writer):
    """Closes a StreamWriter with a TCP RST."""
    if writer is None:
        return
    try:
        sock = writer.get_extra_info("socket")
        if sock is not None:
            fmt = "hh" if sys.platform == "win32" else "ii"
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack(fmt, 1, 0))
    except OSError as e:
        LOG.debug("SO_LINGER failed: %s", e)
    writer.transport.abort()


def close_writer(writer):
    """Closes a StreamWriter with a FIN."""
    if writer is None:
        return
    try:
        writer.close()
    except Exception:
        pass


def write_stats_file():
    """Writes the current stats to ARGS.stats_file and ARGS.kills_file."""
    if ARGS and ARGS.kills_file:
        try:
            with open(ARGS.kills_file, "w") as f:
                f.write(str(STATS["kills"]))
        except Exception as e:
            LOG.debug("failed to write kills file: %s", e)
    if ARGS and ARGS.stats_file:
        try:
            with open(ARGS.stats_file, "w") as f:
                for k, v in STATS.items():
                    f.write(f"{k}: {v}\n")
        except Exception as e:
            LOG.debug("failed to write stats file: %s", e)


def kill(tunnel, reason):
    """Applies the configured kill mode to an open tunnel."""
    global SESSION_HAD_DROP
    if tunnel.state != "open":
        return
    tunnel.kill_reason = reason
    STATS["kills"] += 1
    SESSION_HAD_DROP = True
    write_stats_file()
    LOG.warning(
        "%s KILL mode=%s: %s | age=%.1fs idle=%.1fs up=%dB down=%dB",
        tunnel,
        ARGS.mode,
        reason,
        tunnel.age(),
        tunnel.idle(),
        tunnel.up_bytes,
        tunnel.down_bytes,
    )
    if ARGS.mode == "close":
        tunnel.state = "killed"
        close_writer(tunnel.client_writer)
        close_writer(tunnel.server_writer)
    elif ARGS.mode == "reset":
        tunnel.state = "killed"
        reset_writer(tunnel.client_writer)
        reset_writer(tunnel.server_writer)
    elif ARGS.mode == "corrupt":
        tunnel.state = "killed"
        try:
            tunnel.client_writer.write(
                b"HTTP/1.1 502 Bad Gateway\r\n\r\n" + b"\xff" * 64
            )
        except Exception:
            pass
        close_writer(tunnel.client_writer)
        reset_writer(tunnel.server_writer)
    elif ARGS.mode == "blackhole":
        tunnel.state = "blackholed"
        reset_writer(tunnel.server_writer)
    elif ARGS.mode == "stale-reset":
        tunnel.state = "stale"
        reset_writer(tunnel.server_writer)
    else:
        raise ValueError(f"unknown mode {ARGS.mode!r}")


async def relay(reader, writer, tunnel, direction):
    """Copies bytes bidirectionally between client and server."""
    try:
        while True:
            data = await reader.read(READ_CHUNK)
            if not data:
                return "eof"
            if tunnel.state in ("blackholed", "stale"):
                if direction == "up" and tunnel.state == "stale":
                    LOG.warning(
                        "%s client sent %d bytes on stale tunnel -> TCP RST",
                        tunnel,
                        len(data),
                    )
                    tunnel.state = "killed"
                    reset_writer(tunnel.client_writer)
                    return "stale-reset"
                LOG.debug(
                    "%s swallowing %d bytes (%s)", tunnel, len(data), tunnel.state
                )
                continue
            if direction == "down":
                kill_threshold = (
                    ARGS.reconnect_kill_after_bytes
                    if STATS["kills"] > 0 and ARGS.reconnect_kill_after_bytes > 0
                    else ARGS.kill_after_bytes
                )
                if (
                    kill_threshold > 0
                    and (ARGS.max_kills <= 0 or STATS["kills"] < ARGS.max_kills)
                    and tunnel.monitored
                    and tunnel.state == "open"
                    and tunnel.down_bytes + len(data) >= kill_threshold
                ):
                    tunnel.down_bytes += len(data)
                    if ARGS.mode == "corrupt":
                        corrupted = bytes(b ^ 0xFF for b in data)
                        writer.write(corrupted)
                        await writer.drain()
                    else:
                        writer.write(data)
                        await writer.drain()
                    kill(
                        tunnel,
                        f"{tunnel.down_bytes} bytes relayed >= {kill_threshold}",
                    )
                    return "killed"
            writer.write(data)
            await writer.drain()
            now = time.monotonic()
            tunnel.last_activity = now
            if direction == "up":
                tunnel.up_bytes += len(data)
                if tunnel.monitored:
                    down_idle = now - tunnel.last_down_activity
                    if down_idle > 1.0:
                        if now - tunnel.window_start >= 1.0:
                            if tunnel.window_packets > 5:
                                STATS["ping_floods"] += 1
                                write_stats_file()
                                LOG.warning(
                                    "%s PING_FLOOD: client sent %d packets/sec during idle stream",
                                    tunnel,
                                    tunnel.window_packets,
                                )
                            tunnel.window_start = now
                            tunnel.window_packets = 0
                        tunnel.window_packets += 1
                    else:
                        tunnel.window_start = now
                        tunnel.window_packets = 0
            else:
                tunnel.down_bytes += len(data)
                if len(data) > 256:
                    tunnel.last_down_activity = now
    except asyncio.CancelledError:
        raise
    except (ConnectionError, OSError, asyncio.IncompleteReadError) as e:
        return f"error: {e}"


async def handle_client(client_reader, client_writer):
    peer = client_writer.get_extra_info("peername") or ("?", 0)
    try:
        head = await asyncio.wait_for(
            client_reader.readuntil(b"\r\n\r\n"), timeout=ARGS.connect_timeout
        )
    except (
        asyncio.IncompleteReadError,
        asyncio.LimitOverrunError,
        asyncio.TimeoutError,
        ConnectionError,
        OSError,
    ):
        close_writer(client_writer)
        return

    request_line = head.split(b"\r\n", 1)[0].decode("latin-1", "replace")
    parts = request_line.split()
    if len(parts) < 2 or parts[0].upper() != "CONNECT":
        LOG.warning(
            "rejecting non-CONNECT request from %s:%s: %r",
            peer[0],
            peer[1],
            request_line,
        )
        client_writer.write(
            b"HTTP/1.1 405 Method Not Allowed\r\n" b"Connection: close\r\n\r\n"
        )
        try:
            await client_writer.drain()
        except (ConnectionError, OSError):
            pass
        close_writer(client_writer)
        return

    target = parts[1]
    host, _, port = target.rpartition(":")
    host = host.strip("[]")
    port = int(port) if port.isdigit() else 443

    tunnel = Tunnel(target, peer)
    tunnel.monitored = (not ARGS.target_filter) or (ARGS.target_filter in target)
    try:
        server_reader, server_writer = await asyncio.wait_for(
            asyncio.open_connection(host, port), timeout=ARGS.connect_timeout
        )
    except (OSError, asyncio.TimeoutError) as e:
        LOG.error("%s upstream connect failed: %s", tunnel, e)
        client_writer.write(
            b"HTTP/1.1 502 Bad Gateway\r\n" b"Connection: close\r\n\r\n"
        )
        try:
            await client_writer.drain()
        except (ConnectionError, OSError):
            pass
        close_writer(client_writer)
        return

    tunnel.client_writer = client_writer
    tunnel.server_writer = server_writer
    client_writer.write(b"HTTP/1.1 200 Connection established\r\n\r\n")
    try:
        await client_writer.drain()
    except (ConnectionError, OSError):
        close_writer(client_writer)
        close_writer(server_writer)
        return

    global SESSION_HAD_DROP
    TUNNELS.add(tunnel)
    STATS["tunnels"] += 1
    if tunnel.monitored:
        STATS["storage_tunnels"] += 1
        if SESSION_HAD_DROP:
            STATS["reconnects"] += 1
            SESSION_HAD_DROP = False
            LOG.warning(
                "%s RECONNECT: emergency reconnect after connection dropped", tunnel
            )
        write_stats_file()
    LOG.info(
        "%s OPEN from %s:%s%s",
        tunnel,
        peer[0],
        peer[1],
        "" if tunnel.monitored else " (not monitored)",
    )

    up = asyncio.ensure_future(relay(client_reader, server_writer, tunnel, "up"))
    down = asyncio.ensure_future(relay(server_reader, client_writer, tunnel, "down"))
    try:
        done, _ = await asyncio.wait({up, down}, return_when=asyncio.FIRST_COMPLETED)
        if tunnel.state in ("blackholed", "stale") and up not in done:
            await up
        for task in (up, down):
            if not task.done():
                task.cancel()
        await asyncio.gather(up, down, return_exceptions=True)
    finally:
        TUNNELS.discard(tunnel)
        close_writer(client_writer)
        close_writer(server_writer)
        closed_by_server = (
            down in done and up not in done and tunnel.kill_reason is None
        )
        if tunnel.monitored and closed_by_server:
            STATS["server_drops"] += 1
            SESSION_HAD_DROP = True
            write_stats_file()
            LOG.warning(
                "%s SERVER_DROP: closed by server after %.1fs (ping flood rejection / rate limit)",
                tunnel,
                tunnel.age(),
            )
        elif not TUNNELS and tunnel.kill_reason is None:
            SESSION_HAD_DROP = False
        how = (
            f"killed by proxy ({tunnel.kill_reason})"
            if tunnel.kill_reason
            else "closed by a peer"
        )
        LOG.info(
            "%s CLOSED after %.1fs up=%dB down=%dB - %s",
            tunnel,
            tunnel.age(),
            tunnel.up_bytes,
            tunnel.down_bytes,
            how,
        )


async def watchdog():
    """Monitors and kills tunnels exceeding the idle timeout."""
    while True:
        await asyncio.sleep(0.25)
        if ARGS.idle_timeout <= 0:
            continue
        for tunnel in list(TUNNELS):
            if tunnel.state != "open" or not tunnel.monitored:
                continue
            idle = tunnel.idle()
            if idle >= ARGS.idle_timeout:
                kill(
                    tunnel,
                    f"idle for {idle:.1f}s >= --idle-timeout {ARGS.idle_timeout:.0f}s",
                )


async def status_reporter():
    """Emits periodic heartbeat log messages."""
    while True:
        await asyncio.sleep(ARGS.status_interval)
        open_tunnels = [t for t in TUNNELS if t.state == "open"]
        LOG.info(
            "status: %d open tunnel(s), %d opened in total, %d killed%s",
            len(open_tunnels),
            STATS["tunnels"],
            STATS["kills"],
            "".join(
                f"\n    {t} idle={t.idle():.1f}s up={t.up_bytes}B down={t.down_bytes}B"
                for t in open_tunnels
            ),
        )


def parse_args(argv):
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        "--host", default="127.0.0.1", help="address to listen on (default: 127.0.0.1)"
    )
    parser.add_argument(
        "--port", type=int, default=8888, help="port to listen on (default: 8888)"
    )
    parser.add_argument(
        "--idle-timeout",
        type=float,
        default=10.0,
        help="kill monitored tunnels after this many idle seconds (default: 10)",
    )
    parser.add_argument(
        "--kill-after-bytes",
        type=int,
        default=0,
        help="kill monitored tunnels once this many bytes relayed (default: 0)",
    )
    parser.add_argument(
        "--reconnect-kill-after-bytes",
        type=int,
        default=0,
        help="kill reconnected tunnels once this many bytes relayed (default: 0)",
    )
    parser.add_argument(
        "--max-kills",
        type=int,
        default=0,
        help="maximum number of tunnels to kill (default: 0 = unlimited)",
    )
    parser.add_argument(
        "--mode",
        default="reset",
        choices=["close", "reset", "corrupt", "blackhole", "stale-reset"],
        help="how to kill a tunnel (default: reset)",
    )
    parser.add_argument(
        "--target-filter",
        default="bigquerystorage",
        help="filter target hostname (default: bigquerystorage)",
    )
    parser.add_argument(
        "--connect-timeout",
        type=float,
        default=10.0,
        help="upstream connect timeout in seconds (default: 10)",
    )
    parser.add_argument(
        "--status-interval",
        type=float,
        default=30.0,
        help="seconds between heartbeat logs (default: 30)",
    )
    parser.add_argument(
        "--kills-file",
        default=DEFAULT_KILLS_FILE,
        help="file to record proxy kill count (default: %(default)s)",
    )
    parser.add_argument(
        "--stats-file",
        default=DEFAULT_STATS_FILE,
        help="file to record proxy statistics (default: %(default)s)",
    )
    parser.add_argument(
        "-v", "--verbose", action="store_true", help="enable verbose debug logging"
    )
    return parser.parse_args(argv)


async def serve():
    write_stats_file()
    server = await asyncio.start_server(
        handle_client, ARGS.host, ARGS.port, limit=READ_CHUNK
    )
    asyncio.ensure_future(watchdog())
    if ARGS.status_interval > 0:
        asyncio.ensure_future(status_reporter())
    LOG.info(
        "listening on %s:%d  mode=%s idle-timeout=%ss "
        "kill-after-bytes=%d target-filter=%r",
        ARGS.host,
        ARGS.port,
        ARGS.mode,
        ARGS.idle_timeout,
        ARGS.kill_after_bytes,
        ARGS.target_filter,
    )
    LOG.info(
        "point the ODBC driver at it with:  ProxyHost=%s;ProxyPort=%d",
        ARGS.host,
        ARGS.port,
    )
    async with server:
        await server.serve_forever()


def main(argv=None):
    global ARGS
    ARGS = parse_args(sys.argv[1:] if argv is None else argv)
    logging.basicConfig(
        level=logging.DEBUG if ARGS.verbose else logging.INFO,
        format="%(asctime)s.%(msecs)03d %(levelname)-7s %(message)s",
        datefmt="%H:%M:%S",
    )
    try:
        asyncio.run(serve())
    except KeyboardInterrupt:
        LOG.info(
            "bye: %d tunnel(s) opened, %d killed", STATS["tunnels"], STATS["kills"]
        )


if __name__ == "__main__":
    main()
