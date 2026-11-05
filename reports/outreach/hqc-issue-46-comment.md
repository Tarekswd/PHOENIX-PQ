**Estimate of the DFR impact of defect 1 (peak search width)**

The issue and pq-verify's AUDITS.md both note that the effect of defect 1 on the decryption failure rate was not estimated. We measured it on v5.1.0 (`06567ccb`). Short version: for honestly generated ciphertexts it is negligible at all three levels.

| | HQC-1 | HQC-3 | HQC-5 |
|---|---|---|---|
| RM blocks under real decryption noise | 8.84e8 | 4.03e8 | 3.92e8 |
| Reference RM block error rate p | 9.43e-4 | 6.97e-5 | 5.20e-4 |
| Blocks where AVX2 was wrong and ref right | 0 | 0 | 0 |
| 95% upper bound on that rate (blocks / only keys independent) | 3.4e-9 / 2.5e-6 | 7.5e-9 / 6.7e-6 | 7.7e-9 / 1.1e-5 |
| Exact-model rate q | 7.8e-12 | 7.6e-33 | 7.3e-34 |
| q / p | 8.3e-9 | 1.1e-28 | 1.4e-30 |

**Why.** With block noise weight w, the true peak is N2/2 - w. The defect needs that peak above the cap (64 for HQC-1, 128 for HQC-3/5), so it needs a *light* block: w < 128 (HQC-1) or w < 192 (HQC-3/5). It also needs a second codeword above the cap, which is a ~7-sigma event at those weights. Failures come from heavy blocks instead. The mean block weight is 132.5 / 232.7 / 240.0, the defect's probability peaks at w = 125 for HQC-1 (6.8e-11), and reference errors start around w = 139 / 255 / 253.

**Method.**
- The simulator builds `encode(m) + Truncate(x*r2 + r1*y + e)` exactly as in `hqc_pke_decrypt`, with uniform supports of weight omega / omega_r / omega_e.
- Each block is decoded by three decoders linked side by side: the ref decoder, the official AVX2 decoder (with `-fno-strict-aliasing`, to keep defect 2 out), and AVX2 with the proposed `1 << (6 + MULTIPLICITY / 2)`.
- Positive control: on blocks built like the example in this issue (N2/4 - 1 flips from one codeword, N2/4 + 1 from another), the official AVX2 decoder returned the farther codeword about 50% of the time. Ref and patched were always right.
- The exact model is hypergeometric per block weight, with a union bound over the 127 competitors. It matches simulation where probabilities are small: at w = 150 for HQC-1, model 1.436e-3 vs 1.440e-3 (uniform noise) and 1.460e-3 (real HQC noise). That also shows real noise behaves like uniform noise given its weight.

**The proposed fix.** The patched decoder agreed with the ref decoder on all 1.68e9 real-noise blocks and on every crafted block.

**Checks.**
- *Real sampler.* Re-running with HQC's own `vect_sample_fixed_weight2` for r2, e, r1 (another 4.9e8 blocks) gives the same rates within statistical error. For HQC-1, the ref error rate is 9.442e-4 vs 9.431e-4 with uniform supports.
- *Crafted ciphertexts.* We built 5,000 HQC-1 ciphertexts with v = Truncate(u*y) + R, where R makes the two decoders disagree on 11-46 blocks each. A reference build and a build with only the RM decoder swapped to AVX2 returned identical shared secrets for all of them, as well as for 500 honest and 500 random ciphertexts. This is consistent with implicit rejection: K_bar = J(H(ek), sigma, c) does not depend on the decoded message.

**Limits.** The DFR mapping uses an independent-block binomial model and reports only the relative change (about 1.3e-7 for HQC-1).

Code and full write-up: https://github.com/Tarekswd/PHOENIX-PQ (`experiments/hqc-dfr/`, `reports/hqc-findpeaks-dfr.md`; draft paper `paper/main.pdf`, Section 6). Thanks to Nicholas Clifford Maino and pq-verify for the original report.

— Tarek Ahmadieh
