#!/usr/bin/env bash
# Build one HQC source tree across {ref, avx256} x {gcc, clang} x {O0,O1,O2,O3,Os},
# run the official KAT check for each build, build the ctgrind harness, and scan
# objects for variable-latency division instructions.
#
# usage: build_matrix.sh <hqc_src_dir> <out_dir> <label> [variants, default "1"]
#
# Flags mirror the HQC CMake build (C99, -mavx -mavx2 -mbmi -mpclmul and unroll
# flags for avx256); only the -O level varies. -g adds debug info for reports.
set -euo pipefail

SRC=$(realpath "$1"); OUT=$2; LABEL=$3; VARIANTS=${4:-1}
HERE=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$OUT"
OUT=$(realpath "$OUT")

if [ -f "$SRC/src/common/hqc-kem.c" ]; then API=HQC_API_V51; else API=HQC_API_V50; fi
SUMMARY="$OUT/build_summary.tsv"
printf "build\tkat\tharness\n" > "$SUMMARY"

for arch in ref avx256; do
for cc in gcc clang; do
for opt in O0 O1 O2 O3 Os; do
for v in $VARIANTS; do
    id="$LABEL-$arch-$cc-$opt-hqc$v"
    d="$OUT/$id"; rm -rf "$d"; mkdir -p "$d/obj"

    if [ "$arch" = ref ]; then
        srcs=$(ls "$SRC"/src/common/*.c "$SRC"/src/ref/*.c "$SRC"/src/ref/hqc-$v/*.c 2>/dev/null || true)
        inc="-I$SRC/src/common -I$SRC/src/common/hqc-$v -I$SRC/src/ref -I$SRC/src/ref/hqc-$v"
        flags="-DHQC_ARCH_REF=1"
        gold="$SRC/kats/ref/hqc-$v"
    else
        srcs=$(ls "$SRC"/src/common/*.c "$SRC"/src/x86_64/common/*.c "$SRC"/src/x86_64/common/hqc-$v/*.c \
                  "$SRC"/src/x86_64/avx256/*.c "$SRC"/src/x86_64/avx256/hqc-$v/*.c 2>/dev/null || true)
        inc="-I$SRC/src/common -I$SRC/src/common/hqc-$v -I$SRC/src/x86_64/common -I$SRC/src/x86_64/common/hqc-$v"
        inc="$inc -I$SRC/src/x86_64/avx256 -I$SRC/src/x86_64/avx256/hqc-$v"
        flags="-DHQC_ARCH_X86_64=1 -DHQC_ARCH_X86_64_AVX256=1 -mavx -mavx2 -mbmi -mpclmul"
        if [ "$cc" = clang ]; then flags="$flags -funroll-loops"; else flags="$flags -funroll-all-loops"; fi
        gold="$SRC/kats/x86_64/avx256/hqc-$v"
    fi
    inc="$inc -I$SRC/lib/fips202"
    cflags="-std=c99 -g -$opt $flags $inc"

    for f in $srcs "$SRC/lib/fips202/fips202.c"; do
        $cc $cflags -c "$f" -o "$d/obj/$(basename "${f%.c}").o"
    done
    ar rcs "$d/libhqc.a" "$d"/obj/*.o

    # Official KAT: regenerate the .rsp and compare with the shipped gold file
    kat=FAIL
    katx=("$SRC/tests/kats/test_kat.c")
    ldx=()
    if [ -f "$SRC/lib/nist-rng/rng.c" ]; then katx+=("$SRC/lib/nist-rng/rng.c" "-I$SRC/lib/nist-rng"); ldx+=(-lcrypto); fi
    if $cc $cflags -w "${katx[@]}" "$d/libhqc.a" "${ldx[@]}" -o "$d/test_kat" 2> "$d/kat_build.log"; then
        # A failing KAT is a result to record, not a reason to stop the matrix
        if (cd "$d" && ./test_kat > kat_run.log 2>&1); then
            rsp=$(cd "$d" && ls PQCkemKAT_*.rsp)
            if cmp -s "$d/$rsp" "$gold/$rsp"; then kat=PASS; else kat=MISMATCH; fi
        fi
    fi

    harness=FAIL
    if $cc $cflags -D$API "$HERE/ct_harness.c" "$d/libhqc.a" -o "$d/ct_harness" 2> "$d/harness_build.log"; then
        harness=OK
    fi

    # Division / remainder instructions (operand-dependent latency on many CPUs)
    objdump -d --no-show-raw-insn "$d/libhqc.a" | awk '
        /^[0-9a-f]+ <.*>:$/ { fn = $2 }
        /\t(div|idiv)[bwlq]?\s/ { print fn "\t" $0 }' > "$d/divs.txt" || true

    printf "%s\t%s\t%s\n" "$id" "$kat" "$harness" | tee -a "$SUMMARY"
done; done; done; done
