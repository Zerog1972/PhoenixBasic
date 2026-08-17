/*
 * lexer.h - Analyseur lexical GFA Basic 3.5
 * ==========================================
 * Tokenisation d'un programme source GFA Basic.
 *
 * Reference : cahier-des-charges-gfabasic.md, section 3
 */

#ifndef GFA_LEXER_H
#define GFA_LEXER_H

#include "token.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Structure du lexer                                                 */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *source;      /* Texte source complet              */
    int         src_len;     /* Longueur du source                */
    int         pos;         /* Position courante dans le source  */
    int         line;        /* Ligne courante (1-indexee)        */
    int         column;      /* Colonne courante (1-indexee)      */
    int         line_start;  /* Position du debut de ligne        */

    gfa_token   current;     /* Token courant                     */
    gfa_token   peek;        /* Token suivant (lookahead 1)       */
    int         has_peek;    /* 1 si peek est valide              */
    int         at_stmt_start; /* 1 si le prochain token est au
                                  debut d'instruction (apres EOL,
                                  ':' ou debut de programme)        */

    lexer_error error;       /* Derniere erreur                   */
    int         error_line;
    int         error_column;
    char        error_msg[128];

    /* Gestion des abreviations de commandes (ex: p -> PRINT) */
    int         expand_abbrev;
} gfa_lexer;

/* ------------------------------------------------------------------ */
/* API du lexer                                                       */
/* ------------------------------------------------------------------ */

/*
 * gfa_lexer_init - Initialise un lexer avec un texte source.
 * Le source doit rester valide pendant toute la duree de vie du lexer.
 */
gfa_lexer *gfa_lexer_init(const char *source);

/*
 * gfa_lexer_free - Libere les ressources du lexer.
 */
void gfa_lexer_free(gfa_lexer *lexer);

/*
 * gfa_lexer_next - Lit le token suivant et le place dans lexer->current.
 * Retourne le type du token, ou TOK_EOF si fin du source.
 */
gfa_token_type gfa_lexer_next(gfa_lexer *lexer);

/*
 * gfa_lexer_peek_token - Lit le token suivant sans avancer.
 * Retourne le type du token.
 */
gfa_token_type gfa_lexer_peek_token(gfa_lexer *lexer);

/*
 * gfa_lexer_current_token - Retourne le type du token courant.
 */
gfa_token_type gfa_lexer_current_token(gfa_lexer *lexer);

/*
 * gfa_lexer_expect - Verifie que le token courant est du type attendu.
 * Si oui, avance au token suivant. Sinon, signale une erreur.
 */
int gfa_lexer_expect(gfa_lexer *lexer, gfa_token_type type);

/*
 * gfa_lexer_skip_to_eol - Saute jusqu'a la fin de la ligne courante.
 */
void gfa_lexer_skip_to_eol(gfa_lexer *lexer);

/*
 * gfa_lexer_get_error - Retourne le dernier code d'erreur.
 */
lexer_error gfa_lexer_get_error(gfa_lexer *lexer);

/*
 * gfa_lexer_get_error_msg - Retourne le message d'erreur.
 */
const char *gfa_lexer_get_error_msg(gfa_lexer *lexer);

/*
 * gfa_lexer_get_line - Retourne la ligne courante.
 */
int gfa_lexer_get_line(gfa_lexer *lexer);

/*
 * gfa_lexer_get_column - Retourne la colonne courante.
 */
int gfa_lexer_get_column(gfa_lexer *lexer);

/*
 * gfa_lexer_set_expand - Active/desactive l'expansion des abreviations.
 */
void gfa_lexer_set_expand(gfa_lexer *lexer, int expand);

#ifdef __cplusplus
}
#endif

#endif /* GFA_LEXER_H */
