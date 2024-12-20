#!/bin/bash

n_runs=200
max_depth=6

./timing_script.sh "$n_runs" ./tree --depth="$max_depth" /
./timing_script.sh "$n_runs" ./test-O0 --depth="$max_depth" /
./timing_script.sh "$n_runs" ./test-O0 -B --depth="$max_depth" /
./timing_script.sh "$n_runs" ./test-O2 --depth="$max_depth" /
./timing_script.sh "$n_runs" ./test-O2 -B --depth="$max_depth" /
