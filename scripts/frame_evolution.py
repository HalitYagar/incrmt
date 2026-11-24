#!/usr/bin/env python3
"""Analyse field evolution across numbered frame files."""

import argparse
import contextlib
import re
import warnings
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from matplotlib import animation

from field_io import read_named_frame


FILENAME = ""
FIELD_LABEL = "phi"
EVO_MODE = "stats"
EVO_STRIDE = 1
EVO_PARALLEL = False
EVO_SLICE_Y = 0.0
EVO_DOWNSAMPLE = 1
EVO_CLIM = None
EVO_VIDEO_NAME = "field_evolution.mp4"
EVO_FPS = 15


def find_frames(odr_dir, field):
    pattern = re.compile(rf"^{re.escape(field)}\.(\d+)$")
    frames = []
    for path in Path(odr_dir).iterdir():
        match = pattern.match(path.name)
        if match:
            frames.append(int(match.group(1)))
    return sorted(frames)


def run_stats(odr_dir, field, frames, use_parallel):
    def stats(frame):
        _, _, z = read_named_frame(odr_dir, field, frame)
        return z.min(), z.max(), z.mean(), z.std(ddof=1)

    print(f"Scanning {len(frames)} frames for {field} statistics...")
    if use_parallel:
        with ThreadPoolExecutor() as pool:
            results = list(pool.map(stats, frames))
    else:
        results = []
        for index, frame in enumerate(frames, start=1):
            results.append(stats(frame))
            if index % 50 == 0:
                print(f"  {index} / {len(frames)}")

    tmin, tmax, tmean, tstd = (np.array(column) for column in zip(*results))
    frame_numbers = np.asarray(frames)
    fig, ax = plt.subplots(num=f"{field} statistics over time")
    ax.fill_between(frame_numbers, tmean - tstd, tmean + tstd,
                    color=(0.3, 0.3, 0.9), alpha=0.15, lw=0,
                    label=r"mean $\pm$ 1 std")
    ax.plot(frame_numbers, tmin, "b-", lw=1, label="min")
    ax.plot(frame_numbers, tmax, "r-", lw=1, label="max")
    ax.plot(frame_numbers, tmean, "k-", lw=1.8, label="mean")
    ax.legend(loc="best")
    ax.set_xlabel("frame")
    ax.set_ylabel(field)
    ax.grid(True)
    ax.set_title(f"{field} evolution over {len(frames)} frames")


def run_kymograph(odr_dir, field, frames, slice_y):
    print(f"Building kymograph from {len(frames)} frames (y={slice_y:.4g} slice)...")
    x0, y0, z0 = read_named_frame(odr_dir, field, frames[0])
    y_index = int(np.argmin(np.abs(y0 - slice_y)))
    kymograph = np.zeros((len(frames), x0.size))
    kymograph[0] = z0[y_index]
    for index in range(1, len(frames)):
        _, _, z = read_named_frame(odr_dir, field, frames[index])
        kymograph[index] = z[y_index]
        if (index + 1) % 50 == 0:
            print(f"  {index + 1} / {len(frames)}")

    fig, ax = plt.subplots(num=f"{field} kymograph")
    image = ax.imshow(kymograph, origin="lower", aspect="auto", cmap="viridis",
                      extent=[x0[0], x0[-1], frames[0], frames[-1]])
    fig.colorbar(image, ax=ax, label=field)
    ax.set_xlabel("x")
    ax.set_ylabel("frame")
    ax.set_title(f"{field}(x,t) at y = {y0[y_index]:.4f}")


def run_animate(odr_dir, field, frames, downsample, clim, save_video, video_name, fps):
    x, y, z = read_named_frame(odr_dir, field, frames[0])
    if downsample > 1:
        x, y, z = x[::downsample], y[::downsample], z[::downsample, ::downsample]
    xx, yy = np.meshgrid(x, y)

    fig, ax = plt.subplots(num=f"{field} animation")
    mesh = ax.pcolormesh(xx, yy, z, cmap="viridis", shading="gouraud")
    fig.colorbar(mesh, ax=ax, label=field)
    ax.set_aspect("equal")
    ax.set_xlim(x.min(), x.max())
    ax.set_ylim(y.min(), y.max())
    ax.set_xlabel("x")
    ax.set_ylabel("y")
    title = ax.set_title(f"{field} - frame {frames[0]}")
    if clim is not None:
        mesh.set_clim(*clim)

    writer = animation.FFMpegWriter(fps=fps) if save_video else None
    context = (writer.saving(fig, video_name, dpi=100)
               if save_video else contextlib.nullcontext())
    print(f"Playing/recording {len(frames)} frames...")
    with context:
        for frame in frames:
            _, _, z = read_named_frame(odr_dir, field, frame)
            if downsample > 1:
                z = z[::downsample, ::downsample]
            mesh.set_array(z.ravel())
            if clim is None:
                mesh.set_clim(z.min(), z.max())
            title.set_text(f"{field} - frame {frame}")
            if save_video:
                writer.grab_frame()
            else:
                plt.pause(0.001)
    if save_video:
        print(f"Saved video to {video_name}")


def run_evolution(filename, field=FIELD_LABEL):
    odr_dir = Path(filename).parent
    frames = find_frames(odr_dir, field)
    if not frames:
        warnings.warn(f"No {field}.N files found in {odr_dir} - skipping evolution section.")
        return
    if EVO_STRIDE > 1:
        frames = frames[::EVO_STRIDE]

    if EVO_MODE == "stats":
        run_stats(odr_dir, field, frames, EVO_PARALLEL)
    elif EVO_MODE == "kymograph":
        run_kymograph(odr_dir, field, frames, EVO_SLICE_Y)
    elif EVO_MODE == "video":
        run_animate(odr_dir, field, frames, EVO_DOWNSAMPLE, EVO_CLIM,
                    True, EVO_VIDEO_NAME, EVO_FPS)
    else:
        run_animate(odr_dir, field, frames, EVO_DOWNSAMPLE, EVO_CLIM,
                    False, EVO_VIDEO_NAME, EVO_FPS)


def main(argv=None):
    global EVO_MODE, EVO_STRIDE

    parser = argparse.ArgumentParser(description="Analyse evolution of numbered field frames.")
    parser.add_argument("input", nargs="?", default=FILENAME)
    parser.add_argument("-i", "--input", dest="named_input")
    parser.add_argument("--field", default=FIELD_LABEL)
    parser.add_argument("--mode", choices=("stats", "kymograph", "animate", "video"),
                        default=EVO_MODE)
    parser.add_argument("--stride", type=int, default=EVO_STRIDE)
    args = parser.parse_args(argv)

    EVO_MODE = args.mode
    EVO_STRIDE = args.stride
    filename = args.named_input or args.input
    run_evolution(filename, args.field)
    plt.show()


if __name__ == "__main__":
    main()
