# PHX-Tri v0: implementation and evaluation report

Date: 2026-11-06. Specification: [phx-tri/SPEC.md](../phx-tri/SPEC.md). Security argument: [phx-tri/SECURITY-ARGUMENT.md](../phx-tri/SECURITY-ARGUMENT.md). Proposal (approved 2026-11-06): [research/proposals/phx-tri-hybrid-kem.md](../research/proposals/phx-tri-hybrid-kem.md).

## 1. Constant-time hardening of HQC (patch)

`phx-tri/hqc-ct-hardening.patch` applies to official HQC v5.1.0 `src/ref` and adds three value barriers:
- one in `vect_write_support_to_vector` (vector.c);
- two in `gf_carryless_mul` (gf.c).

Each barrier sits on the selection *mask*, so the compiler cannot know it is 0 or all-ones and cannot turn `x & mask` back into a branch.

- **First attempt.** A barrier on the input `tmp` instead of the mask did not work. Clang still branched at the `val |= bit_tab[j] & mask` line, and `gf_carryless_mul` gained a branch at -Os. This matches the Clang x86 cmov-converter behaviour reported in liboqs #2402 / PQClean #608.
- **Result.** HQC-3, ref, {GCC 13.3, Clang 18.1.3} x {-O0..-Os}: all 10 builds pass the official KATs. The Clang `cmp; je` in `vect_write_support_to_vector` (class E1) and the Clang -O1 `gf_mul` branch (E2) are gone at every level.
- **Remaining reports** (pre-existing):
  - the by-design rejection sampler;
  - `find_peaks` at -O0 only;
  - a Clang -O2/-O3 size-1 store report at vector.c:144 whose address is the loop counter, classified as a likely Valgrind artifact. It is not proven to be one.

## 2. Functional tests (`phx-tri/tests`)

- **Self-tests** (`test_phx_tri`), all passing with GCC -O3:
  - 1,000 random keygen/encaps/decaps round trips;
  - deterministic derand encapsulation;
  - a flipped bit in ct_M, ct_H or ct_X gives a different key;
  - a low-order ct_X (u = 0) is handled deterministically;
  - the seed expansion is domain-separated from X-Wing's;
  - a garbage ciphertext yields an implicit-rejection key.
- **Test vectors.** 8 deterministic vectors in `phx-tri/vectors/phx_tri_v0.txt`.
  - `check_vectors.py` recomputes the combiner from the exported component values using Python's `hashlib`, independently of the C/OpenSSL path.
  - Builds with Clang -O1, -O2 and -O3 reproduce the GCC vectors byte for byte.

## 3. Constant-time check of PHX-Tri (`phx-tri/ct`, Valgrind memcheck)

Secrets marked undefined: the keygen seed, the encapsulation seed, and the decapsulation key. Builds: GCC -O3, Clang -O1/-O2/-O3.

| Report location | Assessment |
|---|---|
| HQC `vect_generate_random_support1` | By design: the rejection sampler's iteration count (reports/hqc-ct-audit.md, class A) |
| ML-KEM `mlk_rej_uniform`, `mlk_rej_uniform_asm`, `poly_rej_uniform(_x4)` under `gen_matrix` | False positive: expansion of the **public** matrix from rho. Taint flows because rho is derived from the secret seed. CT-KAT reports the same pattern for mlkem-native |
| OpenSSL libcrypto, called from `x25519` (`EVP_PKEY_derive`) | One branch, most likely OpenSSL's all-zero-result check, which depends only on whether the public point has low order {unverified: libcrypto has no symbols here} |
| HQC vector.c:144, Clang -O2/-O3 only | Likely tool artifact (see section 1) |

No report was attributed to PHX-Tri's own code or to the hardened HQC selection code.

## 4. Performance (GCC -O3, AMD Ryzen 7 PRO 4750U, WSL2, pinned core, median of 201)

| Operation | Cycles |
|---|---|
| keygen | 8,374,387 |
| encaps | 16,473,833 |
| decaps | 32,706,436 |
| combiner as specified (166 B hashed) | 2,244 |
| generic combiner (also hashes ct_M and ct_H: 166 + 10,066 B) | 48,059 |

- HQC-3's portable reference code dominates the cost. Decapsulation also regenerates every component key from the seed.
- The QSF-style omission of the post-quantum ciphertexts saves about 46,000 cycles, roughly 0.14% of decapsulation. **That is negligible here.** The design follows X-Wing for its security structure, not for speed.

| | PHX-Tri v0 | X-Wing |
|---|---|---|
| Public key | 5,730 B | 1,216 B |
| Ciphertext | 10,098 B | 1,120 B |
| Hardness assumptions | Module-LWE, QC syndrome decoding, ECDH | Module-LWE, ECDH |

## 5. Limitations

- **Security argument.** It is assembled from published theorems and is not peer reviewed; its open points are listed in SECURITY-ARGUMENT.md.
- **Platform.** One x86-64 machine under WSL2, so the timings carry WSL noise. No ARM.
- **Valgrind's reach.** It sees only executed paths and branch or address dependence. dudect was not run on PHX-Tri.
- **Dependencies.** The ML-KEM and X25519 code comes from liboqs and OpenSSL, and its constant-time behaviour is their responsibility. Their reports here are triaged, not proven absent.
- **Novelty.** PHX-Tri is not a new primitive. A TLS-level hybrid of the same three components exists (draft-preussmattsson-tls-hqckem-01). PHX-Tri differs in being a standalone KEM with a single QSF-style combiner.
