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
| E. Unconfirmed | `vect_write_support_to_vector` (ref clang -O1..-O3), `gf_carryless_mul` (ref clang -O1) | Non-default compiler | Under triage. The clang -O3 report points at a store whose address is the loop counter, so it is likely a tool artifact. Details withheld until triage is complete (see SECURITY.md). |

### v5.0.0 to v5.1.0 difference

- Removed: class C (`compute_error_values`).
- Unchanged: classes A, B, D. The A locations moved because the sampler was rewritten, but it is still variable-time by design.
- New in v5.1.0: `keccak_inc_squeeze` reports in avx256. These come from the new buffered sampler's variable refill length (class A).

## What this pass does not show

- It does not show absence of leaks. Valgrind sees only executed paths and only branch or address dependence. It does not catch variable-latency instructions; the separate division scan (`divs.txt`) has not been reviewed yet.
- No timing measurements yet. WSL2 cannot disable CPU boost, so timing results will be weak evidence.
- One microarchitecture (x86-64 Zen 2), one parameter set (HQC-1), no ARM.

## Next steps

1. Finish triage of class E. If confirmed, disclose privately to the HQC team.
2. Review the division scan output.
3. Run the matrix for HQC-3 and HQC-5 under the documented -O3 build.
4. dudect timing on the class B branch, to see whether it is measurable under WSL.
