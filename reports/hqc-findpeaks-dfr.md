# Decryption-Failure Impact of the HQC AVX2 `find_peaks` Width Bug (HQC issue #46, defect 1)

Date: 2026-11-03. Target: HQC v5.1.0 (`06567ccb`), HQC-1/3/5. Tooling: `experiments/hqc-dfr/`.

## Question

HQC issue #46 (reported by Nicholas Clifford Maino using pq-verify) describes two defects in the AVX2 Reed-Muller decoder. Defect 1: the peak search in `find_peaks` starts from width `1 << (5 + MULTIPLICITY/2)`, which is 64 for HQC-1 and 128 for HQC-3/5, too small for HQC's odd multiplicities. When two transform values both exceed that cap, the decoder returns the lower-indexed one instead of the larger. Both the issue and pq-verify's AUDITS.md state that the effect on the decryption failure rate (DFR) "was not estimated". This report estimates it.

## Answer

| | HQC-1 | HQC-3 | HQC-5 |
|---|---|---|---|
| Real-noise blocks simulated | 884,080,256 | 402,773,504 | 391,553,280 |
| Encryptions simulated | 19,219,136 | 7,192,384 | 4,350,592 |
| Reference RM block error rate p | 9.43e-4 | 6.97e-5 | 5.20e-4 |
| Bug-only errors observed (AVX2 wrong, reference right) | 0 | 0 | 0 |
| Simulation 95% upper bound on bug rate q | 3.4e-9 | 7.5e-9 | 7.7e-9 |
| Exact-model bug rate q per block | 7.8e-12 | 7.6e-33 | 7.3e-34 |
| q / p | 8.3e-9 | 1.1e-28 | 1.4e-30 |
| Relative DFR increase (independent-block binomial model) | ~1.3e-7 | ~0 | ~0 |

**For honestly generated ciphertexts, defect 1 has a negligible effect on HQC's decryption failure rate at every security level.** HQC-1 is the only set where it is even measurable in a model, at about one extra block error per 10^11 blocks.

### Why it is negligible

For a block with noise weight w, the true peak has magnitude `N2/2 - w`. The defect can only fire when the true peak is above the cap, which means a *light* block (w < 128 for HQC-1, w < 192 for HQC-3/5). It also needs a second codeword's peak above the cap as well, which is a ~7-sigma event for such light blocks. Decryption failures come from the opposite end, from *heavy* blocks:

| | HQC-1 | HQC-3 | HQC-5 |
|---|---|---|---|
| Mean block noise weight (real HQC noise) | 132.5 | 232.7 | 240.0 |
| Fraction of blocks light enough for the defect | 0.298 | <0.001 | <0.001 |
| Weight where the defect is most likely | 125 (P = 6.8e-11) | 189 (P = 9.8e-29) | 189 (P = 9.8e-29) |
| Weights where reference errors occur (sim) | >= 139 | >= 255 | >= 253 |

## Method

1. **Noise.** HQC decryption gives the RM decoder `encode(m) + Truncate(x*r2 + r1*y + e)` (src/ref/hqc.c). The simulator samples x, y (weight omega, new key every 16 encryptions) and r1, r2, e (omega_r, omega_e) with uniform supports, forms the sparse products exactly in F2[X]/(X^n - 1), and truncates to n1*n2 bits.
2. **Decoders.** Three decoders are linked side by side in one binary, with symbols renamed via objcopy:
   - the reference decoder (exact maximum likelihood);
   - the official AVX2 decoder, built with `-fno-strict-aliasing` so that defect 2 (HQC #39) does not interfere;
   - AVX2 with the width fix `1 << (6 + MULTIPLICITY/2)` from #46.
3. **Positive control (`dfr_sim selftest`).** The harness must be able to see the bug. In crafted blocks at distance N2/4 - 1 from the sent codeword and N2/4 + 1 from a competitor (the #46 pattern), the official AVX2 decoder returned the farther codeword in 51% / 50% / 50% of 13,745 / 16,736 / 26,915 blocks. The reference and fixed decoders were correct in 100%.
4. **Exact model.** For uniform noise of weight w in a block, each of the 127 competing transform entries has magnitude |2k - w| with k ~ Hypergeometric(N2, N2/2, w).
   - Reference error = some competitor exceeds the true peak, with ties counted 1/2.
   - Defect = true peak above the cap, and some competitor strictly between the cap and the true peak, with the lower index (1/2).
   - Both use a union bound over the 127 competitors.
5. **Validation of the model.**
   - (a) *Real noise behaves like uniform noise given its weight.* Reference error at w = 150 was 1.460e-3 with real HQC noise and 1.440e-3 with uniform noise. At w = 160: 0.1153 vs 0.1154.
   - (b) *The formula matches simulation where probabilities are small.* HQC-1 w = 150: model 1.436e-3, sim 1.440e-3. w = 155: 1.554e-2 vs 1.542e-2. HQC-3/5 w = 270: 8.115e-3 vs 8.125e-3. The union bound overshoots once probabilities approach 1 (w >= 160 for HQC-1), which does not affect the defect estimate, since the defect lives entirely in the small-probability regime.
6. **DFR mapping.** DFR(p) = P(Bin(n1, p) >= delta + 1) with delta = (n1 - k)/2 = 15 / 16 / 29. Only the *relative* change from p to p + q is reported. The absolute binomial-model DFR is not HQC's official DFR analysis and is not claimed.

## Side result: the #46 width fix

The fixed AVX2 decoder agreed with the reference decoder on all 1.68e9 real-noise blocks and on all uniform-noise validation blocks (`fix_ne_ref = 0`), and it fixed every crafted block in the positive control. This independently supports the patch proposed in #46.

## Check: HQC's own sampler instead of uniform supports

`dfr_sim_hqcsampler` draws r2, e, r1 with HQC's `vect_sample_fixed_weight2`, from a fresh 32-byte theta, in the same order as `hqc_pke_encrypt`. A 10-minute run (another 4.9e8 blocks) gives the same numbers as the uniform model within statistical error:

| | HQC-1 | HQC-3 | HQC-5 |
|---|---|---|---|
| Ref block error rate (HQC sampler) | 9.442e-4 | 7.058e-5 | 5.234e-4 |
| Ref block error rate (uniform) | 9.431e-4 | 6.973e-5 | 5.204e-4 |
| Mean block weight (HQC sampler / uniform) | 132.5 / 132.5 | 232.7 / 232.7 | 240.0 / 240.0 |
| Bug-only errors (HQC sampler) | 0 / 2.88e8 | 0 / 1.08e8 | 0 / 9.49e7 |

The sampler's non-uniformity does not change the result.

## Crafted ciphertexts: does the decoder difference reach the shared secret?

**Experiment** (`experiments/hqc-dfr/crafted.c`, `crafted.sh`; HQC-1):
- With the secret key known, the test builds ciphertexts with `v = Truncate(u*y) + R`, where `R` is a word on which the reference and official AVX2 decoders disagree. In every crafted ciphertext, 11 to 46 of the 46 blocks disagreed (mean 22.9).
- The same file is decapsulated by a reference build and by a build whose RM decoder is the AVX2 one, with nothing else changed.

**Result:**
- All 6,000 ciphertexts (5,000 crafted, 500 honest, 500 random) gave identical shared secrets in both builds.
- All honest ones decapsulated correctly.
- The crafted ones received distinct implicit-rejection keys.

**Why** (matches the code):
- A crafted ciphertext is not an honest encryption of whatever message either decoder returns, so the FO re-encryption check fails in both builds.
- The output is then `K_bar = J(H(ek), sigma, c)` (`hqc_kem_decaps_internal`), which does not depend on the decoded message.
- An attacker cannot instead build an *honest* ciphertext that lands in the disagreement region: the encryption noise r1, r2, e is fixed by `G(H(ek) || m || salt)`, and the decoder input also depends on the secret x, y.

So the decoder difference affects only the failure rate of honest ciphertexts, computed above. It does not affect what decapsulation returns for malformed ones. Timing is separate (see reports/hqc-ct-audit.md, class B).

## Limits

- The crafted-ciphertext test covers HQC-1 and one key per run; the argument above is general but not formally proven.
- The main runs sample r1, r2, e uniformly; the check above with HQC's own sampler gives the same result.
- Union bound and independence between competitors in the model. These were validated against simulation in the relevant regime (above), not proven.
- Under the binomial model, the decryptions that dominate the DFR are skewed toward heavy blocks, where the defect cannot fire. The unconditional q used here therefore likely *over*-states the DFR effect.
- One x86-64 host (AMD Ryzen 7 PRO 4750U), gcc 13.3.0.

## Reproduce

```sh
cd experiments/hqc-dfr
for v in 1 3 5; do bash build.sh <hqc-src> $v out/hqc$v; out/hqc$v/dfr_sim selftest 300; done
bash run_all.sh out runs 900          # ~15 min on 16 threads
python3 analyze.py runs
```
