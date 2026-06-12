#!/usr/bin/env python3
"""
plot_topology.py - visualise a CBTC result.

Draws two panels side by side:
    left  = full-power graph  (every pair within Rmax)   -- the "before"
    right = CBTC graph        (edges from the .edges file)-- the "after"

Usage:
    # first produce the edges with the C program:
    ./cbtc networks/grid.txt --edges grid.edges
    # then plot:
    python3 plot_topology.py networks/grid.txt grid.edges [out.png]

Needs: matplotlib  (pip install matplotlib)
"""
import sys
import math
import matplotlib
matplotlib.use("Agg")                 # save to file without a display
import matplotlib.pyplot as plt


def load_network(path):
    """Return (Rmax, [(x, y), ...]) from a network file."""
    rmax, pts, header = None, [], False
    with open(path) as f:
        for line in f:
            s = line.strip()
            if not s or s.startswith("#"):
                continue
            if not header:
                _, rmax = s.split()[:2]
                rmax = float(rmax)
                header = True
            else:
                x, y = s.split()[:2]
                pts.append((float(x), float(y)))
    return rmax, pts


def full_power_edges(pts, rmax):
    """All pairs within Rmax (the full-power topology)."""
    e = []
    for i in range(len(pts)):
        for j in range(i + 1, len(pts)):
            dx = pts[i][0] - pts[j][0]
            dy = pts[i][1] - pts[j][1]
            if math.hypot(dx, dy) <= rmax:
                e.append((i, j))
    return e


def load_edges(path):
    """Read CBTC edges (u v ...) from the .edges file."""
    e = []
    with open(path) as f:
        for line in f:
            s = line.strip()
            if not s or s.startswith("#"):
                continue
            a, b = s.split()[:2]
            e.append((int(a), int(b)))
    return e


def draw(ax, pts, edges, title):
    for u, v in edges:
        ax.plot([pts[u][0], pts[v][0]], [pts[u][1], pts[v][1]],
                "-", color="#2E75B6", linewidth=0.8, zorder=1)
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    ax.scatter(xs, ys, s=30, color="#C00000", zorder=2)
    avg_deg = 2 * len(edges) / len(pts) if pts else 0
    ax.set_title(f"{title}\n{len(edges)} edges,  avg degree {avg_deg:.2f}")
    ax.set_aspect("equal")
    ax.grid(True, linestyle=":", alpha=0.4)


def main():
    if len(sys.argv) < 3:
        print("usage: python3 plot_topology.py <network.txt> <edges.txt> [out.png]")
        sys.exit(1)

    net, edgefile = sys.argv[1], sys.argv[2]
    out = sys.argv[3] if len(sys.argv) > 3 else "topology.png"

    rmax, pts = load_network(net)
    before = full_power_edges(pts, rmax)
    after = load_edges(edgefile)

    fig, (axL, axR) = plt.subplots(1, 2, figsize=(11, 5.4))
    draw(axL, pts, before, f"Full power (Rmax={rmax:g})")
    draw(axR, pts, after, "After CBTC")
    fig.suptitle(net, fontsize=11)
    fig.tight_layout()
    fig.savefig(out, dpi=130)
    print("saved", out)


if __name__ == "__main__":
    main()
