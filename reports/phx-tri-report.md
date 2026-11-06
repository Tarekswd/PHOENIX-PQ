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
| OpenSSL `ecx_derive` (providers/implementations/exchange/ecx_exch.c:157), called from `x25519` | **Confirmed benign** (see section 3a) |
| HQC vector.c:144, Clang -O2/-O3 only | Likely tool artifact (see section 1) |

No report was attributed to PHX-Tri's own code or to the hardened HQC selection code.

### 3a. The OpenSSL branch, identified

- **Getting symbols.** Debug symbols for the installed libcrypto came from Ubuntu's ddebs archive (`libssl3t64-dbgsym`). They were available only for 3.0.13-0ubuntu3.16, so the system OpenSSL was updated from -0ubuntu3.15 to that version and the tests re-run.
- **Where it is.** With symbols, Valgrind places the report at `ecx_derive (ecx_exch.c:157)`, i.e. `if (ossl_x25519(secret, privkey, peer_pubkey) == 0)`.
- **What it tests.** In OpenSSL 3.0.13 (crypto/ec/curve25519.c:5616-5623), `ossl_x25519` performs the scalar multiplication and then returns `CRYPTO_memcmp(kZeros, out_shared_key, 32) != 0`, commented "The all-zero output results when the input is a point of small order".
- **Conclusion.** The comparison is constant-time, and the branch reveals only whether the result is all-zero. That happens exactly for small-order peer points, a property of the *public* input. Benign.

### 3b. dudect timing of decapsulation (`phx-tri/ct/dudect_phx_tri.c`)

Setup: fixed-vs-random Welch t-test; inputs prepared per batch of 256 before timing; GCC -O3; core 3 pinned; nothing else running; WSL2 with CPU boost on.

| Test | What varies in class 1 | N | Max \|t\| over 5 crops | Result |
|---|---|---|---|---|
| `ct` | fresh valid ciphertexts, same key | 20,000 | 1.25 | no dependence detected |
| `reject` | ct_M and ct_H each with one bit flipped (implicit rejection) | 20,000 | 0.98 | no dependence detected |
| `key` | fresh key pairs (positive control: HQC's rejection sampler does vary with the key) | 6,000 | 1.70 | **not detected, so the control failed** |

- **The positive control failed.** The `key` test should differ: HQC's rejection sampler makes key derivation time depend on the secret seed by design (reports/hqc-ct-audit.md, class A). dudect did not detect it. The timings were extremely noisy, with class means moving from about 28M to 69M cycles across crop levels, so these runs can resolve only large differences.
- **What this means for `ct` and `reject`.** Their "no dependence detected" is weak evidence. It rules out large ciphertext- or rejection-dependent timing on this host. It says nothing about effects of the size of the rejection sampler's.
- **What would be needed.** A quiet bare-metal Linux machine with frequency scaling and boost disabled, many more measurements, and the `key` control detected first. Not available here.
- **What they do not mean, in any case.** They are not proofs, and they are limited to this host.
- **Machine slowdown.** During these runs the median decapsulation took about 79.8M cycles, against 32.7M in the section 4 benchmark, most likely thermal or power throttling on the laptop {unverified}. Both classes were interleaved under the same conditions, so the comparison stands, but absolute times from this run are not benchmarks.

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

## 4a. ARM (AArch64, ARMv7) under QEMU

`phx-tri/arm.sh` cross-compiles for both targets and runs the tests in QEMU user mode (8.2.2). Toolchain: GCC 13.3 cross-compilers; OpenSSL 3.0.13 built from the release tarball (SHA-256 checked against the published value); liboqs `8adabb7`; static linking.

| Target | Self-tests (quick: 20 round trips + all other checks) | Test vectors vs the x86 file |
|---|---|---|
| AArch64 (`linux-aarch64`) | all pass | identical, 8/8 |
| ARMv7 hard-float (`linux-armv4 -march=armv7-a+fp`) | all pass | identical, 8/8 |

Two portability findings along the way:

1. **HQC's "portable" reference code does not compile on ARM.** In v5.1.0, `src/ref/{vector,parsing,hqc}.h` include `<immintrin.h>`, which is x86-only, although no reference `.c` file uses an intrinsic. `phx-tri/hqc-portability.patch` removes the three includes. HQC issue #4 (macOS compatibility) may be related; not confirmed.
2. **Symbol collision with static OpenSSL.** HQC exports generic global names. `gf_mul` collides with OpenSSL's static libcrypto (`f_impl64.o`, Curve448); the linker refused here, but in other setups a wrong binding is possible. `build.sh` now partially links HQC into one object and keeps only `hqc_kem_keygen/encaps/decaps` global. Verified with `nm`: those three are the only defined globals left.

Limits: QEMU user mode is not real hardware. In particular it may not model alignment traps, so HQC #31 (`uint8_t` buffers cast to `uint64_t *`, also present in v5.1.0's `hqc-kem.c`) is not ruled out on real Armv7.

## 5. Limitations

- **Security argument.** It is assembled from published theorems and is not peer reviewed; its open points are listed in SECURITY-ARGUMENT.md.
- **Platform.** One x86-64 machine under WSL2, so the timings carry WSL noise. No ARM.
- **Valgrind's reach.** It sees only executed paths and branch or address dependence.
- **dudect's reach.** The PHX-Tri dudect runs failed their positive control (section 3b), so they rule out only large timing differences.
- **Dependencies.** The ML-KEM and X25519 code comes from liboqs and OpenSSL, and its constant-time behaviour is their responsibility. Their reports here are triaged, not proven absent.
- **Novelty.** PHX-Tri is not a new primitive. A TLS-level hybrid of the same three components exists (draft-preussmattsson-tls-hqckem-01). PHX-Tri differs in being a standalone KEM with a single QSF-style combiner.
