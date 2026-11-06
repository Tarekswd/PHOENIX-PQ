# PHX-Tri v0: specification

Experimental three-way hybrid KEM: **X25519 + ML-KEM-768 + HQC-3**, with an X-Wing-style (QSF) combiner. Research artifact, not for production use. Author: Tarek Ahmadieh. Reference implementation: `phx-tri/src/phx_tri.c`. Test vectors: `phx-tri/vectors/phx_tri_v0.txt`.

## Components

| Component | Standard / version | Implementation used |
|---|---|---|
| ML-KEM-768 | FIPS 203 | liboqs `8adabb7` (`OQS_KEM_ml_kem_768_*_derand`) |
| HQC-3 | HQC specification 2025-08-22 (FIPS 207 pending) | official v5.1.0 reference code + `hqc-ct-hardening.patch` |
| X25519 | RFC 7748 | OpenSSL 3 |
| SHA3-256, SHAKE256 | FIPS 202 | OpenSSL 3 |

## Sizes (bytes)

| | Value | Layout |
|---|---|---|
| Public key | 5,730 | pk_M (1,184) \|\| pk_H (4,514) \|\| pk_X (32) |
| Secret key | 32 | seed |
| Ciphertext | 10,098 | ct_M (1,088) \|\| ct_H (8,978) \|\| ct_X (32) |
| Shared secret | 32 | |

## Algorithms

```
KEYGEN_DOMAIN = "PHX-Tri-v0/keygen"        (17 ASCII bytes)
LABEL         = "PHX-Tri-v0"               (10 ASCII bytes)

Expand(seed):                                              seed: 32 bytes
    e = SHAKE256(KEYGEN_DOMAIN || seed, 128 bytes)
    return d_M = e[0:64], seed_H = e[64:96], sk_X = e[96:128]

KeyGenDerand(seed):
    (d_M, seed_H, sk_X) = Expand(seed)
    (pk_M, _)  = ML-KEM-768.KeyGen_internal(d_M)           (64-byte d || z)
    (pk_H, _)  = HQC-3.KeyGen with seed_kem = seed_H
    pk_X       = X25519(sk_X, 9)
    return pk = pk_M || pk_H || pk_X,  sk = seed

EncapsDerand(pk, eseed):                                   eseed: 104 bytes
    (ss_M, ct_M) = ML-KEM-768.Encaps_internal(pk_M, eseed[0:32])
    (ss_H, ct_H) = HQC-3.Encaps with m = eseed[32:56], salt = eseed[56:72]
    ek_X = eseed[72:104];  ct_X = X25519(ek_X, 9);  ss_X = X25519(ek_X, pk_X)
    ss = SHA3-256(ss_M || ss_H || ss_X || ct_X || pk_X || LABEL)
    return (ss, ct = ct_M || ct_H || ct_X)

Decaps(sk, ct):
    (d_M, seed_H, sk_X) = Expand(sk);  regenerate (pk_M, sk_M) from d_M and pk_X from sk_X
    ss_M = ML-KEM-768.Decaps(sk_M, ct_M)
    ss_H = HQC-3.Decaps(seed_H, ct_H)                     (HQC's decapsulation key is its seed)
    ss_X = X25519(sk_X, ct_X)
    return SHA3-256(ss_M || ss_H || ss_X || ct_X || pk_X || LABEL)
```

`KeyGen` and `Encaps` call the derandomized versions with fresh randomness from the operating system (OpenSSL `RAND_priv_bytes`).

## Design notes

- **Combiner.** This follows X-Wing's QSF (draft-connolly-cfrg-xwing-kem-11): the post-quantum ciphertexts are omitted from the hash, while the X25519 ciphertext and public key are included. Omitting ct_M and ct_H relies on both KEMs being C2PRI (X-Wing; Starfighters, IEEE S&P 2026, sections 4.1-4.2). See `SECURITY-ARGUMENT.md`.
- **Domain separation.** X-Wing expands its seed as `SHAKE256(seed)` with no prefix. Without `KEYGEN_DOMAIN`, the same seed used in X-Wing and in PHX-Tri would yield identical ML-KEM and X25519 keys. A test checks that the two expansions differ.
- **Low-order X25519 points.** RFC 7748 defines the result as 32 zero bytes. OpenSSL reports an error instead; PHX-Tri then uses 32 zero bytes, as X-Wing's combiner would hash. The other two shared secrets still enter the combiner.
- **Failure behaviour.** Every function returns -1 on failure with its outputs zeroed. There is no fallback to weaker behaviour. HQC's randomness is fed from a per-call, thread-local buffer, and a short buffer is an error.
- **Cost.** Decapsulation regenerates all component keys from the 32-byte seed, as X-Wing does. This favours a small secret key over speed.
