/*
 * Valgrind constant-time harness for PHX-Tri (ctgrind technique, as in experiments/hqc-ct).
 * Secret inputs are marked undefined; memcheck reports every branch or memory index that depends on them.
 *   ct_phx_tri keygen   secret: key-generation seed
 *   ct_phx_tri encaps   secret: encapsulation seed (all of it, including the HQC salt, which is public)
 *   ct_phx_tri decaps   secret: decapsulation key (seed); valid ciphertext
 * Public outputs (pk, ct) and the final shared secret are re-marked defined.
 */
#include <stdio.h>
#include <string.h>
#include <valgrind/memcheck.h>
#include "phx_tri.h"

static uint8_t pk[PHX_TRI_PUBLICKEYBYTES], ct[PHX_TRI_CIPHERTEXTBYTES];

int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "decaps";
    uint8_t seed[32], eseed[PHX_TRI_ENCAPS_SEEDBYTES], sk[32], ss[32], ss2[32];
    for (int i = 0; i < 32; i++) seed[i] = (uint8_t)(3 * i + 1);
    for (int i = 0; i < PHX_TRI_ENCAPS_SEEDBYTES; i++) eseed[i] = (uint8_t)(7 * i + 2);

    if (!strcmp(mode, "keygen")) VALGRIND_MAKE_MEM_UNDEFINED(seed, sizeof seed);
    if (phx_tri_keygen_derand(pk, sk, seed)) return 2;
    VALGRIND_MAKE_MEM_DEFINED(pk, sizeof pk);
    VALGRIND_MAKE_MEM_DEFINED(sk, sizeof sk);

    if (!strcmp(mode, "encaps")) VALGRIND_MAKE_MEM_UNDEFINED(eseed, sizeof eseed);
    if (phx_tri_encaps_derand(ss, ct, pk, eseed)) return 2;
    VALGRIND_MAKE_MEM_DEFINED(ct, sizeof ct);
    VALGRIND_MAKE_MEM_DEFINED(ss, sizeof ss);

    if (!strcmp(mode, "decaps")) VALGRIND_MAKE_MEM_UNDEFINED(sk, sizeof sk);
    if (phx_tri_decaps(ss2, ct, sk)) return 2;
    VALGRIND_MAKE_MEM_DEFINED(ss2, sizeof ss2);
    if (memcmp(ss, ss2, sizeof ss)) { fprintf(stderr, "FUNCTIONAL FAILURE\n"); return 3; }
    return 0;
}
