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
| HQC #46 decoder defect: DFR impact | Done ([report](reports/hqc-findpeaks-dfr.md), tooling in `experiments/hqc-dfr/`) |
| Direction 1, G1.2: C2PRI of HQC v5 transform | Closed by prior art: Starfighters, IEEE S&P 2026 ([note](research/g1.2-hqc-v5-c2pri.md)) |
| 7 Paper | Draft ([paper/main.pdf](paper/main.pdf), source `paper/main.tex`) |

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

## Reproducing the decryption-failure study (HQC issue #46)

```sh
cd experiments/hqc-dfr
for v in 1 3 5; do bash build.sh ../../../hqc $v out/hqc$v; out/hqc$v/dfr_sim selftest 300; done
bash run_all.sh out runs 900        # ~15 min on 16 threads: real-noise runs + fixed-weight validation
python3 analyze.py runs             # exact model vs simulation, failure-rate impact
bash crafted.sh ../../../hqc out/crafted 5000 500   # decapsulation outputs, reference vs AVX2 decoder
```

CI (`.github/workflows/reproduce.yml`) re-runs the build matrix with its KAT gate, the simulator's positive controls, a short simulation with analysis, and the crafted-ciphertext check on every push. The Valgrind matrix, the long simulations and dudect stay manual.

## Citation and license

See [CITATION.cff](CITATION.cff). Code and text are released under the [MIT License](LICENSE). Author: Tarek Ahmadieh.

## Tracking

[PROGRESS](research/PROGRESS.md) · [DECISIONS](research/DECISIONS.md) · [OPEN_QUESTIONS](research/OPEN_QUESTIONS.md) · [FAILED_HYPOTHESES](research/FAILED_HYPOTHESES.md) · [ASSUMPTIONS](research/ASSUMPTIONS.md) · [SECURITY_STATUS](reports/SECURITY_STATUS.md)
