# Outreach: what to send, where, and in what order

| # | File | Recipient | Channel | Contents |
|---|---|---|---|---|
| 1 | [1-hqc-team.md](1-hqc-team.md) | HQC team | New work item at gitlab.com/pqc-hqc/hqc (their README asks for reports there), or their mailing list | Everything: #39/#46 reproduction, DFR impact of #46, crafted ciphertexts, Clang branch not in #11, dudect timing of the #11 RM branch |
| 1b | [hqc-issue-46-comment.md](hqc-issue-46-comment.md) | HQC #46 thread | Comment on gitlab.com/pqc-hqc/hqc/-/issues/46 | The DFR result only, for readers of that issue |
| 2 | [2-liboqs-issues.md](2-liboqs-issues.md) | liboqs maintainers | Two public GitHub issues (A: HQC #31 casts; B: root cause of suppressed Clang branch) | Public per liboqs SECURITY.md: documented constant-time behaviour and a hardening issue |

A review-request email about a C2PRI proof sketch was **withdrawn** on 2026-11-05: the result is already proven in Starfighters (IEEE S&P 2026). See research/FAILED_HYPOTHESES.md, F3.

## Order

1. **HQC first.** liboqs issue B says the Clang branch is also being reported upstream.
2. Then the liboqs issues.

## Before sending

- [ ] `git push`. The messages link to files on GitHub `main`: `reports/hqc-findpeaks-dfr.md`, `reports/hqc-ct-audit.md`, `paper/main.pdf`.
- [ ] Open https://github.com/Tarekswd/PHOENIX-PQ and check that those links work.
- [ ] Attach `paper/main.pdf` to message 1.

## Numbers used in these messages (all from this repository's runs)

- DFR study: 8.84e8 / 4.03e8 / 3.92e8 real-noise blocks (HQC-1/3/5); 4.9e8 more with HQC's sampler; 0 defect-only errors; model q = 7.8e-12 / 7.6e-33 / 7.3e-34. Source: `reports/hqc-findpeaks-dfr.md`.
- Crafted ciphertexts: 5,000 crafted (11-46 divergent blocks, mean 22.9) + 500 honest + 500 random, all identical. Source: `experiments/hqc-dfr/crafted.sh`.
- dudect: AVX2 max |t| = 88.4, reference control <= 1.54, inputs prepared before timing. Source: `reports/hqc-ct-audit.md`, Result 5.
- Clang branch (E1): ref, Clang -O1/-O2/-O3/-Os, HQC-1/3/5. Source: `reports/hqc-ct-audit.md`, class E1.
