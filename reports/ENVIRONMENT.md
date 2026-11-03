# Environment Report (Phase 0)

Captured 2026-11-03 via PowerShell on the research machine.

| Item | Value |
|---|---|
| OS | Windows 11 Pro 10.0.26300 |
| CPU | AMD Ryzen 7 PRO 4750U, 8 cores / 16 threads |
| RAM | 15.3 GB |
| Free disk (C:) | 10.2 GB (tight; avoid large toolchains on Windows side) |
| Python | 3.14.8 (sympy, numpy, pytest present; hypothesis, cryptography, liboqs bindings absent) |
| Rust | rustc / cargo 1.89.0 |
| SageMath | not installed |
| Git | 2.53.0 |
| WSL | Ubuntu and docker-desktop distros present |

## Implications

- Constant-time tooling (Valgrind/TIMECOP, dudect) is Linux-native: run Direction 2 inside WSL Ubuntu.
- SageMath is only needed for Direction 3 (new construction), which is deferred. Install in WSL if/when approved.
- Laptop CPU with frequency scaling: timing measurements need a pinned core and disabled boost, otherwise dudect results are noisy. Record this in every benchmark.
- Missing Python deps (hypothesis, cryptography) are installed only when a phase needs them.
