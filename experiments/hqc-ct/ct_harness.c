/*
 * Valgrind constant-time harness for HQC (ctgrind technique).
 *
 * Secrets are marked "undefined" for memcheck; any branch or memory index that
 * depends on them is then reported. Public outputs (pk, ct) are re-marked
 * defined so they do not propagate taint.
 *
 * Modes (one phase tainted per run, so reports attribute cleanly):
 *   keygen          secret: keygen randomness
 *   encaps          secret: encaps randomness (m; salt is also tainted though public)
 *   decaps          secret: dk; valid ciphertext
 *   decaps-invalid  secret: dk; ciphertext with one bit flipped (implicit rejection path)
 *
 * Exit codes: 0 ok, 2 API error, 3 functional failure.
 */
#define _DEFAULT_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/random.h>
#include <valgrind/memcheck.h>

#ifdef HQC_API_V50
#include "api.h"
#include "fips202.h"
#include "symmetric.h"
#define KEYGEN(pk, sk)     crypto_kem_keypair(pk, sk)
#define ENCAPS(ss, ct, pk) crypto_kem_enc(ct, ss, pk)
#define DECAPS(ss, sk, ct) crypto_kem_dec(ss, ct, sk)
extern shake256incctx shake256_prng_ctx;

static void rng_setup(int taint) {
    uint8_t e[48];
    if (getrandom(e, sizeof e, 0) != (ssize_t)sizeof e) return;
    prng_init(e, NULL, 48, 0);
    // ctx[0..24] is the Keccak state; ctx[25] is the squeeze position, which is public
    if (taint) VALGRIND_MAKE_MEM_UNDEFINED(shake256_prng_ctx.ctx, 25 * sizeof(uint64_t));
}
#else
#include "hqc-kem.h"
#include "randombytes.h"
#define KEYGEN(pk, sk)     hqc_kem_keygen(pk, sk)
#define ENCAPS(ss, ct, pk) hqc_kem_encaps(ss, ct, pk)
#define DECAPS(ss, sk, ct) hqc_kem_decaps(ss, sk, ct)
static int g_taint = 0;

int randombytes(uint8_t *out, size_t outlen) {
    size_t done = 0;
    while (done < outlen) {
        ssize_t r = getrandom(out + done, outlen - done, 0);
        if (r <= 0) return -1;
        done += (size_t)r;
    }
    if (g_taint) VALGRIND_MAKE_MEM_UNDEFINED(out, outlen);
    return 0;
}

static void rng_setup(int taint) { g_taint = taint; }
#endif

int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "decaps";
    static uint8_t pk[CRYPTO_PUBLICKEYBYTES], sk[CRYPTO_SECRETKEYBYTES], ct[CRYPTO_CIPHERTEXTBYTES];
    uint8_t ss[CRYPTO_BYTES], ss2[CRYPTO_BYTES];
    int invalid = !strcmp(mode, "decaps-invalid");
    int decaps = invalid || !strcmp(mode, "decaps");

    if (!decaps && strcmp(mode, "keygen") && strcmp(mode, "encaps")) {
        fprintf(stderr, "unknown mode %s\n", mode);
        return 2;
    }

    rng_setup(!strcmp(mode, "keygen"));
    if (KEYGEN(pk, sk) != 0) return 2;
    VALGRIND_MAKE_MEM_DEFINED(pk, sizeof pk);
    VALGRIND_MAKE_MEM_DEFINED(sk, sizeof sk);

    rng_setup(!strcmp(mode, "encaps"));
    if (ENCAPS(ss, ct, pk) != 0) return 2;
    VALGRIND_MAKE_MEM_DEFINED(ct, sizeof ct);
    VALGRIND_MAKE_MEM_DEFINED(ss, sizeof ss);
    rng_setup(0);

    if (invalid) ct[0] ^= 1;
    if (decaps) VALGRIND_MAKE_MEM_UNDEFINED(sk, sizeof sk);
    if (DECAPS(ss2, sk, ct) != 0) return 2;
    VALGRIND_MAKE_MEM_DEFINED(ss2, sizeof ss2);

    // Functional self-check: valid ct must agree, tampered ct must be rejected
    if ((memcmp(ss, ss2, sizeof ss) == 0) == invalid) {
        fprintf(stderr, "FUNCTIONAL FAILURE in mode %s\n", mode);
        return 3;
    }
    return 0;
}
