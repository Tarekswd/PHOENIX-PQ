**Send via:** a new work item on https://gitlab.com/pqc-hqc/hqc (the README asks for reports there), or the HQC mailing list if you have its address. Attach `paper/main.pdf`.

**Subject:** HQC v5.1.0 implementation audit: DFR impact of #46, a Clang constant-time issue not in #11, and timing of the #11 Reed-Muller branch

---

Dear HQC team,

I audited the official HQC implementation (v5.0.0 `f46e542` and v5.1.0 `06567ccb`; HQC-1/3/5). The build matrix was {ref, avx256} x {GCC 13.3.0, Clang 18.1.3} x {-O0, -O1, -O2, -O3, -Os}. Each build was checked against your KAT files, run under Valgrind memcheck with secrets marked undefined, scanned for division instructions, and timed with dudect. Most of what I found you already track. Below are the reproductions in brief, then the four results that may be new to you. All code, data and a draft paper are at https://github.com/Tarekswd/PHOENIX-PQ.

**Already tracked (independent reproduction)**

- **#39 / #46, defect 2 (aliasing).** avx256 built with GCC at -O2 or -Os fails the KATs in v5.0.0 and v5.1.0, for HQC-1, -3 and -5. With random keys, 294 of 300 HQC-1 decapsulations of valid ciphertexts failed. Bisection isolates `src/x86_64/avx256/reed_muller.c`. Replacing the `uint16_t *` read of `tmp` in `find_peaks` with `memcpy` makes all 40 builds of both versions pass.
- **#11, Reed-Muller under AVX -O3.** Confirmed by Valgrind (`vptest; sete; jne` on peak comparisons at reed_muller.c:188). The v5.0.0 `compute_error_values` reports are gone in v5.1.0, as your RS update intended.

**1. DFR impact of #46 defect 1 (peak-search width): negligible**

Both #46 and pq-verify note this was not estimated. I simulated real HQC decryption noise, `encode(m) + Truncate(x*r2 + r1*y + e)` as in `hqc_pke_decrypt`, and decoded every block with three decoders: the reference decoder, the official AVX2 decoder, and AVX2 with the proposed `1 << (6 + MULTIPLICITY / 2)`.

| | HQC-1 | HQC-3 | HQC-5 |
|---|---|---|---|
| Blocks simulated | 8.84e8 | 4.03e8 | 3.92e8 |
| Reference block error rate p | 9.43e-4 | 6.97e-5 | 5.20e-4 |
| Blocks where AVX2 was wrong and ref right | 0 | 0 | 0 |
| 95% bound on that rate (blocks / only keys independent) | 3.4e-9 / 2.5e-6 | 7.5e-9 / 6.7e-6 | 7.7e-9 / 1.1e-5 |
| Exact per-weight model, rate q | 7.8e-12 | 7.6e-33 | 7.3e-34 |
| q / p | 8.3e-9 | 1.1e-28 | 1.4e-30 |

The defect needs a *light* block: true peak N2/2 - w above the cap, so w < 128 for HQC-1 and w < 192 for HQC-3/5. It also needs a second codeword above the cap, which is a ~7-sigma event there. Decryption failures come from heavy blocks: mean block weight is 132.5 / 232.7 / 240.0, and reference errors start around w = 139 / 255 / 253.

The exact model (hypergeometric per weight, union bound over the 127 competitors, ties counted 1/2) matches simulation where probabilities are small. For HQC-1 at w = 150 it gives 1.436e-3, against 1.440e-3 for uniform noise and 1.460e-3 for real HQC noise. Under an independent-block binomial model, the relative DFR increase is about 1.3e-7 for HQC-1. Checks:
- *Positive control.* On blocks built like the #46 example, the official AVX2 decoder returned the farther codeword in about 50% of 57,396 blocks; ref and patched were always right.
- *Your own sampler.* Re-running with `vect_sample_fixed_weight2` for r2, e, r1 (another 4.9e8 blocks) gives the same rates (HQC-1: 9.442e-4 vs 9.431e-4).
- *The proposed width fix* agreed with the reference decoder on all 1.68e9 real-noise blocks.

**2. Crafted ciphertexts: the decoder difference does not reach decapsulation outputs**

Using y, I built 5,000 HQC-1 ciphertexts with v = Truncate(u*y) + R, where R makes the ref and AVX2 decoders disagree on 11 to 46 of the 46 blocks (mean 22.9). I decapsulated them with a reference build and with a build where only the RM decoder is the AVX2 one. The shared secrets were identical for every crafted ciphertext and for 500 honest and 500 random ones. This is consistent with K_bar = J(H(ek), sigma, c) not depending on the decoded message.

**3. Clang-introduced branch in support-to-vector, not listed in #11 or #12**

- In the ref implementation built with Clang at -O1, -O2, **-O3** and -Os (HQC-1/3/5), `vect_write_support_to_vector` is compiled into `cmp; je`, taken when the loop word equals a secret support word. The source mask is `1 ^ ((tmp | -tmp) >> 31)` at vector.c:154.
- At -O2/-O3 this is inlined into `vect_sample_fixed_weight1`, so it runs while sampling the long-term x, y. Since decapsulation re-derives x, y from the seed, it runs on every decapsulation, and it reveals which 64-bit word holds each secret position.
- At -O1/-Os it also appears in `vect_sample_fixed_weight2` (r1, r2, e).
- Prior art: liboqs already suppresses these memcheck reports ("not validated against branching on secret", `tests/constant_time/kem/issues/hqc-*`). I did not find it in your tracker.
- Minor: Clang -O1 also turns `gf_carryless_mul` (inlined into `gf_mul`) into branches on field-element bits.

**4. Timing of the #11 Reed-Muller branch is measurable**

dudect fixed-vs-random Welch test on HQC-1 RM decoding (noise weight 132, 2e6 measurements, pinned core, inputs prepared before timing):
- official AVX2 decoder (gcc -O3): max |t| = 88.4, with fresh inputs about 70-260 cycles slower (0.2-0.8% of ~31,700);
- reference decoder, as a control: |t| <= 1.54.

This was measured under WSL2 with CPU boost enabled, so it shows input dependence, not an attack cost.

**Context for hybrids.** Connolly et al., "Starfighters" (IEEE S&P 2026, ePrint 2025/1397), prove C2PRI for your 2025-08-22 transform and show HQC works in X-Wing's QSF combiner. Results 1 and 2 above add the implementation side: the decoder defect does not undermine that, and result 3 marks the code a constant-time hybrid would need to harden.

Limitations of all of the above: one x86-64 host (Zen 2) under WSL2; Valgrind sees only executed paths; the crafted-ciphertext test covers HQC-1.

Thank you for the open tracker and the KAT files, which made this audit possible.

Best regards,
Tarek Ahmadieh
https://github.com/Tarekswd/PHOENIX-PQ
