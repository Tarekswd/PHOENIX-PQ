# Outreach: what to send, where, in what order, with which attachments

| # | Draft | Recipient | Channel | Attachments |
|---|---|---|---|---|
| 1 | [1-hqc-team.md](1-hqc-team.md) | HQC team | New work item at gitlab.com/pqc-hqc/hqc (their README asks for reports there), or their mailing list | `paper\main.pdf`, `phx-tri\hqc-ct-hardening.patch`, `phx-tri\hqc-portability.patch` |
| 1b | [hqc-issue-46-comment.md](hqc-issue-46-comment.md) | HQC issue #46 thread | Comment on gitlab.com/pqc-hqc/hqc/-/issues/46 | none (links only) |
| 2A | [2-liboqs-issues.md](2-liboqs-issues.md), Issue A | liboqs maintainers | Public issue at github.com/open-quantum-safe/liboqs | none |
| 2B | [2-liboqs-issues.md](2-liboqs-issues.md), Issue B | liboqs maintainers | Public issue (separate from A) | none required; the patch is inline (optionally also drag in `phx-tri\hqc-ct-hardening.patch`) |

Full paths of the attachment files (all under `C:\Users\Admin\Desktop\encryption_research\`):

| File | What it is |
|---|---|
| `paper\main.pdf` | 6-page draft paper, author Tarek Ahmadieh |
| `phx-tri\hqc-ct-hardening.patch` | Value barriers that remove the Clang branches (HQC v5.1.0 `src/ref`, `patch -p1`) |
| `phx-tri\hqc-portability.patch` | Removes the unused x86-only `<immintrin.h>` includes from `src/ref` |

Both patches were re-checked on 2026-11-06: they apply cleanly, together, to a fresh v5.1.0 tree.

A review-request email about a C2PRI proof sketch was **withdrawn** on 2026-11-05, because the result is already proven in Starfighters (IEEE S&P 2026); see research/FAILED_HYPOTHESES.md, F3.

## Order

1. **HQC first (1, then 1b).** liboqs issue B says the fix is also being offered upstream.
2. **Then liboqs issues A and B**, as two separate issues.

## Before sending

- [ ] `git push`. The messages link to files on GitHub `main`: `reports/hqc-findpeaks-dfr.md`, `reports/hqc-ct-audit.md`, `phx-tri/hqc-ct-hardening.patch`, `paper/main.pdf`.
- [ ] Open https://github.com/Tarekswd/PHOENIX-PQ and check that those links work.
- [ ] Attach the three files to message 1.
- [ ] When pasting into GitLab or GitHub, copy from the line after `---` in each draft; the lines above it are notes for you.

## Numbers used in these messages (all from this repository's runs)

- **DFR study:** 8.84e8 / 4.03e8 / 3.92e8 real-noise blocks (HQC-1/3/5), plus 4.9e8 more with HQC's own sampler; 0 defect-only errors; model q = 7.8e-12 / 7.6e-33 / 7.3e-34. Source: `reports/hqc-findpeaks-dfr.md`.
- **Crafted ciphertexts:** 5,000 crafted (11-46 divergent blocks, mean 22.9), 500 honest and 500 random, all identical. Source: `experiments/hqc-dfr/crafted.sh`.
- **dudect on the RM decoder:** AVX2 max |t| = 88.4; reference control <= 1.54; inputs prepared before timing. Source: `reports/hqc-ct-audit.md`, Result 5.
- **Clang branch (E1):** ref, Clang -O1/-O2/-O3/-Os, HQC-1/3/5. Patch verified on HQC-3 (all 10 ref builds pass KATs). Source: `reports/hqc-ct-audit.md` class E1 and `reports/phx-tri-report.md` section 1.
- **ARM:** HQC-3 inside PHX-Tri, AArch64 and ARMv7 under QEMU, vectors identical to x86. Source: `reports/phx-tri-report.md` section 4a.
