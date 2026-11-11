#!/bin/bash
#
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

set -euo pipefail

source "$(dirname "$0")/../../lib/init.sh"
source module ci/lib/io.sh

PROXY_PORT=8888
PROXY_PID=""
PROXY_LOG="/tmp/idle_connect_proxy.log"
PROXY_KILLS="/tmp/idle_connect_proxy.kills"
PROXY_STATS="/tmp/idle_connect_proxy.stats"

cleanup_proxy() {
  if [[ -n "${PROXY_PID}" ]] && kill -0 "${PROXY_PID}" 2>/dev/null; then
    echo "Terminating idle connect proxy..."
    kill "${PROXY_PID}" 2>/dev/null || true
    wait "${PROXY_PID}" 2>/dev/null || true
  fi
  if [[ -f "${PROXY_LOG}" ]]; then
    echo "=== Idle Connect Proxy Log Output ==="
    cat "${PROXY_LOG}" || true
    echo "=== End Idle Connect Proxy Log Output ==="
  fi
}
trap cleanup_proxy EXIT

echo "Starting idle connect proxy on port ${PROXY_PORT}..."
python3 "${PROJECT_ROOT}/tools/idle_connect_proxy/idle_connect_proxy.py" \
  --port "${PROXY_PORT}" \
  --idle-timeout 40 \
  --mode reset \
  --kills-file "${PROXY_KILLS}" \
  --stats-file "${PROXY_STATS}" \
  >"${PROXY_LOG}" 2>&1 &
PROXY_PID=$!
sleep 1
if ! kill -0 "${PROXY_PID}" 2>/dev/null; then
  echo "ERROR: Idle connect proxy failed to start."
  exit 1
fi

export BQ_ODBC_IDLE_PROXY="127.0.0.1:${PROXY_PORT}"
export BQ_ODBC_IDLE_PROXY_PAUSE_SECONDS="45"
export BQ_ODBC_IDLE_PROXY_EXPECT="success"
export BQ_ODBC_IDLE_PROXY_KILLS_FILE="${PROXY_KILLS}"
export BQ_ODBC_IDLE_PROXY_STATS_FILE="${PROXY_STATS}"
echo "Idle connect proxy started with PID ${PROXY_PID} on 127.0.0.1:${PROXY_PORT}"

echo "Running IdleProxy resilience tests..."
io::run env -C cmake-out ctest --output-on-failure --progress -j 1 -R "IdleProxy"

if [[ -f "${PROXY_LOG}" ]] && grep -q "KILL mode=" "${PROXY_LOG}"; then
  echo "ERROR: Idle connect proxy killed one or more connections due to idle timeout."
  echo "gRPC keepalive failed to keep the connection alive!"
  exit 1
fi

if [[ -f "${PROXY_LOG}" ]] && grep -q "SERVER_DROP" "${PROXY_LOG}"; then
  echo "ERROR: Server severed connection during pause (ping flood / too_many_pings rejection)!"
  exit 1
fi

if [[ -f "${PROXY_LOG}" ]] && grep -q "PING_FLOOD" "${PROXY_LOG}"; then
  echo "ERROR: Excessive keepalive ping flood detected in proxy log!"
  exit 1
fi

kill "${PROXY_PID}" 2>/dev/null || true
wait "${PROXY_PID}" 2>/dev/null || true
PROXY_PID=""

echo "Starting idle connect proxy with --kill-after-bytes for stream retry test..."
python3 "${PROJECT_ROOT}/tools/idle_connect_proxy/idle_connect_proxy.py" \
  --port "${PROXY_PORT}" \
  --idle-timeout 0 \
  --kill-after-bytes 4000000 \
  --reconnect-kill-after-bytes 100000 \
  --max-kills 2 \
  --mode corrupt \
  --kills-file "${PROXY_KILLS}" \
  --stats-file "${PROXY_STATS}" \
  >>"${PROXY_LOG}" 2>&1 &
PROXY_PID=$!
sleep 1
if ! kill -0 "${PROXY_PID}" 2>/dev/null; then
  echo "ERROR: Idle connect proxy failed to start for stream retry test."
  exit 1
fi

echo "Running HTAPI stream retry and resume resilience test..."
io::run env -C cmake-out ctest --output-on-failure --progress -j 1 -R "SQLExecDirect_HTAPI_StreamRetryAndResume"
