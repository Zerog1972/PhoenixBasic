/*
 * parser.h - Analyseur syntaxique GFA Basic 3.5
 * ==============================================
 * Parsing recursif descendant (LL(1)) du GFA Basic.
 * Transforme un flux de tokens en arbre syntaxique (AST).
 *
 * Reference : cahier-des-charges-gfabasic.md, section 4
 */

#ifndef GFA_PARSER_H
#define GFA_PARSER_H

#include "lexer.h"
#include "ast.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Structure du parser                                                */
/* ------------------------------------------------------------------ */

typedef struct {
    gfa_lexer *lexer;        /* Analyseur lexical associe           */
    ast_node  *ast;          /* AST construit                       */
    int        error_count;  /* Nombre d'erreurs rencontrees        */
    char       error_msg[256];
    int        error_line;

    /* Table des labels pour la resolution GOTO/GOSUB */
    struct {
        char   *name;
        int     ast_node_index;  /* index dans le programme */
    } labels[256];
    int        label_count;
} gfa_parser;

/* ------------------------------------------------------------------ */
/* API du parser                                                      */
/* ------------------------------------------------------------------ */

/*
 * gfa_parser_init - Cree un parser pour le texte source donne.
 */
gfa_parser *gfa_parser_init(const char *source);

/*
 * gfa_parser_free - Libere le parser et l'AST.
 */
void gfa_parser_free(gfa_parser *parser);

/*
 * gfa_parser_parse - Analyse le source et construit l'AST.
 * Retourne la racine de l'AST, ou NULL si erreur.
 */
ast_node *gfa_parser_parse(gfa_parser *parser);

/*
 * gfa_parser_get_error - Retourne le message d'erreur.
 */
const char *gfa_parser_get_error(gfa_parser *parser);

/*
 * gfa_parser_get_error_count - Retourne le nombre d'erreurs.
 */
int gfa_parser_get_error_count(gfa_parser *parser);

#ifdef __cplusplus
}
#endif

#endif /* GFA_PARSER_H */
