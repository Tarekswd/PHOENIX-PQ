#!/usr/bin/env bash
# Crafted-ciphertext experiment for HQC-1 (v5.1.0): compare decapsulation outputs of a reference build
# and a build whose RM decoder is the official AVX2 one.
# usage: crafted.sh <hqc_src> <work_dir> [n_crafted] [n_controls]
set -euo pipefail
S=$(realpath "$1"); W=$2; NC=${3:-2000}; NK=${4:-200}; V=1
HERE=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$W"; W=$(realpath "$W")
REF_INC="-I$S/src/common -I$S/src/common/hqc-$V -I$S/src/ref -I$S/src/ref/hqc-$V -I$S/lib/fips202"
AVX_INC="-I$S/src/common -I$S/src/common/hqc-$V -I$S/src/x86_64/common -I$S/src/x86_64/common/hqc-$V -I$S/src/x86_64/avx256 -I$S/src/x86_64/avx256/hqc-$V -I$S/lib/fips202"
AVX_FL="-DHQC_ARCH_X86_64=1 -DHQC_ARCH_X86_64_AVX256=1 -mavx -mavx2 -mbmi -mpclmul -funroll-all-loops"
FL="-std=c99 -O3 -g -fno-strict-aliasing"

# Reference library objects (all of src/common, src/ref, fips202)
mkdir -p "$W/ref"
for f in "$S"/src/common/*.c "$S"/src/ref/*.c "$S/lib/fips202/fips202.c"; do
    gcc $FL -DHQC_ARCH_REF=1 $REF_INC -c "$f" -o "$W/ref/$(basename "${f%.c}").o"
done
# AVX2 RM decoder with the same public symbol names, to swap into the reference library
gcc $FL $AVX_FL $AVX_INC -c "$S/src/x86_64/avx256/reed_muller.c" -o "$W/avx_reed_muller.o"
# Renamed side-by-side decoders for the divergence check (built by build.sh)
[ -f "$W/side/ref_rm.o" ] || bash "$HERE/build.sh" "$S" $V "$W/side" > /dev/null

gcc $FL -DHQC_ARCH_REF=1 -DCRAFT $REF_INC "$HERE/crafted.c" "$W"/ref/*.o "$W/side/ref_rm.o" "$W/side/avx_rm.o" -o "$W/craft"
gcc $FL -DHQC_ARCH_REF=1 $REF_INC "$HERE/crafted.c" "$W"/ref/*.o -o "$W/decaps_ref"
gcc $FL -DHQC_ARCH_REF=1 $REF_INC "$HERE/crafted.c" $(ls "$W"/ref/*.o | grep -v '/reed_muller.o$') "$W/avx_reed_muller.o" -o "$W/decaps_avx"

"$W/craft" "$W/cts.bin" "$NC" "$NK"
"$W/decaps_ref" "$W/cts.bin" > "$W/k_ref.txt"
"$W/decaps_avx" "$W/cts.bin" > "$W/k_avx.txt"
echo "records: $(wc -l < "$W/k_ref.txt")"
if cmp -s "$W/k_ref.txt" "$W/k_avx.txt"; then
    echo "RESULT: identical shared secrets from reference and AVX2-decoder builds for every ciphertext"
else
    echo "RESULT: outputs DIFFER"; diff "$W/k_ref.txt" "$W/k_avx.txt" | head -10
fi
# Sanity: crafted ciphertexts must all be rejected (implicit rejection keys are distinct per ciphertext)
awk '$2==1 {print $4}' "$W/k_ref.txt" | sort | uniq -d | wc -l | xargs echo "duplicate rejection keys among crafted:"
