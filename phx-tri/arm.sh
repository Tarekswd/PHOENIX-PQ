#!/usr/bin/env bash
# Cross-build PHX-Tri for AArch64 and ARMv7 (hard-float), run the self-tests under QEMU user mode, and check
# that the ARM builds reproduce the committed (x86-generated) test vectors byte for byte.
#
# usage: arm.sh <hqc_v5.1.0_src> <liboqs_src> <cross_openssl_root> <out_dir>
#   <cross_openssl_root>/{aarch64,armv7} are OpenSSL 3.0.13 installs built with no-shared
#   (Configure linux-aarch64 / linux-armv4 -march=armv7-a+fp).
# Needs gcc-aarch64-linux-gnu, gcc-arm-linux-gnueabihf (+ libc6-dev-*-cross) and qemu-user.
set -euo pipefail
HQC=$1; OQS=$2; XSSL=$(realpath "$3"); O=$4
HERE=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$O"; O=$(realpath "$O")

for t in aarch64 armv7; do
    case $t in
        aarch64) cc=aarch64-linux-gnu-gcc;   proc=aarch64; qemu=qemu-aarch64 ;;
        armv7)   cc=arm-linux-gnueabihf-gcc; proc=armv7;   qemu=qemu-arm ;;
    esac
    tc="$O/$t.cmake"
    cat > "$tc" <<EOF
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR $proc)
set(CMAKE_C_COMPILER $cc)
EOF
    CC=$cc OBJCOPY=${cc%gcc}objcopy AR=${cc%gcc}ar \
    OPENSSL_DIR="$XSSL/$t" OQS_CMAKE_ARGS="-DCMAKE_TOOLCHAIN_FILE=$tc" LDEXTRA=-static TESTS_ONLY=1 \
        bash "$HERE/build.sh" "$HQC" "$OQS" "$O/$t" > "$O/$t.build.log" 2>&1 || { tail -20 "$O/$t.build.log"; exit 1; }
    echo "== $t: $(file -b "$O/$t/test_phx_tri" | cut -d, -f1-2)"
    $qemu "$O/$t/test_phx_tri" quick
    $qemu "$O/$t/test_phx_tri" vectors 8 > "$O/$t/vectors.txt"
    python3 "$HERE/tests/check_vectors.py" "$O/$t/vectors.txt" "$HERE/vectors/phx_tri_v0.txt"
done
