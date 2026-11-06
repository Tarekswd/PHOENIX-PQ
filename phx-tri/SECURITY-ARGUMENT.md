# PHX-Tri v0: security argument (reduction to published results; NOT peer reviewed)

**Goal.** PHX-Tri is IND-CCA secure if at least one of the following holds:
- (a) ML-KEM-768 is IND-CCA;
- (b) HQC-3 is IND-CCA;
- (c) the strong Diffie-Hellman assumption holds for X25519.

SHA3-256 is modelled as a random oracle. Cases (a) and (b) are post-quantum; case (c) is classical only, since Shor's algorithm breaks X25519.

This is an argument assembled from published theorems. **It still needs a cryptographer to check that each theorem's hypotheses are met exactly** (see "Open points").

## Ingredients (all published)

| Fact | Source |
|---|---|
| ML-KEM is C2PRI; classical bound (q+3)/2^256, quantum 8(2q+3)^2/2^256 | Starfighters (Connolly et al., IEEE S&P 2026), Table 3; also X-Wing |
| HQC (2025-08-22 transform) is C2PRI, same bounds | Starfighters, section 4.2, Table 3 |
| n-KEM combiner theorem: W = F(k_1..k_n, c_{l+1}..c_n) is IND-CCA if one KEM is IND-CCA and the omitted ones (other than it) are C2PRI, F a split-key PRF; stated for QPT adversaries | Alagic-Bajaj-Kocoglu, ePrint 2025/1444, Theorem 8 |
| QSF (X-Wing's combiner) is classically IND-CCA under strong DH in the X25519 nominal group, with ct_X and pk_X hashed | X-Wing (Barbosa et al., IACR CiC 1(1), 2024); generalised by Starfighters |

## Lemma (product C2PRI)

Let K_MH be the product KEM with ciphertext (ct_M, ct_H) and key (ss_M, ss_H). If ML-KEM and HQC are both C2PRI, so is K_MH.

*Proof.* Suppose (ct_M\*, ct_H\*) != (ct_M, ct_H) decapsulates to the same pair (ss_M, ss_H). Then at least one component's ciphertext differs while that component's key is unchanged, which is a C2PRI break of that component. The advantage is therefore at most the sum of the two C2PRI advantages.

## Cases

- **(a) ML-KEM IND-CCA, post-quantum.**
  - Apply ABK25 Theorem 8 with n = 3: ML-KEM is the secure KEM; the omitted ciphertexts are {ct_M, ct_H}; ct_X is included.
  - The only omitted ciphertext of a KEM other than the secure one is ct_H, so the hypothesis needed is that HQC is C2PRI (Starfighters, quantum bound).
  - X25519's ciphertext is included in the hash, so no property of X25519 is needed.
- **(b) HQC IND-CCA, post-quantum.** Symmetric to (a); it needs ML-KEM to be C2PRI.
- **(c) Strong DH, classical.**
  - Write PHX-Tri as QSF(K_MH, X25519), with the post-quantum key ss_M || ss_H (64 bytes) in place of ML-KEM's 32-byte key.
  - QSF needs the post-quantum KEM to be C2PRI; by the lemma, K_MH is.
  - The X-Wing / Starfighters theorem then gives classical IND-CCA under strong DH.

The intuition behind all three cases is the same. A decapsulation query that changes some ciphertext either changes a hashed input (ct_X, or a shared secret), which gives an independent random-oracle output, or keeps every hashed input equal, which requires a C2PRI break.

## Open points (to be checked by an expert)

1. ABK25 Theorem 8 is stated with a split-key PRF F. Here F is SHA3-256 on a fixed-order input with a label, modelled as a random oracle. That RO-to-split-key-PRF step is standard (X-Wing uses it), but it should be confirmed for three inputs.
2. In case (a)/(b), X25519 enters Theorem 8 as a KEM whose ciphertext is included. Theorem 8 makes no demand on it there, but the formal fit should be checked: X25519 is not itself IND-CCA, and pk_X is also hashed.
3. In case (c), QSF is applied with a 64-byte post-quantum key, where X-Wing uses 32 bytes. In the ROM this should not matter; confirm it against the published theorem.
4. Concrete bounds: sum the terms. They are dominated by the C2PRI bounds above and by the components' own IND-CCA advantages.
5. Binding properties (Cremers et al. framework): not analysed. Starfighters analyses QSF's binding for two components.

## What is NOT claimed

- That any component is secure. These are assumptions.
- That PHX-Tri is better than X-Wing. It is far larger (10,098-byte ciphertext against 1,120) and slower, in exchange for one more independent hardness assumption.
- Novelty of the cryptography. The contribution is the concrete specification, the constant-time implementation and the measurements.
