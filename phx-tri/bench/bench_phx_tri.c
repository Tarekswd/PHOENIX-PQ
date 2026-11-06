/*
 * PHX-Tri benchmark: median cycles (rdtsc) for keygen/encaps/decaps, and the cost of the combiner hash
 * as specified (166 bytes) against a generic combiner that also hashes both post-quantum ciphertexts.
 * usage: bench_phx_tri [iterations, default 201]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <x86intrin.h>
#include <openssl/evp.h>
#include "phx_tri.h"

static uint8_t pk[PHX_TRI_PUBLICKEYBYTES], ct[PHX_TRI_CIPHERTEXTBYTES], buf[166 + 1088 + 8978];

static int cmp(const void *a, const void *b) {
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

static uint64_t median(uint64_t *t, int n) { qsort(t, n, sizeof *t, cmp); return t[n / 2]; }

int main(int argc, char **argv) {
    int n = argc > 1 ? atoi(argv[1]) : 201;
    uint64_t *t = malloc(n * sizeof *t);
    uint8_t sk[32], ss[32], ss2[32], h[32];
    unsigned int hl, aux;

#define TIME(label, expr)                                              \
    for (int i = 0; i < n; i++) {                                      \
        uint64_t t0 = __rdtscp(&aux);                                  \
        if (expr) { fprintf(stderr, "%s failed\n", label); return 1; } \
        t[i] = __rdtscp(&aux) - t0;                                    \
    }                                                                  \
    printf("%-34s %12lu cycles (median of %d)\n", label, median(t, n), n);

    TIME("keygen", phx_tri_keygen(pk, sk));
    TIME("encaps", phx_tri_encaps(ss, ct, pk));
    TIME("decaps", phx_tri_decaps(ss2, ct, sk));
    if (memcmp(ss, ss2, 32)) { fprintf(stderr, "round trip failed\n"); return 1; }
    TIME("combiner as specified (166 B)", !EVP_Digest(buf, 166, h, &hl, EVP_sha3_256(), NULL));
    TIME("generic combiner (166 + 10066 B)", !EVP_Digest(buf, sizeof buf, h, &hl, EVP_sha3_256(), NULL));
    printf("sizes: pk %d B, ct %d B, sk %d B, ss %d B\n", PHX_TRI_PUBLICKEYBYTES, PHX_TRI_CIPHERTEXTBYTES,
           PHX_TRI_SECRETKEYBYTES, PHX_TRI_SHAREDSECRETBYTES);
    free(t);
    return 0;
}
