#!/bin/bash

total=0 # Variable to store total time

if [ $# -lt 2 ]; then
  echo invalid args: \'"$*"\' >&2
  exit 1
fi

echo "Timing '${*:2}' over $1 runs..."

for _ in $(seq 1 "$1"); do
  # Capture the 'real' time, suppress stdout, and extract the time
  real_time=$( (time "${@:2}" >/dev/null) 2>&1 | grep real | awk '{print $2}')

  # Convert time (e.g., 0m1.234s) to seconds (floating-point format)
  time_in_seconds=$(echo "$real_time" | awk -F'[ms]' '{print $1 * 60 + $2}')

  total=$(echo "$total + $time_in_seconds" | bc)

  # maybe todo?
  # if [ "$1" -lt 10 ]; then
  #   echo "Run $_: ${time_in_seconds} seconds"
  # fi
done

# Calculate the average time
average=$(echo "scale=4; $total / $1" | bc)

echo "avg real time: $average seconds"
# TODO:
#  echo "total real time: $total seconds"
