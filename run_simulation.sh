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
stdbuf -oL -eL ./fconfig "$config_file" "$run_dir" 2>&1 | tee -a "$log_file"

odr="$run_dir/$simulation_name.odr"
if [ ! -d "$odr" ]; then
    echo "Simulation output directory not found: $odr" >&2
    exit 1
fi
cp "$config_file" "$run_dir/$simulation_name.cfg"

echo "Generating plot and movie..." >&2
./gnuplot_movie.pl -t "$odr" w -10 10 2>&1 | tee -a "$log_file"
