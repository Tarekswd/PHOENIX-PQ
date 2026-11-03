/*
 * Crafted-ciphertext test: do HQC-KEM decapsulation outputs depend on which RM decoder is used?
 *
 * Built twice from one file:
 *   -DCRAFT  writes a key and ciphertexts to a file. Using the secret y it sets
 *            v = Truncate(u*y) + R, so the decoder input is exactly R, a word on which the
 *            reference and official AVX2 RM decoders are checked to disagree.
 *   (none)   reads the file and prints the decapsulated shared secret of every ciphertext.
 *            Linked once with the reference RM decoder and once with the AVX2 one.
 *
 * Ciphertext types: 0 honest (from encaps; expected K stored), 1 crafted decoder-divergent,
 * 2 uniformly random u and v.
 */
#define _DEFAULT_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include "hqc-kem.h"
#include "parameters.h"

int randombytes(uint8_t *out, size_t outlen) {
    size_t done = 0;
    while (done < outlen) {
        ssize_t r = getrandom(out + done, outlen - done, 0);
        if (r <= 0) return -1;
        done += (size_t)r;
    }
    return 0;
}

typedef struct {
    uint8_t type, diverging_blocks;
    uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
    uint8_t expected_k[CRYPTO_BYTES];
} record_t;

#ifdef CRAFT
#include "gf2x.h"
#include "hqc.h"
#include "parsing.h"
#include "symmetric.h"
#include "vector.h"
void ref_rm_encode(uint64_t *cdw, const uint64_t *msg);
void ref_rm_decode(uint64_t *msg, const uint64_t *cdw);
void avx_rm_decode(uint64_t *msg, const uint64_t *cdw);

static uint64_t rnd64(void) { uint64_t x; randombytes((uint8_t *)&x, sizeof x); return x; }
static uint32_t below(uint32_t n) { return (uint32_t)(((uint64_t)(uint32_t)rnd64() * n) >> 32); }  // bias < 2^-17, test only
static int getbit(const uint64_t *v, int p) { return (v[p >> 6] >> (p & 63)) & 1; }

/* R: per block, HALF/2 - 1 flips from codeword(a) toward a non-complementary codeword(b). Returns #divergent blocks. */
static int make_divergent_word(uint64_t *R) {
    uint64_t a[VEC_N1_SIZE_64] = {0}, b[VEC_N1_SIZE_64] = {0}, cb[VEC_N1N2_SIZE_64] = {0};
    uint64_t mr[VEC_N1_SIZE_64] = {0}, ma[VEC_N1_SIZE_64] = {0};
    static int diff[PARAM_N2];
    for (int i = 0; i < PARAM_N1; i++) {
        uint8_t x = (uint8_t)rnd64(), y;
        do y = (uint8_t)rnd64(); while (y == x);
        ((uint8_t *)a)[i] = x; ((uint8_t *)b)[i] = y;
    }
    memset(R, 0, 8 * VEC_N1N2_SIZE_64);
    ref_rm_encode(R, a);
    ref_rm_encode(cb, b);
    for (int i = 0; i < PARAM_N1; i++) {
        int nd = 0;
        for (int p = i * PARAM_N2; p < (i + 1) * PARAM_N2; p++) if (getbit(R, p) != getbit(cb, p)) diff[nd++] = p;
        if (nd != PARAM_N2 / 2) continue;
        for (int j = 0; j < nd / 2 - 1; j++) {
            int r = j + (int)below((uint32_t)(nd - j));
            int t = diff[j]; diff[j] = diff[r]; diff[r] = t;
            R[diff[j] >> 6] ^= 1ULL << (diff[j] & 63);
        }
    }
    ref_rm_decode(mr, R);
    avx_rm_decode(ma, R);
    int d = 0;
    for (int i = 0; i < PARAM_N1; i++) d += ((uint8_t *)mr)[i] != ((uint8_t *)ma)[i];
    return d;
}

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: craft <out> <n_crafted> <n_controls>\n"); return 2; }
    int n_crafted = atoi(argv[2]), n_ctrl = atoi(argv[3]);
    FILE *f = fopen(argv[1], "wb");
    if (!f) return 2;
    uint8_t ek[CRYPTO_PUBLICKEYBYTES], dk[CRYPTO_SECRETKEYBYTES];
    if (hqc_kem_keygen(ek, dk)) return 2;

    // Re-derive y exactly as hqc_kem_decaps_internal does
    shake256_xof_ctx ctx;
    uint8_t seed_pke[SEED_BYTES], ek_pke[PUBLIC_KEY_BYTES_PKE], dk_pke[SECRET_KEY_BYTES_PKE];
    static uint64_t y[VEC_N_SIZE_64], u[VEC_N_SIZE_64], uy[VEC_N_SIZE_64], v[VEC_N1N2_SIZE_64], R[VEC_N1N2_SIZE_64];
    xof_init_dom_sep(&ctx, dk, SEED_BYTES, PARAM_K);
    xof_get_bytes(&ctx, seed_pke, SEED_BYTES);
    hqc_pke_keygen(ek_pke, dk_pke, seed_pke);
    hqc_dk_pke_from_string(y, dk_pke);

    int total = n_crafted + 2 * n_ctrl, min_div = PARAM_N1, sum_div = 0;
    fwrite(dk, 1, sizeof dk, f);
    fwrite(&total, sizeof total, 1, f);
    for (int k = 0; k < total; k++) {
        record_t rec = {0};
        rec.type = k < n_crafted ? 1 : (k < n_crafted + n_ctrl ? 0 : 2);
        if (rec.type == 0) {
            hqc_kem_encaps(rec.expected_k, rec.ct, ek);
        } else {
            uint8_t salt[SALT_BYTES];
            randombytes((uint8_t *)u, sizeof u);
            if (PARAM_N % 64) u[VEC_N_SIZE_64 - 1] &= (1ULL << (PARAM_N % 64)) - 1;
            randombytes(salt, sizeof salt);
            if (rec.type == 1) {
                int d = make_divergent_word(R);
                rec.diverging_blocks = (uint8_t)d;
                if (d < min_div) min_div = d;
                sum_div += d;
                vect_mul(uy, y, u);
                vect_truncate(uy);
                for (int i = 0; i < VEC_N1N2_SIZE_64; i++) v[i] = uy[i] ^ R[i];
            } else {
                randombytes((uint8_t *)v, sizeof v);
                if (PARAM_N1N2 % 64) v[VEC_N1N2_SIZE_64 - 1] &= (1ULL << (PARAM_N1N2 % 64)) - 1;
            }
            hqc_c_kem_to_string(rec.ct, u, v, salt);
        }
        fwrite(&rec, sizeof rec, 1, f);
    }
    fclose(f);
    printf("wrote %d records: %d crafted (divergent RM blocks per ct: min %d, mean %.1f of %d), %d honest, %d random\n",
           total, n_crafted, min_div, n_crafted ? (double)sum_div / n_crafted : 0.0, PARAM_N1, n_ctrl, n_ctrl);
    return min_div > 0 ? 0 : 1;
}

#else  /* decaps reader */
int main(int argc, char **argv) {
    if (argc < 2) return 2;
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 2;
    uint8_t dk[CRYPTO_SECRETKEYBYTES], k[CRYPTO_BYTES];
    int total;
    if (fread(dk, 1, sizeof dk, f) != sizeof dk || fread(&total, sizeof total, 1, f) != 1) return 2;
    int honest_bad = 0;
    for (int i = 0; i < total; i++) {
        record_t rec;
        if (fread(&rec, sizeof rec, 1, f) != 1) return 2;
        hqc_kem_decaps(k, dk, rec.ct);
        if (rec.type == 0 && memcmp(k, rec.expected_k, sizeof k)) honest_bad++;
        printf("%d %d %d ", i, rec.type, rec.diverging_blocks);
        for (size_t j = 0; j < sizeof k; j++) printf("%02x", k[j]);
        printf("\n");
    }
    fprintf(stderr, "honest ciphertexts with wrong key: %d\n", honest_bad);
    return 0;
}
#endif
