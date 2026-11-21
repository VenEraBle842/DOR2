#!/usr/bin/env python3
import csv
import math
import sys

import matplotlib.pyplot as plt
from matplotlib.patches import Patch
from matplotlib.ticker import FuncFormatter


ORDERS = ("Ascending", "Descending", "Random")
PANELS = (
    ("Build", "Build structures", ("TreeDictionary", "SortedArrayDictionary")),
    ("ExactSearch", "Exact search", ("Linear", "TreeDictionary", "BinarySearch")),
    ("RangeSearch", "Range search", ("Linear", "BinarySearch")),
)
COLORS = {
    "Linear": "#df8833",
    "TreeDictionary": "#268578",
    "SortedArrayDictionary": "#8c6bb1",
    "BinarySearch": "#357abd",
}


def read_results(source: str) -> list[dict]:
    with open(source, newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    if not rows:
        raise ValueError("The benchmark CSV is empty.")

    seen = set()
    for row in rows:
        key = (row["Operation"], row["InputOrder"], row["Algorithm"])
        if key in seen:
            raise ValueError("The CSV must contain a single benchmark run, without duplicate rows.")
        seen.add(key)
        if row["Verified"].lower() != "true":
            raise ValueError(f"Cannot plot an unverified measurement: {key}.")
        for field in ("DatasetSize", "QueryCount", "Seed"):
            row[field] = int(row[field])
            if row[field] < 0:
                raise ValueError(f"{field} must not be negative.")
        row["ElapsedUs"] = float(row["ElapsedUs"])
        if not math.isfinite(row["ElapsedUs"]) or row["ElapsedUs"] < 0:
            raise ValueError("ElapsedUs must be finite and nonnegative.")

    if len({(row["DatasetSize"], row["Seed"]) for row in rows}) != 1:
        raise ValueError("The CSV must contain a single dataset size and seed.")
    expected = {
        (operation, order, algorithm)
        for operation, _, algorithms in PANELS
        for order in ORDERS
        for algorithm in algorithms
    }
    if seen != expected:
        raise ValueError("The CSV must contain all 21 supported benchmark measurements.")
    return rows


def format_time(value: float) -> str:
    if 0 < value < 0.001:
        return f"{value:.2g}"
    if value >= 1000:
        return f"{value:,.0f}"
    if value >= 10:
        return f"{value:.1f}"
    if value >= 1:
        return f"{value:.2f}"
    return f"{value:.3f}".rstrip("0").rstrip(".")


def draw_panel(axis, rows: list[dict], operation: str, title: str, algorithms: tuple) -> None:
    selected = [row for row in rows if row["Operation"] == operation]
    query_counts = {row["QueryCount"] for row in selected}
    if len(query_counts) != 1:
        raise ValueError(f"{operation} must use the same query count for all algorithms and orders.")
    query_count = query_counts.pop()
    if operation == "Build" and query_count != 1:
        raise ValueError("Build measurements must represent one build.")
    lookup = {(row["InputOrder"], row["Algorithm"]): row for row in selected}
    values = {}
    for key, row in lookup.items():
        values[key] = (
            row["ElapsedUs"] if operation == "Build"
            else row["ElapsedUs"] / query_count if query_count else math.nan
        )

    finite = [value for value in values.values() if math.isfinite(value)]
    logarithmic = bool(finite) and min(finite) > 0 and max(finite) / min(finite) >= 50
    if logarithmic:
        axis.set_yscale("log")
        axis.set_ylim(min(finite) / 2.5, max(finite) * 2)
        axis.yaxis.set_major_formatter(FuncFormatter(lambda value, _: f"{value:g}"))
    else:
        axis.set_ylim(0, max(finite, default=0) * 1.28 or 1)

    width = 0.72 / len(algorithms)
    for index, algorithm in enumerate(algorithms):
        positions = [group + (index - (len(algorithms) - 1) / 2) * width for group in range(3)]
        heights = [values[order, algorithm] for order in ORDERS]
        bars = axis.bar(positions, heights, width=width * 0.9, color=COLORS[algorithm], zorder=3)
        axis.bar_label(bars, labels=[format_time(value) if math.isfinite(value) else ""
                                    for value in heights], padding=5, fontsize=9, color="#24354b")
        for position, value in zip(positions, heights):
            if not math.isfinite(value):
                axis.text(position, 0.025, "N/A", transform=axis.get_xaxis_transform(),
                          ha="center", fontsize=9, color="#5b6b7e")

    axis.set_title(title, loc="left", fontsize=15, fontweight="bold", pad=37, color="#172b45")
    detail = "One build" if operation == "Build" else f"{query_count:,} queries per batch"
    scale = "log scale" if logarithmic else "linear scale"
    axis.text(0, 1.025, f"{detail} · {scale}", transform=axis.transAxes, fontsize=10, color="#5b6b7e")
    axis.set_xticks(range(3), ORDERS)
    axis.set_ylabel("Time per build (µs)" if operation == "Build" else "Time per query (µs)")
    axis.grid(axis="y", which="major", color="#e3e8ef", linewidth=0.8, zorder=0)
    axis.minorticks_off()
    axis.tick_params(axis="both", labelsize=10, length=0, pad=8)
    axis.spines[["top", "right"]].set_visible(False)
    for side in ("left", "bottom"):
        axis.spines[side].set_color("#d1d9e3")


def main() -> None:
    source = sys.argv[1] if len(sys.argv) > 1 else "benchmark_results.csv"
    target = sys.argv[2] if len(sys.argv) > 2 else "benchmark_plot.png"
    rows = read_results(source)

    with plt.rc_context({"font.family": "DejaVu Sans", "axes.labelcolor": "#44546a",
                         "xtick.color": "#44546a", "ytick.color": "#44546a"}):
        figure, axes = plt.subplots(1, 3, figsize=(16, 7), facecolor="#f7f9fc")
        figure.subplots_adjust(left=0.065, right=0.98, top=0.73, bottom=0.18, wspace=0.30)
        figure.suptitle("Student search benchmark", x=0.065, y=0.96, ha="left",
                         fontsize=23, fontweight="bold", color="#172b45")
        figure.text(0.065, 0.895,
                    f"{rows[0]['DatasetSize']:,} students  |  seed {rows[0]['Seed']}  |  all results verified",
                    fontsize=11, color="#5b6b7e")
        figure.legend(handles=[Patch(facecolor=color, label=algorithm) for algorithm, color in COLORS.items()],
                      loc="upper left", bbox_to_anchor=(0.058, 0.865), ncol=4, frameon=False, fontsize=10)
        for axis, (operation, title, algorithms) in zip(axes, PANELS):
            draw_panel(axis, rows, operation, title, algorithms)

        figure.text(0.065, 0.085, "Independent y-axes; logarithmic scales are marked. Lower is better.",
                    fontsize=10, color="#44546a")
        figure.text(0.065, 0.045,
                    "Search time per query = median batch time / query count. Zero-query batches are shown as N/A.",
                    fontsize=10, color="#5b6b7e")
        figure.savefig(target, dpi=180, facecolor=figure.get_facecolor())
        plt.close(figure)

if __name__ == "__main__":
    try:
        main()
    except (KeyError, ValueError) as error:
        sys.exit(f"Invalid benchmark CSV: {error}")
