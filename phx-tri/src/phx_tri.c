/*
 * PHX-Tri v0 implementation. See phx-tri/SPEC.md for the normative description.
 *
 * Components (no custom primitives, IMPLEMENTATION.md:125-127):
 *   ML-KEM-768          liboqs (OQS_KEM_ml_kem_768_*_derand)
 *   HQC-3               official HQC v5.1.0 reference code + phx-tri/hqc-ct-hardening.patch
 *   X25519, SHA3-256,
 *   SHAKE256, OS RNG    OpenSSL 3 (EVP)
 */
#include "phx_tri.h"

#include <string.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <oqs/oqs.h>
#include "hqc-kem.h"  /* HQC-3 build: hqc_kem_keygen/encaps/decaps and CRYPTO_* sizes */

#define MLKEM_PK 1184
#define MLKEM_SK 2400
#define MLKEM_CT 1088
#define HQC_PK   4514
#define HQC_CT   8978
#define HQC_M    24
#define HQC_SALT 16
#define X_LEN    32

_Static_assert(CRYPTO_PUBLICKEYBYTES == HQC_PK && CRYPTO_CIPHERTEXTBYTES == HQC_CT, "HQC-3 sizes");
_Static_assert(CRYPTO_SECRETKEYBYTES == 32 && CRYPTO_BYTES == 32, "HQC-3 key sizes");
_Static_assert(PHX_TRI_PUBLICKEYBYTES == MLKEM_PK + HQC_PK + X_LEN, "pk layout");
_Static_assert(PHX_TRI_CIPHERTEXTBYTES == MLKEM_CT + HQC_CT + X_LEN, "ct layout");
_Static_assert(PHX_TRI_ENCAPS_SEEDBYTES == 32 + HQC_M + HQC_SALT + X_LEN, "eseed layout");

static const uint8_t KEYGEN_DOMAIN[] = "PHX-Tri-v0/keygen";  /* differs from X-Wing's unprefixed SHAKE256(seed) */
static const uint8_t COMBINER_LABEL[] = "PHX-Tri-v0";        /* 10 bytes, without the terminating NUL */

/* HQC draws its randomness through randombytes(); we feed it from a per-call, thread-local buffer.
 * An empty or short buffer is an error, never a silent fallback. */
static _Thread_local const uint8_t *rb_buf;
static _Thread_local size_t rb_left;

int randombytes(uint8_t *out, size_t outlen) {
    if (rb_buf == NULL || outlen > rb_left) return -1;
    memcpy(out, rb_buf, outlen);
    rb_buf += outlen;
    rb_left -= outlen;
    return 0;
}

static void rb_set(const uint8_t *buf, size_t len) { rb_buf = buf; rb_left = len; }
static int rb_drained(void) { int ok = rb_left == 0; rb_buf = NULL; rb_left = 0; return ok; }

static int shake256(uint8_t *out, size_t outlen, const uint8_t *a, size_t alen, const uint8_t *b, size_t blen) {
    EVP_MD_CTX *c = EVP_MD_CTX_new();
    int ok = c && EVP_DigestInit_ex(c, EVP_shake256(), NULL) && EVP_DigestUpdate(c, a, alen) &&
             EVP_DigestUpdate(c, b, blen) && EVP_DigestFinalXOF(c, out, outlen);
    EVP_MD_CTX_free(c);
    return ok ? 0 : -1;
}

static int combine(uint8_t ss[32], const uint8_t ss_m[32], const uint8_t ss_h[32], const uint8_t ss_x[32],
                   const uint8_t ct_x[32], const uint8_t pk_x[32]) {
    EVP_MD_CTX *c = EVP_MD_CTX_new();
    unsigned int n = 0;
    int ok = c && EVP_DigestInit_ex(c, EVP_sha3_256(), NULL) && EVP_DigestUpdate(c, ss_m, 32) &&
             EVP_DigestUpdate(c, ss_h, 32) && EVP_DigestUpdate(c, ss_x, 32) && EVP_DigestUpdate(c, ct_x, 32) &&
             EVP_DigestUpdate(c, pk_x, 32) && EVP_DigestUpdate(c, COMBINER_LABEL, sizeof COMBINER_LABEL - 1) &&
             EVP_DigestFinal_ex(c, ss, &n) && n == 32;
    EVP_MD_CTX_free(c);
    return ok ? 0 : -1;
}

/* X25519(scalar, point). OpenSSL rejects an all-zero result (low-order point); RFC 7748 defines that
 * result as 32 zero bytes, which is what X-Wing's combiner would hash, so we return zeros in that case.
 * The other two shared secrets still enter the combiner. */
static int x25519(uint8_t out[32], const uint8_t scalar[32], const uint8_t point[32]) {
    EVP_PKEY *sk = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, NULL, scalar, 32);
    EVP_PKEY *pk = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, NULL, point, 32);
    EVP_PKEY_CTX *c = sk ? EVP_PKEY_CTX_new(sk, NULL) : NULL;
    size_t n = 32;
    int ret = -1;
    if (c && pk && EVP_PKEY_derive_init(c) == 1 && EVP_PKEY_derive_set_peer(c, pk) == 1) {
        if (EVP_PKEY_derive(c, out, &n) == 1 && n == 32) ret = 0;
        else { memset(out, 0, 32); ret = 0; }
    }
    EVP_PKEY_CTX_free(c);
    EVP_PKEY_free(pk);
    EVP_PKEY_free(sk);
    return ret;
}

static int x25519_public(uint8_t pub[32], const uint8_t scalar[32]) {
    EVP_PKEY *sk = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, NULL, scalar, 32);
    size_t n = 32;
    int ok = sk && EVP_PKEY_get_raw_public_key(sk, pub, &n) == 1 && n == 32;
    EVP_PKEY_free(sk);
    return ok ? 0 : -1;
}

/* expanded = SHAKE256(KEYGEN_DOMAIN || seed, 128): [0,64) ML-KEM d||z, [64,96) HQC seed, [96,128) X25519 scalar */
static int expand(uint8_t expanded[128], const uint8_t seed[32]) {
    return shake256(expanded, 128, KEYGEN_DOMAIN, sizeof KEYGEN_DOMAIN - 1, seed, 32);
}

int phx_tri_keygen_derand(uint8_t *pk, uint8_t *sk, const uint8_t seed[PHX_TRI_KEYGEN_SEEDBYTES]) {
    uint8_t ex[128], sk_m[MLKEM_SK], dk_h[32];
    int ret = -1;
    OQS_init();
    if (expand(ex, seed)) goto out;
    if (OQS_KEM_ml_kem_768_keypair_derand(pk, sk_m, ex) != OQS_SUCCESS) goto out;
    rb_set(ex + 64, 32);
    if (hqc_kem_keygen(pk + MLKEM_PK, dk_h) != 0 || !rb_drained()) goto out;
    if (x25519_public(pk + MLKEM_PK + HQC_PK, ex + 96)) goto out;
    memcpy(sk, seed, 32);
    ret = 0;
out:
    rb_drained();
    OPENSSL_cleanse(ex, sizeof ex);
    OPENSSL_cleanse(sk_m, sizeof sk_m);
    OPENSSL_cleanse(dk_h, sizeof dk_h);
    if (ret) { memset(pk, 0, PHX_TRI_PUBLICKEYBYTES); memset(sk, 0, PHX_TRI_SECRETKEYBYTES); }
    return ret;
}

int phx_tri_keygen(uint8_t *pk, uint8_t *sk) {
    uint8_t seed[32];
    int ret = RAND_priv_bytes(seed, sizeof seed) == 1 ? phx_tri_keygen_derand(pk, sk, seed) : -1;
    OPENSSL_cleanse(seed, sizeof seed);
    return ret;
}

#ifdef PHX_TRI_DEBUG
static uint8_t dbg[5][32];
void phx_tri_debug_last(uint8_t ss_m[32], uint8_t ss_h[32], uint8_t ss_x[32], uint8_t ct_x[32], uint8_t pk_x[32]) {
    memcpy(ss_m, dbg[0], 32); memcpy(ss_h, dbg[1], 32); memcpy(ss_x, dbg[2], 32);
    memcpy(ct_x, dbg[3], 32); memcpy(pk_x, dbg[4], 32);
}
#endif

int phx_tri_encaps_derand(uint8_t *ss, uint8_t *ct, const uint8_t *pk, const uint8_t eseed[PHX_TRI_ENCAPS_SEEDBYTES]) {
    uint8_t ss_m[32], ss_h[32], ss_x[32];
    const uint8_t *pk_x = pk + MLKEM_PK + HQC_PK, *ek_x = eseed + 32 + HQC_M + HQC_SALT;
    uint8_t *ct_x = ct + MLKEM_CT + HQC_CT;
    int ret = -1;
    OQS_init();
    if (OQS_KEM_ml_kem_768_encaps_derand(ct, ss_m, pk, eseed) != OQS_SUCCESS) goto out;
    rb_set(eseed + 32, HQC_M + HQC_SALT);  /* HQC draws m, then salt (hqc_kem_encaps) */
    if (hqc_kem_encaps(ss_h, ct + MLKEM_CT, pk + MLKEM_PK) != 0 || !rb_drained()) goto out;
    if (x25519_public(ct_x, ek_x) || x25519(ss_x, ek_x, pk_x)) goto out;
    if (combine(ss, ss_m, ss_h, ss_x, ct_x, pk_x)) goto out;
#ifdef PHX_TRI_DEBUG
    memcpy(dbg[0], ss_m, 32); memcpy(dbg[1], ss_h, 32); memcpy(dbg[2], ss_x, 32);
    memcpy(dbg[3], ct_x, 32); memcpy(dbg[4], pk_x, 32);
#endif
    ret = 0;
out:
    rb_drained();
    OPENSSL_cleanse(ss_m, 32); OPENSSL_cleanse(ss_h, 32); OPENSSL_cleanse(ss_x, 32);
    if (ret) { memset(ss, 0, PHX_TRI_SHAREDSECRETBYTES); memset(ct, 0, PHX_TRI_CIPHERTEXTBYTES); }
    return ret;
}

int phx_tri_encaps(uint8_t *ss, uint8_t *ct, const uint8_t *pk) {
    uint8_t eseed[PHX_TRI_ENCAPS_SEEDBYTES];
    int ret = RAND_priv_bytes(eseed, sizeof eseed) == 1 ? phx_tri_encaps_derand(ss, ct, pk, eseed) : -1;
    OPENSSL_cleanse(eseed, sizeof eseed);
    return ret;
}

int phx_tri_decaps(uint8_t *ss, const uint8_t *ct, const uint8_t *sk) {
    uint8_t ex[128], pk_m[MLKEM_PK], sk_m[MLKEM_SK], ss_m[32], ss_h[32], ss_x[32], pk_x[32];
    const uint8_t *ct_x = ct + MLKEM_CT + HQC_CT;
    int ret = -1;
    OQS_init();
    if (expand(ex, sk)) goto out;
    if (OQS_KEM_ml_kem_768_keypair_derand(pk_m, sk_m, ex) != OQS_SUCCESS) goto out;
    if (OQS_KEM_ml_kem_768_decaps(ss_m, ct, sk_m) != OQS_SUCCESS) goto out;
    if (hqc_kem_decaps(ss_h, ex + 64, ct + MLKEM_CT) != 0) goto out;  /* HQC's dk_kem is its 32-byte seed */
    if (x25519_public(pk_x, ex + 96) || x25519(ss_x, ex + 96, ct_x)) goto out;
    if (combine(ss, ss_m, ss_h, ss_x, ct_x, pk_x)) goto out;
    ret = 0;
out:
    OPENSSL_cleanse(ex, sizeof ex); OPENSSL_cleanse(sk_m, sizeof sk_m);
    OPENSSL_cleanse(ss_m, 32); OPENSSL_cleanse(ss_h, 32); OPENSSL_cleanse(ss_x, 32);
    if (ret) memset(ss, 0, PHX_TRI_SHAREDSECRETBYTES);
    return ret;
}
