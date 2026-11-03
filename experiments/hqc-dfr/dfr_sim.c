/*
 * Monte Carlo comparison of HQC Reed-Muller decoders under real HQC decryption noise.
 *
 * Received word seen by the decoder (src/ref/hqc.c, v5.1.0):
 *   encode(m) + Truncate(x*r2 + r1*y + e)   over F2[X]/(X^n - 1)
 * with |x| = |y| = OMEGA (per key), |r1| = |r2| = OMEGA_R, |e| = OMEGA_E (per encryption).
 * Supports are sampled uniformly (the spec's model); HQC's fast sampler for r1, r2, e is
 * only approximately uniform, which this simulation does not reproduce.
 *
 * Each block (one RM(1,7) byte, PARAM_N2 bits) is decoded by:
 *   ref  - reference decoder (exact maximum likelihood over the 256 codewords)
 *   avx  - official AVX2 decoder (peak search width 1 << (5 + MULTIPLICITY/2))
 *   fix  - AVX2 decoder with width 1 << (6 + MULTIPLICITY/2) (patch from HQC issue #46)
 *
 * usage: dfr_sim <seconds> [trials_per_key]
 * output: key=value counters and per-weight histograms, summed by aggregate.py
 */
#define _DEFAULT_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <time.h>
#include "parameters.h"

void ref_rm_encode(uint64_t *cdw, const uint64_t *msg);
void ref_rm_decode(uint64_t *msg, const uint64_t *cdw);
void avx_rm_decode(uint64_t *msg, const uint64_t *cdw);
void fix_rm_decode(uint64_t *msg, const uint64_t *cdw);

static uint64_t rs[4];
static inline uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }
static inline uint64_t next64(void) {  // xoshiro256**
    uint64_t r = rotl(rs[1] * 5, 7) * 9, t = rs[1] << 17;
    rs[2] ^= rs[0]; rs[3] ^= rs[1]; rs[1] ^= rs[2]; rs[0] ^= rs[3]; rs[2] ^= t; rs[3] = rotl(rs[3], 45);
    return r;
}
static inline uint32_t below(uint32_t n) {  // unbiased integer in [0, n) (Lemire)
    uint64_t m = (uint64_t)(uint32_t)next64() * n;
    uint32_t l = (uint32_t)m;
    if (l < n) {
        uint32_t t = (uint32_t)(-n) % n;
        while (l < t) { m = (uint64_t)(uint32_t)next64() * n; l = (uint32_t)m; }
    }
    return (uint32_t)(m >> 32);
}

static uint8_t mark[PARAM_N];
static void sample_support(uint32_t *sup, int w) {
    for (int k = 0; k < w;) {
        uint32_t c = below(PARAM_N);
        if (!mark[c]) { mark[c] = 1; sup[k++] = c; }
    }
    for (int k = 0; k < w; k++) mark[sup[k]] = 0;
}

#ifdef HQC_SAMPLER
/* Use HQC's own fast sampler (vect_sample_fixed_weight2, as in hqc_pke_encrypt) for r2, e, r1. */
#include "symmetric.h"
#include "vector.h"
static void dense_to_support(const uint64_t *v, uint32_t *sup, int w) {
    int k = 0;
    for (uint32_t p = 0; p < PARAM_N && k < w; p++) if ((v[p >> 6] >> (p & 63)) & 1) sup[k++] = p;
    if (k != w) { fprintf(stderr, "sampler weight %d != %d\n", k, w); exit(1); }
}
static void sample_encryption_randomness(uint32_t *r1, uint32_t *r2, uint32_t *e) {
    static uint64_t vr1[VEC_N_SIZE_64], vr2[VEC_N_SIZE_64], ve[VEC_N_SIZE_64];
    uint8_t theta[SEED_BYTES];
    shake256_xof_ctx ctx;
    if (getrandom(theta, sizeof theta, 0) != (ssize_t)sizeof theta) exit(1);
    xof_init(&ctx, theta, SEED_BYTES);
    memset(vr1, 0, sizeof vr1); memset(vr2, 0, sizeof vr2); memset(ve, 0, sizeof ve);
    vect_sample_fixed_weight2(&ctx, vr2, PARAM_OMEGA_R);  // same order as hqc_pke_encrypt
    vect_sample_fixed_weight2(&ctx, ve, PARAM_OMEGA_E);
    vect_sample_fixed_weight2(&ctx, vr1, PARAM_OMEGA_R);
    dense_to_support(vr2, r2, PARAM_OMEGA_R);
    dense_to_support(ve, e, PARAM_OMEGA_E);
    dense_to_support(vr1, r1, PARAM_OMEGA_R);
}
#endif

#define NW VEC_N1N2_SIZE_64
static inline void flip(uint64_t *v, uint32_t p) { if (p < PARAM_N1N2) v[p >> 6] ^= 1ULL << (p & 63); }

static void add_product(uint64_t *v, const uint32_t *a, int wa, const uint32_t *b, int wb) {
    for (int i = 0; i < wa; i++)
        for (int j = 0; j < wb; j++) {
            uint32_t p = a[i] + b[j];
            flip(v, p >= PARAM_N ? p - PARAM_N : p);
        }
}

static int block_weight(const uint64_t *v, int blk) {  // noise bits in block blk
    int w = 0;
    for (int b = blk * PARAM_N2; b < (blk + 1) * PARAM_N2; b++) w += (v[b >> 6] >> (b & 63)) & 1;
    return w;
}

static void decode3(const uint64_t *cdw, uint64_t *mr, uint64_t *ma, uint64_t *mf) {
    memset(mr, 0, 8 * VEC_N1_SIZE_64); memset(ma, 0, 8 * VEC_N1_SIZE_64); memset(mf, 0, 8 * VEC_N1_SIZE_64);
    ref_rm_decode(mr, cdw);
    avx_rm_decode(ma, cdw);
    fix_rm_decode(mf, cdw);
}

static void random_msg(uint64_t *msg) {
    for (int i = 0; i < VEC_N1_SIZE_64; i++) msg[i] = next64();
    if (PARAM_N1 % 8) msg[VEC_N1_SIZE_64 - 1] &= (1ULL << (8 * (PARAM_N1 % 8))) - 1;
}

static inline int getbit(const uint64_t *v, int p) { return (v[p >> 6] >> (p & 63)) & 1; }

/*
 * Positive control (pattern from HQC issue #46): in every block, move the received word to
 * distance HALF/2 - 1 from the sent codeword a and HALF/2 + 1 from a second codeword b, where
 * HALF = N2/2 = d(a, b). The ML answer is a; both peaks exceed the AVX2 search cap, so the
 * official AVX2 decoder must return b whenever b has the lower index.
 */
static int selftest(int iters) {
    uint64_t msg[VEC_N1_SIZE_64], alt[VEC_N1_SIZE_64], mr[VEC_N1_SIZE_64], ma[VEC_N1_SIZE_64], mf[VEC_N1_SIZE_64];
    static uint64_t ca[NW], cb[NW];
    static int diff[PARAM_N2];
    uint64_t blocks = 0, ref_ok = 0, avx_ok = 0, fix_ok = 0, avx_gave_b = 0;
    for (int it = 0; it < iters; it++) {
        random_msg(msg);
        memcpy(alt, msg, sizeof alt);
        uint8_t *ba = (uint8_t *)alt, *bm = (uint8_t *)msg;
        for (int i = 0; i < PARAM_N1; i++) do ba[i] = (uint8_t)next64(); while (ba[i] == bm[i]);
        memset(ca, 0, sizeof ca); memset(cb, 0, sizeof cb);
        ref_rm_encode(ca, msg);
        ref_rm_encode(cb, alt);
        int skip[PARAM_N1] = {0};
        for (int i = 0; i < PARAM_N1; i++) {
            int nd = 0;
            for (int p = i * PARAM_N2; p < (i + 1) * PARAM_N2; p++) if (getbit(ca, p) != getbit(cb, p)) diff[nd++] = p;
            if (nd != PARAM_N2 / 2) { skip[i] = 1; continue; }  // b is the complement of a: not a competitor
            int k = nd / 2 - 1;
            for (int j = 0; j < k; j++) {  // partial Fisher-Yates: k distinct differing positions
                int r = j + (int)below((uint32_t)(nd - j));
                int t = diff[j]; diff[j] = diff[r]; diff[r] = t;
                ca[diff[j] >> 6] ^= 1ULL << (diff[j] & 63);
            }
        }
        decode3(ca, mr, ma, mf);
        const uint8_t *br = (uint8_t *)mr, *bav = (uint8_t *)ma, *bf = (uint8_t *)mf;
        for (int i = 0; i < PARAM_N1; i++) {
            if (skip[i]) continue;
            blocks++;
            ref_ok += br[i] == bm[i]; avx_ok += bav[i] == bm[i]; fix_ok += bf[i] == bm[i]; avx_gave_b += bav[i] == ba[i];
        }
    }
    printf("selftest blocks %lu ref_ok %lu avx_ok %lu fix_ok %lu avx_returned_far_codeword %lu\n",
           blocks, ref_ok, avx_ok, fix_ok, avx_gave_b);
    return !(ref_ok == blocks && fix_ok == blocks && avx_gave_b > 0);
}

/* Conditional run: every block gets exactly w noise bits, placed uniformly in that block. */
static void cond(int w, double seconds) {
    uint64_t msg[VEC_N1_SIZE_64], mr[VEC_N1_SIZE_64], ma[VEC_N1_SIZE_64], mf[VEC_N1_SIZE_64];
    static uint64_t cdw[NW];
    static int pos[PARAM_N2];
    uint64_t blocks = 0, ref_err = 0, bug_only = 0, avx_only_right = 0, fix_ne_ref = 0;
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (uint64_t it = 0;; it++) {
        if ((it & 255) == 0) {
            clock_gettime(CLOCK_MONOTONIC, &t1);
            if ((t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9 > seconds) break;
        }
        random_msg(msg);
        memset(cdw, 0, sizeof cdw);
        ref_rm_encode(cdw, msg);
        for (int i = 0; i < PARAM_N1; i++) {
            for (int p = 0; p < PARAM_N2; p++) pos[p] = i * PARAM_N2 + p;
            for (int j = 0; j < w; j++) {
                int r = j + (int)below((uint32_t)(PARAM_N2 - j));
                int t = pos[j]; pos[j] = pos[r]; pos[r] = t;
                cdw[pos[j] >> 6] ^= 1ULL << (pos[j] & 63);
            }
        }
        decode3(cdw, mr, ma, mf);
        const uint8_t *bm = (uint8_t *)msg, *br = (uint8_t *)mr, *ba = (uint8_t *)ma, *bf = (uint8_t *)mf;
        for (int i = 0; i < PARAM_N1; i++) {
            int r = br[i] != bm[i], a = ba[i] != bm[i];
            ref_err += r; bug_only += a && !r; avx_only_right += r && !a; fix_ne_ref += bf[i] != br[i];
        }
        blocks += PARAM_N1;
    }
    printf("cond w %d blocks %lu ref_err %lu bug_only %lu avx_only_right %lu fix_ne_ref %lu\n",
           w, blocks, ref_err, bug_only, avx_only_right, fix_ne_ref);
}

static int natural(double seconds, int per_key);

int main(int argc, char **argv) {
    if (getrandom(rs, sizeof rs, 0) != (ssize_t)sizeof rs) return 1;
    if (argc > 1 && !strcmp(argv[1], "selftest")) return selftest(argc > 2 ? atoi(argv[2]) : 200);
    if (argc > 2 && !strcmp(argv[1], "cond")) { cond(atoi(argv[2]), argc > 3 ? atof(argv[3]) : 10); return 0; }
    return natural(argc > 1 ? atof(argv[1]) : 10, argc > 2 ? atoi(argv[2]) : 16);
}

static int natural(double seconds, int per_key) {
    static uint32_t x[PARAM_OMEGA], y[PARAM_OMEGA], r1[PARAM_OMEGA_R], r2[PARAM_OMEGA_R], e[PARAM_OMEGA_E];
    static uint64_t noise[NW], cdw[NW];
    uint64_t msg[VEC_N1_SIZE_64], mr[VEC_N1_SIZE_64], ma[VEC_N1_SIZE_64], mf[VEC_N1_SIZE_64];
    static uint64_t hw_all[PARAM_N2 + 1], hw_ref[PARAM_N2 + 1], hw_bug[PARAM_N2 + 1], hw_avxonly[PARAM_N2 + 1];
    static uint64_t hist_ref[PARAM_N1 + 1], hist_avx[PARAM_N1 + 1], hist_fix[PARAM_N1 + 1];
    uint64_t trials = 0, blocks = 0, ref_err = 0, avx_err = 0, fix_err = 0, bug_only = 0, avx_only_right = 0;
    uint64_t avx_ne_ref = 0, fix_ne_ref = 0;

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (;;) {
        if (trials % per_key == 0) {
            clock_gettime(CLOCK_MONOTONIC, &t1);
            if ((t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9 > seconds) break;
            sample_support(x, PARAM_OMEGA);
            sample_support(y, PARAM_OMEGA);
        }
#ifdef HQC_SAMPLER
        sample_encryption_randomness(r1, r2, e);
#else
        sample_support(r1, PARAM_OMEGA_R);
        sample_support(r2, PARAM_OMEGA_R);
        sample_support(e, PARAM_OMEGA_E);
#endif
        memset(noise, 0, sizeof noise);
        add_product(noise, x, PARAM_OMEGA, r2, PARAM_OMEGA_R);
        add_product(noise, y, PARAM_OMEGA, r1, PARAM_OMEGA_R);
        for (int i = 0; i < PARAM_OMEGA_E; i++) flip(noise, e[i]);

        memset(msg, 0, sizeof msg);
        for (int i = 0; i < VEC_N1_SIZE_64; i++) msg[i] = next64();
        if (PARAM_N1 % 8) msg[VEC_N1_SIZE_64 - 1] &= (1ULL << (8 * (PARAM_N1 % 8))) - 1;
        memset(cdw, 0, sizeof cdw);
        ref_rm_encode(cdw, msg);
        for (int i = 0; i < NW; i++) cdw[i] ^= noise[i];

        memset(mr, 0, sizeof mr); memset(ma, 0, sizeof ma); memset(mf, 0, sizeof mf);
        ref_rm_decode(mr, cdw);
        avx_rm_decode(ma, cdw);
        fix_rm_decode(mf, cdw);

        const uint8_t *bm = (const uint8_t *)msg, *br = (const uint8_t *)mr, *ba = (const uint8_t *)ma, *bf = (const uint8_t *)mf;
        int nr = 0, na = 0, nf = 0;
        for (int i = 0; i < PARAM_N1; i++) {
            int r = br[i] != bm[i], a = ba[i] != bm[i], f = bf[i] != bm[i];
            int w = block_weight(noise, i);
            hw_all[w]++;
            nr += r; na += a; nf += f;
            if (r) hw_ref[w]++;
            if (a && !r) { bug_only++; hw_bug[w]++; }
            if (r && !a) { avx_only_right++; hw_avxonly[w]++; }
            avx_ne_ref += ba[i] != br[i];
            fix_ne_ref += bf[i] != br[i];
        }
        blocks += PARAM_N1; ref_err += nr; avx_err += na; fix_err += nf;
        hist_ref[nr]++; hist_avx[na]++; hist_fix[nf]++;
        trials++;
    }

    printf("param N1=%d N2=%d OMEGA=%d OMEGA_R=%d OMEGA_E=%d\n", PARAM_N1, PARAM_N2, PARAM_OMEGA, PARAM_OMEGA_R, PARAM_OMEGA_E);
    printf("trials %lu\nblocks %lu\nref_err %lu\navx_err %lu\nfix_err %lu\nbug_only %lu\navx_only_right %lu\navx_ne_ref %lu\nfix_ne_ref %lu\n",
           trials, blocks, ref_err, avx_err, fix_err, bug_only, avx_only_right, avx_ne_ref, fix_ne_ref);
    for (int w = 0; w <= PARAM_N2; w++)
        if (hw_all[w]) printf("hw %d %lu %lu %lu %lu\n", w, hw_all[w], hw_ref[w], hw_bug[w], hw_avxonly[w]);
    for (int k = 0; k <= PARAM_N1; k++)
        if (hist_ref[k] || hist_avx[k] || hist_fix[k]) printf("hist %d %lu %lu %lu\n", k, hist_ref[k], hist_avx[k], hist_fix[k]);
    return 0;
}
