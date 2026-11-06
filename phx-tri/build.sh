#!/usr/bin/env bash
# Build PHX-Tri: hardened HQC-3 (upstream v5.1.0 + hqc-ct-hardening.patch), ML-KEM-768 from liboqs,
# X25519/SHA-3 from OpenSSL 3. Produces libphx_tri.a, test_phx_tri, ct_phx_tri and bench_phx_tri.
#
# usage: build.sh <hqc_v5.1.0_src> <liboqs_src> <out_dir>      (CC=clang to build with Clang; OPT=-O2 etc.)
set -euo pipefail
HQC=$(realpath "$1"); OQS=$(realpath "$2"); O=$3
HERE=$(cd "$(dirname "$0")" && pwd)
CC=${CC:-gcc}; OPT=${OPT:--O3}
mkdir -p "$O"; O=$(realpath "$O")

# 1. HQC-3 reference code, pinned to v5.1.0, with the constant-time hardening patch
rm -rf "$O/hqc" && mkdir -p "$O/hqc"
if [ -d "$HQC/.git" ]; then git -C "$HQC" archive v5.1.0 | tar -x -C "$O/hqc"; else cp -r "$HQC"/. "$O/hqc"; fi
(cd "$O/hqc" && patch -p1 --quiet < "$HERE/hqc-ct-hardening.patch")
HINC="-I$O/hqc/src/common -I$O/hqc/src/common/hqc-3 -I$O/hqc/src/ref -I$O/hqc/src/ref/hqc-3 -I$O/hqc/lib/fips202"
mkdir -p "$O/hqcobj"
for f in "$O"/hqc/src/common/*.c "$O"/hqc/src/ref/*.c "$O/hqc/lib/fips202/fips202.c"; do
    $CC -std=c99 -g $OPT -DHQC_ARCH_REF=1 $HINC -c "$f" -o "$O/hqcobj/$(basename "${f%.c}").o"
done

# 2. liboqs with only ML-KEM-768 (static, no OpenSSL inside)
if [ ! -f "$O/liboqs/lib/liboqs.a" ]; then
    cmake -S "$OQS" -B "$O/liboqs" -DCMAKE_BUILD_TYPE=Release -DOQS_MINIMAL_BUILD="KEM_ml_kem_768" \
          -DOQS_USE_OPENSSL=OFF -DBUILD_SHARED_LIBS=OFF -DOQS_BUILD_ONLY_LIB=ON -DOQS_DIST_BUILD=ON > "$O/liboqs.cfg.log" 2>&1
    cmake --build "$O/liboqs" -j"$(nproc)" > "$O/liboqs.build.log" 2>&1
fi
OQSINC="-I$O/liboqs/include"; OQSLIB="$O/liboqs/lib/liboqs.a"

# 3. PHX-Tri library, test binary (debug hooks on), constant-time harness and benchmark
CFLAGS="-std=c11 -g $OPT -Wall -Wextra -I$HERE/include $HINC $OQSINC"
$CC $CFLAGS -c "$HERE/src/phx_tri.c" -o "$O/phx_tri.o"
$CC $CFLAGS -DPHX_TRI_DEBUG -c "$HERE/src/phx_tri.c" -o "$O/phx_tri_dbg.o"
ar rcs "$O/libphx_tri.a" "$O/phx_tri.o" "$O"/hqcobj/*.o
LIBS="$OQSLIB -lcrypto"
$CC $CFLAGS -DPHX_TRI_DEBUG "$HERE/tests/test_phx_tri.c" "$O/phx_tri_dbg.o" "$O"/hqcobj/*.o $LIBS -o "$O/test_phx_tri"
$CC $CFLAGS "$HERE/ct/ct_phx_tri.c" "$O/libphx_tri.a" $LIBS -o "$O/ct_phx_tri"
$CC $CFLAGS "$HERE/bench/bench_phx_tri.c" "$O/libphx_tri.a" $LIBS -o "$O/bench_phx_tri"
echo "built PHX-Tri in $O with $CC $OPT"
