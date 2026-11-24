#!/usr/bin/env python3
"""Display one or more binary field-frame files."""

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

from field_io import read_frame


FILENAME = ""
FIELD_LABEL = "phi"
COLORMAP_CHOICE = "parula"
N_CONTOURS = 10
VIEW_MAX_ROWS = 10
VIEW_MAX_COLS = 5
SAVE_TABLE_CSV = None


def mpl_cmap(name):
    return "viridis" if name.lower() == "parula" else name


def add_domain_badge(ax, is_unit):
    if is_unit:
        txt, clr = "unit domain [-1,1]^2", (0.1, 0.5, 0.1)
    else:
        txt, clr = "Physical Domain", (0.5, 0.1, 0.1)
    ax.text(0.01, 0.97, txt, transform=ax.transAxes, va="top", fontsize=8,
            color=clr, bbox=dict(facecolor="white", alpha=0.6, edgecolor="none"))


def run_single_file_view(filename):
    x, y, Z = read_frame(filename, warn=True)
    n, n_rows = x.size, y.size
    tol = 0.02
    is_unit_grid = (
        x.min() >= -1 - tol and x.max() <= 1 + tol
        and y.min() >= -1 - tol and y.max() <= 1 + tol
    )
    is_square = n_rows == n

    print(f"\n File   : {filename}")
    print(f" Grid   : {n} cols x {n_rows} rows{'  [square]' if is_square else '  [rectangular]'}")
    print(f" Domain : x in [{x.min():g}, {x.max():g}],  y in [{y.min():g}, {y.max():g}]"
          + ("  --> unit square [-1,1]^2 detected" if is_unit_grid
             else "  --> general / physical domain"))
    print(f" {FIELD_LABEL}     : min = {Z.min():g},  max = {Z.max():g}")
    print(f" Mean   :  {Z.mean():g}")
    print(f" Std    :  {Z.std(ddof=1):g}")

    r_show = min(n_rows, VIEW_MAX_ROWS)
    c_show = min(n, VIEW_MAX_COLS)
    print(f"--- Data table (rows: y, cols: x)  [{r_show} x {c_show} of {n_rows} x {n}] ---\n")
    print(f"{'y \\ x':>12}", end="")
    for j in range(c_show):
        print(f"  {x[j]:10.5f}", end="")
    if c_show < n:
        print("   ...", end="")
    print("\n" + "-" * (14 + c_show * 12 + 6))
    for i in range(r_show):
        print(f"{y[i]:12.5f}", end="")
        for j in range(c_show):
            print(f"  {Z[i, j]:10.6f}", end="")
        if c_show < n:
            print("   ...", end="")
        print()
    if r_show < n_rows:
        print(f"  ... ({n_rows - r_show} more rows)")
    print()

    if SAVE_TABLE_CSV:
        try:
            import pandas as pd
            df = pd.DataFrame(Z, index=[f"y={v:.4f}" for v in y],
                              columns=[f"x={v:.4f}" for v in x])
            df.to_csv(SAVE_TABLE_CSV)
            print(f"Full data table written to {SAVE_TABLE_CSV}")
        except ImportError:
            print("pandas not installed -- skipping CSV export.")

    X, Y = np.meshgrid(x, y)
    cmap = mpl_cmap(COLORMAP_CHOICE)
    short = Path(filename).name

    def set_limits(ax):
        if is_unit_grid:
            ax.set_xlim(-1, 1)
            ax.set_ylim(-1, 1)
        else:
            ax.set_xlim(x.min(), x.max())
            ax.set_ylim(y.min(), y.max())

    fig, ax = plt.subplots(num=f"{filename} - Colour Map")
    mesh = ax.pcolormesh(X, Y, Z, cmap=cmap, shading="gouraud")
    fig.colorbar(mesh, ax=ax, label=FIELD_LABEL)
    set_limits(ax)
    ax.set_aspect("equal")
    ax.set_xlabel("x")
    ax.set_ylabel("y")
    ax.set_title(f"{FIELD_LABEL} - Colour Map | {short}")
    add_domain_badge(ax, is_unit_grid)

    if is_square or n_rows > 1:
        fig, ax = plt.subplots(num=f"{filename} - Contours")
        cf = ax.contourf(X, Y, Z, N_CONTOURS, cmap=cmap)
        fig.colorbar(cf, ax=ax, label=FIELD_LABEL)
        ax.contour(X, Y, Z, N_CONTOURS, colors="k", linewidths=0.3)
        set_limits(ax)
        ax.set_aspect("equal")
        ax.set_xlabel("x")
        ax.set_ylabel("y")
        ax.set_title(f"{FIELD_LABEL} - contours | {short}")
        add_domain_badge(ax, is_unit_grid)

    fig = plt.figure(num=f"{filename} - 3D Surface")
    ax = fig.add_subplot(111, projection="3d")
    surf = ax.plot_surface(X, Y, Z, cmap=cmap, edgecolor="none", rcount=200, ccount=200)
    fig.colorbar(surf, ax=ax, label=FIELD_LABEL, shrink=0.7)
    ax.set_xlabel("x")
    ax.set_ylabel("y")
    ax.set_zlabel(FIELD_LABEL)
    ax.set_title(f"{FIELD_LABEL} - 3D surface")
    ax.view_init(elev=30, azim=-45)
    if is_unit_grid:
        ax.set_xlim(-1, 1)
        ax.set_ylim(-1, 1)

    mid_xi = int(np.argmin(np.abs(x)))
    mid_yi = int(np.argmin(np.abs(y)))
    fig, (ax1, ax2) = plt.subplots(2, 1, num=f"{filename} - Cross Sections")
    ax1.plot(x, Z[mid_yi, :], "b-", lw=1.5)
    ax1.set_xlabel("x")
    ax1.set_ylabel(FIELD_LABEL)
    ax1.set_title(f"Horizontal slice at y = {y[mid_yi]:.4f}")
    ax1.grid(True)
    ax2.plot(y, Z[:, mid_xi], "r-", lw=1.5)
    ax2.set_xlabel("y")
    ax2.set_ylabel(FIELD_LABEL)
    ax2.set_title(f"Vertical slice at x = {x[mid_xi]:.4f}")
    ax2.grid(True)
    if is_unit_grid:
        ax1.set_xlim(-1, 1)
        ax2.set_xlim(-1, 1)
    fig.suptitle(f"{FIELD_LABEL} - cross sections")
    fig.tight_layout()
    return "[unit square]" if is_unit_grid else "[physical domain]"


def main(argv=None):
    global FIELD_LABEL

    parser = argparse.ArgumentParser(description="View binary field-frame files.")
    parser.add_argument("input_files", nargs="*", help="Field frame paths")
    parser.add_argument("-i", "--input", action="append", dest="named", metavar="PATH")
    parser.add_argument("--field", default=FIELD_LABEL)
    args = parser.parse_args(argv)

    FIELD_LABEL = args.field
    files = list(args.input_files) + list(args.named or [])
    if not files:
        files = [FILENAME]
    for filename in files:
        run_single_file_view(filename)
    plt.show()


if __name__ == "__main__":
    main()
