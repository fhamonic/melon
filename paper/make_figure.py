"""Render paper/figure.png from a melon_benchmark results directory.

Usage: python make_figure.py <path/to/melon_benchmark> [<run id>]

Reads the raw Google Benchmark JSON tracked under results/<run id>/ (default:
the only run directory) through melon_benchmark's own bench_results.py, so the
figure and the benchmark site cannot disagree about which rows count.
"""

import os
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

# ---- data ------------------------------------------------------------------

bench_root = sys.argv[1]
sys.path.insert(0, bench_root)
from bench_results import collect, iter_runs  # noqa: E402

runs = list(iter_runs(os.path.join(bench_root, "results")))
run_id = sys.argv[2] if len(sys.argv) > 2 else runs[0][0]
run_dir = os.path.join(bench_root, "results", run_id)
medians, _ = collect(run_dir)

INSTANCE = "USA-road-t.NE"
LIBS = ["melon", "lemon", "boost"]
LABELS = {"melon": "MELON", "lemon": "LEMON", "boost": "Boost.Graph"}
# Categorical slots 1-3 (validated adjacent-pair CVD safe on a light surface).
COLORS = {"melon": "#2a78d6", "lemon": "#eb6834", "boost": "#1baf7a"}
INK, INK2, MUTED, GRID, AXIS = "#0b0b0b", "#52514e", "#898781", "#e1e0d9", "#c3c2b7"

PANEL_A = [
    ("breadth_first_search", "", "BFS"),
    ("depth_first_search", "", "DFS"),
    ("dijkstra", "int|4-heap", "Dijkstra\n(int, 4-heap)"),
    ("strongly_connected_components", "", "Strong\ncomponents"),
    ("weakly_connected_components", "", "Weak\ncomponents"),
    ("traversal_forest", "", "Traversal\nforest"),
]


def fastest(series_to_values, lib):
    """Each library in its fastest container on INSTANCE, or None."""
    best = None
    for series, values in series_to_values.items():
        if series.split("::")[0] != lib or INSTANCE not in values:
            continue
        if best is None or values[INSTANCE] < best:
            best = values[INSTANCE]
    return best


panel_a = {
    label: {lib: fastest(medians[(alg, params, "9th_dimacs")], lib) for lib in LIBS}
    for alg, params, label in PANEL_A
}

KS = [("k100", "100"), ("k1000", "1 000"), ("k10000", "10 000"), ("kall", "all\n(1.52 M)")]
PANEL_B = [
    ("melon::static_digraph:streaming", "MELON, streaming", "melon", "-", "o"),
    ("melon::static_digraph:stored", "MELON, storing maps", "melon", "--", "s"),
    ("lemon::StaticDigraph:streaming", "LEMON, streaming", "lemon", "-", "^"),
    ("boost::compressed_sparse_row:streaming", "Boost.Graph, streaming", "boost", "-", "D"),
]
panel_b = {
    series: [medians[("dijkstra_bounded", "int|" + k, "9th_dimacs")][series][INSTANCE] for k, _ in KS]
    for series, *_ in PANEL_B
}

# ---- figure ----------------------------------------------------------------

plt.rcParams.update({
    "font.family": "sans-serif",
    "font.size": 8,
    "axes.edgecolor": AXIS,
    "axes.labelcolor": INK2,
    "xtick.color": INK2,
    "ytick.color": INK2,
    "xtick.labelcolor": INK2,
    "ytick.labelcolor": INK2,
    "text.color": INK,
})

fig, (ax_a, ax_b) = plt.subplots(
    1, 2, figsize=(8.0, 3.2), gridspec_kw={"width_ratios": [1.55, 1]}
)

# Panel A: grouped bars, one group per algorithm, one bar per library.
groups = list(panel_a)
x = np.arange(len(groups))
width = 0.26
for i, lib in enumerate(LIBS):
    values = [panel_a[g][lib] for g in groups]
    offset = (i - 1) * width
    bars = ax_a.bar(
        x + offset,
        [v if v is not None else 0 for v in values],
        width * 0.92,
        color=COLORS[lib],
        label=LABELS[lib],
        linewidth=0,
    )
    for rect, v in zip(bars, values):
        if v is None:
            ax_a.text(rect.get_x() + rect.get_width() / 2, 2, "n/a", ha="center",
                      va="bottom", fontsize=6.5, color=MUTED, rotation=90)
            continue
        ax_a.text(rect.get_x() + rect.get_width() / 2, v + 1.5, f"{v:.0f}",
                  ha="center", va="bottom", fontsize=6.5, color=INK2)
ax_a.set_xticks(x)
ax_a.set_xticklabels(groups, fontsize=7)
ax_a.set_ylabel("milliseconds per run (median of 5)")
ax_a.set_title("(a) Full-graph runs", loc="left", fontsize=9, color=INK)
ax_a.legend(frameon=False, fontsize=7, loc="upper left")
ax_a.yaxis.grid(True, color=GRID, linewidth=0.8)
ax_a.set_axisbelow(True)
ax_a.margins(y=0.12)
for side in ("top", "right"):
    ax_a.spines[side].set_visible(False)
ax_a.tick_params(length=0)

# Panel B: k-nearest query cost as a function of k, log y.
xb = np.arange(len(KS))
for series, label, lib, style, marker in PANEL_B:
    ax_b.plot(xb, panel_b[series], style, color=COLORS[lib], marker=marker,
              markersize=4.5, linewidth=1.6, label=label,
              markeredgecolor="white", markeredgewidth=0.8)
ax_b.set_yscale("log")
ax_b.set_xticks(xb)
ax_b.set_xticklabels([lbl for _, lbl in KS], fontsize=7)
ax_b.set_xlabel("k settled vertices per query")
ax_b.set_ylabel("milliseconds per query, log scale")
ax_b.set_title("(b) Early-exit Dijkstra (k-nearest)", loc="left", fontsize=9, color=INK)
ax_b.legend(frameon=False, fontsize=6.5, loc="upper left")
ax_b.yaxis.grid(True, color=GRID, linewidth=0.8, which="major")
ax_b.set_axisbelow(True)
ax_b.set_xlim(-0.3, len(KS) - 0.7)
for side in ("top", "right"):
    ax_b.spines[side].set_visible(False)
ax_b.tick_params(length=0)

fig.tight_layout(w_pad=2.0)
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "figure.png")
fig.savefig(out, dpi=220, facecolor="white")
print("wrote", out)
