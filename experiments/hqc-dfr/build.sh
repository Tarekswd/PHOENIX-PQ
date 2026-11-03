#!/usr/bin/env bash
# Build dfr_sim for one HQC parameter set, linking three Reed-Muller decoders side by side.
# usage: build.sh <hqc_src> <variant 1|3|5> <out_dir>
set -euo pipefail
S=$(realpath "$1"); V=$2; O=$3; HERE=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$O"; O=$(realpath "$O")
CC=gcc
REF_INC="-I$S/src/common -I$S/src/common/hqc-$V -I$S/src/ref -I$S/src/ref/hqc-$V -I$S/lib/fips202"
AVX_INC="-I$S/src/common -I$S/src/common/hqc-$V -I$S/src/x86_64/common -I$S/src/x86_64/common/hqc-$V -I$S/src/x86_64/avx256 -I$S/src/x86_64/avx256/hqc-$V -I$S/lib/fips202"
AVX_FL="-DHQC_ARCH_X86_64=1 -DHQC_ARCH_X86_64_AVX256=1 -mavx -mavx2 -mbmi -mpclmul -funroll-all-loops"
# -fno-strict-aliasing keeps the separate find_peaks aliasing bug (HQC #39) out of the measurement
COMMON="-std=c99 -O3 -fno-strict-aliasing -g"

# Rename the two entry points and hide every other global symbol, so three decoders can coexist
rename() {  # $1 obj  $2 prefix
    objcopy --keep-global-symbol=reed_muller_encode --keep-global-symbol=reed_muller_decode "$1"
    objcopy --redefine-sym reed_muller_encode=$2_rm_encode --redefine-sym reed_muller_decode=$2_rm_decode "$1"
}

$CC $COMMON -DHQC_ARCH_REF=1 $REF_INC -c "$S/src/ref/reed_muller.c" -o "$O/ref_rm.o"; rename "$O/ref_rm.o" ref
$CC $COMMON $AVX_FL $AVX_INC -c "$S/src/x86_64/avx256/reed_muller.c" -o "$O/avx_rm.o"; rename "$O/avx_rm.o" avx

grep -q 'int32_t width = 1 << (5 + MULTIPLICITY / 2);' "$S/src/x86_64/avx256/reed_muller.c"
sed 's/int32_t width = 1 << (5 + MULTIPLICITY \/ 2);/int32_t width = 1 << (6 + MULTIPLICITY \/ 2);/' \
    "$S/src/x86_64/avx256/reed_muller.c" > "$O/reed_muller_fix.c"
$CC $COMMON $AVX_FL $AVX_INC -c "$O/reed_muller_fix.c" -o "$O/fix_rm.o"; rename "$O/fix_rm.o" fix

$CC $COMMON -march=native $REF_INC "$HERE/dfr_sim.c" "$O/ref_rm.o" "$O/avx_rm.o" "$O/fix_rm.o" -o "$O/dfr_sim"

# Variant using HQC's own sampler for r1, r2, e: needs the reference vector/XOF code
mkdir -p "$O/lib"
for f in "$S/src/ref/vector.c" "$S/src/common/symmetric.c" "$S/lib/fips202/fips202.c"; do
    $CC $COMMON -DHQC_ARCH_REF=1 $REF_INC -c "$f" -o "$O/lib/$(basename "${f%.c}").o"
done
$CC $COMMON -march=native -DHQC_SAMPLER -DHQC_ARCH_REF=1 $REF_INC "$HERE/dfr_sim.c" "$O/ref_rm.o" "$O/avx_rm.o" "$O/fix_rm.o" \
    "$O"/lib/*.o -o "$O/dfr_sim_hqcsampler"
echo "built $O/dfr_sim and $O/dfr_sim_hqcsampler (HQC-$V)"
