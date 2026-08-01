/*
 * gfamath.h - Fonctions mathematiques GFA Basic 3.5
 * =================================================
 * Implemente l'ensemble des fonctions mathematiques integrees :
 * trigonometrie, logarithmes, arrondis, aleatoires, combinaisons.
 *
 * Reference : cahier-des-charges-gfabasic.md, section 8.8
 */

#ifndef GFA_MATH_H
#define GFA_MATH_H

#include "os_layer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Constantes                                                         */
/* ------------------------------------------------------------------ */

#define GFA_PI        3.14159265358979323846
#define GFA_E         2.71828182845904523536
#define GFA_RAD2DEG   (180.0 / GFA_PI)
#define GFA_DEG2RAD   (GFA_PI / 180.0)

/* ------------------------------------------------------------------ */
/* Trigonometrie (radians)                                            */
/* ------------------------------------------------------------------ */

double gfa_sin(double x);
double gfa_cos(double x);
double gfa_tan(double x);
double gfa_atn(double x);
double gfa_asin(double x);
double gfa_acos(double x);

/* Hyperboliques */
double gfa_sinh(double x);
double gfa_cosh(double x);
double gfa_tanh(double x);

/* ------------------------------------------------------------------ */
/* Trigonometrie (degres)                                             */
/* ------------------------------------------------------------------ */

double gfa_sinq(double x);
double gfa_cosq(double x);

/* ------------------------------------------------------------------ */
/* Exponentielle et logarithmes                                       */
/* ------------------------------------------------------------------ */

double gfa_exp(double x);
double gfa_log(double x);
double gfa_log10(double x);
double gfa_sqr(double x);

/* ------------------------------------------------------------------ */
/* Arrondis et troncatures                                            */
/* ------------------------------------------------------------------ */

double gfa_abs(double x);
double gfa_sgn(double x);
double gfa_int(double x);      /* Partie entiere (vers -inf)   */
double gfa_frac(double x);     /* Partie fractionnaire         */
double gfa_fix(double x);      /* Partie entiere (vers 0)      */
double gfa_round(double x);    /* Arrondi au plus proche       */
double gfa_ceil(double x);     /* Arrondi a l'entier superieur */
double gfa_trunc(double x);    /* Troncature entiere (vers 0)  */

/* ------------------------------------------------------------------ */
/* Arithmetique entiere et bits                                       */
/* ------------------------------------------------------------------ */

os_int32 gfa_mod(os_int32 a, os_int32 b);
os_int32 gfa_div_int(os_int32 a, os_int32 b);
os_int32 gfa_min_int(os_int32 a, os_int32 b);
os_int32 gfa_max_int(os_int32 a, os_int32 b);

double gfa_min(double a, double b);
double gfa_max(double a, double b);

/* ------------------------------------------------------------------ */
/* Tests logiques                                                     */
/* ------------------------------------------------------------------ */

int gfa_even(os_int32 x);
int gfa_odd(os_int32 x);

/* ------------------------------------------------------------------ */
/* Combinaisons / factorielle                                         */
/* ------------------------------------------------------------------ */

double gfa_fact(int n);
double gfa_combin(int n, int k);
double gfa_variat(int n, int k);  /* Arrangements : n!/(n-k)! */

/* ------------------------------------------------------------------ */
/* Nombres aleatoires                                                 */
/* ------------------------------------------------------------------ */

/*
 * gfa_rnd - Nombre aleatoire GFA.
 * x < 0  : reinitialise le generateur avec |x| comme graine
 * x = 0  : retourne le precedent
 * x > 0  : retourne un nombre dans [0, x[
 *
 * Equivalent GFA : RND(x)
 */
double gfa_rnd(double x);

/*
 * gfa_random - Nombre aleatoire brut (compatible ST).
 * Equivalent GFA : RANDOM
 */
double gfa_random(void);

/*
 * gfa_randomize - Initialise le generateur aleatoire.
 * Equivalent GFA : RANDOMIZE [seed]
 */
void gfa_randomize(os_int32 seed);

/* ------------------------------------------------------------------ */
/* Conversions                                                        */
/* ------------------------------------------------------------------ */

/*
 * gfa_cfloat - Conversion entier -> flottant.
 * Equivalent GFA : CFLOAT(x)
 */
double gfa_cfloat(double x);

/*
 * gfa_cint - Conversion flottant -> entier.
 * Equivalent GFA : CINT(x)
 */
int gfa_cint(double x);

/*
 * gfa_deg - Conversion radians -> degres.
 * Equivalent GFA : DEG(x)
 */
double gfa_deg(double x);

/*
 * gfa_rad - Conversion degres -> radians.
 * Equivalent GFA : RAD(x)
 */
double gfa_rad(double x);

/* ------------------------------------------------------------------ */
/* Predicats sur les entiers                                          */
/* ------------------------------------------------------------------ */

os_int32 gfa_pred(os_int32 x);
os_int32 gfa_succ(os_int32 x);

#ifdef __cplusplus
}
#endif

#endif /* GFA_MATH_H */
