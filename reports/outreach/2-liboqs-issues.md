**Send via:** two separate public issues at https://github.com/open-quantum-safe/liboqs/issues. Under liboqs's SECURITY.md, both fall under "report publicly": one is constant-time behaviour already documented in tests/constant_time, the other a hardening issue with no demonstrated impact.

**Attachments:** none needed. Issue B contains the patch inline. Optionally drag `C:\Users\Admin\Desktop\encryption_research\phx-tri\hqc-ct-hardening.patch` into the issue as well.

---

## Issue A

**Title:** HQC: imported code still has upstream HQC issue #31 (uint8_t buffers passed as uint64_t *)

liboqs imports HQC from `gitlab.com/pqc-hqc/hqc` at `161cd4fdf6b4a5198cf40b3a1243f9f27f13e03d` (`scripts/copy_from_upstream/copy_from_upstream.yml`). That is the commit cited in upstream HQC issue #31 (https://gitlab.com/pqc-hqc/hqc/-/issues/31):

> `crypto_kem_enc()` and `crypto_kem_dec()` declare the message buffer as a `uint8_t` array and then pass it to `hqc_pke_encrypt()` / `hqc_pke_decrypt()` through a `(uint64_t *)` cast. [...] This is undefined behaviour, and on Armv7 it can fault.

The same casts are in liboqs at `src/kem/hqc/pqc-hqc_hqc-{1,3,5}_ref/kem.c`, lines 124, 192 and 200. The buffers are `uint8_t m[PARAM_SECURITY_BYTES]` / `uint8_t m_prime[...]` with no alignment attribute. `CONFIGURE.md` lists ARM32v7 as a supported target. The integration patch already fixes the related 32-bit `BITMASK` problem (upstream #26), but not this one.

What I checked:
- Cross-built liboqs at `8adabb7` for ARMv7 hard-float (`arm-linux-gnueabihf-gcc` 13.3, `OQS_MINIMAL_BUILD="KEM_hqc_1;KEM_hqc_3;KEM_hqc_5"`, `OQS_OPT_TARGET=generic`, Release).
- `tests/test_kem` passes for HQC-1/3/5 under qemu-arm 8.2.2.
- So there is **no observed failure**. QEMU user mode may not model alignment traps, though, and I have not tested on Armv7 hardware.

Note for the planned HQC update (#2670): upstream v5.1.0 still has the same casts in `src/common/hqc-kem.c`, `(uint64_t *)m` in encapsulation and `(uint64_t *)m_prime` in decapsulation. Updating the import alone will not fix this.

Suggestion: track upstream #31 (milestone v5.1.1 there), or carry a small patch in the meantime, for example declaring the two buffers as `uint64_t[CEIL_DIVIDE(PARAM_SECURITY_BYTES, 8)]` or aligning them.

Unrelated to the AVX2 decoder defects in upstream #39/#46: liboqs ships only the reference HQC implementation, which has neither.

---

## Issue B (WITHDRAWN 2026-11-06: do not file; already known to the liboqs maintainers)

**Title:** HQC: root cause of the suppressed `vect_write_support_to_vector` memcheck reports (Clang turns the mask into a branch)

`tests/constant_time/kem/issues/hqc-{1,3,5}` suppress `Memcheck:Cond` / `Value1` reports in `vect_write_support_to_vector`, called from `vect_sample_fixed_weight1` (key generation and `crypto_kem_dec`) and `vect_sample_fixed_weight2` (encryption). In an audit of the upstream HQC v5.1.0 reference code, I traced where these come from.

- **Cause.** Clang 18.1.3 compiles the constant-time mask `1 ^ ((tmp | -tmp) >> 31)` (upstream `src/ref/vector.c:154`) into `cmp; je`, taken when the loop word index equals a secret support word index.
  - At -O2 and -O3 the branch sits in the inlined copy inside `vect_sample_fixed_weight1`, i.e. while sampling the secret x and y.
  - At -O1 and -Os it is also in `vect_sample_fixed_weight2` (r1, r2, e).
  - GCC 13.3 does not produce the branch at any level I tested.
- **Why it matters.** HQC's decapsulation re-derives x and y from the seed, so the branch runs on the long-term key in every `crypto_kem_dec`. It reveals which 64-bit word contains each secret position.
- **Status upstream.** Not listed in upstream HQC's compiler constant-time tracker (https://gitlab.com/pqc-hqc/hqc/-/work_items/11). I am reporting it there as well.
- **Caveat.** I observed this in upstream's code. I did not rebuild liboqs's copy with Clang; your suppressions match the same call stacks.

**A tested fix exists.** The patch below, against HQC v5.1.0 `src/ref` (`patch -p1`), puts a value barrier, `__asm__("" : "+r"(mask))`, on the selection masks in `vect_write_support_to_vector` and `gf_carryless_mul`.
- With it, the branches are gone under GCC 13.3 and Clang 18.1.3 at -O0 to -Os, and the HQC-3 KATs pass.
- A barrier on `tmp` alone is not enough: Clang's cmov-converter still turns the `& mask` select into a branch, the same mechanism as liboqs #2402.
- liboqs's copy (`161cd4f`) may need the hunk offsets adjusted; I have not applied it there.

```diff
--- a/src/ref/gf.c
+++ b/src/ref/gf.c
@@ -115,7 +115,9 @@
 
     for (int i = 0; i < 4; i++) {
         uint32_t tmp2 = tmp1 - i;
-        g ^= (u[i] & -(1 - ((tmp2 | -tmp2) >> 31)));
+        uint32_t mask = -(1 - ((tmp2 | -tmp2) >> 31));
+        __asm__("" : "+r"(mask));  // value barrier: mask stays data, not control flow
+        g ^= (u[i] & mask);
     }
 
     l = g;
@@ -126,7 +128,9 @@
         uint16_t tmp3 = (a >> i) & 3;
         for (int j = 0; j < 4; ++j) {
             uint32_t tmp2 = tmp3 - j;
-            g ^= (u[j] & -(1 - ((tmp2 | -tmp2) >> 31)));
+            uint32_t mask = -(1 - ((tmp2 | -tmp2) >> 31));
+            __asm__("" : "+r"(mask));  // value barrier: mask stays data, not control flow
+            g ^= (u[j] & mask);
         }
 
         l ^= g << i;
--- a/src/ref/vector.c
+++ b/src/ref/vector.c
@@ -153,6 +153,7 @@
             uint32_t tmp = i - index_tab[j];
             int val1 = 1 ^ ((tmp | -tmp) >> 31);
             uint64_t mask = -val1;
+            __asm__("" : "+r"(mask));  // value barrier: mask stays data, not control flow
             val |= (bit_tab[j] & mask);
         }
         v[i] |= val;
```

I have also offered the patch upstream. Once it, or an equivalent, lands, these suppressions could be removed. Until then, a short comment in the suppression file naming the Clang cause might help future readers.

Details and reproduction: https://github.com/Tarekswd/PHOENIX-PQ (`reports/hqc-ct-audit.md`, class E1; `phx-tri/hqc-ct-hardening.patch`).

— Tarek Ahmadieh
