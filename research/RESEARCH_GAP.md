# Research Gap Register

Details and evidence: [literature/phase1-review.md](literature/phase1-review.md).

| ID | Gap | Direction | Status | Kills it if... |
|---|---|---|---|---|
| G1.1 | Concrete X25519 + ML-KEM + HQC/McEliece KEM with tight concrete bound | 1 | Candidate | ABK25 or a CFRG concrete-hybrid draft already specifies it |
| G1.2 | Does HQC's FIPS 207 FO variant fall within ABK25's C2PRI-proven transform class? | 1 | Candidate, sharpest question | ABK25 covers it explicitly |
| G1.3 | Measured cost of three-way combiners vs X-Wing | 1 | Weak (Panther/Cobra overlap) | Panther/Cobra already measure it |
| G2.1 | Multi-tool constant-time audit of HQC v5.1.0 beyond the maintainers' known list (work item #11) | 2 | Open but narrowing: first pass found only known or by-design items; class E under triage. HQC v5 is also being audited by others (SAW, mailing-list reports) | A finding duplicates #11/#39/#46 (then it is not new) |
| G2.2 | Compiler/optimisation-level-induced timing leaks in HQC/ML-KEM | 2 | Candidate | Prior systematic study found |
| G2.3 | Timing leakage in HQC's FO re-encryption path | 2 | Candidate | Codeword-masking paper (2026/071) already covers timing, not only power |
| G3 | New primitive | 3 | No gap identified | n/a |

Theory of generic n-way combiners (IND-CCA if one component is) is NOT a gap: covered by GHP18, ePrint 2020/1364, ABK25.
