/*
 * bit_ops.c - Implementation des operations sur les bits
 * ======================================================
 * Fonctions de test, modification, decalage et rotation de bits.
 * Conventions C89 strictes.
 *
 * Reference : cahier-des-charges-gfabasic.md, section 8
 */

#include "bit_ops.h"
#include <limits.h>

/* ------------------------------------------------------------------ */
/* Test de bit                                                        */
/* ------------------------------------------------------------------ */

int gfa_btst(os_int32 value, int bit)
{
    if (bit < 0 || bit > 31) {
        return 0;
    }
    return (value & (1U << bit)) ? -1 : 0;
}

/* ------------------------------------------------------------------ */
/* Modification de bits                                               */
/* ------------------------------------------------------------------ */

int gfa_bset(os_int32 value, int bit)
{
    if (bit < 0 || bit > 31) {
        return (int)value;
    }
    return (int)(value | (1U << bit));
}

int gfa_bclr(os_int32 value, int bit)
{
    if (bit < 0 || bit > 31) {
        return (int)value;
    }
    return (int)(value & ~(1U << bit));
}

int gfa_bchg(os_int32 value, int bit)
{
    if (bit < 0 || bit > 31) {
        return (int)value;
    }
    return (int)(value ^ (1U << bit));
}

/* ------------------------------------------------------------------ */
/* Decalages                                                          */
/* ------------------------------------------------------------------ */

int gfa_shl(os_int32 value, int count)
{
    if (count <= 0) {
        return (int)value;
    }
    if (count >= 32) {
        return 0;
    }
    return (int)((unsigned int)value << count);
}

int gfa_shr(os_int32 value, int count)
{
    unsigned int u;
    if (count <= 0) {
        return (int)value;
    }
    if (count >= 32) {
        return 0;
    }
    /* Cast en unsigned pour garantir un decalage logique (fill a 0) */
    u = (unsigned int)value;
    return (int)(u >> count);
}

/* ------------------------------------------------------------------ */
/* Rotations (16 bits)                                                */
/* ------------------------------------------------------------------ */

int gfa_rol(os_int32 value, int count)
{
    unsigned int u;
    unsigned int result;

    if (count <= 0) {
        return (int)((short)value);  /* Masquer sur 16 bits meme sans rotation */
    }

    /* Masquer sur 16 bits */
    u = (unsigned int)value & 0xFFFFU;
    count = count & 0xF;  /* count % 16 */

    if (count == 0) {
        return (int)((short)u);  /* Preserve le signe */
    }

    result = (u << count) | (u >> (16 - count));
    result &= 0xFFFFU;

    /* Re-signer le resultat 16 bits */
    return (int)((short)result);
}

int gfa_ror(os_int32 value, int count)
{
    unsigned int u;
    unsigned int result;

    if (count <= 0) {
        return (int)((short)value);  /* Masquer sur 16 bits meme sans rotation */
    }

    /* Masquer sur 16 bits */
    u = (unsigned int)value & 0xFFFFU;
    count = count & 0xF;  /* count % 16 */

    if (count == 0) {
        return (int)((short)u);  /* Preserve le signe */
    }

    result = (u >> count) | (u << (16 - count));
    result &= 0xFFFFU;

    /* Re-signer le resultat 16 bits */
    return (int)((short)result);
}
