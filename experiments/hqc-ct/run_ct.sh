#!/usr/bin/env bash
# Run the ctgrind harness under Valgrind memcheck for every build in <out_dir>.
# usage: run_ct.sh <out_dir> [jobs, default nproc]
set -euo pipefail
OUT=$(realpath "$1"); JOBS=${2:-$(nproc)}

run_one() {
    d=$1; mode=$2
    [ -x "$d/ct_harness" ] || exit 0
    valgrind --tool=memcheck --error-limit=no --num-callers=8 \
        --log-file="$d/vg.$mode.log" "$d/ct_harness" "$mode" > /dev/null 2>&1
    echo "$? $(basename "$d") $mode" >> "$d/vg.exit"
}
export -f run_one

for d in "$OUT"/*/; do rm -f "$d/vg.exit"; done
for d in "$OUT"/*/; do
    for m in keygen encaps decaps decaps-invalid; do echo "${d%/} $m"; done
done | xargs -P "$JOBS" -n 2 bash -c 'run_one "$0" "$1"'
cat "$OUT"/*/vg.exit | awk '$1 != 0 { bad++; print "NONZERO EXIT:", $0 } END { print "harness runs with nonzero exit:", bad + 0 }'
