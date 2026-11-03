/*
 * dudect-style fixed-vs-random timing test of HQC Reed-Muller decoding (Reparaz et al., DATE 2017).
 *
 * Class 0: one fixed received word (a codeword plus noise of the typical block weight).
 * Class 1: a fresh received word (random codeword plus fresh noise of the same weight) per call.
 * Calls are interleaved at random; cycles are measured with rdtscp; Welch's t-test is computed on
 * all samples and on samples below several percentiles (dudect's cropping).
 * |t| > 4.5 is dudect's usual "leak detected" threshold; |t| < 4.5 means no leak was detected,
 * under these conditions only.
 *
 * usage: dudect_rm <decoder: ref|avx> <measurements> <noise_weight_per_block>
 * Links against renamed decoders ref_rm_* / avx_rm_* built by experiments/hqc-dfr/build.sh.
 */
#define _DEFAULT_SOURCE
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <x86intrin.h>
#include "parameters.h"

void ref_rm_encode(uint64_t *cdw, const uint64_t *msg);
void ref_rm_decode(uint64_t *msg, const uint64_t *cdw);
void avx_rm_decode(uint64_t *msg, const uint64_t *cdw);

#define NW VEC_N1N2_SIZE_64
#define NCROP 5
static const double crop[NCROP] = {1.0, 0.9, 0.75, 0.5, 0.25};

static uint64_t rs[4];  // xoshiro256**, seeded once from getrandom (one syscall per value was far too slow)
static inline uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }
static uint64_t rnd(void) {
    uint64_t r = rotl(rs[1] * 5, 7) * 9, t = rs[1] << 17;
    rs[2] ^= rs[0]; rs[3] ^= rs[1]; rs[1] ^= rs[2]; rs[0] ^= rs[3]; rs[2] ^= t; rs[3] = rotl(rs[3], 45);
    return r;
}

static void make_word(uint64_t *cdw, int w) {
    uint64_t msg[VEC_N1_SIZE_64];
    static int pos[PARAM_N2];
    for (int i = 0; i < VEC_N1_SIZE_64; i++) msg[i] = rnd();
    if (PARAM_N1 % 8) msg[VEC_N1_SIZE_64 - 1] &= (1ULL << (8 * (PARAM_N1 % 8))) - 1;
    memset(cdw, 0, 8 * NW);
    ref_rm_encode(cdw, msg);
    for (int b = 0; b < PARAM_N1; b++) {
        for (int p = 0; p < PARAM_N2; p++) pos[p] = b * PARAM_N2 + p;
        for (int j = 0; j < w; j++) {
            int r = j + (int)(rnd() % (uint64_t)(PARAM_N2 - j));
            int t = pos[j]; pos[j] = pos[r]; pos[r] = t;
            cdw[pos[j] >> 6] ^= 1ULL << (pos[j] & 63);
        }
    }
}

static int cmp_u64(const void *a, const void *b) {
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: dudect_rm ref|avx N w\n"); return 2; }
    if (getrandom(rs, sizeof rs, 0) != (ssize_t)sizeof rs) return 1;
    void (*dec)(uint64_t *, const uint64_t *) = strcmp(argv[1], "avx") ? ref_rm_decode : avx_rm_decode;
    long n = atol(argv[2]);
    int w = atoi(argv[3]);
    uint64_t *t = malloc(n * sizeof *t), *sorted = malloc(n * sizeof *t);
    uint8_t *cls = malloc(n);
    static uint64_t fixed[NW], fresh[NW], out[VEC_N1_SIZE_64];
    make_word(fixed, w);
    for (long i = 0; i < n; i++) {
        cls[i] = rnd() & 1;
        if (cls[i]) make_word(fresh, w);
        const uint64_t *in = cls[i] ? fresh : fixed;
        unsigned aux;
        uint64_t t0 = __rdtscp(&aux);
        dec(out, in);
        uint64_t t1 = __rdtscp(&aux);
        t[i] = t1 - t0;
    }
    memcpy(sorted, t, n * sizeof *t);
    qsort(sorted, n, sizeof *t, cmp_u64);
    printf("decoder=%s N=%ld w=%d median_cycles=%lu\n", argv[1], n, w, sorted[n / 2]);
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
        printf("  crop<=p%.0f  n0=%.0f n1=%.0f  mean0=%.1f mean1=%.1f  t=%+.2f%s\n",
               100 * crop[c], k[0], k[1], m[0], m[1], tt, fabs(tt) > 4.5 ? "  LEAK?" : "");
    }
    return 0;
}
