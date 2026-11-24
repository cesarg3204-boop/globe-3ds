#!/bin/sh
# Build the desktop simulator and render screenshots: tools/sim/run.sh <script> <outdir>
set -e
cd "$(dirname "$0")/../.."
gcc -O2 -Itools/sim -o tools/sim/globe_sim source/main.c source/game.c source/globe.c source/countrydata.c tools/sim/sim.c -lm
python3 tools/sim/mkscript.py "$1" > tools/sim/.script.full
mkdir -p "$2"
SIM_SCRIPT=tools/sim/.script.full SIM_OUT="$2/log.txt" ./tools/sim/globe_sim
python3 tools/sim/render.py "$2/log.txt" "$2"
