#!/usr/bin/env python3

import argparse
import re
import statistics
from pathlib import Path


TIME_RE = re.compile(
    r"\[\s*OK\s*\]\s+(\S+)\s+\(([\d.]+)\s*(ns|us|ms|s)\)"
)


def clean_test_name(name):
    """Normalize GTest test names for comparison."""
    # GTest names can be:
    #   Instantiation/TestSuite.TestCase/Param
    # Remove everything before the first '.'
    if "." in name:
        name = name.split(".", 1)[1]

    # Remove legacy suffixes so old/new benchmark names compare correctly.
    name = re.sub(r"/(?:With|Without)HTAPI$", "", name)

    return name


def parse_time_to_ms(value, unit):
    """Convert a numeric GTest duration to milliseconds."""
    if unit == "s":
        return value * 1000.0
    if unit == "us":
        return value / 1000.0
    if unit == "ns":
        return value / 1_000_000.0
    return value


def format_ms(value):
    """Format milliseconds similarly to the existing benchmark table."""
    if value is None:
        return "N/A"

    # Keep the table output in milliseconds.
    return f"{value:.0f}ms"


def parse_gtest_output(path):
    """
    Parse repeated GTest benchmark output.

    Each test may appear multiple times because the benchmark is run for
    multiple iterations. Return the median execution time in milliseconds.
    """
    path = Path(path)

    if not path.exists():
        raise FileNotFoundError(
            f"Benchmark output not found: {path}"
        )

    samples = {}

    for line in path.read_text(errors="replace").splitlines():
        match = TIME_RE.search(line)

        if not match:
            continue

        test_name = clean_test_name(match.group(1))
        value = float(match.group(2))
        unit = match.group(3).lower()

        value_ms = parse_time_to_ms(value, unit)

        samples.setdefault(test_name, []).append(value_ms)

    results = {}

    for test_name, values in samples.items():
        median = statistics.median(values)
        results[test_name] = median

        if len(values) > 1:
            print(
                f"{test_name}: "
                f"median={median:.0f}ms of {len(values)} runs "
                f"(min={min(values):.0f}ms "
                f"max={max(values):.0f}ms)"
            )

    return results


def get_percentage_str(value_ms, reference_ms):
    """
    Return percentage change.

    Negative = faster/improvement
    Positive = slower/degradation
    """
    if (
        value_ms is None
        or reference_ms is None
        or reference_ms == 0
    ):
        return " (N/A)"

    pct = round(
        ((value_ms - reference_ms) / reference_ms) * 100
    )

    if pct > 0:
        return f" (+{pct}%)"
    if pct < 0:
        return f" ({pct}%)"

    return " (0%)"


def main():
    parser = argparse.ArgumentParser(
        description="Parse ODBC performance benchmark output."
    )

    parser.add_argument(
        "--existing",
        required=True,
        help="Existing driver benchmark output",
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

    parser.add_argument(
        "--branch-name",
        default="Current",
        help="Branch name used in the Google Driver column header",
    )

    args = parser.parse_args()

    existing_data = parse_gtest_output(args.existing)
    current_data = parse_gtest_output(args.current)
    main_data = parse_gtest_output(args.main)

    if not current_data:
        print(
        "WARNING: No current Google benchmark results were found. "
        "Google Current will be shown as N/A."
    )

    if not main_data:
        print(
        "WARNING: No main Google benchmark results were found. "
        "Google Main will be shown as N/A."
    )

    if not existing_data:
        print(
            "WARNING: No Existing benchmark results were found."
        )

    all_tests = (
        set(existing_data)
        | set(current_data)
        | set(main_data)
    )

    sorted_tests = sorted(all_tests)

    rows = []

    for test_name in sorted_tests:
        existing_ms = existing_data.get(test_name)
        current_ms = current_data.get(test_name)
        main_ms = main_data.get(test_name)

        existing_raw = format_ms(existing_ms)
        current_raw = format_ms(current_ms)
        main_raw = format_ms(main_ms)

        current_pct = ""
        if current_ms is not None:
            current_pct = get_percentage_str(
                current_ms,
                existing_ms,
            )

        main_pct = ""
        if main_ms is not None:
            main_pct = get_percentage_str(
                main_ms,
                current_ms,
            )

        current_value = f"{current_raw}{current_pct}"
        main_value = f"{main_raw}{main_pct}"

        rows.append(
            (
                test_name,
                existing_raw,
                current_value,
                main_value,
            )
        )

    # Match the GitHub Actions benchmark table format.
    h1 = "Test Case"
    h2 = "Existing Driver (Current)"
    h3 = f"Google Driver ({args.branch_name})"
    h4 = "Google Driver (Main)"

    w1 = (
        max([len(h1)] + [len(row[0]) for row in rows])
        if rows
        else len(h1)
    )
    w2 = (
        max([len(h2)] + [len(row[1]) for row in rows])
        if rows
        else len(h2)
    )
    w3 = (
        max([len(h3)] + [len(row[2]) for row in rows])
        if rows
        else len(h3)
    )
    w4 = (
        max([len(h4)] + [len(row[3]) for row in rows])
        if rows
        else len(h4)
    )

    table = (
        f"*Percentages in **{h3}** show change relative to "
        f"**{h2}**. Percentages in **{h4}** show change relative "
        f"to **{h3}**. Negative values indicate improvement "
        f"(faster test execution), positive values indicate "
        f"degradation (slower).*\n\n"
    )

    table += (
        f"| {h1.ljust(w1)} "
        f"| {h2.ljust(w2)} "
        f"| {h3.ljust(w3)} "
        f"| {h4.ljust(w4)} |\n"
    )

    table += (
        "|-" + ("-" * w1)
        + "-|-" + ("-" * w2)
        + "-|-" + ("-" * w3)
        + "-|-" + ("-" * w4)
        + "-|\n"
    )

    for row in rows:
        table += (
            f"| {row[0].ljust(w1)} "
            f"| {row[1].ljust(w2)} "
            f"| {row[2].ljust(w3)} "
            f"| {row[3].ljust(w4)} |\n"
        )

    output_path = Path(args.output)
    output_path.write_text(table)

    print()
    print(table)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())