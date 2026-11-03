**Title:** HQC: imported code still has upstream HQC issue #31 (uint8_t buffers passed as uint64_t *)

liboqs imports HQC from `gitlab.com/pqc-hqc/hqc` at `161cd4fdf6b4a5198cf40b3a1243f9f27f13e03d` (`scripts/copy_from_upstream/copy_from_upstream.yml`). That is the commit cited in upstream HQC issue #31 (https://gitlab.com/pqc-hqc/hqc/-/issues/31):

> `crypto_kem_enc()` and `crypto_kem_dec()` declare the message buffer as a `uint8_t` array and then pass it to `hqc_pke_encrypt()` / `hqc_pke_decrypt()` through a `(uint64_t *)` cast. [...] This is undefined behaviour, and on Armv7 it can fault.

The same casts are in liboqs at `src/kem/hqc/pqc-hqc_hqc-{1,3,5}_ref/kem.c`, lines 124, 192 and 200. The buffers are `uint8_t m[PARAM_SECURITY_BYTES]` / `uint8_t m_prime[...]` with no alignment attribute. `CONFIGURE.md` lists ARM32v7 as a supported target. The integration patch already fixes the related 32-bit `BITMASK` problem (upstream #26), but not this one.

What we checked:
- Cross-built liboqs at `8adabb7` for ARMv7 hard-float (`arm-linux-gnueabihf-gcc` 13.3, `OQS_MINIMAL_BUILD="KEM_hqc_1;KEM_hqc_3;KEM_hqc_5"`, `OQS_OPT_TARGET=generic`, Release).
- `tests/test_kem` passes for HQC-1/3/5 under qemu-arm 8.2.2.
- So there is **no observed failure**. QEMU user mode may not model alignment traps, though, and we have not tested on Armv7 hardware.

Suggestion: track upstream #31 (milestone v5.1.1 there), or carry a small patch in the meantime, for example declaring the two buffers as `uint64_t[CEIL_DIVIDE(PARAM_SECURITY_BYTES, 8)]` or aligning them.

Unrelated to the AVX2 decoder defects in upstream #39/#46: liboqs ships only the reference HQC implementation, which has neither.
