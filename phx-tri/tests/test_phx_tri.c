/*
 * PHX-Tri self-tests and test vectors.
 *   test_phx_tri                 run all self-tests (exit 0 on success)
 *   test_phx_tri vectors <n>     print n deterministic test vectors (with combiner inputs, for check_vectors.py)
 * Built with -DPHX_TRI_DEBUG so the combiner inputs of each encapsulation can be exported.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>
#include <oqs/oqs.h>
#include "phx_tri.h"

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)

static uint8_t pk[PHX_TRI_PUBLICKEYBYTES], ct[PHX_TRI_CIPHERTEXTBYTES], ct2[PHX_TRI_CIPHERTEXTBYTES];

static void sha3(uint8_t out[32], const uint8_t *in, size_t len) {
    unsigned int n;
    EVP_Digest(in, len, out, &n, EVP_sha3_256(), NULL);
}

static void hex(const char *label, const uint8_t *b, size_t n) {
    printf("%s = ", label);
    for (size_t i = 0; i < n; i++) printf("%02x", b[i]);
    printf("\n");
}

static void fixed_seeds(int i, uint8_t seed[32], uint8_t eseed[PHX_TRI_ENCAPS_SEEDBYTES]) {
    for (int j = 0; j < 32; j++) seed[j] = (uint8_t)(i * 32 + j);
    for (int j = 0; j < PHX_TRI_ENCAPS_SEEDBYTES; j++) eseed[j] = (uint8_t)((i * 104 + j) ^ 0x5a);
}

static int vectors(int n) {
    uint8_t seed[32], eseed[PHX_TRI_ENCAPS_SEEDBYTES], sk[32], ss[32], ss2[32], h[32], c[5][32];
    for (int i = 0; i < n; i++) {
        fixed_seeds(i, seed, eseed);
        CHECK(phx_tri_keygen_derand(pk, sk, seed) == 0);
        CHECK(phx_tri_encaps_derand(ss, ct, pk, eseed) == 0);
        CHECK(phx_tri_decaps(ss2, ct, sk) == 0 && memcmp(ss, ss2, 32) == 0);
        phx_tri_debug_last(c[0], c[1], c[2], c[3], c[4]);
        printf("count = %d\n", i);
        hex("seed", seed, 32);
        hex("eseed", eseed, sizeof eseed);
        sha3(h, pk, sizeof pk); hex("sha3_256_pk", h, 32);
        sha3(h, ct, sizeof ct); hex("sha3_256_ct", h, 32);
        hex("ss", ss, 32);
        hex("ss_M", c[0], 32); hex("ss_H", c[1], 32); hex("ss_X", c[2], 32);
        hex("ct_X", c[3], 32); hex("pk_X", c[4], 32);
        printf("\n");
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc > 2 && !strcmp(argv[1], "vectors")) return vectors(atoi(argv[2]));

    uint8_t seed[32], eseed[PHX_TRI_ENCAPS_SEEDBYTES], sk[32], ss[32], ss2[32], pk_m_other[1184], sk_m_other[2400];

    /* 1. random round trips ("quick": 20, for emulated targets) */
    int trips = argc > 1 && !strcmp(argv[1], "quick") ? 20 : 1000;
    for (int i = 0; i < trips; i++) {
        CHECK(phx_tri_keygen(pk, sk) == 0);
        CHECK(phx_tri_encaps(ss, ct, pk) == 0);
        CHECK(phx_tri_decaps(ss2, ct, sk) == 0);
        CHECK(memcmp(ss, ss2, 32) == 0);
    }
    printf("ok  %d random keygen/encaps/decaps round trips\n", trips);

    /* 2. determinism of the derand API */
    fixed_seeds(7, seed, eseed);
    CHECK(phx_tri_keygen_derand(pk, sk, seed) == 0);
    CHECK(phx_tri_encaps_derand(ss, ct, pk, eseed) == 0);
    CHECK(phx_tri_encaps_derand(ss2, ct2, pk, eseed) == 0);
    CHECK(memcmp(ss, ss2, 32) == 0 && memcmp(ct, ct2, sizeof ct) == 0);
    printf("ok  derand encapsulation is deterministic\n");

    /* 3. tampering with each component changes the key (implicit rejection / binding of ct_X) */
    const size_t offs[3] = {0, 1088, 1088 + 8978};
    const char *names[3] = {"ct_M", "ct_H", "ct_X"};
    for (int k = 0; k < 3; k++) {
        memcpy(ct2, ct, sizeof ct);
        ct2[offs[k] + 5] ^= 0x01;
        CHECK(phx_tri_decaps(ss2, ct2, sk) == 0);
        CHECK(memcmp(ss, ss2, 32) != 0);
        printf("ok  flipped bit in %s gives a different key\n", names[k]);
    }

    /* 4. low-order X25519 point (u = 0): decapsulation still succeeds, deterministically */
    memcpy(ct2, ct, sizeof ct);
    memset(ct2 + offs[2], 0, 32);
    uint8_t ss3[32];
    CHECK(phx_tri_decaps(ss2, ct2, sk) == 0 && phx_tri_decaps(ss3, ct2, sk) == 0);
    CHECK(memcmp(ss2, ss3, 32) == 0 && memcmp(ss2, ss, 32) != 0);
    printf("ok  low-order ct_X handled deterministically\n");

    /* 5. domain separation from X-Wing's seed expansion: SHAKE256(seed) must not give our ML-KEM key */
    uint8_t xwing_ex[96];
    unsigned char *p = xwing_ex;
    EVP_MD_CTX *c = EVP_MD_CTX_new();
    CHECK(c && EVP_DigestInit_ex(c, EVP_shake256(), NULL) && EVP_DigestUpdate(c, seed, 32) && EVP_DigestFinalXOF(c, p, 96));
    EVP_MD_CTX_free(c);
    CHECK(OQS_KEM_ml_kem_768_keypair_derand(pk_m_other, sk_m_other, xwing_ex) == OQS_SUCCESS);
    CHECK(memcmp(pk_m_other, pk, 1184) != 0);
    printf("ok  seed expansion is domain-separated from X-Wing's\n");

    /* 6. failure leaves outputs zeroed: decapsulation of garbage still returns a key (implicit rejection) */
    for (size_t i = 0; i < sizeof ct2; i++) ct2[i] = (uint8_t)(i * 31 + 7);
    CHECK(phx_tri_decaps(ss2, ct2, sk) == 0 && memcmp(ss2, ss, 32) != 0);
    printf("ok  garbage ciphertext yields an implicit-rejection key\n");

    printf("ALL TESTS PASSED\n");
    return 0;
}
