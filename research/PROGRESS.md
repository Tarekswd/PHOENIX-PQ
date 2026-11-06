# Progress

| Date | Phase | Item |
|---|---|---|
| 2026-11-03 | 0 | Environment report: reports/ENVIRONMENT.md |
| 2026-11-03 | 0 | Repo structure and tracking files created; git initialised |
| 2026-11-03 | 1 | Scoped literature review: research/literature/phase1-review.md |
| 2026-11-03 | 1 | Gap register, assumptions, bibliography (partial) |
| 2026-11-03 | 1 | STOPPED for human review (per IMPLEMENTATION.md Phase 1) |

| 2026-11-03 | 1 | Direction 2 approved by project lead |
| 2026-11-03 | 2 | Proposal: research/proposals/direction2-hqc-ct-audit.md. STOPPED for approval |

| 2026-11-03 | 2 | Approved; Valgrind installed by project lead |
| 2026-11-03 | 5 | HQC audit first pass: 40 builds, KAT gate, Valgrind triage. Report: reports/hqc-ct-audit.md |
| 2026-11-03 | - | Repository linked to github.com/Tarekswd/PHOENIX-PQ |
| 2026-11-03 | 5 | Downstream check (liboqs, PQClean): no AVX2 HQC shipped; liboqs inherits HQC #31; ARMv7 QEMU test passes |
| 2026-11-03 | 5 | DFR impact of #46 defect 1 estimated (1.68e9 real-noise blocks + exact model): reports/hqc-findpeaks-dfr.md |
| 2026-11-03 | 5 | Class E resolved (E1 clang x,y-sampling branch: real, already suppressed in liboqs; E2 clang -O1 gf_mul); HQC-3/5 CT matrix; dudect: AVX2 find_peaks timing measurable (t=98) |
| 2026-11-03 | 5 | DFR study re-run with HQC's own sampler (same result); crafted-ciphertext test: 6,000 cts, identical decaps outputs ref vs AVX2 decoder |
| 2026-11-04 | 2 (Dir. 1) | G1.2: v5 transform not covered by ABK25/KSW; classical-ROM C2PRI proof sketch (research/g1.2-hqc-v5-c2pri.md) |
| 2026-11-04 | 7 | Paper draft, 6 pages, compiles cleanly: paper/main.tex, paper/main.pdf. Sole author: Tarek Ahmadieh |
| 2026-11-04 | - | Code review fixes: dudect inputs prepared before timing (control bias removed; AVX2 \|t\| = 88.4, ref <= 1.5); dependence-robust zero-event bounds; popcount block weights. Spec 2025-08-22 checked against code. MIT license, CITATION.cff, CI workflow (verified locally) |
| 2026-11-05 | 1 | C2PRI of HQC v5 found already proven (Starfighters); paper and outreach corrected (F3) |
| 2026-11-06 | 2-6 | PHX-Tri: proposal approved; HQC CT hardening patch (E1/E2 removed, KATs pass); X25519 + ML-KEM-768 + HQC-3 KEM implemented with tests, vectors, CT check, benchmark and CI (verified locally). Report: reports/phx-tri-report.md |
| 2026-11-06 | 5 | OpenSSL branch identified (ecx_exch.c:157, benign all-zero check); ARM: AArch64 + ARMv7 pass under QEMU with x86-identical vectors; hqc-portability.patch; HQC symbols localized (gf_mul clash with static OpenSSL); dudect `ct` test: no dependence (\|t\| <= 1.25) |
| PAUSED | 5 | Resume: read ~/phoenix/dudect/{reject,key}.txt (re-run `taskset -c 3 ~/phoenix/phx-gcc/dudect_phx_tri reject 20000` / `key 6000` if missing), add results to reports/phx-tri-report.md section 3b, commit |
| next | 8 | Expert review of phx-tri/SECURITY-ARGUMENT.md; post outreach drafts; push |

## Next (after approval)
- Resolve Q1-Q3 by reading primary sources (FIPS 207 status, ABK25 full text, CT-KAT).
- Verify the size table in phase1-review.md section 5 against specs.
- Phase 2 proposal for the approved direction(s).
