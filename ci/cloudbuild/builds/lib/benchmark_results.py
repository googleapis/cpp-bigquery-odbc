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

import argparse
import re
import statistics
from pathlib import Path

TIME_RE = re.compile(r"\[\s*OK\s*\]\s+(.+?)\s+\(([\d.]+)\s*(ns|us|ms|s)\)")


def clean_test_name(name):
    """Normalize GTest test names for comparison."""
    name = name.strip().rstrip(",")

    # Ignore GTest summary lines such as:
    #
    #   19 tests, listed below:
    #
    if re.match(r"^\d+\s+tests?,\s+listed below:$", name):
        return None

    # Remove GTest parameter description.
    #
    # Example:
    #
    #   Benchmark/all_bq_types_2, where GetParam() =
    #   ("all_bq_types_2", "SELECT * FROM ...")
    #
    # becomes:
    #
    #   Benchmark/all_bq_types_2
    #
    name = re.sub(
        r",\s*where\s+GetParam\(\)\s*=.*$",
        "",
        name,
    )

    # Remove everything before the first '.'.
    #
    # Example:
    #
    #   Instantiation/TestSuite.TestCase/0
    #
    # becomes:
    #
    #   TestCase/0
    if "." in name:
        name = name.split(".", 1)[1]

    # Remove legacy HTAPI suffixes.
    name = re.sub(
        r"/(?:With|Without)HTAPI$",
        "",
        name,
    )

    return name


def parse_time_to_ms(value, unit):
    """Convert a GTest duration to milliseconds."""
    if unit == "s":
        return value * 1000.0

    if unit == "us":
        return value / 1000.0

    if unit == "ns":
        return value / 1_000_000.0

    return value


def format_ms(value):
    """Format milliseconds for the benchmark table."""
    if value is None:
        return "N/A"

    return f"{value:.0f}ms"


def parse_gtest_output(path):
    """
    Parse repeated GTest benchmark output.

    A benchmark is run multiple times.

    Rules:

      1. Only successful iterations are used.

      2. Failed iterations are ignored.

      3. The median execution time is calculated from all successful
         iterations.

      4. If all iterations fail or a test is completely missing,
         it is displayed as N/A.
    """
    path = Path(path)

    if not path.exists():
        raise FileNotFoundError(f"Benchmark output not found: {path}")

    samples = {}

    for line in path.read_text(errors="replace").splitlines():
        # ---------------------------------------------------------------
        # Successful test
        # ---------------------------------------------------------------

        time_match = TIME_RE.search(line)

        if time_match:
            test_name = clean_test_name(time_match.group(1))

            # Ignore GTest summary/non-test lines.
            if test_name is None:
                continue

            value = float(time_match.group(2))
            unit = time_match.group(3).lower()

            value_ms = parse_time_to_ms(
                value,
                unit,
            )

            samples.setdefault(
                test_name,
                [],
            ).append(value_ms)

    # -------------------------------------------------------------------
    # Build final result.
    # -------------------------------------------------------------------

    results = {}

    for test_name, values in samples.items():
        if not values:
            results[test_name] = None
            continue

        results[test_name] = statistics.median(values)

    return results


def get_percentage_str(value_ms, reference_ms):
    """
    Return percentage change.

    Negative = faster/improvement.
    Positive = slower/degradation.
    """
    if value_ms is None or reference_ms is None or reference_ms == 0:
        return " (N/A)"

    pct = round(((value_ms - reference_ms) / reference_ms) * 100)

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

    # -------------------------------------------------------------------
    # Parse benchmark outputs.
    # -------------------------------------------------------------------

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
            "WARNING: No Existing benchmark results were found. "
            "Existing Driver will be shown as N/A."
        )

    # -------------------------------------------------------------------
    # Union of all test names.
    # -------------------------------------------------------------------

    all_tests = (
        set(existing_data.keys()) | set(current_data.keys()) | set(main_data.keys())
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

        # ---------------------------------------------------------------
        # Current vs Existing
        # ---------------------------------------------------------------

        current_pct = ""

        if current_ms is not None:
            current_pct = get_percentage_str(
                current_ms,
                existing_ms,
            )

        # ---------------------------------------------------------------
        # Main vs Current
        # ---------------------------------------------------------------

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

    # -------------------------------------------------------------------
    # Generate Markdown table.
    #
    # This intentionally follows the existing GHA table format.
    # -------------------------------------------------------------------

    h1 = "Test Case"
    h2 = "Existing Driver (Current)"
    h3 = f"Google Driver ({args.branch_name})"
    h4 = "Google Driver (Main)"

    w1 = max([len(h1)] + [len(row[0]) for row in rows]) if rows else len(h1)
    w2 = max([len(h2)] + [len(row[1]) for row in rows]) if rows else len(h2)
    w3 = max([len(h3)] + [len(row[2]) for row in rows]) if rows else len(h3)
    w4 = max([len(h4)] + [len(row[3]) for row in rows]) if rows else len(h4)

    table = (
        f"*Percentages in **{h3}** show change relative to "
        f"**{h2}**. Percentages in **{h4}** show change relative "
        f"to **{h3}**. Negative values indicate improvement "
        f"(faster test execution), positive values indicate "
        f"degradation (slower).*"
        "\n\n"
    )

    table += (
        f"| {h1.ljust(w1)} "
        f"| {h2.ljust(w2)} "
        f"| {h3.ljust(w3)} "
        f"| {h4.ljust(w4)} |\n"
    )

    table += (
        "|-"
        + ("-" * w1)
        + "-|-"
        + ("-" * w2)
        + "-|-"
        + ("-" * w3)
        + "-|-"
        + ("-" * w4)
        + "-|\n"
    )

    for row in rows:
        table += (
            f"| {row[0].ljust(w1)} "
            f"| {row[1].ljust(w2)} "
            f"| {row[2].ljust(w3)} "
            f"| {row[3].ljust(w4)} |\n"
        )

    # -------------------------------------------------------------------
    # Write output.
    # -------------------------------------------------------------------

    output_path = Path(args.output)
    output_path.write_text(table)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
