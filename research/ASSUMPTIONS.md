# Assumptions Register

No new hardness assumptions are introduced by this project at present.

| Assumption | Used by | Classical / quantum status |
|---|---|---|
| Module-LWE | ML-KEM | Conjectured hard for both; reductions from worst-case module lattice problems (to be cited from spec in Phase 2) |
| QC syndrome decoding (HQC) | HQC | Conjectured hard for both; structured variant of SD |
| Binary Goppa decoding | Classic McEliece | Conjectured hard for both; longest-studied PQ assumption |
| Strong Diffie-Hellman on X25519 | X25519, X-Wing | Classical only; broken by Shor |
| SHA3 / SHAKE as PRF or random oracle | All combiners | ROM proofs are heuristic; QROM proofs needed for quantum claims |
| Split-key PRF | GHP18-style combiners | Instantiated via hash functions |

Every combiner result is "secure if at least one component is secure, under the stated KDF assumption". No proof of security for any underlying problem exists.
