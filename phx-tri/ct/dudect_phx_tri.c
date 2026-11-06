/*
 * dudect-style fixed-vs-random timing tests of PHX-Tri decapsulation (Reparaz et al., DATE 2017).
 * Inputs for each batch are prepared before any timing, in separate buffers for both classes.
 *
 *   ct      fixed key; class 0 = one fixed valid ciphertext, class 1 = fresh valid ciphertexts
 *           (does decapsulation time depend on the ciphertext / decrypted message?)
 *   reject  fixed key; class 0 = the fixed valid ciphertext, class 1 = it with one bit flipped in ct_M
 *           and one in ct_H (does timing reveal implicit rejection?)
 *   key     class 0 = fixed key and ciphertext, class 1 = fresh key pairs with their ciphertexts
 *           (expected to differ: HQC's rejection sampler re-derives x, y from the key seed)
 *
 * |t| > 4.5 is dudect's usual threshold. Below it means no dependence was detected under these
 * conditions only.  usage: dudect_phx_tri <ct|reject|key> <measurements>
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <x86intrin.h>
#include <openssl/rand.h>
#include "phx_tri.h"

enum { B = 256 };
#define NCROP 5
static const double crop[NCROP] = {1.0, 0.9, 0.75, 0.5, 0.25};
static uint8_t cts[B][PHX_TRI_CIPHERTEXTBYTES], sks[B][32], pk[PHX_TRI_PUBLICKEYBYTES], pk_i[PHX_TRI_PUBLICKEYBYTES];

static uint32_t rnd32(void) { uint32_t x; if (RAND_bytes((uint8_t *)&x, 4) != 1) exit(1); return x; }
static int cmp(const void *a, const void *b) { uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b; return (x > y) - (x < y); }

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: dudect_phx_tri <ct|reject|key> <n>\n"); return 2; }
    const char *test = argv[1];
    long n = atol(argv[2]);
    uint64_t *t = malloc(n * sizeof *t), *sorted = malloc(n * sizeof *t);
    uint8_t *cls = malloc(n), sk0[32], ct0[PHX_TRI_CIPHERTEXTBYTES], ss[32];
    if (phx_tri_keygen(pk, sk0) || phx_tri_encaps(ss, ct0, pk)) return 1;

    for (long b = 0; b < n; b += B) {
        long nb = n - b < B ? n - b : B;
        for (long j = 0; j < nb; j++) {
            cls[b + j] = rnd32() & 1;
            memcpy(sks[j], sk0, 32);
            memcpy(cts[j], ct0, sizeof ct0);
            if (!cls[b + j]) continue;
            if (!strcmp(test, "ct")) {
                if (phx_tri_encaps(ss, cts[j], pk)) return 1;
            } else if (!strcmp(test, "reject")) {
                cts[j][rnd32() % 1088] ^= (uint8_t)(1u << (rnd32() % 8));
                cts[j][1088 + rnd32() % 8978] ^= (uint8_t)(1u << (rnd32() % 8));
            } else if (!strcmp(test, "key")) {
                if (phx_tri_keygen(pk_i, sks[j]) || phx_tri_encaps(ss, cts[j], pk_i)) return 1;
            } else { fprintf(stderr, "unknown test\n"); return 2; }
        }
        for (long j = 0; j < nb; j++) {
            unsigned aux;
            uint64_t t0 = __rdtscp(&aux);
            if (phx_tri_decaps(ss, cts[j], sks[j])) return 1;
            t[b + j] = __rdtscp(&aux) - t0;
        }
    }
    memcpy(sorted, t, n * sizeof *t);
    qsort(sorted, n, sizeof *t, cmp);
    printf("test=%s N=%ld median_cycles=%lu\n", test, n, sorted[n / 2]);
    for (int c = 0; c < NCROP; c++) {
        uint64_t thr = sorted[(long)(crop[c] * (n - 1))];
        double m[2] = {0}, s[2] = {0}, k[2] = {0};
        for (long i = 0; i < n; i++) {
            if (t[i] > thr) continue;
            int g = cls[i];
            k[g]++;
            double d = t[i] - m[g];
            m[g] += d / k[g];
            s[g] += d * (t[i] - m[g]);
        }
        double tt = (m[0] - m[1]) / sqrt(s[0] / (k[0] - 1) / k[0] + s[1] / (k[1] - 1) / k[1]);
        printf("  crop<=p%-3.0f n0=%-6.0f n1=%-6.0f mean0=%-11.0f mean1=%-11.0f t=%+7.2f%s\n",
               100 * crop[c], k[0], k[1], m[0], m[1], tt, fabs(tt) > 4.5 ? "  DEPENDENCE" : "");
    }
    return 0;
}
