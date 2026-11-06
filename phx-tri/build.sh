#!/usr/bin/env bash
# Build PHX-Tri: hardened HQC-3 (upstream v5.1.0 + hqc-ct-hardening.patch), ML-KEM-768 from liboqs,
# X25519/SHA-3 from OpenSSL 3. Produces libphx_tri.a, test_phx_tri, ct_phx_tri and bench_phx_tri.
#
# usage: build.sh <hqc_v5.1.0_src> <liboqs_src> <out_dir>      (CC=clang to build with Clang; OPT=-O2 etc.)
# Cross builds (see phx-tri/arm.sh): OPENSSL_DIR=<target OpenSSL prefix>, OQS_CMAKE_ARGS=<cmake toolchain args>,
# LDEXTRA=-static, TESTS_ONLY=1 (skip the x86-only constant-time harness and benchmark).
set -euo pipefail
HQC=$(realpath "$1"); OQS=$(realpath "$2"); O=$3
HERE=$(cd "$(dirname "$0")" && pwd)
CC=${CC:-gcc}; OPT=${OPT:--O3}; OPENSSL_DIR=${OPENSSL_DIR:-}; OQS_CMAKE_ARGS=${OQS_CMAKE_ARGS:-}
LDEXTRA=${LDEXTRA:-}; TESTS_ONLY=${TESTS_ONLY:-0}; OBJCOPY=${OBJCOPY:-objcopy}; AR=${AR:-ar}
mkdir -p "$O"; O=$(realpath "$O")

# 1. HQC-3 reference code, pinned to v5.1.0, with the constant-time hardening patch
rm -rf "$O/hqc" && mkdir -p "$O/hqc"
if [ -d "$HQC/.git" ]; then git -C "$HQC" archive v5.1.0 | tar -x -C "$O/hqc"; else cp -r "$HQC"/. "$O/hqc"; fi
(cd "$O/hqc" && patch -p1 --quiet < "$HERE/hqc-ct-hardening.patch" && patch -p1 --quiet < "$HERE/hqc-portability.patch")
HINC="-I$O/hqc/src/common -I$O/hqc/src/common/hqc-3 -I$O/hqc/src/ref -I$O/hqc/src/ref/hqc-3 -I$O/hqc/lib/fips202"
mkdir -p "$O/hqcobj"
for f in "$O"/hqc/src/common/*.c "$O"/hqc/src/ref/*.c "$O/hqc/lib/fips202/fips202.c"; do
    $CC -std=c99 -g $OPT -DHQC_ARCH_REF=1 $HINC -c "$f" -o "$O/hqcobj/$(basename "${f%.c}").o"
done
# HQC exports generic names (gf_mul, vect_mul, ...) that collide with OpenSSL's static libcrypto (gf_mul in
# curve448). Partially link HQC into one object and keep only its KEM entry points global.
$CC -r -nostdlib "$O"/hqcobj/*.o -o "$O/hqc_all.o"
$OBJCOPY --keep-global-symbol=hqc_kem_keygen --keep-global-symbol=hqc_kem_encaps \
         --keep-global-symbol=hqc_kem_decaps "$O/hqc_all.o"

# 2. liboqs with only ML-KEM-768 (static, no OpenSSL inside)
if [ ! -f "$O/liboqs/lib/liboqs.a" ]; then
    cmake -S "$OQS" -B "$O/liboqs" -DCMAKE_BUILD_TYPE=Release -DOQS_MINIMAL_BUILD="KEM_ml_kem_768" \
          -DOQS_USE_OPENSSL=OFF -DBUILD_SHARED_LIBS=OFF -DOQS_BUILD_ONLY_LIB=ON -DOQS_DIST_BUILD=ON \
          $OQS_CMAKE_ARGS > "$O/liboqs.cfg.log" 2>&1
    cmake --build "$O/liboqs" -j"$(nproc)" > "$O/liboqs.build.log" 2>&1
fi
OQSINC="-I$O/liboqs/include"; OQSLIB="$O/liboqs/lib/liboqs.a"

# 3. PHX-Tri library, test binary (debug hooks on), constant-time harness and benchmark
if [ -n "$OPENSSL_DIR" ]; then
    SSLINC="-I$OPENSSL_DIR/include"; SSLLIB="$(ls "$OPENSSL_DIR"/lib*/libcrypto.a | head -1) -ldl -pthread"
else
    SSLINC=""; SSLLIB="-lcrypto"
fi
CFLAGS="-std=c11 -g $OPT -Wall -Wextra -I$HERE/include $HINC $OQSINC $SSLINC"
$CC $CFLAGS -c "$HERE/src/phx_tri.c" -o "$O/phx_tri.o"
$CC $CFLAGS -DPHX_TRI_DEBUG -c "$HERE/src/phx_tri.c" -o "$O/phx_tri_dbg.o"
$AR rcs "$O/libphx_tri.a" "$O/phx_tri.o" "$O/hqc_all.o"
LIBS="$OQSLIB $SSLLIB $LDEXTRA"
$CC $CFLAGS -DPHX_TRI_DEBUG "$HERE/tests/test_phx_tri.c" "$O/phx_tri_dbg.o" "$O/hqc_all.o" $LIBS -o "$O/test_phx_tri"
if [ "$TESTS_ONLY" != 1 ]; then
    $CC $CFLAGS "$HERE/ct/ct_phx_tri.c" "$O/libphx_tri.a" $LIBS -o "$O/ct_phx_tri"
    $CC $CFLAGS "$HERE/bench/bench_phx_tri.c" "$O/libphx_tri.a" $LIBS -o "$O/bench_phx_tri"
    $CC $CFLAGS "$HERE/ct/dudect_phx_tri.c" "$O/libphx_tri.a" $LIBS -lm -o "$O/dudect_phx_tri"
fi
echo "built PHX-Tri in $O with $CC $OPT"
