/*
 * bit_ops.h - Operations sur les bits GFA Basic 3.5
 * =================================================
 * Implemente les operations binaires integrees :
 * BTST, BSET, BCLR, BCHG, SHL, SHR, ROL, ROR.
 *
 * Reference : cahier-des-charges-gfabasic.md, section 8
 */

#ifndef GFA_BIT_OPS_H
#define GFA_BIT_OPS_H

#include "os_layer.h"
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Test de bit                                                        */
/* ------------------------------------------------------------------ */

/*
 * gfa_btst - Teste si un bit est a 1.
 * Retourne -1 si le bit est set, 0 sinon.
 * Equivalent GFA : BTST(valeur, bit)
 */
int gfa_btst(os_int32 value, int bit);

/* ------------------------------------------------------------------ */
/* Modification de bits                                               */
/* ------------------------------------------------------------------ */

/*
 * gfa_bset - Met un bit a 1.
 * Retourne la nouvelle valeur.
 * Equivalent GFA : BSET(valeur, bit)
 */
int gfa_bset(os_int32 value, int bit);

/*
 * gfa_bclr - Met un bit a 0.
 * Retourne la nouvelle valeur.
 * Equivalent GFA : BCLR(valeur, bit)
 */
int gfa_bclr(os_int32 value, int bit);

/*
 * gfa_bchg - Inverse un bit.
 * Retourne la nouvelle valeur.
 * Equivalent GFA : BCHG(valeur, bit)
 */
int gfa_bchg(os_int32 value, int bit);

/* ------------------------------------------------------------------ */
/* Decalages                                                          */
/* ------------------------------------------------------------------ */

/*
 * gfa_shl - Decalage a gauche.
 * Retourne la valeur decalee de count bits vers la gauche.
 * Equivalent GFA : SHL(valeur, count)
 */
int gfa_shl(os_int32 value, int count);

/*
 * gfa_shr - Decalage a droite (remplissage a zero).
 * Retourne la valeur decalee de count bits vers la droite.
 * Equivalent GFA : SHR(valeur, count)
 */
int gfa_shr(os_int32 value, int count);

/* ------------------------------------------------------------------ */
/* Rotations (16 bits)                                                */
/* ------------------------------------------------------------------ */

/*
 * gfa_rol - Rotation a gauche sur 16 bits.
 * La valeur est masquee sur 16 bits avant rotation.
 * Retourne la valeur rotative de count bits vers la gauche.
 * Equivalent GFA : ROL(valeur, count)
 */
int gfa_rol(os_int32 value, int count);

/*
 * gfa_ror - Rotation a droite sur 16 bits.
 * La valeur est masquee sur 16 bits avant rotation.
 * Retourne la valeur rotative de count bits vers la droite.
 * Equivalent GFA : ROR(valeur, count)
 */
int gfa_ror(os_int32 value, int count);

#ifdef __cplusplus
}
#endif

#endif /* GFA_BIT_OPS_H */
