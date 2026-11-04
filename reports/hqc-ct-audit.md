# HQC v5.1.0 Constant-Time and Build-Correctness Audit: First Pass

Date: 2026-11-03. Status: first pass, structural analysis only. Timing measurements (dudect) not yet run.

## Scope

| Item | Value |
|---|---|
| Target | HQC official implementation, `gitlab.com/pqc-hqc/hqc`, tags v5.1.0 (06567ccb) and v5.0.0 (f46e542) |
| Parameter set | HQC-1 |
| Builds | {ref, avx256} x {gcc 13.3.0, clang 18.1.3} x {-O0, -O1, -O2, -O3, -Os} = 20 per version, 40 total |
| Platform | Ubuntu 24.04.4 on WSL2, AMD Ryzen 7 PRO 4750U (AVX2) |
| Tools | Valgrind 3.22.0 memcheck with secrets marked undefined (ctgrind technique); objdump; official KAT files |
| Tooling | `experiments/hqc-ct/` (build_matrix.sh, ct_harness.c, run_ct.sh, parse_vg.py) |

HQC's documented build is CMake Release, which compiles at -O3. The HQC README limits the team's claim to timing attacks and calls compiler hardening "a work in progress".

## Result 1: KAT failures in AVX2 builds with GCC at -O2 and -Os (known upstream)

| | v5.0.0 | v5.1.0 |
|---|---|---|
| Builds passing official KATs | 18/20 | 18/20 |
| Failing builds | avx256 gcc -O2, avx256 gcc -Os | same |

- With random keys, decapsulation of valid ciphertexts failed in 294 of 300 runs (avx256, gcc -O2, v5.1.0).
- Bisection: compiling only `src/x86_64/avx256/reed_muller.c` with strict aliasing reproduces the failure. Every other file is innocent.
- Root cause: `find_peaks` reads an `__m256i` through `uint16_t *ptr = (uint16_t *)&tmp;`. That is a strict-aliasing violation (undefined behaviour), which GCC exploits at -O2/-Os.
- Confirmation: replacing that read with `memcpy` makes all 40 builds pass in both versions. `-fno-strict-aliasing` also fixes it.
- **Prior art: already reported.** HQC issue #39 (2026-09-24) and #46 (2026-10-13, Nicholas Clifford Maino, with patch), both scheduled for v5.1.1. The same pattern was fixed in PQClean earlier (issue #453, PR #455). This audit is an independent reproduction only.
- Impact: anyone building the AVX2 code at -O2 (a common distribution default) gets a KEM that fails nearly every exchange. The documented -O3 build is not affected.

## Result 2: Valgrind constant-time triage

There were 2374 raw reports, which reduce to 59 unique (kind, location) pairs across 40 builds and 4 modes (keygen, encaps, decaps, decaps with a tampered ciphertext). Classification:

| Class | Locations | Builds | Assessment |
|---|---|---|---|
| A. Rejection-sampling iteration count | `vect_generate_random_support1` (all lines), `is_equal` inlined into it (avx256 gcc), `keccak_inc_squeeze` (avx256, v5.1.0) | All | By design. The sampler loops until `weight` distinct values are accepted, so the iteration count, and the buffer refill and squeeze length that follow from it, depend on the secret randomness. The flagged `is_equal` jumps are a jump table on the trip-count remainder (`and $0x7; je; cmp $1; je ...`), not on candidate values. For an ideal XOF, the count of rejected candidates is independent of the accepted set. Not reported as a leak. |
| B. Reed-Muller peak search | `find_peaks` reed_muller.c:188 (v5.1.0) / :189 (v5.0.0) | avx256, gcc and clang, -O3 | Real secret-dependent branch (`vptest; sete; test; jne` on peak comparisons) in decapsulation. Already listed upstream: HQC #11, "Reed-Muller" under AVX -O3. |
| C. RS `compute_error_values` | reed_solomon.c | v5.0.0 only (clang) | Absent in v5.1.0, which agrees with HQC #11 ("to do as part of RS update") and the v5.1.0 RS rewrite. Fixed. |
| D. Reed-Muller, -O0 only | `find_peaks` reed_muller.c:155-161 | ref, -O0 | Unoptimised build only. Not a deployment configuration. |
| E1. Clang-introduced branch in support-to-vector | `vect_write_support_to_vector` vector.c:156, inlined into `vect_sample_fixed_weight1` (and `..._weight2` at -O1/-Os) | ref, clang -O1, -O2, **-O3**, -Os; HQC-1/3/5 | **Real.** Clang compiles the mask `1 ^ ((tmp \| -tmp) >> 31)` (vector.c:154) into `cmp; je` on (loop word == secret support word). At -O3 it runs while sampling the long-term key x, y, which HQC re-derives on *every* decapsulation. It reveals which 64-bit word holds each secret position. **Prior art:** liboqs already suppresses these memcheck reports for HQC ("not validated against branching on secret", tests/constant_time/kem/issues/hqc-*). Not listed in upstream HQC #11/#12. A separate `movdqa` "size 1" report at -O2/-O3 (vector.c:144) is a store indexed by the loop counter, so likely a tool artifact. |
| E2. Clang -O1 branch in GF multiply | `gf_carryless_mul` gf.c:118/129, inlined into `gf_mul` | ref, clang -O1 only | Real `cmp; je` on secret field-element bits (RS encode/decode). -O1 is not a documented build. |

### v5.0.0 to v5.1.0 difference

- Removed: class C (`compute_error_values`).
- Unchanged: classes A, B, D. The A locations moved because the sampler was rewritten, but it is still variable-time by design.
- New in v5.1.0: `keccak_inc_squeeze` reports in avx256. These come from the new buffered sampler's variable refill length (class A).

## Result 2b: downstream copies (liboqs, PQClean)

- Neither liboqs (`8adabb7`, 2026-11-03) nor PQClean (`0586a82`, 2026-08-04) ships HQC's AVX2 code. liboqs has only `pqc-hqc_hqc-{1,3,5}_ref`, and PQClean only `clean`. Neither carries the #39/#46 AVX2 decoder defects. pq-verify independently reports PQClean clean as VERIFIED.
- liboqs imports HQC commit `161cd4f`, the exact commit cited in HQC issue #31 (`uint8_t` buffers passed as `uint64_t *`, which is alignment UB on Armv7). liboqs's `kem.c` keeps those casts (lines 124, 192, 200). liboqs's own integration patch already fixes the related 32-bit `BITMASK` issue (HQC #26). No liboqs issue tracks #31.
- Cross-built liboqs (HQC only) for ARMv7 hard-float (gcc 13.3, `-mcpu` generic) and ran `test_kem` under qemu-arm 8.2.2: HQC-1/3/5 all pass. The `strd` instructions in liboqs `kem.c` are stack-relative zeroing. So there is no observed failure, but QEMU user mode may not model alignment traps; real Armv7 hardware is needed to settle it.
- Upstream HQC `161cd4f` "ref" does not build for ARM at all (`src/ref/hqc.h` includes `<immintrin.h>`); liboqs's patch removes that.

## Result 3: division-instruction scan

- -O1, -O2, -O3: no `div`/`idiv` in any build.
- v5.1.0: divisions only in `shake128`/`shake256` (-O0, -Os). These divide output length by the SHAKE rate, both public.
- v5.0.0: one extra `div` in `reed_solomon_decode` (ref and avx256, gcc -Os). Absent in v5.1.0. Not investigated further, since the code was replaced.

## What this pass does not show

- It does not show absence of leaks. Valgrind sees only executed paths and only branch or address dependence. The division scan covers `div`/`idiv` only, not other variable-latency instructions.
- No timing measurements yet. WSL2 cannot disable CPU boost, so timing results will be weak evidence.
- One microarchitecture (x86-64 Zen 2), one parameter set (HQC-1), no ARM.

## Result 4: HQC-3 and HQC-5

The same 20-build matrix for HQC-3 and HQC-5 (v5.1.0) gives the same picture as HQC-1:
- KAT failures only for avx256 gcc -O2/-Os (the #39/#46 aliasing defect);
- the same Valgrind classes at the same source lines (A, B, D, E1, E2);
- no locations that HQC-1 did not also show.

## Result 5: timing of the class B branch (dudect)

`experiments/hqc-ct/dudect_rm.c`: fixed-vs-random Welch t-test on RM decoding (HQC-1, block noise weight 132, 2,000,000 interleaved measurements, rdtscp, pinned core, WSL2 with boost on).

| Decoder | Median cycles | Max \|t\| over crops | Reading |
|---|---|---|---|
| Reference, gcc -O3 (control) | 57,630 | 1.54 | no input dependence detected |
| Official AVX2, gcc -O3 | 31,739 | **88.4** | input-dependent timing, ~70-260 cycles (0.2-0.8%) slower on fresh inputs |

Inputs for each batch of 4,096 measurements are prepared before any timing, in separate buffers for both classes. A first version generated each class-1 input right before timing it, which biased the control to |t| = 6.4. That version's numbers are superseded.

Fresh inputs are slower, which is the signature of a data-dependent branch that the predictor can learn for a fixed input. This turns the #11 "Reed-Muller" item from a tool report into a measured timing difference. It does not show *which* secret bits leak or how many traces an attack needs.

## Next steps

1. Report E1's root cause and affected optimisation levels to HQC #11 (prior art: liboqs suppressions).
2. Repeat dudect on bare-metal Linux with boost disabled, and measure E1 (clang -O3 keygen/decaps) the same way.
