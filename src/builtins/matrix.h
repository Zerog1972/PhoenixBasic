/*
 * matrix.h - Operations matricielles GFA Basic 3.5 (MAT)
 * ========================================================
 * Implémente MAT READ/INPUT/PRINT/CLR/ONE/SET/CPY/ADD/SUB/MUL/
 * TRANS/INV/DET/RANG/NORM/BASE sur des tableaux 2D flottants
 * (gfa_variable de type GFA_VAR_ARRAY, is_matrix = 1).
 *
 * C89 strict.
 */

#ifndef GFA_MATRIX_H
#define GFA_MATRIX_H

#include "../runtime/runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Sous-opérations MAT (values of OP_MAT_EXEC operand.int_val) */
#define MAT_OP_READ    0
#define MAT_OP_INPUT   1
#define MAT_OP_PRINT   2
#define MAT_OP_CLR     3
#define MAT_OP_ONE     4
#define MAT_OP_SET     5
#define MAT_OP_CPY     6
#define MAT_OP_ADD     7
#define MAT_OP_SUB     8
#define MAT_OP_MUL     9
#define MAT_OP_TRANS   10
#define MAT_OP_INV     11
#define MAT_OP_DET     12
#define MAT_OP_RANG    13
#define MAT_OP_NORM    14
#define MAT_OP_BASE    15

/*
 * gfa_matrix_get — Recherche une matrice par nom.
 * Retourne NULL si absente ou si ce n'est pas une matrice.
 */
gfa_variable *gfa_matrix_get(gfa_symbol_table *tab, const char *name);

/*
 * gfa_matrix_ensure — Recherche ou cree la matrice name (rows x cols).
 * Si elle existe avec d'autres dimensions, elle est redimensionnee
 * (donnees reinitialisees). Retourne NULL sur echec memoire.
 */
gfa_variable *gfa_matrix_ensure(gfa_symbol_table *tab, const char *name,
                                int rows, int cols);

/*
 * gfa_matrix_exec — Execute une operation MAT.
 *
 *   sub_op    : MAT_OP_*
 *   target    : nom de la matrice cible (SET/CPY/ADD/...)
 *   src1      : premier operande (nom) ou NULL
 *   src2      : second operande (nom) ou NULL
 *   has_value : 1 si value porte le scalaire (MAT SET, MAT BASE)
 *   value     : scalaire
 *
 * Retourne 0 si succes, code d'erreur GFA sinon.
 */
int gfa_matrix_exec(gfa_runtime *rt, int sub_op, const char *target,
                    const char *src1, const char *src2,
                    int has_value, double value);

/*
 * gfa_matrix_print — Affiche une matrice (MAT PRINT).
 */
int gfa_matrix_print(gfa_runtime *rt, gfa_variable *m);

/*
 * gfa_matrix_scalar - Calcule un scalaire (DET/QDET/RANG/NORM)
 * sur la matrice src et l'affiche (forme statement sans cible :
 * MAT DET(a)). Retourne 0 si succes, code d'erreur GFA sinon.
 */
int gfa_matrix_scalar(gfa_runtime *rt, int sub_op, const char *src);

/*
 * gfa_matrix_scalar_value - Comme gfa_matrix_scalar mais renvoie la
 * valeur dans *out au lieu de l'afficher (pour l'usage en expression,
 * ex. x = MAT DET(a)).
 * 0 = succes, -1 = echec.
 */
int gfa_matrix_scalar_value(gfa_runtime *rt, int sub_op, const char *src,
                            double *out);

/*
 * gfa_matrix_read_data — Remplit une matrice depuis le flux DATA.
 * Retourne 0 si succes.
 */
int gfa_matrix_read_data(gfa_runtime *rt, gfa_variable *m);

/*
 * gfa_matrix_input — Lit une matrice depuis la console (MAT INPUT).
 * Retourne 0 si succes.
 */
int gfa_matrix_input(gfa_runtime *rt, gfa_variable *m);

#ifdef __cplusplus
}
#endif

#endif /* GFA_MATRIX_H */
