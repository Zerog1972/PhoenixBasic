/*
 * strings.h - Bibliotheque de manipulation de chaines GFA Basic
 * =============================================================
 * Implemente l'ensemble des fonctions chaines du GFA Basic 3.5.
 * Toutes les fonctions respectent la norme C89 stricte.
 *
 * Reference : cahier-des-charges-gfabasic.md, section 8.9
 */

#ifndef GFA_STRINGS_H
#define GFA_STRINGS_H

#include "os_layer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Fonctions de base                                                  */
/* ------------------------------------------------------------------ */

/*
 * gfa_asc - Retourne le code ASCII du premier caractere.
 * Equivalent GFA : ASC(s$)
 */
int gfa_asc(const char *s);

/*
 * gfa_chr - Retourne un caractere a partir de son code ASCII.
 * Equivalent GFA : CHR$(n)
 * Le caractere est retourne dans un buffer statique.
 */
const char *gfa_chr(int code);

/*
 * gfa_len - Retourne la longueur d'une chaine.
 * Equivalent GFA : LEN(s$)
 */
int gfa_len(const char *s);

/*
 * gfa_mid - Extrait une sous-chaine.
 * Equivalent GFA : MID$(s$, pos[, len])
 */
char *gfa_mid(const char *s, int pos, int len);

/*
 * gfa_mid_assign - Modifie une partie de chaine (MID$ en ecriture).
 * Equivalent GFA : MID$(s$, pos, len) = value$
 * Retourne 0 si succes, -1 si erreur.
 */
int gfa_mid_assign(char *s, int pos, int len, const char *value);

/*
 * gfa_left - Extrait les n premiers caracteres.
 * Equivalent GFA : LEFT$(s$, n)
 */
char *gfa_left(const char *s, int n);

/*
 * gfa_right - Extrait les n derniers caracteres.
 * Equivalent GFA : RIGHT$(s$, n)
 */
char *gfa_right(const char *s, int n);

/*
 * gfa_instr - Recherche une sous-chaine.
 * Equivalent GFA : INSTR([pos,] s$, recherche$)
 * Retourne la position (1-indexee) ou 0 si non trouve.
 */
int gfa_instr(int pos, const char *haystack, const char *needle);

/*
 * gfa_rinstr - Recherche une sous-chaine depuis la fin.
 * Equivalent GFA : RINSTR([pos,] s$, recherche$)
 * Retourne la position (1-indexee) ou 0 si non trouve.
 */
int gfa_rinstr(int pos, const char *haystack, const char *needle);

/* ------------------------------------------------------------------ */
/* Conversion                                                         */
/* ------------------------------------------------------------------ */

/*
 * gfa_str_f - Convertit un flottant en chaine.
 * Equivalent GFA : STR$(x)
 */
char *gfa_str_float(double value);

/*
 * gfa_str_fmt - Convertit un flottant avec format.
 * Equivalent GFA : STR$(x, total_digits, decimals)
 */
char *gfa_str_float_fmt(double value, int total_digits, int decimals);

/*
 * gfa_str_long - Convertit un entier long en chaine.
 * Equivalent GFA : STR$(x%)
 */
char *gfa_str_long(os_int32 value);

/*
 * gfa_val - Convertit une chaine en nombre.
 * Equivalent GFA : VAL(s$)
 */
double gfa_val(const char *s);

/*
 * gfa_val_count - Nombre de caracteres convertibles en debut de chaine.
 * Equivalent GFA : VAL?(s$)
 */
int gfa_val_count(const char *s);

/*
 * gfa_bin - Conversion en binaire.
 * Equivalent GFA : BIN$(x[, digits])
 */
char *gfa_bin(os_int32 value, int digits);

/*
 * gfa_hex - Conversion en hexadecimal.
 * Equivalent GFA : HEX$(x[, digits])
 */
char *gfa_hex(os_int32 value, int digits);

/*
 * gfa_oct - Conversion en octal.
 * Equivalent GFA : OCT$(x[, digits])
 */
char *gfa_oct(os_int32 value, int digits);

/*
 * gfa_upper - Conversion en majuscules.
 * Equivalent GFA : UPPER$(s$)
 */
char *gfa_upper(const char *s);

/*
 * gfa_lower - Conversion en minuscules.
 * Equivalent GFA : LCASE$(s$) / LOWER$(s$)
 */
char *gfa_lower(const char *s);

/* ------------------------------------------------------------------ */
/* Generation de chaines                                              */
/* ------------------------------------------------------------------ */

/*
 * gfa_space - Genere n espaces.
 * Equivalent GFA : SPACE$(n)
 */
char *gfa_space(int n);

/*
 * gfa_string - Repete une chaine n fois.
 * Equivalent GFA : STRING$(n, s$)
 */
char *gfa_string(int n, const char *s);

/*
 * gfa_string_char - Repete un caractere n fois.
 * Equivalent GFA : STRING$(n, code)
 */
char *gfa_string_char(int n, int code);

/* ------------------------------------------------------------------ */
/* Nettoyage                                                          */
/* ------------------------------------------------------------------ */

/*
 * gfa_trim - Supprime les espaces en debut et fin de chaine.
 * Equivalent GFA : TRIM$(s$)
 */
char *gfa_trim(const char *s);

/*
 * gfa_ltrim - Supprime les espaces en debut de chaine.
 */
char *gfa_ltrim(const char *s);

/*
 * gfa_rtrim - Supprime les espaces en fin de chaine.
 */
char *gfa_rtrim(const char *s);

/* ------------------------------------------------------------------ */
/* Conversion binaire <-> chaine (donnees typees)                     */
/* ------------------------------------------------------------------ */

/*
 * gfa_mki - Convertit un entier 16 bits en chaine de 2 octets.
 * Equivalent GFA : MKI$(x&)
 */
char *gfa_mki(os_int32 value);

/*
 * gfa_mkl - Convertit un entier 32 bits en chaine de 4 octets.
 * Equivalent GFA : MKL$(x%)
 */
char *gfa_mkl(os_int32 value);

/*
 * gfa_mks - Convertit un flottant en chaine de 4 octets.
 * Equivalent GFA : MKS$(x!)
 */
char *gfa_mks(double value);

/*
 * gfa_mkf - Convertit un flottant en chaine de 6 octets.
 * Equivalent GFA : MKF$(x!)
 */
char *gfa_mkf(double value);

/*
 * gfa_mkd - Convertit un double en chaine de 8 octets.
 * Equivalent GFA : MKD$(x#)
 */
char *gfa_mkd(double value);

/*
 * gfa_cvi - Convertit une chaine de 2 octets en entier.
 * Equivalent GFA : CVI(s$)
 */
int gfa_cvi(const char *s);

/*
 * gfa_cvl - Convertit une chaine de 4 octets en entier long.
 * Equivalent GFA : CVL(s$)
 */
os_int32 gfa_cvl(const char *s);

/*
 * gfa_cvs - Convertit une chaine de 4 octets en flottant.
 * Equivalent GFA : CVS(s$)
 */
double gfa_cvs(const char *s);

/*
 * gfa_cvf - Convertit une chaine de 6 octets en flottant.
 * Equivalent GFA : CVF(s$)
 */
double gfa_cvf(const char *s);

/*
 * gfa_cvd - Convertit une chaine de 8 octets en double.
 * Equivalent GFA : CVD(s$)
 */
double gfa_cvd(const char *s);

/*
 * gfa_insert - Insere une chaine dans une autre.
 * Equivalent GFA : INSERT(a$, b$)
 */
char *gfa_insert(const char *target, const char *source);

/* ------------------------------------------------------------------ */
/* Utilitaires internes (exposes pour les autres modules)             */
/* ------------------------------------------------------------------ */

/*
 * gfa_str_new - Alloue une copie de chaine.
 * A liberer avec os_mem_free().
 */
char *gfa_str_new(const char *s);

/*
 * gfa_str_dup_n - Alloue et copie n caracteres de s.
 */
char *gfa_str_dup_n(const char *s, int n);

/*
 * gfa_str_concat - Concatene deux chaines (alloue le resultat).
 */
char *gfa_str_concat(const char *a, const char *b);

/*
 * gfa_str_concat3 - Concatene trois chaines.
 */
char *gfa_str_concat3(const char *a, const char *b, const char *c);

#ifdef __cplusplus
}
#endif

#endif /* GFA_STRINGS_H */
