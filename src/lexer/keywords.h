/*
 * keywords.h - Table des mots-cles GFA Basic 3.5
 * ===============================================
 * Associe chaque mot-cle a son type de token.
 * Le lexer est insensible a la casse.
 *
 * Reference : cahier-des-charges-gfabasic.md, section 3
 */

#ifndef GFA_KEYWORDS_H
#define GFA_KEYWORDS_H

#include "token.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Structure d'entree dans la table des mots-cles.
 */
typedef struct {
    const char     *name;       /* Nom du mot-cle (majuscules) */
    gfa_token_type  token;      /* Type de token associe       */
} gfa_keyword_entry;

/*
 * Table globale des mots-cles (definie dans keywords.c).
 */
extern const gfa_keyword_entry gfa_keywords[];
extern const int gfa_keyword_count;

/*
 * gfa_keyword_lookup - Recherche un mot-cle par son nom (insensible casse).
 * Retourne le type de token, ou TOK_IDENTIFIER si non trouve.
 */
gfa_token_type gfa_keyword_lookup(const char *name);

/*
 * gfa_keyword_get_name - Retourne le nom d'un type de token.
 * Utile pour les messages d'erreur.
 */
const char *gfa_keyword_get_name(gfa_token_type type);

#ifdef __cplusplus
}
#endif

#endif /* GFA_KEYWORDS_H */
