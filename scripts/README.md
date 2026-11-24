# Analysing output fields

The scripts in this directory read binary fields produced by the IncRMT C++ code:

- `field_viewer.py` displays selected field frames;
- `frame_evolution.py` analyses evolution over numbered frame files;
- `grid_convergence.py` analyses grid convergence and Richardson extrapolation.

## Requirements

Install Python 3 and the required packages.

```bash
python -m pip install numpy scipy matplotlib contourpy pandas
```

Video export with `EVO_MODE = "video"` additionally requires FFmpeg available
as `ffmpeg` on `PATH`.

## Input file format

Each input is a binary field frame whose name has the form `field.frame`, for
example `phi.20`. The file layout is a 32-bit little-endian float grid:

```text
[ grid width n ]
[ n x-coordinates ]
[ y-coordinate, n field values ]
[ y-coordinate, n field values ]
...
```

For evolution analysis, pass a frame file from the output directory. The script
automatically searches that frame's parent directory for files with the selected
field name.

## Basic usage

From the repository root:

```bash
python scripts/field_viewer.py /path/to/circ-move.odr/phi.20
```

The input can also be supplied with `-i` or `--input`:

```bash
python scripts/field_viewer.py --input /path/to/circ-move.odr/phi.20
```

To inspect several files:

```bash
python scripts/field_viewer.py \
  /path/to/circ-move.odr/phi.20 \
  /path/to/circ-move.odr/phi.40
```

## Script options

Run `python scripts/<script-name>.py --help` to show each script's options.

- `field_viewer.py [INPUT ...]` and `-i PATH` view one or more field frames.
- `frame_evolution.py [INPUT]`, `--field NAME`, `--mode MODE`, and
  `--stride N` analyse frame evolution.
- `grid_convergence.py [BASE_DIRECTORY]` and `-c PATH` run convergence analysis.
  The base directory contains subdirectories such as `256`, `512`, `1280`, and
  `2520`.

## Examples

View one frame:

```bash
python scripts/field_viewer.py /path/to/output/circ-move.odr/phi.20
```

Analyse the evolution of `phi`:

```bash
python scripts/frame_evolution.py --field phi --mode stats \
  /path/to/output/circ-move.odr/phi.20
```

Run a convergence study:

```bash
python scripts/grid_convergence.py /path/to/convergence-results
```

## Configuration

The settings near the top of each dedicated script control file names, plotting,
evolution modes, and convergence grids. Key settings include:

- `FILENAME` / `FIELD_LABEL`: fallback input and field name.
- `SAVE_TABLE_CSV`: set to a filename in `field_viewer.py` to export a table as
  CSV.
- `EVO_MODE` / `EVO_STRIDE`: evolution mode and frame interval in
  `frame_evolution.py`.
- `CONV_BASE_DIR`, `CONV_RES`, `CONV_FRAMES`: convergence settings in
  `grid_convergence.py`.
