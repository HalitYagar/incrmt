#!/usr/bin/env python3
"""Run grid-convergence and Richardson extrapolation analysis."""

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from contourpy import contour_generator
from scipy.interpolate import RegularGridInterpolator
from scipy.ndimage import map_coordinates

from field_io import read_named_frame


CONV_BASE_DIR = ""
CONV_RES = ["256", "512", "1280", "2520"]
CONV_FRAMES = [2, 7]
CONV_FRAME_LBLS = ["Pre-contact (t=0.10)", "At contact (t=0.35)"]
CONV_REF_RES = "2520"
CONV_COMMON_N = 200
CONV_RICHARDSON_TRIPLE = ["2520", "1280", "512"]


def shape_fidelity_error(res_dir, frame):
    _, _, phi = read_named_frame(res_dir, "phi", frame)
    _, _, xx = read_named_frame(res_dir, "X", frame)
    _, _, yy = read_named_frame(res_dir, "Y", frame)
    segments = contour_generator(z=phi).lines(0.0)
    if len(segments) == 0:
        raise RuntimeError(f"No zero-contour found in {res_dir} frame {frame}")
    segment = max(segments, key=len)
    columns, rows = segment[:, 0], segment[:, 1]
    xc = map_coordinates(xx, [rows, columns], order=1, mode="nearest")
    yc = map_coordinates(yy, [rows, columns], order=1, mode="nearest")
    cx, cy = xc.mean(), yc.mean()
    radius = np.hypot(xc - cx, yc - cy)
    return 100 * radius.std(ddof=1) / radius.mean()


def norms_relative(diff_z, ref_z):
    mask = ~np.isnan(diff_z) & ~np.isnan(ref_z)
    diff = diff_z[mask]
    ref = ref_z[mask]
    l1 = 100 * np.sum(np.abs(diff)) / np.sum(np.abs(ref))
    l2 = 100 * np.linalg.norm(diff) / np.linalg.norm(ref)
    linf = 100 * np.max(np.abs(diff)) / np.max(np.abs(ref))
    return l1, l2, linf


def interp_field_on_grid(x, y, z, xs, ys):
    x, y, z = np.asarray(x), np.asarray(y), np.asarray(z)
    if x[0] > x[-1]:
        x, z = x[::-1], z[:, ::-1]
    if y[0] > y[-1]:
        y, z = y[::-1], z[::-1, :]
    interp = RegularGridInterpolator((y, x), z, method="linear",
                                     bounds_error=False, fill_value=np.nan)
    xx, yy = np.meshgrid(xs, ys)
    return interp(np.stack([yy, xx], axis=-1))


def common_axes(x_result, x_ref, common_n):
    lo = max(x_result.min(), x_ref.min()) * 0.98
    hi = min(x_result.max(), x_ref.max()) * 0.98
    xs = np.linspace(lo, hi, common_n)
    return xs, xs.copy()


def field_error_norms(res_dir, ref_dir, field, frame, common_n):
    xr, yr, zr = read_named_frame(res_dir, field, frame)
    xf, yf, zf = read_named_frame(ref_dir, field, frame)
    xs, ys = common_axes(xr, xf, common_n)
    zrc = interp_field_on_grid(xr, yr, zr, xs, ys)
    zfc = interp_field_on_grid(xf, yf, zf, xs, ys)
    return norms_relative(zrc - zfc, zfc)


def velocity_error_norms(res_dir, ref_dir, frame, common_n):
    xr, yr, ur = read_named_frame(res_dir, "u", frame)
    _, _, vr = read_named_frame(res_dir, "v", frame)
    xf, yf, uf = read_named_frame(ref_dir, "u", frame)
    _, _, vf = read_named_frame(ref_dir, "v", frame)
    xs, ys = common_axes(xr, xf, common_n)
    urc = interp_field_on_grid(xr, yr, ur, xs, ys)
    vrc = interp_field_on_grid(xr, yr, vr, xs, ys)
    ufc = interp_field_on_grid(xf, yf, uf, xs, ys)
    vfc = interp_field_on_grid(xf, yf, vf, xs, ys)
    diff_mag = np.sqrt((urc - ufc) ** 2 + (vrc - vfc) ** 2)
    ref_mag = np.sqrt(ufc ** 2 + vfc ** 2)
    return norms_relative(diff_mag, ref_mag)


def observed_order_loglog(hs, errors):
    hs, errors = np.asarray(hs, float), np.asarray(errors, float)
    valid = errors > 0
    if valid.sum() < 2:
        return np.nan
    return np.polyfit(np.log(hs[valid]), np.log(errors[valid]), 1)[0]


def richardson_order_3grid(h1, f1, h2, f2, h3, f3):
    r21 = h2 / h1
    r32 = h3 / h2
    eps32 = f3 - f2
    eps21 = f2 - f1
    if eps21 == 0:
        return np.nan, f1, np.nan
    s = np.sign(eps32 / eps21) if eps32 != 0 else 1.0
    p = (abs(np.log(abs(eps32 / eps21)) / np.log(r21))
         if eps32 != 0 else 1.0)
    for _ in range(100):
        denom = r32 ** p - s
        q = np.log((r21 ** p - s) / denom) if denom != 0 else 0.0
        p_new = abs((1 / np.log(r21)) * np.log(abs(eps32 / eps21)) + q)
        if abs(p_new - p) < 1e-10:
            p = p_new
            break
        p = p_new
    f_extrap = f1 + (f1 - f2) / (r21 ** p - 1)
    e21 = abs((f1 - f2) / f1) if f1 != 0 else np.nan
    gci = 1.25 * e21 / (r21 ** p - 1) * 100
    return p, f_extrap, gci


def scalar_quantity_rms(res_dir, field, frame, xs, ys):
    if field == "vel":
        xu, yu, u = read_named_frame(res_dir, "u", frame)
        _, _, v = read_named_frame(res_dir, "v", frame)
        uc = interp_field_on_grid(xu, yu, u, xs, ys)
        vc = interp_field_on_grid(xu, yu, v, xs, ys)
        arr = np.sqrt(uc ** 2 + vc ** 2)
    else:
        xf, yf, z = read_named_frame(res_dir, field, frame)
        arr = interp_field_on_grid(xf, yf, z, xs, ys)
    arr = arr[~np.isnan(arr)]
    return np.linalg.norm(arr) / np.sqrt(arr.size)


def run_convergence_study(base_dir):
    base = Path(base_dir)
    nfr = len(CONV_FRAMES)
    shape_err = np.zeros((len(CONV_RES), nfr))
    print("\n--- Circle shape-fidelity error (radius std/mean %) ---")
    for ri, res in enumerate(CONV_RES):
        for fi, frame in enumerate(CONV_FRAMES):
            pct = shape_fidelity_error(base / res, frame)
            shape_err[ri, fi] = pct
            print(f"  N={res:<6s} frame={frame}  r_std/r_mean = {pct:.3f}%")

    fig, ax = plt.subplots(num="Circle shape-fidelity convergence")
    ax.plot(range(1, len(CONV_RES) + 1), shape_err, "-o", lw=1.8, ms=6)
    ax.set_xticks(range(1, len(CONV_RES) + 1))
    ax.set_xticklabels(CONV_RES)
    ax.set_xlabel("Grid resolution (N)")
    ax.set_ylabel("Radius std. dev. (% of mean radius)")
    ax.set_title("Circle shape-fidelity error vs. grid resolution")
    ax.legend(CONV_FRAME_LBLS, loc="best")
    ax.grid(True)

    compare_res = [r for r in CONV_RES if r != CONV_REF_RES]
    hs = 1.0 / np.array([float(r) for r in compare_res])
    nc = len(compare_res)
    tables = {k: np.zeros((nc, nfr))
              for k in ("vL1", "vL2", "vLi", "pL1", "pL2", "pLi")}
    print(f"\n--- Field convergence relative to N={CONV_REF_RES} ---")
    ref_dir = base / CONV_REF_RES
    for fi, frame in enumerate(CONV_FRAMES):
        print(f"\n{CONV_FRAME_LBLS[fi]}")
        print(f"{'N':>6} {'v,L1':>9} {'v,L2':>9} {'v,Linf':>9} "
              f"{'p,L1':>9} {'p,L2':>9} {'p,Linf':>9}")
        cols = {k: np.zeros(nc) for k in tables}
        for ri, res in enumerate(compare_res):
            res_dir = base / res
            vl1, vl2, vli = velocity_error_norms(res_dir, ref_dir, frame, CONV_COMMON_N)
            pl1, pl2, pli = field_error_norms(res_dir, ref_dir, "p", frame, CONV_COMMON_N)
            values = (vl1, vl2, vli, pl1, pl2, pli)
            keys = ("vL1", "vL2", "vLi", "pL1", "pL2", "pLi")
            for key, value in zip(keys, values):
                cols[key][ri] = value
            print(f"{res:>6} {vl1:8.3f}% {vl2:8.3f}% {vli:8.3f}% "
                  f"{pl1:8.3f}% {pl2:8.3f}% {pli:8.3f}%")
        for key in tables:
            tables[key][:, fi] = cols[key]
        orders = [observed_order_loglog(hs, cols[key]) for key in tables]
        print(f"{'order':>6} " + " ".join(f"({order:6.2f})" for order in orders))

    titles = {"vL1": "Velocity (L1)", "vL2": "Velocity (L2)", "vLi": "Velocity (Linf)",
              "pL1": "Pressure (L1)", "pL2": "Pressure (L2)", "pLi": "Pressure (Linf)"}
    fig, axes = plt.subplots(2, 3, num="Field convergence (L1, L2, Linf)")
    for index, (ax, key) in enumerate(zip(axes.ravel(), tables)):
        ax.plot(range(1, nc + 1), tables[key], "-o", lw=1.8)
        ax.set_xticks(range(1, nc + 1))
        ax.set_xticklabels(compare_res)
        ax.set_xlabel("Grid resolution (N)")
        ax.set_ylabel(f"Rel. error vs. N={CONV_REF_RES} (%)")
        ax.set_title(titles[key])
        if index == 0:
            ax.legend(CONV_FRAME_LBLS, loc="best")
        ax.grid(True)
    fig.tight_layout()

    trip = CONV_RICHARDSON_TRIPLE
    print(f"\n--- Richardson extrapolation (self-convergence, grids {', '.join(trip)}) ---")
    r1dir, r2dir, r3dir = (base / res for res in trip)
    h1, h2, h3 = (1.0 / float(res) for res in trip)
    x1, _, _ = read_named_frame(r1dir, "p", CONV_FRAMES[0])
    x3, _, _ = read_named_frame(r3dir, "p", CONV_FRAMES[0])
    lo = max(x1.min(), x3.min()) * 0.98
    hi = min(x1.max(), x3.max()) * 0.98
    conv_xs = np.linspace(lo, hi, CONV_COMMON_N)
    conv_ys = conv_xs.copy()

    for fi, frame in enumerate(CONV_FRAMES):
        print(f"\n{CONV_FRAME_LBLS[fi]}")
        for field in ("vel", "p"):
            f1 = scalar_quantity_rms(r1dir, field, frame, conv_xs, conv_ys)
            f2 = scalar_quantity_rms(r2dir, field, frame, conv_xs, conv_ys)
            f3 = scalar_quantity_rms(r3dir, field, frame, conv_xs, conv_ys)
            p_est, f_extrap, gci = richardson_order_3grid(h1, f1, h2, f2, h3, f3)
            print(f"  {field:<6}: f(N={trip[0]})={f1:.6f}  f(N={trip[1]})={f2:.6f}  "
                  f"f(N={trip[2]})={f3:.6f}")
            print(f"          observed order p={p_est:.3f}   "
                  f"extrapolated value={f_extrap:.6f}   GCI(finest)={gci:.3f}%")


def main(argv=None):
    parser = argparse.ArgumentParser(description="Run grid-convergence analysis.")
    parser.add_argument("base_dir", nargs="?", default=CONV_BASE_DIR)
    parser.add_argument("-c", "--convergence-dir", dest="named_dir")
    args = parser.parse_args(argv)
    run_convergence_study(args.named_dir or args.base_dir)
    plt.show()


if __name__ == "__main__":
    main()
