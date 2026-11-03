# Phase 1 Literature Review: Scope Limited to Three Selected Directions

Status: DRAFT for human review. Date: 2026-11-03.

Directions selected by the project lead:

1. Hybrid combiners: formal security of three-way lattice + code + classical KEM combiners, and their cost.
2. Implementation security: side-channel / constant-time audit of HQC or ML-KEM reference code.
3. A new construction: only if this review shows a real gap.

Evidence key: every source below was located via web search or page fetch on 2026-11-03.
"Read" = abstract or page content was read. "Located" = only the title/snippet was seen; full paper not yet read.
Peer review status is marked; ePrint = preprint unless a venue is listed.

---

## 1. Standards baseline

| Item | Status | Source |
|---|---|---|
| ML-KEM | FIPS 203, August 2024, DOI 10.6028/NIST.FIPS.203 | cited in RFC 9954 references (read) |
| HQC | Selected by NIST on 2025-03-11 as backup KEM to ML-KEM; draft standard to be FIPS 207 | nist.gov news 2025-03 (read); CSRC "FIPS 207: HQC-KEM" talk, Sept 2025 (located) |
| HQC draft publication | UNCLEAR: secondary sources disagree on whether the FIPS 207 initial public draft is out | open question Q1 |
| Hybrid TLS 1.3 | RFC 9954 "Hybrid Key Exchange in TLS 1.3" | rfc-editor.org (read) |
| Generic hybrid KEMs | IRTF CFRG draft-irtf-cfrg-hybrid-kems, at revision -12; defines PQ + traditional (two-component) frameworks | datatracker (located) |

---

## 2. Direction 1: Hybrid KEM combiners

### 2.1 Prior work

| # | Work | Venue / status | What it covers | Relevance |
|---|---|---|---|---|
| C1 | Giacon, Heuer, Poettering, "KEM Combiners" | PKC 2018, pp. 190-218 (peer-reviewed) | Formalises KEM combiners; parallel combiners with split-key PRF core are IND-CCA if one input is | Foundation |
| C2 | Bindel, Brendel, Fischlin, Goncalves, Stebila, "Hybrid KEMs and Authenticated Key Exchange" | PQCrypto 2019, LNCS 11505 (peer-reviewed) | Hybrid KEM security against adversaries with varying quantum capability; dualPRF combiner | Foundation; quantum-adversary model |
| C3 | "Security of Hybrid Key Encapsulation", ePrint 2020/1364 | Preprint | n-KEM combiners CtKDF (concatenate-then-KDF) and CasKDF (cascade); proofs mechanised in Coq; concrete bounds | Already handles n > 2 components |
| C4 | Barbosa, Connolly, Duarte, Kaiser, Schwabe, Varner, Westerbaan, "X-Wing" | IACR CiC vol 1 (peer-reviewed); ePrint 2024/039 | Concrete X25519 + ML-KEM-768 KEM; omits ML-KEM ciphertext from KDF using an ML-KEM-specific property | Optimisation template |
| C5 | Alagic, Bajaj, Kocoglu, "The Best of Both KEMs", ePrint 2025/1444 | Preprint | Generalises X-Wing's trick to multiple KEMs: ciphertexts of C2PRI KEMs can be dropped when a split-key PRF is used; proves FO-derived KEMs (BIKE, Classic McEliece, HQC, ML-KEM) are C2PRI in the ROM | Directly overlaps three-way lattice + code + classical |
| C6 | CuKEM, ePrint 2025/1862; also ACM CCS 2025 proceedings entry (located) | Located | Unified hybrid KEM built directly from PKEs | Alternative design |
| C7 | ePrint 2026/569, hybrid KEMs from classical PKEs + PQ KEMs with C2PRI | Preprint, located | Classical PKE + PQ KEM | Adjacent |
| C8 | ePrint 2026/427, hybrid PQ KEMs from IND-CCA2 PKEs (e.g. RSA) | Preprint, located | Adjacent |
| C9 | Palaniswamy, Palmieri, Das, "Panther", ePrint 2026/781 | Preprint | FrodoKEM + ML-KEM combiners, benchmarks vs NIST round 1-4 KEMs | Overlaps cost-benchmark angle. Uses an unfamiliar "Market-Theoretic Security Framework"; treat with caution until read |
| C10 | "Cobra", ePrint 2026/778 | Preprint, located | Nested hybrid KEM; benchmarks 15 methods | Overlaps cost angle |
| C11 | NIST CSRC 2025 talk, "Split-key PRFs and a new notion for hybrid KEM security" | Talk, located | New hybrid security notion | Must read |

### 2.2 Gap assessment

The generic theory question "is an n-way combiner IND-CCA if one component is?" is answered by C1, C3 and C5.
Labelling a three-way combiner as novel *theory* would not survive prior-art review.

Possible remaining gaps (each needs confirmation by reading C5, C9, C10, C11 in full):

- G1.1 Concrete, fully specified X25519 + ML-KEM + HQC (or Classic McEliece) instance with a tight concrete-security bound, in the style of X-Wing. Check whether C5 or CFRG concrete-hybrid drafts already define one.
- G1.2 Whether HQC's *actual* FO variant as specified for FIPS 207 matches the transform class C5 proves C2PRI for. If not, that is a precise, checkable question.
- G1.3 Measured cost of a three-way combiner (bandwidth, latency, code size) against X-Wing, using standard implementations. Modest contribution; C9/C10 partially occupy it.

Verdict: narrow and incremental. Feasible. Novelty must be argued per gap, not for "three-way combiners" generally.

---

## 3. Direction 2: Implementation security of HQC / ML-KEM

### 3.1 Prior work

| # | Work | Status | Finding |
|---|---|---|---|
| S1 | "Exploiting secret-dependent division timings in Kyber implementations" (KyberSlash1/2) | TCHES (peer-reviewed) | Timing leaks in several Kyber implementations including the official reference code; key recovery demonstrated; F* typechecking flags the division |
| S2 | Same paper's related work | Read | Prior timing attacks: non-constant-time ECC in LAC, ciphertext comparison in FrodoKEM, non-constant-time decoding in HQC |
| S3 | "A Power Side-Channel Attack on the CCA2-Secure HQC KEM" | Peer-reviewed (Springer LNCS, DOI 10.1007/978-3-030-68487-7_8), located | First power attack on HQC KEM |
| S4 | "A New Key Recovery Side-Channel Attack on HQC with Chosen Ciphertext" | NIST 4th PQC conference 2022, located | Targets RM decoding / Hadamard transform |
| S5 | "A Power Side-Channel Attack on the Reed-Muller Reed-Solomon Version of HQC" | Peer-reviewed, located | RS decoder power leakage on Cortex-M4 |
| S6 | "Codeword Masking Can Be Harmful Under Replay Attacks on HQC", ePrint 2026/071 | Preprint, read (excerpt) | First attack on the RM *encoder* during FO re-encryption; claims no prior attack targeted it |
| S7 | "OT-PCA: new key-recovery plaintext-checking oracle..." arXiv 2609.35205 | Preprint, located | PC-oracle attack on HQC |
| S8 | Reparaz, Balasch, Verbauwhede, "Dude, is my code constant time?" | DATE 2017, pp. 1697-1702 (peer-reviewed) | dudect statistical timing test |
| S9 | "A usability evaluation of constant-time analysis tools", ePrint 2024/2060 | Preprint, located | Tool comparison incl. Valgrind-based checks |
| S10 | "Systematic Timing Leakage Analysis of NIST PQDSS Candidates", arXiv 2509.04010 | Preprint, located | Methodology template: TIMECOP + Binsec/Rel2 + dudect + RTLF; applied to *signatures*, not KEMs |
| S11 | "CT-KAT: Beyond Pass/Fail in Constant-Time Screening of PQC", MDPI Electronics 2026 | Peer-reviewed journal, located | Constant-time screening of PQC; must read to check whether HQC is already covered |
| S12 | liboqs Valgrind constant-time CI | Blog, located | liboqs already runs ctgrind-style checks; any audit must go beyond this |
| S13 | HQC GitLab issues #31, #39, #46, #49 (read via API 2026-11-03) | Public tracker | Known v5.x defects: uint8_t->uint64_t casts (Armv7 faults), AVX2 find_peaks aliasing (-O2 miscompile) and too-small peak width for odd multiplicities, AVX2 toom_3_mult out-of-bounds read (found with SAW). HQC v5 is under active third-party audit |

### 3.2 Gap assessment

HQC has a dense side-channel attack history (S2-S7), so the code has been hardened repeatedly.
Remaining gaps:

- G2.1 Systematic multi-tool timing audit (S10 methodology) of the *FIPS 207-aligned* HQC reference and optimised code, plus widely used Rust ports. S10 did this for signature candidates only. S11 may already cover it: must check first.
- G2.2 Compiler-introduced leaks: same C source compiled with several compilers/optimisation levels (KyberSlash was partly this kind of issue).
- G2.3 Leakage in the FO re-encryption path in general (S6 shows this path is under-studied for HQC).

Verdict: most feasible of the three on current hardware (timing via WSL; power analysis needs lab equipment and is out of scope). Contribution is empirical; still publishable if results are reproducible. A null result is a valid outcome and must be reported as such.

---

## 4. Direction 3: New construction

No gap justifying a new primitive has been identified in this review.
Per IMPLEMENTATION.md section 6, none will be manufactured. Directions 1 and 2 may surface one (e.g. if G1.2 shows HQC's transform falls outside known C2PRI results, a modified transform could be a scoped contribution).
Re-evaluate after Phase 2.

---

## 5. Lattice vs code-based comparison (relevant to Direction 1 component choice)

Values below are from memory and are NOT yet verified against the specifications. Phase 2 must replace them with spec-sourced numbers.

| | ML-KEM-768 | HQC (cat. 3) | Classic McEliece (cat. 3) | X25519 |
|---|---|---|---|---|
| Assumption | Module-LWE | QC syndrome decoding | Binary Goppa decoding | ECDH (not PQ) |
| Public key | ~1.2 KB | ~4.5 KB | ~0.5 MB | 32 B |
| Ciphertext | ~1.1 KB | ~9 KB | ~0.2 KB | 32 B |
| Algebraic structure | Ring/module | Quasi-cyclic | Unstructured-ish | Group |
| Standard | FIPS 203 | FIPS 207 (pending) | ISO track | RFC 7748 |

Design tension: HQC keeps keys small but ciphertexts large; McEliece the reverse. A three-way combiner inherits the sum.

---

## 6. Recommended ordering (for human decision)

1. Direction 2 first: the cheapest to start, needs no novelty claim up front, and yields artifacts reusable in Direction 1's cost evaluation.
2. Direction 1 in parallel as reading: confirm or eliminate G1.1-G1.3 by reading C5, C11, and the CFRG concrete-hybrid draft.
3. Direction 3 stays closed unless a gap emerges.
