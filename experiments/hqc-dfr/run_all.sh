#!/usr/bin/env bash
# Long natural-noise runs (all three parameter sets in parallel) plus conditional validation runs.
# usage: run_all.sh <bin_root> <out_dir> <seconds>
set -u
BIN=$1; OUT=$2; SECS=${3:-900}
mkdir -p "$OUT"
pids=()
for spec in "1 6" "3 4" "5 4"; do
    set -- $spec
    for p in $(seq "$2"); do
        "$BIN/hqc$1/dfr_sim" "$SECS" > "$OUT/natural_hqc$1_$p.txt" & pids+=($!)
    done
done
# Conditional validation of the uniform-within-block model at weights where the reference decoder fails
(
  for w in 150 155 160 165 170 175 180; do "$BIN/hqc1/dfr_sim" cond $w 40; done > "$OUT/cond_hqc1.txt"
  for w in 250 260 270 280 290; do "$BIN/hqc3/dfr_sim" cond $w 40; done > "$OUT/cond_hqc3.txt"
  for w in 250 260 270 280 290; do "$BIN/hqc5/dfr_sim" cond $w 40; done > "$OUT/cond_hqc5.txt"
) & pids+=($!)
wait "${pids[@]}"
echo "done: $(ls "$OUT" | wc -l) files"
