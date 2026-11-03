# Phase 2 Proposal: Constant-Time Audit of HQC v5.1.0

Status: AWAITING HUMAN APPROVAL. Date: 2026-11-03.
Primitive: HQC-KEM (code-based, selected by NIST 2025-03-11, standard FIPS 207 not yet final).
Evidence for every factual statement is listed in section 9.

## 1. Why HQC and not ML-KEM

- ML-KEM has just been screened by CT-KAT (MDPI Electronics, 2026-11-01) with 11 tools, GCC 13.3 / Clang 18.1.3, -O0..-Os. HQC was not included. Repeating ML-KEM adds little.
- HQC v5.1.0 was released about a week before this proposal, and it changes sampling, the GF(2^8) polynomial, RS decoding, the RM(1,7) generator matrix, the parameters and the XOF domain separator. New code has had the least scrutiny.
- The HQC team publicly scopes its claim to timing attacks only, with compiler hardening "a work in progress". That makes timing the right threat model, and the team invites reports through GitLab work items.

## 2. What is already known (must not be claimed as new)

HQC work item #11 lists open compiler-induced constant-time issues at -O3:

| Build | Open items |
|---|---|
| AVX2, -O3 | barret_reduce, gf_reduce, gf2x base_mul (also PQClean issue 608), encode, compute_error_values, Reed-Muller (compute_elp marked done as part of the RS update) |
| Portable, -O3 (best effort) | single_bit_mask, barret_reduce, compute_error_values, gf_reduce, encode |

The work item's own open task "Review code to identify all possible compiler induced problems" is unfinished. Item #12 ("known issues, won't fix") is still empty.

## 3. Research questions

- RQ1 (completeness): Under the HQC team's *recommended* build and under the GCC/Clang x -O0..-O3/-Os matrix, which secret-dependent branches, memory accesses, and variable-latency instructions exist in v5.1.0, beyond those listed in #11?
- RQ2 (new code): Do the v5.1.0 changes (new RS decoder, new sampling, new RM generator, GF(2^8) polynomial) introduce structural leaks that v5.0.0 did not have? Method: same pipeline on both tags, diff the findings.
- RQ3 (exploitability): For each structural finding, is a timing difference measurable in decapsulation with attacker-chosen ciphertexts? Outcomes are reported as measured / not measured under stated conditions. A not-measured result is never reported as "safe".
- RQ4 (downstream, optional): Which HQC version do PQClean / liboqs ship, and do their copies carry the same findings?

Hypothesis H1: at least one leak exists in v5.1.0 under a build configuration the HQC team considers in scope, and it is not in #11.
H1 may be false. A documented negative result for RQ1/RQ2 is still a deliverable.

## 4. Method

1. Build matrix: {ref, x86_64/avx256} x {gcc 13.3, clang 18.1.3} x {-O0, -O1, -O2, -O3, -Os} x {v5.0.0, v5.1.0}, plus the team's recommended flags. Each build passes the official KAT tests first; a build that fails KATs is excluded and logged.
2. Structural (dynamic taint): Valgrind memcheck with secret regions marked undefined (ctgrind/TIMECOP technique) on keygen, encaps, decaps. Secret = sk, decryption intermediates, the message m, and FO re-encryption randomness.
3. Binary scan: objdump for div/idiv and other variable-latency instructions on secret-tainted paths (CT-KAT showed this catches KyberSlash where taint analysis did not).
4. Timing (secondary): dudect-style Welch t-test on decaps with fixed-vs-random ciphertext classes, on a pinned core.
5. Triage: every finding gets (function, build, tool, secret source, in-#11 yes/no, timing evidence yes/no/untested).
6. Optional static verification (Binsec/Rel or CT-LLVM) only on functions flagged in step 2/3, if setup cost is acceptable.

## 5. Threat model and limits

- Attacker: local or co-located process that can submit ciphertexts to decapsulation and measure its time. Power, EM and fault attacks are out of scope, matching the HQC team's stated scope.
- Limits: WSL2 on a laptop (Ryzen 7 PRO 4750U); CPU boost is not controllable inside WSL. Timing results (step 4) are therefore weak evidence. Structural results (steps 2-3) do not depend on noise.
- One x86-64 microarchitecture only; ARM is not covered.
- Valgrind taint covers only executed paths, so coverage is reported per function.

## 6. Deliverables

- reports/hqc-ct-audit.md: findings table, method, coverage, negative results.
- experiments/hqc-ct/: build scripts, harnesses, raw logs, pinned versions (reproducibility package).
- Regression tests for every confirmed finding.
- Disclosure draft for the HQC team (NOT sent without approval).

## 7. Evaluation against Phase 2 criteria

| Criterion | Assessment |
|---|---|
| Originality | Moderate: first multi-tool, multi-build audit of HQC v5.1.0 and a v5.0.0-vs-v5.1.0 diff. Not a new technique. |
| Literature overlap | CT-KAT (method, no HQC); HQC #11 (known list); prior HQC power/timing attacks (older versions, different code). |
| Security justification | N/A (audit, no new scheme). |
| Feasibility | High. gcc, clang, cmake present in WSL; Valgrind must be installed. |
| Risk | Findings may duplicate #11, or timing may be unmeasurable under WSL. Both outcomes are still reportable. |
| Contribution | Practical: helps harden a NIST-selected KEM before FIPS 207 is final. |

## 8. Approval needed

1. Approve this scope (HQC v5.1.0 + v5.0.0 diff; ML-KEM out of scope).
2. Allow installing Valgrind in WSL Ubuntu (`sudo apt install valgrind`; needs your sudo password).
3. Disclosure: findings go to the HQC team privately before any publication? (Recommended: yes. The work item tracker is public, so ask them for a private channel first.)

## 9. Evidence (fetched 2026-11-03)

- HQC repo, README security scope, tags v5.0.0/v5.1.0: https://gitlab.com/pqc-hqc/hqc
- v5.1.0 changelog: https://gitlab.com/pqc-hqc/hqc/-/raw/main/CHANGELOG
- Known issues: https://gitlab.com/pqc-hqc/hqc/-/work_items/11 and /12
- CT-KAT: https://www.mdpi.com/2079-9292/15/21/5068 (DOI 10.3390/electronics15215068)
- WSL toolchain: gcc 13.3.0, clang 18.1.3, cmake 3.28.3, Valgrind missing (envcheck.sh run in WSL Ubuntu 24.04.4)
