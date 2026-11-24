"""Shared binary field-file I/O helpers."""

import warnings
from pathlib import Path

import numpy as np


def read_frame(path, warn=False):
    """Read one binary field file. Returns x (n,), y (n_rows,), Z (n_rows, n)."""
    path = Path(path)
    if not path.is_file():
        raise FileNotFoundError(f"Could not open file: {path}\nCheck the path is correct.")

    raw = np.fromfile(path, dtype="<f4").astype(np.float64)
    n = int(round(raw[0]))
    x = raw[1:n + 1].copy()

    n_rows_exact = (raw.size - (n + 1)) / (n + 1)
    if warn and abs(n_rows_exact - round(n_rows_exact)) > 1e-9:
        warnings.warn("File size does not divide evenly. Expected integer rows, "
                      f"got {n_rows_exact:.6f}. Possible wrong endianness or corrupt file.")
    n_rows = int(round(n_rows_exact))

    body = raw[n + 1:n + 1 + n_rows * (n + 1)].reshape(n_rows, n + 1)
    y = body[:, 0].copy()
    Z = body[:, 1:].copy()
    return x, y, Z


def read_named_frame(odr_dir, field, frame):
    """Read frame `field.frame` from `odr_dir`."""
    return read_frame(Path(odr_dir) / f"{field}.{int(frame)}")
