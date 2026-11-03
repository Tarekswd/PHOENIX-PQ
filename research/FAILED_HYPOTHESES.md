# Failed Hypotheses

## F1 (2026-11-03): "Generic three-way combiner security is an open theory problem"
- Result: False. GHP18, ePrint 2020/1364 (n-KEM CtKDF/CasKDF, Coq-checked) and ABK25 (multi-KEM, C2PRI) already cover it.
- Consequence: Direction 1 narrowed to concrete gaps G1.1-G1.3.

## F2 (2026-11-03): "AVX2 GCC -O2/-Os KAT failure in HQC is a new finding"
- Observation (reproduced here): HQC v5.1.0 and v5.0.0 avx256 built with GCC 13.3 at -O2 or -Os fail the official KATs; valid-ciphertext decaps failed 294/300 random runs. Bisected to `find_peaks` in `src/x86_64/avx256/reed_muller.c` (`uint16_t *ptr = (uint16_t *)&tmp;`, strict-aliasing UB). A one-line `memcpy` fix makes all 40 builds (2 versions x 20 configs) pass.
- Result: NOT new. Already tracked upstream as HQC issues #39 (2026-09-24) and #46 (2026-10-13, reported by Nicholas Clifford Maino with a patch, milestone v5.1.1). The same pattern was fixed in PQClean years earlier (PQClean #453 / PR #455).
- Value: independent reproduction; validates that the build matrix + KAT gate catches real defects.
- Lesson: search the upstream issue tracker (including closed items and mailing-list mirrors) BEFORE root-causing, not after.
