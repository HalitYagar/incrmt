#!/bin/bash
set -e
set -o pipefail

if [ "$#" -ne 1 ]; then
    echo "Usage: ./run_simulation.sh <configuration file>" >&2
    exit 1
fi

config_file=$1
if [ ! -f "$config_file" ]; then
    echo "Configuration file not found: $config_file" >&2
    exit 1
fi

export OMP_NUM_THREADS=4
timestamp=$(date +%Y%m%d_%H%M%S)
run_dir="output/run_${timestamp}"
mkdir -p "$run_dir"
log_file="$run_dir/run.log"

simulation_name=$(sed -n 's/^[[:space:]]*output_name[[:space:]]*=[[:space:]]*//p' "$config_file" | head -n 1)
simulation_name=${simulation_name%$'\r'}
if [ -z "$simulation_name" ]; then
    simulation_name=$(basename "$config_file")
    simulation_name=${simulation_name%.cfg}
fi

echo "Running fconfig..." >&2
simulation_status=0
if stdbuf -oL -eL ./fconfig "$config_file" "$run_dir" 2>&1 | tee -a "$log_file"; then
    :
else
    simulation_status=$?
    echo "Simulation failed with status $simulation_status; checking for partial output." >&2
fi

odr="$run_dir/$simulation_name.odr"
if [ ! -d "$odr" ]; then
    echo "Simulation output directory not found: $odr" >&2
    if [ "$simulation_status" -ne 0 ]; then
        exit "$simulation_status"
    fi
    exit 1
fi
cp "$config_file" "$run_dir/$simulation_name.cfg"

movie_file="$run_dir/${simulation_name}_w.mov"
movie_status=0
if [ ! -f "$odr/w.0" ]; then
    echo "No initial vorticity frame found in $odr; skipping plot/movie generation." >&2
    movie_status=1
else
    echo "Generating plot and movie from available frames..." >&2
    if ./gnuplot_movie.pl -t -u "$odr" w -10 10 2>&1 | tee -a "$log_file"; then
        if [ ! -s "$movie_file" ]; then
            echo "Movie was not created: $movie_file" >&2
            movie_status=1
        fi
    else
        movie_status=$?
        echo "Plot/movie generation failed with status $movie_status." >&2
    fi
fi

if [ "$simulation_status" -ne 0 ]; then
    if [ "$movie_status" -eq 0 ]; then
        echo "Video from partial simulation output: $movie_file" >&2
    fi
    exit "$simulation_status"
fi

exit "$movie_status"
