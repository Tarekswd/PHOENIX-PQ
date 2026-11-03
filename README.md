# PHOENIX-PQ

Independent research on post-quantum key encapsulation (KEM): literature review, implementation security, and (only if a real gap is found) new constructions.

The project charter, with phases, rules and success criteria, is in [IMPLEMENTATION.md](IMPLEMENTATION.md).

> Nothing in this repository is production-ready, and no construction or implementation here is claimed secure.

## Current status

| Phase | State |
|---|---|
| 0 Environment | Done ([reports/ENVIRONMENT.md](reports/ENVIRONMENT.md)) |
| 1 Literature review | Done ([research/literature/phase1-review.md](research/literature/phase1-review.md)) |
| 2 Direction 2: HQC constant-time audit | First pass done ([proposal](research/proposals/direction2-hqc-ct-audit.md), [report](reports/hqc-ct-audit.md)) |

Research directions, chosen after Phase 1:

1. Hybrid KEM combiners (lattice + code + classical): narrowed to concrete gaps, see [RESEARCH_GAP.md](research/RESEARCH_GAP.md).
2. Constant-time audit of HQC v5.1.0: active.
3. New construction: closed until a real gap is shown.

## Layout

```
IMPLEMENTATION.md        project charter
research/                literature, gaps, decisions, assumptions, bibliography
  literature/            reviews
  proposals/             Phase 2 proposals
reports/                 environment report, security status, audit reports
experiments/hqc-ct/      HQC constant-time audit tooling
```

## Reproducing the HQC constant-time audit

Requirements: Linux (tested on Ubuntu 24.04 under WSL2), gcc, clang, Valgrind with `valgrind/memcheck.h`, OpenSSL headers (`libssl-dev`, for the HQC v5.1.0 KAT generator), Python 3.

```sh
git clone https://gitlab.com/pqc-hqc/hqc.git && cd hqc && git worktree add ../hqc-v5.0.0 v5.0.0 && cd ..
cd PHOENIX-PQ/experiments/hqc-ct
bash build_matrix.sh ../../../hqc        out/v5.1.0 v5.1.0   # 20 builds, each checked against official KATs
bash build_matrix.sh ../../../hqc-v5.0.0 out/v5.0.0 v5.0.0
bash run_ct.sh out/v5.1.0 && bash run_ct.sh out/v5.0.0  # Valgrind memcheck, 4 modes per build
python3 parse_vg.py out/v5.1.0 out/v5.0.0 --dest results
```

Method: secrets are marked undefined for Valgrind memcheck (the ctgrind technique), so any branch or memory index that depends on them is reported. Each build is also scanned for division instructions, whose latency can depend on operands. This tests the paths that were executed; it is not a proof of constant-time behaviour.

Raw results stay out of the repository until coordinated disclosure, see [SECURITY.md](SECURITY.md).

## Tracking

[PROGRESS](research/PROGRESS.md) · [DECISIONS](research/DECISIONS.md) · [OPEN_QUESTIONS](research/OPEN_QUESTIONS.md) · [FAILED_HYPOTHESES](research/FAILED_HYPOTHESES.md) · [ASSUMPTIONS](research/ASSUMPTIONS.md) · [SECURITY_STATUS](reports/SECURITY_STATUS.md)

## License

Not yet chosen. Until a license is added, all rights are reserved by the author.
