#!/usr/bin/env python3

import argparse
import math
import re
import statistics
from pathlib import Path


TIME_RE = re.compile(
    r"\[\s*OK\s*\]\s+(.+?)\s+\(([\d.]+)\s*(ns|us|ms|s)\)"
)


def parse_gtest_output(path):
    path = Path(path)

    if not path.exists():
        raise FileNotFoundError(f"Benchmark output not found: {path}")

    samples = {}

    for line in path.read_text(errors="replace").splitlines():
        match = TIME_RE.search(line)

        if not match:
            continue

        test_name = match.group(1).strip()
        value = float(match.group(2))
        unit = match.group(3)

        if unit == "s":
            value *= 1000.0
        elif unit == "us":
            value /= 1000.0
        elif unit == "ns":
            value /= 1_000_000.0

        samples.setdefault(test_name, []).append(value)

    return {
        test_name: statistics.median(values)
        for test_name, values in samples.items()
    }


def format_ms(value):
    if value is None:
        return "N/A"

    if value >= 1000:
        return f"{value / 1000:.3f}s"

    if value >= 1:
        return f"{value:.3f}ms"

    return f"{value * 1000:.3f}us"


def percentage_change(new_value, old_value):
    if old_value is None or old_value == 0 or new_value is None:
        return "N/A"

    pct = ((new_value - old_value) / old_value) * 100

    if abs(pct) < 0.01:
        pct = 0

    return f"{pct:+.2f}%"


def make_table(title, rows):
    print()
    print(title)
    print("=" * len(title))

    headers = [
        "Test Case",
        "Existing",
        "Google Current",
        "Current vs Existing",
        "Google Main",
        "Main vs Current",
    ]

    widths = [
        max(
            len(headers[0]),
            *(len(row[0]) for row in rows),
        ),
        max(
            len(headers[1]),
            *(len(row[1]) for row in rows),
        ),
        max(
            len(headers[2]),
            *(len(row[2]) for row in rows),
        ),
        max(
            len(headers[3]),
            *(len(row[3]) for row in rows),
        ),
        max(
            len(headers[4]),
            *(len(row[4]) for row in rows),
        ),
        max(
            len(headers[5]),
            *(len(row[5]) for row in rows),
        ),
    ]

    def format_row(values):
        return " | ".join(
            value.ljust(width)
            for value, width in zip(values, widths)
        )

    print(format_row(headers))
    print("-+-".join("-" * width for width in widths))

    for row in rows:
        print(format_row(row))


def main():
    parser = argparse.ArgumentParser(
        description="Parse ODBC performance benchmark output."
    )

    parser.add_argument(
        "--existing",
        required=True,
        help="Existing benchmark output",
    )

    parser.add_argument(
        "--current",
        required=True,
        help="Current Google driver benchmark output",
    )

    parser.add_argument(
        "--main",
        required=True,
        help="Main Google driver benchmark output",
    )

    parser.add_argument(
        "--output",
        required=True,
        help="Summary output file",
    )

    args = parser.parse_args()

    existing = parse_gtest_output(args.existing)
    current = parse_gtest_output(args.current)
    main_branch = parse_gtest_output(args.main)

    test_names = sorted(
        set(existing)
        | set(current)
        | set(main_branch)
    )

    rows = []

    for test_name in test_names:
        existing_ms = existing.get(test_name)
        current_ms = current.get(test_name)
        main_ms = main_branch.get(test_name)

        rows.append(
            (
                test_name,
                format_ms(existing_ms),
                format_ms(current_ms),
                percentage_change(current_ms, existing_ms),
                format_ms(main_ms),
                percentage_change(main_ms, current_ms),
            )
        )

    output_lines = []

    headers = [
        "Test Case",
        "Existing",
        "Google Current",
        "Current vs Existing",
        "Google Main",
        "Main vs Current",
    ]

    widths = [
        max(len(headers[0]), *(len(row[0]) for row in rows)),
        max(len(headers[1]), *(len(row[1]) for row in rows)),
        max(len(headers[2]), *(len(row[2]) for row in rows)),
        max(len(headers[3]), *(len(row[3]) for row in rows)),
        max(len(headers[4]), *(len(row[4]) for row in rows)),
        max(len(headers[5]), *(len(row[5]) for row in rows)),
    ]

    def format_row(values):
        return " | ".join(
            value.ljust(width)
            for value, width in zip(values, widths)
        )

    output_lines.append(
        "ODBC Performance Benchmark Summary"
    )
    output_lines.append(
        "=================================="
    )
    output_lines.append("")
    output_lines.append(format_row(headers))
    output_lines.append(
        "-+-".join("-" * width for width in widths)
    )

    for row in rows:
        output_lines.append(format_row(row))

    summary = "\n".join(output_lines)

    Path(args.output).write_text(summary + "\n")

    print()
    print(summary)

    if not existing:
        print(
            "WARNING: No Existing benchmark results were found."
        )

    if not current:
        print(
            "ERROR: No current Google benchmark results were found."
        )
        return 1

    if not main_branch:
        print(
            "ERROR: No main Google benchmark results were found."
        )
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())