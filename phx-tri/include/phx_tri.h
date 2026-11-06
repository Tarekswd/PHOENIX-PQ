/*
 * PHX-Tri v0: experimental three-way hybrid KEM, X25519 + ML-KEM-768 + HQC-3, with an
 * X-Wing-style (QSF) combiner. See phx-tri/SPEC.md. Research code: not for production use.
 *
 * All functions return 0 on success and -1 on failure. A failing call leaves its outputs
 * zeroed; it never falls back to weaker behaviour.
 */
#ifndef PHX_TRI_H
#define PHX_TRI_H

#include <stdint.h>

#define PHX_TRI_PUBLICKEYBYTES    5730   /* pk_M 1184 || pk_H 4514 || pk_X 32 */
#define PHX_TRI_SECRETKEYBYTES    32     /* the key-generation seed */
#define PHX_TRI_CIPHERTEXTBYTES   10098  /* ct_M 1088 || ct_H 8978 || ct_X 32 */
#define PHX_TRI_SHAREDSECRETBYTES 32
#define PHX_TRI_KEYGEN_SEEDBYTES  32
#define PHX_TRI_ENCAPS_SEEDBYTES  104    /* ML-KEM 32 || HQC m 24 || HQC salt 16 || X25519 32 */

int phx_tri_keygen(uint8_t *pk, uint8_t *sk);
int phx_tri_keygen_derand(uint8_t *pk, uint8_t *sk, const uint8_t seed[PHX_TRI_KEYGEN_SEEDBYTES]);
int phx_tri_encaps(uint8_t *ss, uint8_t *ct, const uint8_t *pk);
int phx_tri_encaps_derand(uint8_t *ss, uint8_t *ct, const uint8_t *pk,
                          const uint8_t eseed[PHX_TRI_ENCAPS_SEEDBYTES]);
int phx_tri_decaps(uint8_t *ss, const uint8_t *ct, const uint8_t *sk);

#ifdef PHX_TRI_DEBUG
/* Test-only: the component values of the last encapsulation, for checking the combiner independently. */
void phx_tri_debug_last(uint8_t ss_m[32], uint8_t ss_h[32], uint8_t ss_x[32], uint8_t ct_x[32], uint8_t pk_x[32]);
#endif

#endif
