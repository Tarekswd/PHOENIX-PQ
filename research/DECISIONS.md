# Decisions

## D1 (2026-11-03): Scope limited to three directions
- Decided: Pursue (1) hybrid KEM combiners, (2) HQC / ML-KEM implementation security, (3) new construction only if a gap appears.
- Why: Project lead's choice after the feasibility discussion; a standalone novel KEM is low-probability.
- Alternatives: Novel primitive first (rejected: no evidence of a gap); pure cryptanalysis of other schemes (not selected).
- Uncertainty: Direction 1 novelty is narrow (see RESEARCH_GAP.md).

## D3 (2026-11-03): Direction 2 first; target HQC v5.1.0, not ML-KEM
- Decided: Project lead chose Direction 2. Proposed target is HQC v5.1.0 (with v5.0.0 diff).
- Why: CT-KAT (2026-11-01) already screened ML-KEM; HQC absent. v5.1.0 rewrote RS decoding, sampling, RM generator.
- Alternatives: ML-KEM (overlaps CT-KAT); Rust ports (deferred, cargo absent in WSL).
- Uncertainty: findings may duplicate HQC work item #11; WSL timing noise.
- Status: proposal awaiting approval (research/proposals/direction2-hqc-ct-audit.md).

## D2 (2026-11-03): Constant-time tooling runs in WSL Ubuntu
- Decided: Linux tools (Valgrind/TIMECOP, dudect) run in the existing WSL Ubuntu distro.
- Why: Tools are Linux-native; Windows disk has only 10.2 GB free.
- Uncertainty: WSL timing noise vs bare metal; must be characterised before trusting dudect results.
