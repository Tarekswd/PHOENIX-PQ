# Phase 2 Proposal: a constant-time three-way hybrid KEM (X25519 + ML-KEM-768 + HQC-3)

Status: **APPROVED 2026-11-06** by Tarek Ahmadieh. Scope as below; parameters HQC-3 + ML-KEM-768; libraries chosen: liboqs (ML-KEM-768), OpenSSL 3 (X25519, SHA-3, RNG). Implemented in `phx-tri/`; results in reports/phx-tri-report.md. Date: 2026-11-05. Working name: PHX-Tri.

## What is already done by others (not claimed here)

| Piece | Prior work |
|---|---|
| C2PRI of HQC's 2025-08-22 transform, classical and QROM | Starfighters, IEEE S&P 2026 (ePrint 2025/1397), section 4.2 |
| HQC usable in X-Wing's QSF combiner | Starfighters |
| Multi-KEM combiner theorem (omit ciphertexts of C2PRI KEMs) | Alagic-Bajaj-Kocoglu, ePrint 2025/1444, Theorem 8 |
| X25519 + ML-KEM-768 QSF KEM | X-Wing (draft-connolly-cfrg-xwing-kem-11) |
| HQC + ML-KEM + X25519/X448 for TLS 1.3 (shared secrets concatenated into the TLS key schedule) | draft-preussmattsson-tls-hqckem-01 (15 Oct 2026) |

## What would be new (modest, engineering)

1. **Specification.** A *standalone*, X-Wing-style three-way KEM for use outside TLS: one hash over the three shared secrets and the X25519 values only, with deterministic key derivation and test vectors. Not found in the literature or IETF drafts searched on 2026-11-05; not proven absent.
2. **Constant-time implementation.** HQC's reference key sampling is hardened against the Clang `cmp; je` (reports/hqc-ct-audit.md, class E1), and constant-time behaviour is verified with the existing Valgrind harness under GCC and Clang at all optimisation levels, in CI. The fix is offered upstream to HQC and liboqs.
3. **Measurements.** Sizes, speed and KDF cost against X-Wing, against the TLS draft's concatenation approach, and against a generic combiner that hashes all ciphertexts.

## Construction (draft)

```
KeyGen(seed32):   (seed_M, seed_H, sk_X) = SHAKE256(seed32 || label, 64 + 32 + 32 bytes)
                  pk = pk_M (1184) || pk_H (4514) || pk_X (32)              = 5,730 bytes
Encaps(pk):       (ss_M, ct_M) = ML-KEM-768.Encaps(pk_M)
                  (ss_H, ct_H) = HQC-3.Encaps(pk_H)
                  ek_X random; ct_X = X25519(ek_X, base); ss_X = X25519(ek_X, pk_X)
                  ct = ct_M (1088) || ct_H (8978) || ct_X (32)              = 10,098 bytes
                  ss = SHA3-256(ss_M || ss_H || ss_X || ct_X || pk_X || label)
Decaps(sk, ct):   recompute each ss_*, then the same SHA3-256
```

- The combiner follows X-Wing's QSF: the ML-KEM and HQC ciphertexts are omitted because both KEMs are C2PRI (X-Wing; Starfighters 4.1-4.2).
- The KDF input is 166 bytes. A combiner that also hashed both post-quantum ciphertexts would hash 10,066 more bytes per operation.

**Security goal.** IND-CCA if at least one of the following holds:
- X25519 (strong Diffie-Hellman);
- ML-KEM-768 (Module-LWE);
- HQC-3 (quasi-cyclic syndrome decoding).

**Open proof item.** QSF is proven for one DH component plus one C2PRI KEM. The three-way version needs an adaptation. Starfighters notes concurrent work extending its QSI variant to more than two KEMs, and ABK25 Theorem 8 is already n-KEM. That adaptation must be written out and reviewed; it is not assumed.

**Honest trade-off.** The ciphertext is about 9x X-Wing's 1,120 bytes and the public key about 4.7x its 1,216 bytes. This suits long-term confidentiality where hedging against a break of lattices matters, not bandwidth-sensitive uses. It is not "better than X-Wing" overall.

## Plan

1. **Specification and proof adaptation**, written as a note for external review.
2. **Implementation in C:**
   - ML-KEM from a maintained library (mlkem-native or liboqs; to be decided);
   - HQC from upstream v5.1.0 with the E1 hardening;
   - X25519 from an established library (libsodium or OpenSSL). The charter forbids custom primitives (IMPLEMENTATION.md:125-127).
3. **Tests:**
   - KATs and deterministic test vectors;
   - the Valgrind constant-time matrix in CI;
   - dudect on decapsulation.
4. **Benchmarks**, as listed above.
5. **Write-up**, and offer the hardening patch upstream.

## Decisions needed

1. Approve the scope above (an engineering contribution, not a new primitive).
2. Parameter level: HQC-3 + ML-KEM-768 (category 3, as proposed) or HQC-1 + ML-KEM-512 (smaller).
3. Libraries for ML-KEM and X25519.
