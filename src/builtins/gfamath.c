/*
 * math_builtin.c - Implementation des fonctions mathematiques
 * ===========================================================
 * Fonctions trigonometriques, logarithmes, arrondis, aleatoires.
 * Conventions C89 strictes.
 *
 * Reference : cahier-des-charges-gfabasic.md, section 8.8
 */

#include "math_builtin.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/* Generateur aleatoire GFA (compatible ST)                          */
/* ------------------------------------------------------------------ */

/* Etat interne du generateur */
static os_int32 g_rnd_seed = 1;
static double   g_rnd_last = 0.5;

/*
 * GFA utilise un generateur congruentiel lineaire simple.
 * Sur Atari ST : RND et RANDOM utilisent le registre video.
 * Ici on utilise un LCG 32 bits raisonnable.
 */
static os_int32 lcg_next(void)
{
    /* Parametres LCG (Numerical Recipes) */
    g_rnd_seed = (os_int32)(((unsigned long)g_rnd_seed * 1664525UL + 1013904223UL)
                            & 0x7FFFFFFFUL);
    return g_rnd_seed;
}

double gfa_rnd(double x)
{
    double result;

    if (x < 0.0) {
        /* Reinitialiser le generateur avec |x| comme graine */
        g_rnd_seed = (os_int32)(-x);
        if (g_rnd_seed <= 0) g_rnd_seed = 1;
        g_rnd_last = 0.5;
        return 0.0;
    }

    if (x == 0.0) {
        /* Retourner la valeur precedente */
        return g_rnd_last;
    }

    /* x > 0 : retourne un nombre dans [0, x[ */
    g_rnd_last = (double)lcg_next() / (double)0x80000000UL;
    result = g_rnd_last * x;

    return result;
}

double gfa_random(void)
{
    /*
     * RANDOM retourne une valeur basee sur le registre video
     * de l'Atari ST. Emule avec le LCG.
     */
    g_rnd_last = (double)lcg_next() / (double)0x80000000UL;
    return g_rnd_last;
}

void gfa_randomize(os_int32 seed)
{
    if (seed == 0) {
        /* Utiliser le temps courant comme graine */
        g_rnd_seed = (os_int32)((unsigned long)time(NULL) & 0x7FFFFFFFUL);
        if (g_rnd_seed <= 0) g_rnd_seed = 1;
    } else {
        g_rnd_seed = seed;
    }
    g_rnd_last = 0.5;
}

/* ------------------------------------------------------------------ */
/* Trigonometrie (radians)                                            */
/* ------------------------------------------------------------------ */

double gfa_sin(double x)
{
    return sin(x);
}

double gfa_cos(double x)
{
    return cos(x);
}

double gfa_tan(double x)
{
    double c;
    c = cos(x);
    if (fabs(c) < 1.0e-15) {
        /* Tangente indefinie : retourne un grand nombre */
        return (x > 0.0) ? 1.0e300 : -1.0e300;
    }
    return sin(x) / c;
}

double gfa_atn(double x)
{
    return atan(x);
}

double gfa_asin(double x)
{
    if (x < -1.0 || x > 1.0) {
        return 0.0; /* Hors domaine, GFA emet l'erreur 67 */
    }
    return asin(x);
}

double gfa_acos(double x)
{
    if (x < -1.0 || x > 1.0) {
        return 0.0;
    }
    return acos(x);
}

double gfa_sinh(double x)
{
    return sinh(x);
}

double gfa_cosh(double x)
{
    return cosh(x);
}

double gfa_tanh(double x)
{
    return tanh(x);
}

/* ------------------------------------------------------------------ */
/* Trigonometrie (degres)                                             */
/* ------------------------------------------------------------------ */

double gfa_sinq(double x)
{
    return sin(x * GFA_DEG2RAD);
}

double gfa_cosq(double x)
{
    return cos(x * GFA_DEG2RAD);
}

/* ------------------------------------------------------------------ */
/* Exponentielle et logarithmes                                       */
/* ------------------------------------------------------------------ */

double gfa_exp(double x)
{
    return exp(x);
}

double gfa_log(double x)
{
    if (x <= 0.0) {
        return 0.0; /* GFA emet l'erreur 6 */
    }
    return log(x);
}

double gfa_log10(double x)
{
    if (x <= 0.0) {
        return 0.0;
    }
    return log10(x);
}

double gfa_sqr(double x)
{
    if (x < 0.0) {
        return 0.0; /* GFA emet l'erreur 5 */
    }
    return sqrt(x);
}

/* ------------------------------------------------------------------ */
/* Arrondis et troncatures                                            */
/* ------------------------------------------------------------------ */

double gfa_abs(double x)
{
    return fabs(x);
}

double gfa_sgn(double x)
{
    if (x > 0.0) return 1.0;
    if (x < 0.0) return -1.0;
    return 0.0;
}

double gfa_int(double x)
{
    return floor(x);
}

double gfa_frac(double x)
{
    double int_part;
    int_part = floor(fabs(x));
    return fabs(x) - int_part;
}

double gfa_fix(double x)
{
    /* Troncature vers 0 */
    if (x >= 0.0) {
        return floor(x);
    }
    return ceil(x);
}

double gfa_round(double x)
{
    return floor(x + 0.5);
}

double gfa_ceil(double x)
{
    return ceil(x);
}

double gfa_trunc(double x)
{
    return gfa_fix(x);
}

/* ------------------------------------------------------------------ */
/* Arithmetique entiere                                               */
/* ------------------------------------------------------------------ */

os_int32 gfa_mod(os_int32 a, os_int32 b)
{
    if (b == 0) return 0;
    return a % b;
}

os_int32 gfa_div_int(os_int32 a, os_int32 b)
{
    if (b == 0) return 0;
    return a / b;
}

os_int32 gfa_min_int(os_int32 a, os_int32 b)
{
    return (a < b) ? a : b;
}

os_int32 gfa_max_int(os_int32 a, os_int32 b)
{
    return (a > b) ? a : b;
}

double gfa_min(double a, double b)
{
    return (a < b) ? a : b;
}

double gfa_max(double a, double b)
{
    return (a > b) ? a : b;
}

/* ------------------------------------------------------------------ */
/* Tests logiques                                                     */
/* ------------------------------------------------------------------ */

int gfa_even(os_int32 x)
{
    return ((x & 1) == 0) ? 1 : 0;
}

int gfa_odd(os_int32 x)
{
    return ((x & 1) != 0) ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* Combinaisons / factorielle                                         */
/* ------------------------------------------------------------------ */

double gfa_fact(int n)
{
    double result;
    int i;

    if (n < 0) return 0.0;
    if (n <= 1) return 1.0;

    result = 1.0;
    for (i = 2; i <= n; i++) {
        result *= (double)i;
    }

    return result;
}

double gfa_combin(int n, int k)
{
    double result;
    int i;

    if (k < 0 || k > n) return 0.0;

    /* Optimisation : C(n,k) = C(n, n-k) */
    if (k > n - k) {
        k = n - k;
    }

    result = 1.0;
    for (i = 1; i <= k; i++) {
        result *= (double)(n - k + i);
        result /= (double)i;
    }

    return result;
}

double gfa_variat(int n, int k)
{
    double result;
    int i;

    if (k < 0 || k > n) return 0.0;

    result = 1.0;
    for (i = 0; i < k; i++) {
        result *= (double)(n - i);
    }

    return result;
}

/* ------------------------------------------------------------------ */
/* Conversions                                                        */
/* ------------------------------------------------------------------ */

double gfa_cfloat(double x)
{
    /* Deja un flottant, juste retourner */
    return x;
}

int gfa_cint(double x)
{
    return (int)gfa_fix(x);
}

double gfa_deg(double x)
{
    return x * GFA_RAD2DEG;
}

double gfa_rad(double x)
{
    return x * GFA_DEG2RAD;
}

/* ------------------------------------------------------------------ */
/* Predicats                                                          */
/* ------------------------------------------------------------------ */

os_int32 gfa_pred(os_int32 x)
{
    return x - 1;
}

os_int32 gfa_succ(os_int32 x)
{
    return x + 1;
}
