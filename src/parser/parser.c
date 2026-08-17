/*
 * parser.c - Implementation du parser GFA Basic 3.5
 * ==================================================
 * Analyseur syntaxique recursif descendant (LL(1)).
 * Chaque instruction GFA est analysee par une fonction dediee.
 *
 * Reference : cahier-des-charges-gfabasic.md, section 4
 */

#include "parser.h"
#include "keywords.h"
#include "os_layer.h"
#include "matrix.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ------------------------------------------------------------------ */
/* Macros et forward declarations                                     */
/* ------------------------------------------------------------------ */

#define PARSER_ERROR(p, msg) do { \
    sprintf((p)->error_msg, "%s", msg); \
    (p)->error_count++; \
    (p)->error_line = gfa_lexer_get_line((p)->lexer); \
} while(0)

static ast_node *parse_program(gfa_parser *parser);
static ast_node *parse_statement(gfa_parser *parser);
static ast_node *parse_line(gfa_parser *parser);
static ast_node *parse_if(gfa_parser *parser);
static ast_node *parse_for(gfa_parser *parser);
static ast_node *parse_while(gfa_parser *parser);
static ast_node *parse_repeat(gfa_parser *parser);
static ast_node *parse_do_loop(gfa_parser *parser);
static ast_node *parse_builtin_call(gfa_parser *parser,
                                    gfa_token_type func_tok);
static ast_node *parse_mat(gfa_parser *parser);
static ast_node *parse_matrix_name(gfa_parser *parser);
static int parse_gfx_args(gfa_parser *parser, ast_node *node,
                          int max);
static ast_node *parse_exit_if(gfa_parser *parser);
static ast_node *parse_line_input_file(gfa_parser *parser);
static ast_node *parse_aes_stmt(gfa_parser *parser,
                               gfa_token_type tok);
static int parse_ident_is(const char *s, const char *w);
static ast_node *parse_select(gfa_parser *parser);
static ast_node *parse_print(gfa_parser *parser);
static ast_node *parse_input(gfa_parser *parser);
static ast_node *parse_open(gfa_parser *parser);
static ast_node *parse_dim(gfa_parser *parser);
static ast_node *parse_data(gfa_parser *parser);
static ast_node *parse_procedure(gfa_parser *parser);
static ast_node *parse_function(gfa_parser *parser);
static ast_node *parse_deffn(gfa_parser *parser);
static ast_node *parse_fn(gfa_parser *parser);
static ast_node *parse_sound_stmt(gfa_parser *parser);
static ast_node *parse_graphics(gfa_parser *parser);
static ast_node *parse_comparison(gfa_parser *parser);
static ast_node *parse_expression(gfa_parser *parser);
static ast_node *parse_simple_expr(gfa_parser *parser);
static ast_node *parse_term(gfa_parser *parser);
static ast_node *parse_factor(gfa_parser *parser);
static ast_node *parse_primary(gfa_parser *parser);

/* ------------------------------------------------------------------ */
/* Initialisation                                                     */
/* ------------------------------------------------------------------ */

gfa_parser *gfa_parser_init(const char *source)
{
    gfa_parser *parser;

    if (source == NULL) return NULL;

    parser = (gfa_parser *)calloc(1, sizeof(gfa_parser));
    if (parser == NULL) return NULL;

    parser->lexer = gfa_lexer_init(source);
    if (parser->lexer == NULL) {
        free(parser);
        return NULL;
    }

    /* Desactiver les abreviations dans le parser :
       'c' doit etre un identifiant, pas CASE */
    gfa_lexer_set_expand(parser->lexer, 0);

    parser->ast = NULL;
    parser->label_count = 0;
    parser->error_count = 0;
    parser->error_line = 0;
    parser->error_msg[0] = '\0';

    return parser;
}

void gfa_parser_free(gfa_parser *parser)
{
    if (parser == NULL) return;

    if (parser->lexer != NULL) {
        gfa_lexer_free(parser->lexer);
        parser->lexer = NULL;
    }
    if (parser->ast != NULL) {
        ast_free(parser->ast);
        parser->ast = NULL;
    }

    free(parser);
}

const char *gfa_parser_get_error(gfa_parser *parser)
{
    if (parser == NULL) return "";
    return parser->error_msg;
}

int gfa_parser_get_error_count(gfa_parser *parser)
{
    if (parser == NULL) return 0;
    return parser->error_count;
}

/* ------------------------------------------------------------------ */
/* Point d'entree du parsing                                          */
/* ------------------------------------------------------------------ */

ast_node *gfa_parser_parse(gfa_parser *parser)
{
    ast_node *program;

    if (parser == NULL) return NULL;

    /* Lire le premier token */
    gfa_lexer_next(parser->lexer);

    program = parse_program(parser);

    parser->ast = program;
    return program;
}

/* ------------------------------------------------------------------ */
/* Programme : suite de lignes                                        */
/* ------------------------------------------------------------------ */

static ast_node *parse_program(gfa_parser *parser)
{
    ast_node *prog;
    gfa_token_type tok;

    prog = ast_create(AST_PROGRAM);

    for (;;) {
        tok = gfa_lexer_current_token(parser->lexer);

        if (tok == TOK_EOF) break;

        /* Ligne vide ? */
        if (tok == TOK_EOL) {
            gfa_lexer_next(parser->lexer);
            continue;
        }

        ast_add_child(prog, parse_line(parser));
    }

    return prog;
}

/* ------------------------------------------------------------------ */
/* Ligne : [numero] instruction [: instruction ...]                   */
/* ------------------------------------------------------------------ */

static ast_node *parse_line(gfa_parser *parser)
{
    ast_node *line;
    ast_node *stmt;
    gfa_token_type tok;

    line = ast_create(AST_STATEMENT_LIST);

    /* Lire les instructions sur la ligne */
    for (;;) {
        tok = gfa_lexer_current_token(parser->lexer);

        if (tok == TOK_EOF || tok == TOK_EOL) {
            break;
        }

        stmt = parse_statement(parser);
        if (stmt != NULL) {
            ast_add_child(line, stmt);
        }

        /* Separateur ":" entre instructions sur la meme ligne */
        if (gfa_lexer_current_token(parser->lexer) == TOK_COLON) {
            gfa_lexer_next(parser->lexer);
            continue;
        }
        break;
    }

    /* Consommer le EOL */
    tok = gfa_lexer_current_token(parser->lexer);
    if (tok == TOK_EOL) {
        gfa_lexer_next(parser->lexer);
    }

    return line;
}

/* ------------------------------------------------------------------ */
/* Dispatch des instructions                                          */
/* ------------------------------------------------------------------ */

static ast_node *parse_statement(gfa_parser *parser)
{
    gfa_token_type tok;

    tok = gfa_lexer_current_token(parser->lexer);
    switch (tok) {

        /* Commentaires */
        case TOK_REM:
            gfa_lexer_skip_to_eol(parser->lexer);
            gfa_lexer_next(parser->lexer);
            return ast_create(AST_REM);

        /* Assignation */
        case TOK_LET:
            gfa_lexer_next(parser->lexer);
            /* fall through - LET est optionnel */
            /* Si on a un identifiant suivi de =, c'est une assignation */
            {
                gfa_token_type next;
                next = gfa_lexer_current_token(parser->lexer);
                if (next == TOK_IDENTIFIER) {
                    ast_node *ident;
                    ast_node *assign;
                    ident = ast_create_ident(AST_ASSIGN,
                        parser->lexer->current.value.ident_name);
                    gfa_lexer_next(parser->lexer);

                    if (gfa_lexer_current_token(parser->lexer) == TOK_EQ) {
                        gfa_lexer_next(parser->lexer);
                        assign = ast_create(AST_ASSIGN);
                        ast_add_child(assign, ident);
                        ast_add_child(assign, parse_expression(parser));
                        return assign;
                    }
                }
                /* Sinon, LET sans assignation - erreur */
                PARSER_ERROR(parser, "Expected variable after LET");
                return NULL;
            }

        /* Controle de flux */
        case TOK_IF:      return parse_if(parser);
        case TOK_FOR:     return parse_for(parser);
        case TOK_WHILE:   return parse_while(parser);
        case TOK_REPEAT:  return parse_repeat(parser);
        case TOK_DO:      return parse_do_loop(parser);
        case TOK_EXIT_IF: return parse_exit_if(parser);
        case TOK_CONT:
            /* CONT : reprise apres STOP (no-op en mode script) */
            gfa_lexer_next(parser->lexer);
            return ast_create_int(AST_CALL, 0);

        /* ============================================================ */
        /* System de fichiers                                           */
        /* ============================================================ */
        case TOK_KILL: case TOK_MKDIR: case TOK_RMDIR: case TOK_CHDIR:
        case TOK_CHDRIVE: case TOK_FILES:
            {
                ast_node *node;
                gfa_lexer_next(parser->lexer);
                node = ast_create(AST_CALL);
                node->value.int_val = (long)tok;
                ast_add_arg(node, parse_expression(parser));
                return node;
            }
        case TOK_FSNEXT:
            /* FSNEXT : statement sans argument */
            {
                ast_node *node;
                gfa_lexer_next(parser->lexer);
                node = ast_create(AST_CALL);
                node->value.int_val = (long)TOK_FSNEXT;
                return node;
            }
        case TOK_FSFIRST:
            /* FSFIRST "masque" : statement (resultat depile au codegen) */
            {
                ast_node *node;
                gfa_lexer_next(parser->lexer);
                node = ast_create(AST_CALL);
                node->value.int_val = (long)TOK_FSFIRST;
                ast_add_arg(node, parse_expression(parser));
                return node;
            }
        case TOK_FGETDTA:
            return parse_builtin_call(parser, tok);
        case TOK_FSETDTA:
            {
                ast_node *node;
                gfa_lexer_next(parser->lexer);
                node = ast_create(AST_CALL);
                node->value.int_val = (long)tok;
                ast_add_arg(node, parse_expression(parser));
                return node;
            }
        case TOK_SEEK: case TOK_RELSEEK:
            {
                ast_node *node;
                gfa_lexer_next(parser->lexer);
                node = ast_create(AST_CALL);
                node->value.int_val = (long)tok;
                if (gfa_lexer_current_token(parser->lexer) == TOK_HASH)
                    gfa_lexer_next(parser->lexer);
                ast_add_arg(node, parse_expression(parser));
                if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                    gfa_lexer_next(parser->lexer);
                    ast_add_arg(node, parse_expression(parser));
                }
                return node;
            }
        case TOK_NAME:
            {
                ast_node *node;
                gfa_lexer_next(parser->lexer);  /* NAME */
                node = ast_create(AST_CALL);
                node->value.int_val = (long)TOK_NAME;
                ast_add_arg(node, parse_expression(parser));  /* ancien */
                if (gfa_lexer_current_token(parser->lexer) == TOK_IDENTIFIER &&
                    parse_ident_is(parser->lexer->current.value.ident_name,
                                   "AS"))
                    gfa_lexer_next(parser->lexer);  /* AS */
                ast_add_arg(node, parse_expression(parser));  /* nouveau */
                return node;
            }
        case TOK_FIELD:
            {
                ast_node *node;
                gfa_lexer_next(parser->lexer);  /* FIELD */
                node = ast_create(AST_CALL);
                node->value.int_val = (long)TOK_FIELD;
                if (gfa_lexer_current_token(parser->lexer) == TOK_HASH)
                    gfa_lexer_next(parser->lexer);
                ast_add_arg(node, parse_expression(parser));  /* canal */
                if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA)
                    gfa_lexer_next(parser->lexer);
                ast_add_arg(node, parse_expression(parser));  /* taille */
                if (gfa_lexer_current_token(parser->lexer) == TOK_IDENTIFIER &&
                    parse_ident_is(parser->lexer->current.value.ident_name,
                                   "AS"))
                    gfa_lexer_next(parser->lexer);  /* AS */
                if (gfa_lexer_current_token(parser->lexer) == TOK_IDENTIFIER) {
                    ast_add_arg(node, ast_create_str(AST_ASSIGN,
                        parser->lexer->current.value.ident_name));
                    gfa_lexer_next(parser->lexer);
                }
                return node;
            }
        case TOK_LSET: case TOK_RSET:
            {
                /* LSET/RSET var$ = expr : assignation simple
                   (l'alignement ne s'applique qu'avec FIELD) */
                ast_node *ident, *assign;
                gfa_lexer_next(parser->lexer);
                if (gfa_lexer_current_token(parser->lexer) == TOK_IDENTIFIER) {
                    ident = ast_create_ident(AST_ASSIGN,
                        parser->lexer->current.value.ident_name);
                    gfa_lexer_next(parser->lexer);
                    if (gfa_lexer_current_token(parser->lexer) == TOK_EQ) {
                        gfa_lexer_next(parser->lexer);
                        assign = ast_create(AST_ASSIGN);
                        assign->left = ident;
                        assign->left->right = parse_expression(parser);
                        return assign;
                    }
                }
                return ast_create_int(AST_CALL, 0);
            }
        case TOK_SGET: case TOK_SPUT:
            {
                ast_node *node;
                gfa_lexer_next(parser->lexer);
                node = ast_create(AST_CALL);
                node->value.int_val = (long)tok;
                if (gfa_lexer_current_token(parser->lexer) == TOK_HASH)
                    gfa_lexer_next(parser->lexer);
                ast_add_arg(node, parse_expression(parser));  /* canal */
                if (tok == TOK_SPUT &&
                    gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                    gfa_lexer_next(parser->lexer);
                    ast_add_arg(node, parse_expression(parser));
                }
                return node;
            }

        /* ============================================================ */
        /* Console + tableaux (tri / INSERT / DELETE)                    */
        /* ============================================================ */
        case TOK_VTAB: case TOK_HTAB:
            {
                ast_node *node;
                gfa_lexer_next(parser->lexer);
                node = ast_create(AST_CALL);
                node->value.int_val = (long)tok;
                ast_add_arg(node, parse_expression(parser));
                return node;
            }
        case TOK_QSORT: case TOK_SSORT:
            {
                ast_node *node, *ident;
                gfa_lexer_next(parser->lexer);
                node = ast_create(AST_QSORT_STMT);
                node->value.int_val = (tok == TOK_SSORT) ? 1 : 0;
                if (gfa_lexer_current_token(parser->lexer) == TOK_IDENTIFIER) {
                    ident = ast_create_ident(AST_ASSIGN,
                        parser->lexer->current.value.ident_name);
                    gfa_lexer_next(parser->lexer);
                    node->left = ident;
                    if (gfa_lexer_current_token(parser->lexer) == TOK_LPAREN) {
                        gfa_lexer_next(parser->lexer);
                        if (gfa_lexer_current_token(parser->lexer) == TOK_RPAREN)
                            gfa_lexer_next(parser->lexer);
                    }
                }
                if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA)
                    gfa_lexer_next(parser->lexer);
                node->body = parse_expression(parser);   /* lo */
                if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA)
                    gfa_lexer_next(parser->lexer);
                node->cond = parse_expression(parser);   /* hi */
                return node;
            }
        case TOK_INSERT:
            {
                ast_node *node, *ident;
                gfa_lexer_next(parser->lexer);
                node = ast_create(AST_INSERT_ELEM);
                if (gfa_lexer_current_token(parser->lexer) == TOK_IDENTIFIER &&
                    gfa_lexer_peek_token(parser->lexer) == TOK_LPAREN) {
                    ident = ast_create_ident(AST_ASSIGN,
                        parser->lexer->current.value.ident_name);
                    gfa_lexer_next(parser->lexer);
                    node->left = ident;
                    gfa_lexer_next(parser->lexer);  /* ( */
                    node->body = parse_expression(parser);   /* idx */
                    if (gfa_lexer_current_token(parser->lexer) == TOK_RPAREN)
                        gfa_lexer_next(parser->lexer);
                    if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA)
                        gfa_lexer_next(parser->lexer);
                    node->cond = parse_expression(parser);   /* val */
                    return node;
                }
                return parse_builtin_call(parser, TOK_INSERT);
            }
        case TOK_DELETE:
            {
                ast_node *node, *ident;
                gfa_lexer_next(parser->lexer);
                node = ast_create(AST_DELETE_ELEM);
                if (gfa_lexer_current_token(parser->lexer) == TOK_IDENTIFIER &&
                    gfa_lexer_peek_token(parser->lexer) == TOK_LPAREN) {
                    ident = ast_create_ident(AST_ASSIGN,
                        parser->lexer->current.value.ident_name);
                    gfa_lexer_next(parser->lexer);
                    node->left = ident;
                    gfa_lexer_next(parser->lexer);  /* ( */
                    node->body = parse_expression(parser);   /* idx */
                    if (gfa_lexer_current_token(parser->lexer) == TOK_RPAREN)
                        gfa_lexer_next(parser->lexer);
                    return node;
                }
                return ast_create_int(AST_CALL, 0);
            }

        /* ============================================================ */
        /* Turtle (DRAW) et matrices (MAT)                               */
        /* ============================================================ */
        case TOK_DRAW:
            {
                ast_node *node;
                gfa_lexer_next(parser->lexer);
                node = ast_create(AST_DRAW);
                node->value.int_val = 0;  /* turtle */
                if (gfa_lexer_current_token(parser->lexer) == TOK_LPAREN) {
                    /* fonction DRAW(n) : interrogation */
                    gfa_lexer_next(parser->lexer);
                    node->body = parse_expression(parser);
                    if (gfa_lexer_current_token(parser->lexer) == TOK_RPAREN)
                        gfa_lexer_next(parser->lexer);
                    node->value.int_val = 1;
                } else {
                    node->body = parse_expression(parser);
                }
                return node;
            }
        case TOK_MAT:
            return parse_mat(parser);

        /* ============================================================ */
        /* Graphismes VDI etendus (emulation ANSI)                       */
        /* Modes GFX_STMT : 10=aline 11=hline 12=rbox 13=pellipse       */
        /*   14=plot 15=fill 16=texte 17=achr 18=setcolor 19=mode       */
        /*   20=polyligne 21=polygone 22=bezier 23=polymark 24=clip     */
        /*   25=getbit 26=putbit                                        */
        /* Les arguments sont stockes dans args[] (ordre source).       */
        /* ============================================================ */
        case TOK_ALINE:
            {
                ast_node *node;
                gfa_lexer_next(parser->lexer);
                node = ast_create(AST_GFX_STMT);
                node->value.int_val = (long)10;
                parse_gfx_args(parser, node, 4);
                return node;
            }
        case TOK_HLINE:
            {
                ast_node *node;
                gfa_lexer_next(parser->lexer);
                node = ast_create(AST_GFX_STMT);
                node->value.int_val = (long)11;
                parse_gfx_args(parser, node, 3);
                return node;
            }
        case TOK_RBOX: case TOK_PRBOX:
            {
                ast_node *node;
                gfa_lexer_next(parser->lexer);
                node = ast_create(AST_GFX_STMT);
                node->value.int_val = (long)12;
                parse_gfx_args(parser, node, 4);
                return node;
            }
        case TOK_PELLIPSE:
            {
                ast_node *node;
                gfa_lexer_next(parser->lexer);
                node = ast_create(AST_GFX_STMT);
                node->value.int_val = (long)13;
                parse_gfx_args(parser, node, 6);
                return node;
            }
        case TOK_POLYLINE: case TOK_POLYFILL: case TOK_POLYMARK:
        case TOK_APOLY: case TOK_CURVE:
            {
                ast_node *node;
                int mode;
                gfa_lexer_next(parser->lexer);
                if (tok == TOK_POLYLINE || tok == TOK_APOLY) mode = 20;
                else if (tok == TOK_POLYFILL) mode = 21;
                else if (tok == TOK_CURVE) mode = 22;
                else mode = 23;
                node = ast_create(AST_GFX_STMT);
                node->value.int_val = (long)mode;
                ast_add_arg(node, parse_expression(parser));  /* n */
                if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                    gfa_lexer_next(parser->lexer);
                    ast_add_arg(node, parse_matrix_name(parser));  /* pts */
                }
                return node;
            }
        case TOK_PLOT:
            {
                ast_node *node;
                gfa_lexer_next(parser->lexer);
                node = ast_create(AST_GFX_STMT);
                node->value.int_val = (long)14;
                parse_gfx_args(parser, node, 2);
                return node;
            }
        case TOK_FILL:
            {
                ast_node *node;
                gfa_lexer_next(parser->lexer);
                node = ast_create(AST_GFX_STMT);
                node->value.int_val = (long)15;
                parse_gfx_args(parser, node, 3);
                return node;
            }
        case TOK_ATEXT: case TOK_TEXT:
            {
                ast_node *node;
                gfa_lexer_next(parser->lexer);
                /* "TEXT" seul : retour en mode texte (mode 0) */
                if (gfa_lexer_current_token(parser->lexer) == TOK_EOL ||
                    gfa_lexer_current_token(parser->lexer) == TOK_EOF ||
                    gfa_lexer_current_token(parser->lexer) == TOK_COLON) {
                    node = ast_create(AST_GFX_STMT);
                    node->value.int_val = (long)19;
                    ast_add_arg(node, ast_create_int(AST_ASSIGN, 0));
                    return node;
                }
                node = ast_create(AST_GFX_STMT);
                node->value.int_val = (long)16;
                parse_gfx_args(parser, node, 3);
                return node;
            }
        case TOK_ACHAR:
            {
                ast_node *node;
                gfa_lexer_next(parser->lexer);
                node = ast_create(AST_GFX_STMT);
                node->value.int_val = (long)17;
                parse_gfx_args(parser, node, 3);
                return node;
            }
        case TOK_SETCOLOR:
            {
                ast_node *node;
                gfa_lexer_next(parser->lexer);
                node = ast_create(AST_GFX_STMT);
                node->value.int_val = (long)18;
                parse_gfx_args(parser, node, 2);
                return node;
            }
        case TOK_MODE:
        case TOK_GRAPHICS:
            {
                ast_node *node;
                gfa_lexer_next(parser->lexer);
                node = ast_create(AST_GFX_STMT);
                node->value.int_val = (long)19;
                parse_gfx_args(parser, node, 1);
                return node;
            }
        case TOK_CLIP: case TOK_ACLIP:
            {
                ast_node *node;
                gfa_lexer_next(parser->lexer);
                node = ast_create(AST_GFX_STMT);
                node->value.int_val = (long)24;
                parse_gfx_args(parser, node, 4);
                return node;
            }
        case TOK_GET:
            /* GET #n[, pos] (fichier) vs GET x1,y1,x2,y2,var$ (graphisme).
               GET est consomme d'abord (un peek avant laisserait un
               token en reserve et casserait la suite). */
            {
                gfa_lexer_next(parser->lexer);  /* GET */
                if (gfa_lexer_current_token(parser->lexer) == TOK_HASH) {
                    ast_node *node;
                    gfa_lexer_next(parser->lexer);  /* # */
                    node = ast_create(AST_CALL);
                    node->value.int_val = (long)TOK_GET;
                    ast_add_arg(node, parse_expression(parser));  /* canal */
                    if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                        gfa_lexer_next(parser->lexer);
                        ast_add_arg(node, parse_expression(parser));  /* pos */
                    }
                    return node;
                }
                {
                    ast_node *node;
                    node = ast_create(AST_GFX_STMT);
                    node->value.int_val = (long)25;
                    parse_gfx_args(parser, node, 5);
                    return node;
                }
            }
        case TOK_PUT:
            /* PUT #n (fichier) vs PUT x,y,var$[,mode] (graphisme) */
            {
                gfa_lexer_next(parser->lexer);  /* PUT */
                if (gfa_lexer_current_token(parser->lexer) == TOK_HASH) {
                    ast_node *node;
                    gfa_lexer_next(parser->lexer);  /* # */
                    node = ast_create(AST_CALL);
                    node->value.int_val = (long)TOK_PUT;
                    ast_add_arg(node, parse_expression(parser));  /* canal */
                    if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                        gfa_lexer_next(parser->lexer);
                        ast_add_arg(node, parse_expression(parser));  /* pos */
                    }
                    return node;
                }
                {
                    ast_node *node;
                    node = ast_create(AST_GFX_STMT);
                    node->value.int_val = (long)26;
                    parse_gfx_args(parser, node, 4);
                    return node;
                }
            }
        /* WINDOW (x0,y0), (x1,y1) : fenetre de coordonnees graphiques */
        case TOK_WINDOW:
            {
                ast_node *node;
                int i;
                gfa_lexer_next(parser->lexer);
                node = ast_create(AST_GFX_STMT);
                node->value.int_val = (long)27;
                for (i = 0; i < 2; i++) {
                    if (i > 0 &&
                        gfa_lexer_current_token(parser->lexer) ==
                        TOK_COMMA)
                        gfa_lexer_next(parser->lexer);
                    if (gfa_lexer_current_token(parser->lexer) ==
                        TOK_LPAREN)
                        gfa_lexer_next(parser->lexer);
                    ast_add_arg(node, parse_expression(parser));
                    if (i < 2 &&
                        gfa_lexer_current_token(parser->lexer) ==
                        TOK_COMMA) {
                        gfa_lexer_next(parser->lexer);
                        ast_add_arg(node, parse_expression(parser));
                    }
                    if (gfa_lexer_current_token(parser->lexer) ==
                        TOK_RPAREN)
                        gfa_lexer_next(parser->lexer);
                }
                return node;
            }

        /* ============================================================ */
        /* AES (GEM) : instructions FORM_x, MENU_x, WIND_x, APPL_x,      */
        /* OBJC_x, RSRC_x — emulees par le runtime (resultat 0).        */
        /* ============================================================ */
        case TOK_APPL_INIT: case TOK_APPL_EXIT: case TOK_APPL_FIND:
        case TOK_APPL_READ: case TOK_APPL_WRITE: case TOK_APPL_TPLAY:
        case TOK_APPL_TRECORD:
        case TOK_FORM_ALERT: case TOK_FORM_BUTTON: case TOK_FORM_CENTER:
        case TOK_FORM_DIAL: case TOK_FORM_DO: case TOK_FORM_ERROR:
        case TOK_FORM_KEYBD: case TOK_FORM_INPUT: case TOK_FORM_INPUT_AS:
        case TOK_MENU_BAR: case TOK_MENU_ICHECK: case TOK_MENU_IENABLE:
        case TOK_MENU_REGISTER: case TOK_MENU_TEXT: case TOK_MENU_TNORMAL:
        case TOK_MENU_KILL: case TOK_MENU_OFF:
        case TOK_WIND_OPEN: case TOK_WIND_CLOSE: case TOK_WIND_DELETE:
        case TOK_WIND_FIND: case TOK_WIND_CREATE: case TOK_WIND_CALC:
        case TOK_WIND_GET: case TOK_WIND_SET: case TOK_WIND_UPDATE:
        case TOK_RSRC_LOAD: case TOK_RSRC_FREE: case TOK_RSRC_GADDR:
        case TOK_RSRC_SADDR: case TOK_RSRC_OBFIX:
        case TOK_OBJC_ADD: case TOK_OBJC_CHANGE: case TOK_OBJC_DELETE:
        case TOK_OBJC_DRAW: case TOK_OBJC_EDIT: case TOK_OBJC_FIND:
        case TOK_OBJC_OFFSET: case TOK_OBJC_ORDER: case TOK_OBJC_ADDMOVE:
        case TOK_OBJC_MOVE: case TOK_OBJC_PICK: case TOK_OBJC_STATE:
        case TOK_OBJC_TNORMAL:
        case TOK_GRAF_DRAGBOX: case TOK_RC_COPY: case TOK_RC_INTERSECT:
        case TOK_RCALL: case TOK_SCRP_READ: case TOK_SCRP_WRITE:
        case TOK_ALERT: case TOK_FILESELECT: case TOK_FSEL_INPUT:
        case TOK_EVNT_MULTI: case TOK_EVNT_MESAG: case TOK_EVNT_KEYBD:
        case TOK_EVNT_MOUSE: case TOK_EVNT_BUTTON: case TOK_EVNT_TIMER:
        case TOK_EVNT_DCLICK:
            return parse_aes_stmt(parser, tok);

        /* ============================================================ */
        /* Fenetres GEM : CLEARW/TITLEW/INFOW/TOPW/GETSIZE/MW_OUT/…     */
        /* sub-ops : 0=clearw 1=titlew 2=infow 3=topw 4=mwout           */
        /*   5=getsize 6=windtab 7=setdraw 8=showm 9=hidem              */
        /* GETSIZE : args[] = 5 identifiants (n,x,y,w,h)                */
        /* Autres : args[] = expressions (ordre source)                 */
        /* ============================================================ */
        case TOK_CLEARW: case TOK_TITLEW: case TOK_INFOW: case TOK_TOPW:
        case TOK_MW_OUT: case TOK_GETSIZE: case TOK_WINDTAB:
        case TOK_SETDRAW: case TOK_SHOWM: case TOK_HIDEM:
            {
                ast_node *node;
                int sub;
                gfa_lexer_next(parser->lexer);
                if (tok == TOK_CLEARW) sub = 0;
                else if (tok == TOK_TITLEW) sub = 1;
                else if (tok == TOK_INFOW) sub = 2;
                else if (tok == TOK_TOPW) sub = 3;
                else if (tok == TOK_MW_OUT) sub = 4;
                else if (tok == TOK_GETSIZE) sub = 5;
                else if (tok == TOK_WINDTAB) sub = 6;
                else if (tok == TOK_SETDRAW) sub = 7;
                else if (tok == TOK_SHOWM) sub = 8;
                else sub = 9;
                node = ast_create(AST_WINDOW);
                node->value.int_val = (long)sub;
                if (sub == 5) {
                    /* GETSIZE n,x,y,w,h : 5 variables cibles */
                    int i;
                    for (i = 0; i < 5; i++) {
                        if (i > 0 &&
                            gfa_lexer_current_token(parser->lexer) ==
                            TOK_COMMA)
                            gfa_lexer_next(parser->lexer);
                        if (gfa_lexer_current_token(parser->lexer) ==
                            TOK_IDENTIFIER) {
                            ast_add_arg(node, ast_create_ident(AST_ASSIGN,
                                parser->lexer->current.value.ident_name));
                            gfa_lexer_next(parser->lexer);
                        }
                    }
                } else {
                    ast_add_arg(node, parse_expression(parser));
                    while (gfa_lexer_current_token(parser->lexer) ==
                           TOK_COMMA) {
                        gfa_lexer_next(parser->lexer);
                        ast_add_arg(node, parse_expression(parser));
                    }
                }
                return node;
            }

        /* ============================================================ */
        /* Directives sans effet (parsing valide, execution no-op)      */
        /* ============================================================ */
        case TOK_CLR:
            {
                ast_node *n;
                gfa_lexer_next(parser->lexer);
                n = ast_create(AST_CLEAR);
                return n;
            }
        case TOK_DEFBIT: case TOK_DEFBYT: case TOK_DEFWRD:
        case TOK_DEFNUM: case TOK_DEFFLT: case TOK_DEFSTR:
        case TOK_DEFDBL:
            {
                /* DEFxxx l1-l2 : declarations de type (ignorees,
                   les types GFA sont determines par le suffixe) */
                gfa_lexer_next(parser->lexer);
                if (gfa_lexer_current_token(parser->lexer) ==
                    TOK_INTEGER) {
                    ast_node *lo = parse_expression(parser);
                    (void)lo;
                    if (gfa_lexer_current_token(parser->lexer) == TOK_MINUS)
                        gfa_lexer_next(parser->lexer);
                    if (gfa_lexer_current_token(parser->lexer) ==
                        TOK_INTEGER)
                        parse_expression(parser);
                }
                return ast_create_int(AST_CALL, 0);
            }
        case TOK_DEBUG: case TOK_RESERVE: case TOK_BASEPAGE:
        case TOK_ABSOLUTE: case TOK_DMACONTROL: case TOK_DMASOUND:
        case TOK_EXEC: case TOK_LLIST: case TOK_MERGE: case TOK_RENUM:
        case TOK_AUTO:
            {
                ast_node *node;
                node = ast_create(AST_CALL);
                node->value.int_val = (long)tok;
                gfa_lexer_next(parser->lexer);
                /* arguments optionnels (ex: DMACONTROL 0) */
                while (gfa_lexer_current_token(parser->lexer) != TOK_EOL &&
                       gfa_lexer_current_token(parser->lexer) != TOK_EOF &&
                       gfa_lexer_current_token(parser->lexer) != TOK_COLON) {
                    ast_add_arg(node, parse_expression(parser));
                    if (gfa_lexer_current_token(parser->lexer) ==
                        TOK_COMMA)
                        gfa_lexer_next(parser->lexer);
                    else
                        break;
                }
                return node;
            }

        case TOK_SELECT:  return parse_select(parser);
        case TOK_GOTO:
            {
                ast_node *node;
                const char *label_name;
                gfa_token_type ltok;
                node = ast_create(AST_GOTO);
                gfa_lexer_next(parser->lexer);
                label_name = NULL;
                /* value est une union : tester le type AVANT de lire
                   ident_name (sur un entier = entier reinterprete) */
                ltok = parser->lexer->current.type;
                if (ltok == TOK_INTEGER) {
                    char buf[32];
                    sprintf(buf, "%ld", parser->lexer->current.value.int_value);
                    node->value.ident = os_strdup(buf);
                    node->has_ident = 1;
                } else if (ltok == TOK_IDENTIFIER || ltok == TOK_LABEL) {
                    label_name = parser->lexer->current.value.ident_name;
                } else {
                    label_name = gfa_keyword_get_name(ltok);
                }
                if (label_name != NULL && label_name[0] != '\0') {
                    node->value.ident = os_strdup(label_name);
                    node->has_ident = 1;
                }
                gfa_lexer_next(parser->lexer);
                return node;
            }
        case TOK_GOSUB:
            {
                ast_node *node;
                const char *label_name;
                gfa_token_type ltok;
                node = ast_create(AST_GOSUB);
                gfa_lexer_next(parser->lexer);
                label_name = NULL;
                ltok = parser->lexer->current.type;
                if (ltok == TOK_INTEGER) {
                    char buf[32];
                    sprintf(buf, "%ld", parser->lexer->current.value.int_value);
                    node->value.ident = os_strdup(buf);
                    node->has_ident = 1;
                } else if (ltok == TOK_IDENTIFIER || ltok == TOK_LABEL) {
                    label_name = parser->lexer->current.value.ident_name;
                } else {
                    label_name = gfa_keyword_get_name(ltok);
                }
                if (label_name != NULL && label_name[0] != '\0') {
                    node->value.ident = os_strdup(label_name);
                    node->has_ident = 1;
                }
                gfa_lexer_next(parser->lexer);
                return node;
            }
        case TOK_RETURN:
            {
                ast_node *node;
                node = ast_create(AST_RETURN);
                gfa_lexer_next(parser->lexer);  /* consommer RETURN */
                /* RETURN [expression] pour FUNCTION */
                if (gfa_lexer_current_token(parser->lexer) != TOK_EOL &&
                    gfa_lexer_current_token(parser->lexer) != TOK_EOF &&
                    gfa_lexer_current_token(parser->lexer) != TOK_ELSE &&
                    gfa_lexer_current_token(parser->lexer) != TOK_ENDIF &&
                    gfa_lexer_current_token(parser->lexer) != TOK_ENDFUNC) {
                    ast_add_child(node, parse_expression(parser));
                }
                return node;
            }
        case TOK_STOP:
            gfa_lexer_next(parser->lexer);
            return ast_create(AST_STOP);
        case TOK_END:
            gfa_lexer_next(parser->lexer);
            return ast_create(AST_END);
        case TOK_QUIT:
            {
                ast_node *node;
                node = ast_create(AST_QUIT);
                gfa_lexer_next(parser->lexer);
                /* Code de sortie optionnel : QUIT n */
                if (gfa_lexer_current_token(parser->lexer) != TOK_EOL &&
                    gfa_lexer_current_token(parser->lexer) != TOK_EOF)
                    node->left = parse_expression(parser);
                return node;
            }

        /* I/O */
        case TOK_PRINT:    return parse_print(parser);
        case TOK_INPUT:    return parse_input(parser);
        case TOK_LINE_INPUT:
            {
                ast_node *node;
                node = ast_create(AST_LINE_INPUT);
                gfa_lexer_next(parser->lexer);
                /* Lire la variable chaine */
                if (parser->lexer->current.type == TOK_IDENTIFIER) {
                    ast_add_child(node,
                        ast_create_ident(AST_ASSIGN,
                            parser->lexer->current.value.ident_name));
                }
                gfa_lexer_next(parser->lexer);
                return node;
            }
        case TOK_CLS:
            gfa_lexer_next(parser->lexer);
            return ast_create(AST_CLS);
        case TOK_LOCATE:
            {
                ast_node *node;
                node = ast_create(AST_LOCATE);
                gfa_lexer_next(parser->lexer);
                ast_add_child(node, parse_expression(parser)); /* x */
                if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                    gfa_lexer_next(parser->lexer);
                }
                ast_add_child(node, parse_expression(parser)); /* y */
                return node;
            }

        /* Fichiers */
        case TOK_OPEN:     return parse_open(parser);
        case TOK_CLOSE:
            {
                ast_node *node;
                node = ast_create(AST_CLOSE);
                gfa_lexer_next(parser->lexer);
                /* #n optionnel */
                if (parser->lexer->current.type == TOK_HASH) {
                    gfa_lexer_next(parser->lexer);
                    ast_add_child(node, parse_expression(parser));
                }
                return node;
            }
        case TOK_OPENW:
            {
                ast_node *node;
                node = ast_create(AST_OPENW);
                gfa_lexer_next(parser->lexer);
                ast_add_child(node, parse_expression(parser));
                return node;
            }
        case TOK_CLOSEW:
            {
                ast_node *node;
                node = ast_create(AST_CLOSEW);
                gfa_lexer_next(parser->lexer);
                return node;
            }

        /* Definitions */
        case TOK_DIM:      return parse_dim(parser);
        case TOK_ERASE:
            {
                ast_node *node;
                gfa_lexer_next(parser->lexer);
                node = ast_create(AST_ERASE);
                while (gfa_lexer_current_token(parser->lexer) ==
                       TOK_IDENTIFIER) {
                    ast_add_child(node, ast_create_ident(AST_ASSIGN,
                        parser->lexer->current.value.ident_name));
                    gfa_lexer_next(parser->lexer);
                    if (gfa_lexer_current_token(parser->lexer) ==
                        TOK_COMMA)
                        gfa_lexer_next(parser->lexer);
                }
                return node;
            }
        case TOK_CLEAR:
            gfa_lexer_next(parser->lexer);
            return ast_create(AST_CLEAR);
        case TOK_PROCEDURE: return parse_procedure(parser);
        case TOK_PROC:      return parse_procedure(parser);
        case TOK_FUNCTION:  return parse_function(parser);
        case TOK_DEFFN:     return parse_deffn(parser);
        case TOK_FN:        return parse_fn(parser);
        case TOK_ENDFUNC:
            gfa_lexer_next(parser->lexer);
            return ast_create(AST_ENDFUNC);
        case TOK_ENDPROC:
            gfa_lexer_next(parser->lexer);
            return ast_create_int(AST_CALL, 0);
        case TOK_DATA:     return parse_data(parser);
        case TOK_READ:
            {
                ast_node *node;
                node = ast_create(AST_READ);
                gfa_lexer_next(parser->lexer);
                while (parser->lexer->current.type == TOK_IDENTIFIER) {
                    ast_add_child(node,
                        ast_create_ident(AST_ASSIGN,
                            parser->lexer->current.value.ident_name));
                    gfa_lexer_next(parser->lexer);
                    if (parser->lexer->current.type == TOK_COMMA) {
                        gfa_lexer_next(parser->lexer);
                    }
                }
                return node;
            }
        case TOK_RESTORE:
            {
                ast_node *node;
                node = ast_create(AST_RESTORE);
                gfa_lexer_next(parser->lexer);  /* RESTORE */
                if (gfa_lexer_current_token(parser->lexer) == TOK_IDENTIFIER) {
                    ast_add_child(node,
                        ast_create_ident(AST_ASSIGN,
                            parser->lexer->current.value.ident_name));
                    gfa_lexer_next(parser->lexer);
                }
                return node;
            }

        case TOK_LOCAL:
            {
                ast_node *node;
                node = ast_create(AST_LOCAL);
                gfa_lexer_next(parser->lexer);  /* LOCAL */
                while (gfa_lexer_current_token(parser->lexer) == TOK_IDENTIFIER) {
                    ast_add_child(node,
                        ast_create_ident(AST_ASSIGN,
                            parser->lexer->current.value.ident_name));
                    gfa_lexer_next(parser->lexer);
                    if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                        gfa_lexer_next(parser->lexer);
                    } else {
                        break;
                    }
                }
                return node;
            }

        /* Son */
        case TOK_SOUND:    return parse_sound_stmt(parser);
        case TOK_BEEP:
            gfa_lexer_next(parser->lexer);
            return ast_create(AST_BEEP);

        /* Evenements */
        case TOK_EVERY:
            {
                ast_node *node;
                node = ast_create(AST_EVERY);
                gfa_lexer_next(parser->lexer);
                ast_add_child(node, parse_expression(parser));
                if (gfa_lexer_current_token(parser->lexer) == TOK_GOSUB) {
                    gfa_lexer_next(parser->lexer);
                    if (parser->lexer->current.type == TOK_IDENTIFIER) {
                        ast_add_child(node, ast_create_ident(AST_ASSIGN,
                            parser->lexer->current.value.ident_name));
                    }
                    gfa_lexer_next(parser->lexer);
                }
                return node;
            }
        case TOK_AFTER:
            {
                ast_node *node;
                node = ast_create(AST_AFTER);
                gfa_lexer_next(parser->lexer);
                ast_add_child(node, parse_expression(parser));
                if (gfa_lexer_current_token(parser->lexer) == TOK_GOSUB) {
                    gfa_lexer_next(parser->lexer);
                    if (parser->lexer->current.type == TOK_IDENTIFIER) {
                        ast_add_child(node, ast_create_ident(AST_ASSIGN,
                            parser->lexer->current.value.ident_name));
                    }
                    gfa_lexer_next(parser->lexer);
                }
                return node;
            }
        case TOK_ON:
            {
                gfa_lexer_next(parser->lexer);  /* consommer ON */
                if (gfa_lexer_current_token(parser->lexer) == TOK_ERROR) {
                    ast_node *node;
                    const char *label_name;
                    node = ast_create(AST_ON_ERROR);
                    gfa_lexer_next(parser->lexer);  /* consommer ERROR */
                    if (gfa_lexer_current_token(parser->lexer) == TOK_GOSUB ||
                        gfa_lexer_current_token(parser->lexer) == TOK_GOTO) {
                        gfa_lexer_next(parser->lexer);
                        /* Accepter tout token comme nom de label
                           (ex : "err" est le mots-cle TOK_ERR) */
                        label_name = NULL;
                        if (parser->lexer->current.value.ident_name != NULL) {
                            label_name = parser->lexer->current.value.ident_name;
                        } else {
                            label_name = gfa_keyword_get_name(
                                parser->lexer->current.type);
                        }
                        if (label_name != NULL && label_name[0] != '\0') {
                            ast_add_child(node, ast_create_ident(AST_ASSIGN,
                                label_name));
                        }
                        gfa_lexer_next(parser->lexer);
                    }
                    return node;
                }
                if (gfa_lexer_current_token(parser->lexer) == TOK_KEY) {
                    /* ON KEY GOSUB n : activateur de handler touches */
                    ast_node *node;
                    node = ast_create(AST_CALL);
                    node->value.int_val = (long)TOK_ON_KEY;
                    gfa_lexer_next(parser->lexer);  /* consommer KEY */
                    if (gfa_lexer_current_token(parser->lexer) ==
                        TOK_GOSUB) {
                        gfa_lexer_next(parser->lexer);
                        ast_add_arg(node, parse_expression(parser));
                    }
                    return node;
                }
                if (gfa_lexer_current_token(parser->lexer) == TOK_ON_BREAK) {
                    ast_node *node;
                    node = ast_create(AST_ON_BREAK);
                    gfa_lexer_next(parser->lexer);  /* consommer BREAK */
                    if (gfa_lexer_current_token(parser->lexer) == TOK_GOSUB) {
                        gfa_lexer_next(parser->lexer);
                        if (parser->lexer->current.type == TOK_IDENTIFIER) {
                            ast_add_child(node, ast_create_ident(AST_ASSIGN,
                                parser->lexer->current.value.ident_name));
                        }
                        gfa_lexer_next(parser->lexer);
                    } else if (gfa_lexer_current_token(parser->lexer) ==
                               TOK_CONT) {
                        /* ON BREAK CONT : continue apres Ctrl+C */
                        gfa_lexer_next(parser->lexer);
                    }
                    return node;
                }
                /* ON expr GOTO/GOSUB (ON x GOTO/GOSUB label, ...) */
                {
                    ast_node *node;
                    ast_node *expr_node;
                    expr_node = parse_expression(parser);
                    if (gfa_lexer_current_token(parser->lexer) == TOK_GOTO ||
                        gfa_lexer_current_token(parser->lexer) == TOK_GOSUB) {
                        node = ast_create(AST_ON_GOTO_GOSUB);
                        node->value.int_val = (gfa_lexer_current_token(parser->lexer) == TOK_GOSUB) ? 1 : 0;
                        gfa_lexer_next(parser->lexer);
                        ast_add_child(node, expr_node);
                        while (gfa_lexer_current_token(parser->lexer) == TOK_IDENTIFIER) {
                            ast_add_child(node,
                                ast_create_ident(AST_ASSIGN,
                                    parser->lexer->current.value.ident_name));
                            gfa_lexer_next(parser->lexer);
                            if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                                gfa_lexer_next(parser->lexer);
                            } else break;
                        }
                        return node;
                    }
                    PARSER_ERROR(parser, "Expected ERROR, BREAK, GOTO or GOSUB after ON");
                    return NULL;
                }
            }
        case TOK_ERROR:
            {
                ast_node *node;
                node = ast_create(AST_ERROR);
                gfa_lexer_next(parser->lexer);
                ast_add_child(node, parse_expression(parser));
                return node;
            }

        /* Graphisme */
        case TOK_COLOR:
            {
                ast_node *node;
                node = ast_create(AST_COLOR);
                gfa_lexer_next(parser->lexer);
                ast_add_child(node, parse_expression(parser));
                /* Couleur de fond optionnelle */
                if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                    gfa_lexer_next(parser->lexer);
                    ast_add_child(node, parse_expression(parser));
                }
                return node;
            }
        case TOK_LINE_TOK:
            /* "LINE INPUT [#n,] var$" (console/fichier) ou
               "LINE x1,y1,x2,y2[,...]" (graphisme VDI).
               LINE est consomme d'abord : un peek avant casserait
               la consommation suivante de parse_graphics. */
            {
                gfa_lexer_next(parser->lexer);  /* LINE */
                if (gfa_lexer_current_token(parser->lexer) == TOK_INPUT)
                    return parse_line_input_file(parser);
                {
                    ast_node *node;
                    node = ast_create(AST_LINE);
                    while (gfa_lexer_current_token(parser->lexer) != TOK_EOL &&
                           gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
                        ast_add_child(node, parse_expression(parser));
                        if (gfa_lexer_current_token(parser->lexer) ==
                            TOK_COMMA)
                            gfa_lexer_next(parser->lexer);
                        else
                            break;
                    }
                    return node;
                }
            }
        case TOK_BOX:
        case TOK_PBOX:
        case TOK_CIRCLE_TOK:
        case TOK_PCIRCLE:
            return parse_graphics(parser);

        /* Debug */
        case TOK_TRON:
            gfa_lexer_next(parser->lexer);
            return ast_create(AST_TRON);
        case TOK_TROFF:
            gfa_lexer_next(parser->lexer);
            return ast_create(AST_TROFF);

        /* Memoire */
        case TOK_POKE:
            {
                ast_node *node;
                node = ast_create(AST_POKE);
                gfa_lexer_next(parser->lexer);
                ast_add_child(node, parse_expression(parser)); /* addr */
                if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                    gfa_lexer_next(parser->lexer);
                }
                ast_add_child(node, parse_expression(parser)); /* val */
                return node;
            }
        case TOK_DPOKE:
            {
                ast_node *node;
                node = ast_create(AST_DPOKE);
                gfa_lexer_next(parser->lexer);
                ast_add_child(node, parse_expression(parser));
                if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                    gfa_lexer_next(parser->lexer);
                }
                ast_add_child(node, parse_expression(parser));
                return node;
            }
        case TOK_LPOKE:
            {
                ast_node *node;
                node = ast_create(AST_LPOKE);
                gfa_lexer_next(parser->lexer);
                ast_add_child(node, parse_expression(parser));
                if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                    gfa_lexer_next(parser->lexer);
                }
                ast_add_child(node, parse_expression(parser));
                return node;
            }
        case TOK_SPOKE:
            {
                ast_node *node;
                node = ast_create(AST_SPOKE);
                gfa_lexer_next(parser->lexer);
                ast_add_child(node, parse_expression(parser));
                if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                    gfa_lexer_next(parser->lexer);
                }
                ast_add_child(node, parse_expression(parser));
                return node;
            }
        case TOK_SDPOKE:
            {
                ast_node *node;
                node = ast_create(AST_SDPOKE);
                gfa_lexer_next(parser->lexer);
                ast_add_child(node, parse_expression(parser));
                if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                    gfa_lexer_next(parser->lexer);
                }
                ast_add_child(node, parse_expression(parser));
                return node;
            }
        case TOK_SLPOKE:
            {
                ast_node *node;
                node = ast_create(AST_SLPOKE);
                gfa_lexer_next(parser->lexer);
                ast_add_child(node, parse_expression(parser));
                if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                    gfa_lexer_next(parser->lexer);
                }
                ast_add_child(node, parse_expression(parser));
                return node;
            }

        /* Fenetres */
        case TOK_DEFFILL:
            {
                ast_node *node;
                node = ast_create(AST_DEFFILL);
                gfa_lexer_next(parser->lexer);
                while (gfa_lexer_current_token(parser->lexer) != TOK_EOL &&
                       gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
                    ast_add_child(node, parse_expression(parser));
                    if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                        gfa_lexer_next(parser->lexer);
                    } else {
                        break;
                    }
                }
                return node;
            }
        case TOK_DEFLINE:
            {
                ast_node *node;
                node = ast_create(AST_DEFLINE);
                gfa_lexer_next(parser->lexer);
                while (gfa_lexer_current_token(parser->lexer) != TOK_EOL &&
                       gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
                    ast_add_child(node, parse_expression(parser));
                    if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                        gfa_lexer_next(parser->lexer);
                    } else {
                        break;
                    }
                }
                return node;
            }
        case TOK_DEFTEXT:
        case TOK_DEFMOUSE:
        case TOK_DEFMARK:
            {
                ast_node *node;
                node = ast_create(
                    (tok == TOK_DEFTEXT) ? AST_DEFTEXT :
                    (tok == TOK_DEFMOUSE) ? AST_DEFMOUSE : AST_DEFMARK);
                gfa_lexer_next(parser->lexer);
                while (gfa_lexer_current_token(parser->lexer) != TOK_EOL &&
                       gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
                    ast_add_child(node, parse_expression(parser));
                    if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                        gfa_lexer_next(parser->lexer);
                    } else {
                        break;
                    }
                }
                return node;
            }

        /* Etiquettes */
        case TOK_LABEL:
            {
                ast_node *node;
                const char *lbl;
                lbl = parser->lexer->current.value.ident_name;
                node = ast_create_ident(AST_LABEL, lbl);
                /* Enregistrer le label */
                if (parser->label_count < 256 && lbl != NULL) {
                    parser->labels[parser->label_count].name = os_strdup(lbl);
                    parser->labels[parser->label_count].ast_node_index = 0;
                    parser->label_count++;
                }
                gfa_lexer_next(parser->lexer);
                return node;
            }

        /* Etiquette numerique (numero de ligne) : "900" ou "900: ..." */
        case TOK_INTEGER:
            {
                gfa_token_type nxt;
                nxt = gfa_lexer_peek_token(parser->lexer);
                if (nxt == TOK_EOL || nxt == TOK_EOF || nxt == TOK_COLON) {
                    ast_node *node;
                    char buf[32];
                    sprintf(buf, "%ld", parser->lexer->current.value.int_value);
                    node = ast_create_ident(AST_LABEL, buf);
                    if (parser->label_count < 256) {
                        parser->labels[parser->label_count].name =
                            os_strdup(buf);
                        parser->labels[parser->label_count].ast_node_index = 0;
                        parser->label_count++;
                    }
                    gfa_lexer_next(parser->lexer);  /* consommer le numero */
                    if (gfa_lexer_current_token(parser->lexer) == TOK_COLON) {
                        gfa_lexer_next(parser->lexer);  /* consommer ':' */
                    }
                    return node;
                }
                /* Sinon : expression debutant par un entier */
                return parse_expression(parser);
            }

        /* Identifiant = expression (assignation implicite) */
        case TOK_IDENTIFIER:
            {
                ast_node *ident;
                ast_node *assign;
                /* DEF FN nom(...) = expr : forme alternative de FN */
                if (parse_ident_is(parser->lexer->current.value.ident_name,
                                   "DEF")) {
                    gfa_lexer_next(parser->lexer);  /* consommer DEF */
                    if (gfa_lexer_current_token(parser->lexer) == TOK_FN)
                        return parse_fn(parser);
                    /* DEF sans FN : instruction inconnue, no-op tolerant */
                    return ast_create_int(AST_CALL, 0);
                }
                ident = ast_create_ident(AST_ASSIGN,
                    parser->lexer->current.value.ident_name);
                gfa_lexer_next(parser->lexer);

                if (gfa_lexer_current_token(parser->lexer) == TOK_EQ) {
                    gfa_lexer_next(parser->lexer);
                    assign = ast_create(AST_ASSIGN);
                    ast_add_child(assign, ident);
                    ast_add_child(assign, parse_expression(parser));
                    return assign;
                }

                /* Appel de procedure/fonction ? */
                if (gfa_lexer_current_token(parser->lexer) == TOK_LPAREN) {
                    ast_node *call;
                    call = ast_create(AST_CALL);
                    call->value.ident = ident->value.ident;
                    call->has_ident = ident->has_ident;
                    ident->has_ident = 0;  /* Transfere la propriete */
                    free(ident);
                    gfa_lexer_next(parser->lexer);
                    /* Arguments */
                    while (gfa_lexer_current_token(parser->lexer) != TOK_RPAREN &&
                           gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
                        ast_add_arg(call, parse_expression(parser));
                        if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                            gfa_lexer_next(parser->lexer);
                        }
                    }
                    if (gfa_lexer_current_token(parser->lexer) == TOK_RPAREN) {
                        gfa_lexer_next(parser->lexer);
                    }
                    /* Apres ident(...), verifier si c'est un tableau : ident(...) = expr */
                    if (gfa_lexer_current_token(parser->lexer) == TOK_EQ) {
                        assign = ast_create(AST_ASSIGN);
                        gfa_lexer_next(parser->lexer);
                        ast_add_child(assign, call);
                        ast_add_child(assign, parse_expression(parser));
                        return assign;
                    }
                    return call;
                }

                /* Appel de PROCEDURE sans parentheses : nom arg1, arg2 */
                if (gfa_lexer_current_token(parser->lexer) != TOK_EOL &&
                    gfa_lexer_current_token(parser->lexer) != TOK_EOF &&
                    gfa_lexer_current_token(parser->lexer) != TOK_REM &&
                    gfa_lexer_current_token(parser->lexer) != TOK_COLON) {
                    ast_node *call;
                    call = ast_create(AST_CALL);
                    call->value.ident = ident->value.ident;
                    call->has_ident = ident->has_ident;
                    ident->has_ident = 0;
                    free(ident);
                    while (gfa_lexer_current_token(parser->lexer) != TOK_EOL &&
                           gfa_lexer_current_token(parser->lexer) != TOK_EOF &&
                           gfa_lexer_current_token(parser->lexer) != TOK_REM &&
                           gfa_lexer_current_token(parser->lexer) != TOK_COLON) {
                        ast_add_arg(call, parse_expression(parser));
                        if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                            gfa_lexer_next(parser->lexer);
                        } else {
                            break;
                        }
                    }
                    return call;
                }

                return ident;
            }

        /* Appel via @ (synonyme GOSUB) */
        case TOK_AT:
            {
                ast_node *node;
                const char *label_name;
                gfa_token_type ltok;
                node = ast_create(AST_GOSUB);
                gfa_lexer_next(parser->lexer);
                label_name = NULL;
                ltok = parser->lexer->current.type;
                if (ltok == TOK_INTEGER) {
                    char buf[32];
                    sprintf(buf, "%ld", parser->lexer->current.value.int_value);
                    node->value.ident = os_strdup(buf);
                    node->has_ident = 1;
                } else if (ltok == TOK_IDENTIFIER || ltok == TOK_LABEL) {
                    label_name = parser->lexer->current.value.ident_name;
                } else {
                    label_name = gfa_keyword_get_name(ltok);
                }
                if (label_name != NULL && label_name[0] != '\0') {
                    node->value.ident = os_strdup(label_name);
                    node->has_ident = 1;
                }
                gfa_lexer_next(parser->lexer);
                return node;
            }

        case TOK_VOID:
        case TOK_TILDE:
            {
                ast_node *node;
                node = ast_create((tok == TOK_VOID) ? AST_VOID : AST_TILDE);
                gfa_lexer_next(parser->lexer);
                ast_add_child(node, parse_expression(parser));
                return node;
            }

        case TOK_MONITOR:
            gfa_lexer_next(parser->lexer);
            return ast_create_int(AST_CALL, 0);

        case TOK_SYSTEM:
        case TOK_CHAIN:
        case TOK_RUN:
        case TOK_NEW:
            {
                ast_node *node;
                node = ast_create(AST_CALL);
                node->value.int_val = (long)tok;
                gfa_lexer_next(parser->lexer);
                /* SYSTEM "cmd" / CHAIN "prog" : argument optionnel */
                if (gfa_lexer_current_token(parser->lexer) != TOK_EOL &&
                    gfa_lexer_current_token(parser->lexer) != TOK_EOF &&
                    gfa_lexer_current_token(parser->lexer) != TOK_COLON)
                    ast_add_arg(node, parse_expression(parser));
                return node;
            }

        case TOK_PAUSE:
            {
                ast_node *node;
                node = ast_create(AST_CALL);
                node->value.int_val = (long)TOK_PAUSE;
                gfa_lexer_next(parser->lexer);
                /* PAUSE [n] : argument optionnel */
                if (gfa_lexer_current_token(parser->lexer) != TOK_EOL &&
                    gfa_lexer_current_token(parser->lexer) != TOK_EOF &&
                    gfa_lexer_current_token(parser->lexer) != TOK_COLON)
                    ast_add_arg(node, parse_expression(parser));
                return node;
            }
        case TOK_DELAY:
            {
                ast_node *node;
                node = ast_create(AST_CALL);
                node->value.int_val = (long)tok;
                gfa_lexer_next(parser->lexer);
                ast_add_arg(node, parse_expression(parser));
                return node;
            }

        /* KEY n, chaine$ : defini un buffer de touches (emule : le
           runtime conserve la derniere definition). */
        case TOK_KEY:
        /* CONOUT / CONOUTI chaine$ : ecriture console sans saut de ligne */
        case TOK_CONOUT:
        case TOK_CONOUTI:
        /* CONIN : lit un caractere console (fonction et instruction) */
        case TOK_CONIN:
            {
                ast_node *node;
                node = ast_create(AST_CALL);
                node->value.int_val = (long)tok;
                gfa_lexer_next(parser->lexer);
                if (gfa_lexer_current_token(parser->lexer) != TOK_EOL &&
                    gfa_lexer_current_token(parser->lexer) != TOK_EOF &&
                    gfa_lexer_current_token(parser->lexer) != TOK_COLON)
                    ast_add_arg(node, parse_expression(parser));
                if (tok == TOK_KEY &&
                    gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                    gfa_lexer_next(parser->lexer);
                    ast_add_arg(node, parse_expression(parser));
                }
                return node;
            }

        case TOK_KEYPRESS:
        case TOK_KEYPAD:
            {
                ast_node *node;
                node = ast_create(AST_CALL);
                node->value.int_val = (long)tok;
                gfa_lexer_next(parser->lexer);
                if (gfa_lexer_current_token(parser->lexer) == TOK_LPAREN) {
                    gfa_lexer_next(parser->lexer);
                    while (gfa_lexer_current_token(parser->lexer) != TOK_RPAREN &&
                           gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
                        ast_add_arg(node, parse_expression(parser));
                        if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                            gfa_lexer_next(parser->lexer);
                        }
                    }
                    if (gfa_lexer_current_token(parser->lexer) == TOK_RPAREN) {
                        gfa_lexer_next(parser->lexer);
                    }
                }
                return node;
            }

        case TOK_RANDOMIZE:
            {
                ast_node *node;
                node = ast_create(AST_CALL);
                node->value.int_val = (long)tok;
                gfa_lexer_next(parser->lexer);
                ast_add_arg(node, parse_expression(parser));
                return node;
            }

        case TOK_ARRAYFILL:
            {
                ast_node *node, *ident;
                gfa_lexer_next(parser->lexer);
                node = ast_create(AST_ARRAYFILL);
                if (gfa_lexer_current_token(parser->lexer) == TOK_IDENTIFIER) {
                    ident = ast_create_ident(AST_ASSIGN,
                        parser->lexer->current.value.ident_name);
                    gfa_lexer_next(parser->lexer);
                    node->left = ident;
                    if (gfa_lexer_current_token(parser->lexer) == TOK_LPAREN) {
                        gfa_lexer_next(parser->lexer);
                        if (gfa_lexer_current_token(parser->lexer) == TOK_RPAREN)
                            gfa_lexer_next(parser->lexer);
                    }
                }
                if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA)
                    gfa_lexer_next(parser->lexer);
                node->body = parse_expression(parser);   /* valeur */
                return node;
            }

        case TOK_OPTION_BASE:
            {
                ast_node *node;
                ast_node *expr;
                node = ast_create(AST_OPTION_BASE);
                gfa_lexer_next(parser->lexer);  /* OPTION */
                /* "BASE" facultatif (GFA : OPTION BASE 0/1, OPTION 0/1) */
                if (gfa_lexer_current_token(parser->lexer) == TOK_IDENTIFIER &&
                    parse_ident_is(parser->lexer->current.value.ident_name,
                                   "BASE")) {
                    gfa_lexer_next(parser->lexer);
                }
                expr = parse_expression(parser);
                node->left = expr;
                return node;
            }

        case TOK_BLOAD:
            {
                ast_node *node;
                node = ast_create(AST_BLOAD);
                gfa_lexer_next(parser->lexer);
                ast_add_child(node, parse_expression(parser));  /* filename */
                if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                    gfa_lexer_next(parser->lexer);
                }
                ast_add_child(node, parse_expression(parser));  /* addr */
                return node;
            }
        case TOK_BSAVE:
            {
                ast_node *node;
                node = ast_create(AST_BSAVE);
                gfa_lexer_next(parser->lexer);
                ast_add_child(node, parse_expression(parser));  /* filename */
                if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                    gfa_lexer_next(parser->lexer);
                }
                ast_add_child(node, parse_expression(parser));  /* start */
                if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                    gfa_lexer_next(parser->lexer);
                }
                ast_add_child(node, parse_expression(parser));  /* end */
                return node;
            }
        case TOK_BGET:
            {
                ast_node *node;
                node = ast_create(AST_BGET);
                gfa_lexer_next(parser->lexer);
                if (gfa_lexer_current_token(parser->lexer) == TOK_HASH) {
                    gfa_lexer_next(parser->lexer);
                }
                ast_add_child(node, parse_expression(parser));  /* channel */
                if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                    gfa_lexer_next(parser->lexer);
                }
                ast_add_child(node, parse_expression(parser));  /* addr */
                if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                    gfa_lexer_next(parser->lexer);
                }
                ast_add_child(node, parse_expression(parser));  /* count */
                return node;
            }
        case TOK_BPUT:
            {
                ast_node *node;
                node = ast_create(AST_BPUT);
                gfa_lexer_next(parser->lexer);
                if (gfa_lexer_current_token(parser->lexer) == TOK_HASH) {
                    gfa_lexer_next(parser->lexer);
                }
                ast_add_child(node, parse_expression(parser));  /* channel */
                if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                    gfa_lexer_next(parser->lexer);
                }
                ast_add_child(node, parse_expression(parser));  /* addr */
                if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                    gfa_lexer_next(parser->lexer);
                }
                ast_add_child(node, parse_expression(parser));  /* count */
                return node;
            }
        case TOK_SWAP:
            {
                ast_node *node;
                node = ast_create(AST_SWAP);
                gfa_lexer_next(parser->lexer);
                ast_add_child(node, parse_primary(parser));  /* var1 */
                if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                    gfa_lexer_next(parser->lexer);
                }
                ast_add_child(node, parse_primary(parser));  /* var2 */
                return node;
            }
        case TOK_FATAL:
            {
                ast_node *node;
                node = ast_create(AST_FATAL);
                gfa_lexer_next(parser->lexer);
                ast_add_child(node, parse_expression(parser));  /* error code */
                return node;
            }
        case TOK_RESUME:
            {
                ast_node *node;
                node = ast_create(AST_RESUME);
                gfa_lexer_next(parser->lexer);
                /* Optional NEXT */
                if (gfa_lexer_current_token(parser->lexer) == TOK_NEXT) {
                    ast_node *flag;
                    flag = ast_create_int(AST_ASSIGN, 1);  /* flag = NEXT */
                    ast_add_child(node, flag);
                    gfa_lexer_next(parser->lexer);
                }
                return node;
            }
        case TOK_SETTIME:
            {
                ast_node *node;
                node = ast_create(AST_SETTIME);
                gfa_lexer_next(parser->lexer);
                ast_add_child(node, parse_expression(parser));  /* time$ */
                if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                    gfa_lexer_next(parser->lexer);
                    ast_add_child(node, parse_expression(parser));  /* date$ */
                }
                return node;
            }
        case TOK_BMOVE:
        case TOK_MSHRINK:
            {
                ast_node *node;
                node = ast_create(AST_CALL);
                node->value.int_val = (long)tok;
                gfa_lexer_next(parser->lexer);
                while (gfa_lexer_current_token(parser->lexer) != TOK_EOL &&
                       gfa_lexer_current_token(parser->lexer) != TOK_EOF &&
                       gfa_lexer_current_token(parser->lexer) != TOK_COLON) {
                    ast_add_arg(node, parse_expression(parser));
                    if (gfa_lexer_current_token(parser->lexer) ==
                        TOK_COMMA)
                        gfa_lexer_next(parser->lexer);
                    else
                        break;
                }
                return node;
            }

        case TOK_DUMP:
        case TOK_STORE:
        case TOK_RECALL:
        case TOK_PSAVE:
        case TOK_HARDCOPY:
        case TOK_KEYDEF:
            {
                ast_node *node;
                node = ast_create(AST_CALL);
                node->value.int_val = (long)tok;
                gfa_lexer_next(parser->lexer);
                while (gfa_lexer_current_token(parser->lexer) != TOK_EOL &&
                       gfa_lexer_current_token(parser->lexer) != TOK_EOF &&
                       gfa_lexer_current_token(parser->lexer) != TOK_COLON) {
                    ast_add_arg(node, parse_expression(parser));
                    if (gfa_lexer_current_token(parser->lexer) ==
                        TOK_COMMA)
                        gfa_lexer_next(parser->lexer);
                    else
                        break;
                }
                return node;
            }

        default:
            fprintf(stderr, "PARSE_ERR: tok=%d name=%s line=%d\n", (int)tok, gfa_keyword_get_name(tok), gfa_lexer_get_line(parser->lexer));
            PARSER_ERROR(parser, "Unexpected token");
            gfa_lexer_next(parser->lexer);
            return NULL;
    }
}

/* ------------------------------------------------------------------ */
/* IF ... THEN ... [ELSE ...] ENDIF                                   */
/* ------------------------------------------------------------------ */

static ast_node *parse_if(gfa_parser *parser)
{
    ast_node *node;

    node = ast_create(AST_IF);
    gfa_lexer_next(parser->lexer);  /* consommer IF */

    /* Condition */
    ast_set_cond(node, parse_expression(parser));

    /* THEN */
    if (gfa_lexer_current_token(parser->lexer) == TOK_THEN) {
        gfa_lexer_next(parser->lexer);

        /* THEN sur la meme ligne : une ou plusieurs instructions
           (tout ce qui suit le ':' fait partie de la clause THEN) */
        if (gfa_lexer_current_token(parser->lexer) != TOK_EOL) {
            ast_node *body;
            body = parse_statement(parser);
            if (gfa_lexer_current_token(parser->lexer) == TOK_COLON) {
                ast_node *list;
                list = ast_create(AST_STATEMENT_LIST);
                ast_add_child(list, body);
                gfa_lexer_next(parser->lexer);  /* consommer ':' */
                while (gfa_lexer_current_token(parser->lexer) != TOK_EOL &&
                       gfa_lexer_current_token(parser->lexer) != TOK_EOF &&
                       gfa_lexer_current_token(parser->lexer) != TOK_ELSE) {
                    ast_node *s2;
                    s2 = parse_statement(parser);
                    if (s2 != NULL) ast_add_child(list, s2);
                    if (gfa_lexer_current_token(parser->lexer) == TOK_COLON) {
                        gfa_lexer_next(parser->lexer);
                    } else {
                        break;
                    }
                }
                body = list;
            }
            ast_set_body(node, body);
            /* ELSE sur la meme ligne ? */
            if (gfa_lexer_current_token(parser->lexer) == TOK_ELSE) {
                gfa_lexer_next(parser->lexer);
                ast_set_else(node, parse_statement(parser));
            }
            return node;
        }
        /* Consommer le EOL apres THEN en mode multi-lignes */
        gfa_lexer_next(parser->lexer);
    } else {
        /* THEN absent : consommer le EOL implicite */
        if (gfa_lexer_current_token(parser->lexer) == TOK_EOL) {
            gfa_lexer_next(parser->lexer);
        }
    }
    /* Corps du IF (multi-lignes) */
    {
        ast_node *body;
        body = ast_create(AST_STATEMENT_LIST);

        while (gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
            gfa_token_type tok;
            tok = gfa_lexer_current_token(parser->lexer);

            /* Fin de IF */
            if (tok == TOK_ENDIF) {
                gfa_lexer_next(parser->lexer);
                break;
            }
            if (tok == TOK_ELSE) {
                gfa_lexer_next(parser->lexer);                /* Branche ELSE */
                {
                    ast_node *else_body;
                    else_body = ast_create(AST_STATEMENT_LIST);
                    while (gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
                        tok = gfa_lexer_current_token(parser->lexer);
                        if (tok == TOK_ENDIF) {
                            gfa_lexer_next(parser->lexer);
                            break;
                        }
                        ast_add_child(else_body, parse_line(parser));
                    }
                    ast_set_else(node, else_body);
                }
                break;
            }

            ast_add_child(body, parse_line(parser));
        }
        ast_set_body(node, body);
    }

    return node;
}

/* ------------------------------------------------------------------ */
/* MAT (GFA 8.6)                                                      */
/* ------------------------------------------------------------------ */

/* Nom de matrice : identifiant, eventuellement suivi de (r,c) ou
   d'index (ignorees : les dimensions viennent du DIM). */
static ast_node *parse_matrix_name(gfa_parser *parser)
{
    ast_node *n;

    if (gfa_lexer_current_token(parser->lexer) == TOK_IDENTIFIER) {
        n = ast_create_ident(AST_ASSIGN,
            parser->lexer->current.value.ident_name);
        gfa_lexer_next(parser->lexer);
        if (gfa_lexer_current_token(parser->lexer) == TOK_LPAREN) {
            /* Consommer ( … ) sans l'enregistrer */
            int depth = 0;
            gfa_lexer_next(parser->lexer);
            depth = 1;
            while (depth > 0 &&
                   gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
                if (gfa_lexer_current_token(parser->lexer) == TOK_LPAREN)
                    depth++;
                else if (gfa_lexer_current_token(parser->lexer) ==
                         TOK_RPAREN)
                    depth--;
                gfa_lexer_next(parser->lexer);
            }
        }
        return n;
    }
    return ast_create(AST_ASSIGN);
}

/*
 * parse_mat - Instruction MAT (GFA 8.6).
 *   MAT READ a | MAT INPUT a | MAT PRINT a
 *   MAT CLR a  | MAT ONE a
 *   MAT a = b | MAT a = b + c | MAT a = b - c | MAT a = b * c
 *   MAT a = TRN(b) | MAT a = INV(b) | MAT a = val
 *   MAT DET(a) | MAT QDET(a) | MAT RANG(a) | MAT NORM(a)
 *   MAT BASE = n
 */
static ast_node *parse_mat(gfa_parser *parser)
{
    ast_node *node;
    gfa_token_type t;

    gfa_lexer_next(parser->lexer);  /* MAT */
    node = ast_create(AST_MAT);

    t = gfa_lexer_current_token(parser->lexer);

    if (t == TOK_READ) {
        gfa_lexer_next(parser->lexer);
        node->value.int_val = (long)MAT_OP_READ;
        node->left = parse_matrix_name(parser);
        return node;
    }
    if (t == TOK_INPUT) {
        gfa_lexer_next(parser->lexer);
        node->value.int_val = (long)MAT_OP_INPUT;
        node->left = parse_matrix_name(parser);
        return node;
    }
    if (t == TOK_PRINT) {
        gfa_lexer_next(parser->lexer);
        node->value.int_val = (long)MAT_OP_PRINT;
        node->left = parse_matrix_name(parser);
        return node;
    }
    if (t == TOK_CLR) {
        gfa_lexer_next(parser->lexer);
        node->value.int_val = (long)MAT_OP_CLR;
        node->left = parse_matrix_name(parser);
        return node;
    }
    if (t == TOK_IDENTIFIER) {
        const char *name;
        name = parser->lexer->current.value.ident_name;
        if (parse_ident_is(name, "CLR")) {
            gfa_lexer_next(parser->lexer);
            node->value.int_val = (long)MAT_OP_CLR;
            node->left = parse_matrix_name(parser);
            return node;
        }
        if (parse_ident_is(name, "ONE")) {
            gfa_lexer_next(parser->lexer);
            node->value.int_val = (long)MAT_OP_ONE;
            node->left = parse_matrix_name(parser);
            return node;
        }
        if (parse_ident_is(name, "BASE")) {
            gfa_lexer_next(parser->lexer);
            node->value.int_val = (long)MAT_OP_BASE;
            if (gfa_lexer_current_token(parser->lexer) == TOK_EQ)
                gfa_lexer_next(parser->lexer);
            node->step = parse_expression(parser);
            return node;
        }
        if (parse_ident_is(name, "DET") || parse_ident_is(name, "QDET")) {
            gfa_lexer_next(parser->lexer);
            node->value.int_val = (long)MAT_OP_DET;
            if (gfa_lexer_current_token(parser->lexer) == TOK_LPAREN)
                gfa_lexer_next(parser->lexer);
            node->body = parse_matrix_name(parser);
            if (gfa_lexer_current_token(parser->lexer) == TOK_RPAREN)
                gfa_lexer_next(parser->lexer);
            return node;
        }
        if (parse_ident_is(name, "RANG")) {
            gfa_lexer_next(parser->lexer);
            node->value.int_val = (long)MAT_OP_RANG;
            if (gfa_lexer_current_token(parser->lexer) == TOK_LPAREN)
                gfa_lexer_next(parser->lexer);
            node->body = parse_matrix_name(parser);
            if (gfa_lexer_current_token(parser->lexer) == TOK_RPAREN)
                gfa_lexer_next(parser->lexer);
            return node;
        }
        if (parse_ident_is(name, "NORM")) {
            gfa_lexer_next(parser->lexer);
            node->value.int_val = (long)MAT_OP_NORM;
            if (gfa_lexer_current_token(parser->lexer) == TOK_LPAREN)
                gfa_lexer_next(parser->lexer);
            node->body = parse_matrix_name(parser);
            if (gfa_lexer_current_token(parser->lexer) == TOK_RPAREN)
                gfa_lexer_next(parser->lexer);
            return node;
        }
    }

    /* MAT a = … */
    node->left = parse_matrix_name(parser);
    if (gfa_lexer_current_token(parser->lexer) == TOK_EQ) {
        gfa_lexer_next(parser->lexer);
        t = gfa_lexer_current_token(parser->lexer);
        if (t == TOK_IDENTIFIER) {
            const char *rname;
            rname = parser->lexer->current.value.ident_name;
            if (parse_ident_is(rname, "TRN") ||
                parse_ident_is(rname, "TRANS")) {
                gfa_lexer_next(parser->lexer);
                node->value.int_val = (long)MAT_OP_TRANS;
                if (gfa_lexer_current_token(parser->lexer) == TOK_LPAREN)
                    gfa_lexer_next(parser->lexer);
                node->body = parse_matrix_name(parser);
                if (gfa_lexer_current_token(parser->lexer) == TOK_RPAREN)
                    gfa_lexer_next(parser->lexer);
                return node;
            }
            if (parse_ident_is(rname, "INV")) {
                gfa_lexer_next(parser->lexer);
                node->value.int_val = (long)MAT_OP_INV;
                if (gfa_lexer_current_token(parser->lexer) == TOK_LPAREN)
                    gfa_lexer_next(parser->lexer);
                node->body = parse_matrix_name(parser);
                if (gfa_lexer_current_token(parser->lexer) == TOK_RPAREN)
                    gfa_lexer_next(parser->lexer);
                return node;
            }
            node->body = parse_matrix_name(parser);
            t = gfa_lexer_current_token(parser->lexer);
            if (t == TOK_PLUS) {
                gfa_lexer_next(parser->lexer);
                node->value.int_val = (long)MAT_OP_ADD;
                node->cond = parse_matrix_name(parser);
            } else if (t == TOK_MINUS) {
                gfa_lexer_next(parser->lexer);
                node->value.int_val = (long)MAT_OP_SUB;
                node->cond = parse_matrix_name(parser);
            } else if (t == TOK_STAR) {
                gfa_lexer_next(parser->lexer);
                node->value.int_val = (long)MAT_OP_MUL;
                node->cond = parse_matrix_name(parser);
            } else {
                node->value.int_val = (long)MAT_OP_CPY;
            }
            return node;
        }
        node->value.int_val = (long)MAT_OP_SET;
        node->step = parse_expression(parser);
        return node;
    }
    return node;
}

/* ------------------------------------------------------------------ */
/* FOR var = start TO end [STEP s] ... NEXT                           */
/* ------------------------------------------------------------------ */

static ast_node *parse_for(gfa_parser *parser)
{
    ast_node *node;
    ast_node *var;
    ast_node *body;

    node = ast_create(AST_FOR);
    gfa_lexer_next(parser->lexer);  /* FOR */

    /* Variable de boucle */
    if (gfa_lexer_current_token(parser->lexer) == TOK_IDENTIFIER) {
        var = ast_create_ident(AST_ASSIGN,
            parser->lexer->current.value.ident_name);
        ast_add_child(node, var);
    }
    gfa_lexer_next(parser->lexer);

    /* = */
    if (gfa_lexer_current_token(parser->lexer) == TOK_EQ) {
        gfa_lexer_next(parser->lexer);
    }

    /* Valeur de depart */
    ast_add_child(node, parse_expression(parser));

    /* TO ou DOWNTO */
    if (gfa_lexer_current_token(parser->lexer) == TOK_TO) {
        gfa_lexer_next(parser->lexer);
    } else if (gfa_lexer_current_token(parser->lexer) == TOK_DOWNTO) {
        gfa_lexer_next(parser->lexer);
        node->value.int_val = 1;  /* sens decroissant (pas = -1) */
    }
    ast_add_child(node, parse_expression(parser));

    /* STEP */
    if (gfa_lexer_current_token(parser->lexer) == TOK_STEP) {
        gfa_lexer_next(parser->lexer);
        ast_set_step(node, parse_expression(parser));
    }
    /* Pas de gfa_lexer_next ici : le lexer saute les newlines automatiquement */

    /* Corps */
    body = ast_create(AST_STATEMENT_LIST);
    while (gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
        gfa_token_type tok;
        tok = gfa_lexer_current_token(parser->lexer);
        if (tok == TOK_NEXT) {
            gfa_lexer_next(parser->lexer);
            break;
        }
        ast_add_child(body, parse_line(parser));
    }
    ast_set_body(node, body);
    return node;
}

/* ------------------------------------------------------------------ */
/* WHILE cond ... WEND                                                */
/* ------------------------------------------------------------------ */

static ast_node *parse_while(gfa_parser *parser)
{
    ast_node *node;
    ast_node *body;

    node = ast_create(AST_WHILE);
    gfa_lexer_next(parser->lexer);

    ast_set_cond(node, parse_expression(parser));
    body = ast_create(AST_STATEMENT_LIST);
    while (gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
        if (gfa_lexer_current_token(parser->lexer) == TOK_WEND) {
            gfa_lexer_next(parser->lexer);
            break;
        }
        ast_add_child(body, parse_line(parser));
    }
    ast_set_body(node, body);
    return node;
}

/* ------------------------------------------------------------------ */
/* REPEAT ... UNTIL cond                                              */
/* ------------------------------------------------------------------ */

static ast_node *parse_repeat(gfa_parser *parser)
{
    ast_node *node;
    ast_node *body;

    node = ast_create(AST_REPEAT);
    gfa_lexer_next(parser->lexer);
    body = ast_create(AST_STATEMENT_LIST);
    while (gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
        if (gfa_lexer_current_token(parser->lexer) == TOK_UNTIL) {
            gfa_lexer_next(parser->lexer);
            ast_set_cond(node, parse_expression(parser));
            break;
        }
        ast_add_child(body, parse_line(parser));
    }
    ast_set_body(node, body);
    return node;
}

/* ------------------------------------------------------------------ */
/* DO ... LOOP [WHILE|UNTIL cond]                                     */
/* ------------------------------------------------------------------ */

/* Identifiant identique a un mot (insensible a la casse) */
static int parse_ident_is(const char *s, const char *w)
{
    if (s == NULL) return 0;
    while (*s && *w) {
        if (toupper((unsigned char)*s) != toupper((unsigned char)*w))
            return 0;
        s++;
        w++;
    }
    return (*s == '\0' && *w == '\0');
}

/*
 * parse_aes_stmt - Instruction AES generique (FORM_*, MENU_*, WIND_*,
 * APPL_*, OBJC_*, RSRC_*, …) : arguments = expressions separees par
 * des virgules jusqu'a la fin de ligne. Le runtime emule l'appel
 * (resultat 0).
 */
static ast_node *parse_aes_stmt(gfa_parser *parser, gfa_token_type tok)
{
    ast_node *node;

    gfa_lexer_next(parser->lexer);  /* consommer le mot cle AES */
    node = ast_create(AST_CALL);
    node->value.int_val = (long)tok;
    while (gfa_lexer_current_token(parser->lexer) != TOK_EOL &&
           gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
        ast_add_arg(node, parse_expression(parser));
        if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA)
            gfa_lexer_next(parser->lexer);
        else
            break;
    }
    return node;
}

/*
 * parse_line_input_file - LINE INPUT [#n,] var$
 *   sans #n : console (comportement historique)
 *   avec #n : lecture d'une ligne depuis le canal fichier n
 */
static ast_node *parse_line_input_file(gfa_parser *parser)
{
    ast_node *node;

    gfa_lexer_next(parser->lexer);  /* INPUT (LINE deja consomme) */
    node = ast_create(AST_LINE_INPUT);

    /* Optionnel : #n , */
    if (gfa_lexer_current_token(parser->lexer) == TOK_HASH) {
        gfa_lexer_next(parser->lexer);
        node->cond = parse_expression(parser);
        if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA)
            gfa_lexer_next(parser->lexer);
    }

    if (gfa_lexer_current_token(parser->lexer) == TOK_IDENTIFIER) {
        ast_add_child(node,
            ast_create_ident(AST_ASSIGN,
                parser->lexer->current.value.ident_name));
    }
    return node;
}

static ast_node *parse_exit_if(gfa_parser *parser)
{

    ast_node *node;
    node = ast_create(AST_EXIT_IF);
    gfa_lexer_next(parser->lexer);  /* EXIT */
    if (gfa_lexer_current_token(parser->lexer) == TOK_IF) {
        gfa_lexer_next(parser->lexer);
        ast_set_cond(node, parse_expression(parser));
    }
    return node;
}

static ast_node *parse_do_loop(gfa_parser *parser)
{
    ast_node *node;
    ast_node *body;

    node = ast_create(AST_DO_LOOP);
    gfa_lexer_next(parser->lexer);  /* DO */    body = ast_create(AST_STATEMENT_LIST);
    while (gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
        if (gfa_lexer_current_token(parser->lexer) == TOK_LOOP) {
            gfa_lexer_next(parser->lexer);
            /* WHILE ou UNTIL optionnel */
            if (gfa_lexer_current_token(parser->lexer) == TOK_WHILE ||
                gfa_lexer_current_token(parser->lexer) == TOK_UNTIL) {
                int is_until;
                is_until = (gfa_lexer_current_token(parser->lexer)
                            == TOK_UNTIL) ? 1 : 0;
                gfa_lexer_next(parser->lexer);
                /* 0 = LOOP WHILE, 1 = LOOP UNTIL (codegen) */
                node->value.int_val = (long)is_until;
                ast_set_cond(node, parse_expression(parser));
            }
            break;
        }
        if (gfa_lexer_current_token(parser->lexer) == TOK_EXIT_IF) {
            ast_add_child(body, parse_exit_if(parser));
            continue;
        }
        ast_add_child(body, parse_line(parser));
    }
    ast_set_body(node, body);
    return node;
}

/* ------------------------------------------------------------------ */
/* SELECT expr ... CASE ... ENDSELECT                                 */
/* ------------------------------------------------------------------ */

static ast_node *parse_select(gfa_parser *parser)
{
    ast_node *node;
    ast_node *body;

    node = ast_create(AST_SELECT);
    gfa_lexer_next(parser->lexer);  /* SELECT */

    ast_set_cond(node, parse_expression(parser));
    body = ast_create(AST_STATEMENT_LIST);
    while (gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
        gfa_token_type tok;
        tok = gfa_lexer_current_token(parser->lexer);

        if (tok == TOK_ENDSELECT) {
            gfa_lexer_next(parser->lexer);
            break;
        }
        if (tok == TOK_CASE) {
            ast_node *case_node;
            case_node = ast_create(AST_CASE);
            gfa_lexer_next(parser->lexer);
            ast_set_cond(case_node, parse_expression(parser));
            if (gfa_lexer_current_token(parser->lexer) == TOK_COLON) {
                gfa_lexer_next(parser->lexer);  /* : optionnel apres CASE */
            }
            /* Corps du case */
            {
                ast_node *case_body;
                case_body = ast_create(AST_STATEMENT_LIST);
                while (gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
                    tok = gfa_lexer_current_token(parser->lexer);
                    if (tok == TOK_CASE || tok == TOK_DEFAULT ||
                        tok == TOK_ENDSELECT) {
                        break;
                    }
                    ast_add_child(case_body, parse_line(parser));
                }
                ast_set_body(case_node, case_body);
            }
            ast_add_child(body, case_node);
            continue;
        }
        if (tok == TOK_DEFAULT) {
            ast_node *def_node;
            def_node = ast_create(AST_DEFAULT_CASE);
            gfa_lexer_next(parser->lexer);
            if (gfa_lexer_current_token(parser->lexer) == TOK_COLON) {
                gfa_lexer_next(parser->lexer);  /* : optionnel apres DEFAULT */
            }
            {
                ast_node *def_body;
                def_body = ast_create(AST_STATEMENT_LIST);
                while (gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
                    tok = gfa_lexer_current_token(parser->lexer);
                    if (tok == TOK_CASE || tok == TOK_ENDSELECT) {
                        break;
                    }
                    ast_add_child(def_body, parse_line(parser));
                }
                ast_set_body(def_node, def_body);
            }
            ast_add_child(body, def_node);
            continue;
        }
        ast_add_child(body, parse_line(parser));
    }
    ast_set_body(node, body);
    return node;
}

/* ------------------------------------------------------------------ */
/* PRINT [#n,] [expr][;][,]...                                        */
/* ------------------------------------------------------------------ */

static ast_node *parse_print(gfa_parser *parser)
{
    ast_node *node;

    node = ast_create(AST_PRINT);
    gfa_lexer_next(parser->lexer);  /* PRINT */

    if (gfa_lexer_current_token(parser->lexer) == TOK_HASH) {
        gfa_lexer_next(parser->lexer);
        ast_add_child(node, parse_expression(parser));  /* canal */
        node->value.int_val = 1;  /* flag: canal present */
        if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
            gfa_lexer_next(parser->lexer);
        }
    }

    /* PRINT AT */
    if (gfa_lexer_current_token(parser->lexer) == TOK_PRINT_AT) {
        node->type = AST_PRINT_AT;
        gfa_lexer_next(parser->lexer);
        gfa_lexer_next(parser->lexer);  /* ( */
        ast_add_child(node, parse_expression(parser));  /* x */
        if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
            gfa_lexer_next(parser->lexer);
        }
        ast_add_child(node, parse_expression(parser));  /* y */
        if (gfa_lexer_current_token(parser->lexer) == TOK_RPAREN) {
            gfa_lexer_next(parser->lexer);
        }
        if (gfa_lexer_current_token(parser->lexer) == TOK_SEMICOLON) {
            gfa_lexer_next(parser->lexer);
        }
        ast_add_child(node, parse_expression(parser));
        return node;
    }

    /* PRINT USING */
    if (gfa_lexer_current_token(parser->lexer) == TOK_PRINT_USING) {
        node->type = AST_PRINT_USING;
        gfa_lexer_next(parser->lexer);
        ast_add_child(node, parse_expression(parser));  /* format$ */
        if (gfa_lexer_current_token(parser->lexer) == TOK_SEMICOLON) {
            gfa_lexer_next(parser->lexer);
        }
        ast_add_child(node, parse_expression(parser));
        return node;
    }

    /* Liste d'expressions */
    while (gfa_lexer_current_token(parser->lexer) != TOK_EOL &&
           gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
        gfa_token_type sep;

        ast_add_child(node, parse_expression(parser));

        sep = gfa_lexer_current_token(parser->lexer);
        if (sep == TOK_SEMICOLON || sep == TOK_COMMA) {
            ast_add_child(node, ast_create_int(AST_PRINT_SEP,
                (sep == TOK_SEMICOLON) ? 0 : 1));
            gfa_lexer_next(parser->lexer);
            continue;
        }
        break;
    }

    return node;
}

/* ------------------------------------------------------------------ */
/* INPUT ["prompt";] var [, var...]                                   */
/* ------------------------------------------------------------------ */

static ast_node *parse_input(gfa_parser *parser)
{
    ast_node *node;

    node = ast_create(AST_INPUT);
    gfa_lexer_next(parser->lexer);  /* INPUT */

    /* #n optionnel */
    if (gfa_lexer_current_token(parser->lexer) == TOK_HASH) {
        gfa_lexer_next(parser->lexer);
        ast_add_child(node, parse_expression(parser));
        node->value.int_val = 1;  /* flag: canal present */
        if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
            gfa_lexer_next(parser->lexer);
        }
    }

    /* Prompt optionnel */
    if (gfa_lexer_current_token(parser->lexer) == TOK_STRING) {
        ast_node *prompt;
        prompt = ast_create_str(AST_ASSIGN,
            parser->lexer->current.value.string_value);
        ast_add_child(node, prompt);
        gfa_lexer_next(parser->lexer);
        if (gfa_lexer_current_token(parser->lexer) == TOK_SEMICOLON) {
            gfa_lexer_next(parser->lexer);
        }
    }

    /* Liste de variables */
    while (gfa_lexer_current_token(parser->lexer) == TOK_IDENTIFIER) {
        ast_add_child(node,
            ast_create_ident(AST_ASSIGN,
                parser->lexer->current.value.ident_name));
        gfa_lexer_next(parser->lexer);
        if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
            gfa_lexer_next(parser->lexer);
        } else {
            break;
        }
    }

    return node;
}

/* ------------------------------------------------------------------ */
/* OPEN mode$, #n, "fichier"[, len]                                   */
/* ------------------------------------------------------------------ */

static ast_node *parse_open(gfa_parser *parser)
{
    ast_node *node;

    node = ast_create(AST_OPEN);
    gfa_lexer_next(parser->lexer);

    /* Mode ("I", "O", "A", "R", "U") */
    ast_add_child(node, parse_expression(parser));
    if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
        gfa_lexer_next(parser->lexer);
    }

    /* #n */
    if (gfa_lexer_current_token(parser->lexer) == TOK_HASH) {
        gfa_lexer_next(parser->lexer);
    }
    ast_add_child(node, parse_expression(parser));
    if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
        gfa_lexer_next(parser->lexer);
    }

    /* Nom du fichier */
    ast_add_child(node, parse_expression(parser));

    /* Longueur d'enregistrement optionnelle */
    if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
        gfa_lexer_next(parser->lexer);
        ast_add_child(node, parse_expression(parser));
    }

    return node;
}

/* ------------------------------------------------------------------ */
/* DIM arr(dim1, ...)                                                 */
/* ------------------------------------------------------------------ */

static ast_node *parse_dim(gfa_parser *parser)
{
    ast_node *node;

    node = ast_create(AST_DIM);
    gfa_lexer_next(parser->lexer);

    /* Nom du tableau */
    if (gfa_lexer_current_token(parser->lexer) == TOK_IDENTIFIER) {
        ast_add_child(node,
            ast_create_ident(AST_ASSIGN,
                parser->lexer->current.value.ident_name));
    }
    gfa_lexer_next(parser->lexer);

    /* (dim1, dim2, ...) */
    if (gfa_lexer_current_token(parser->lexer) == TOK_LPAREN) {
        gfa_lexer_next(parser->lexer);
        while (gfa_lexer_current_token(parser->lexer) != TOK_RPAREN &&
               gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
            ast_add_child(node, parse_expression(parser));
            if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                gfa_lexer_next(parser->lexer);
            }
        }
        if (gfa_lexer_current_token(parser->lexer) == TOK_RPAREN) {
            gfa_lexer_next(parser->lexer);
        }
    }

    return node;
}

/* ------------------------------------------------------------------ */
/* DATA val1, val2, ...                                               */
/* ------------------------------------------------------------------ */

static ast_node *parse_data(gfa_parser *parser)
{
    ast_node *node;

    node = ast_create(AST_DATA);
    gfa_lexer_next(parser->lexer);

    while (gfa_lexer_current_token(parser->lexer) != TOK_EOL &&
           gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
        ast_add_child(node, parse_expression(parser));
        if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
            gfa_lexer_next(parser->lexer);
        } else {
            break;
        }
    }

    return node;
}

/* ------------------------------------------------------------------ */
/* PROCEDURE nom[(args)]                                              */
/* ------------------------------------------------------------------ */

static ast_node *parse_procedure(gfa_parser *parser)
{
    ast_node *node;
    ast_node *body;

    node = ast_create(AST_PROCEDURE);
    gfa_lexer_next(parser->lexer);  /* PROCEDURE */

    /* Nom */
    if (gfa_lexer_current_token(parser->lexer) == TOK_IDENTIFIER) {
        node->value.ident =
            os_strdup(parser->lexer->current.value.ident_name);
        node->has_ident = 1;
        /* Register procedure name as a label for GOSUB resolution */
        if (parser->label_count < 256 && node->value.ident) {
            parser->labels[parser->label_count].name = os_strdup(node->value.ident);
            parser->labels[parser->label_count].ast_node_index = 0;
            parser->label_count++;
        }
    }
    gfa_lexer_next(parser->lexer);

    /* (args) optionnels */
    if (gfa_lexer_current_token(parser->lexer) == TOK_LPAREN) {
        int in_var = 0;  /* VAR bascule les suivants en by-ref */
        gfa_lexer_next(parser->lexer);
        while (gfa_lexer_current_token(parser->lexer) != TOK_RPAREN &&
               gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
            if (gfa_lexer_current_token(parser->lexer) == TOK_VAR) {
                in_var = 1;
                gfa_lexer_next(parser->lexer);
                if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                    gfa_lexer_next(parser->lexer);
                }
                continue;
            }
            if (gfa_lexer_current_token(parser->lexer) == TOK_IDENTIFIER) {
                ast_node *param;
                param = ast_create_ident(AST_ASSIGN,
                    parser->lexer->current.value.ident_name);
                param->line = in_var;
                ast_add_child(node, param);
            }
            gfa_lexer_next(parser->lexer);
            if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                gfa_lexer_next(parser->lexer);
            }
        }
        if (gfa_lexer_current_token(parser->lexer) == TOK_RPAREN) {
            gfa_lexer_next(parser->lexer);
        }
    }    /* Corps */
    body = ast_create(AST_STATEMENT_LIST);
    while (gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
        if (gfa_lexer_current_token(parser->lexer) == TOK_RETURN ||
            gfa_lexer_current_token(parser->lexer) == TOK_ENDPROC) {
            ast_add_child(body, ast_create(AST_RETURN));
            gfa_lexer_next(parser->lexer);
            break;
        }
        if (gfa_lexer_current_token(parser->lexer) == TOK_PROCEDURE ||
            gfa_lexer_current_token(parser->lexer) == TOK_FUNCTION) {
            break;
        }
        ast_add_child(body, parse_line(parser));
    }
    ast_set_body(node, body);
    return node;
}

/* ------------------------------------------------------------------ */
/* FUNCTION nom[(args)] ... RETURN expr ... ENDFUNC                   */
/* ------------------------------------------------------------------ */

static ast_node *parse_function(gfa_parser *parser)
{
    ast_node *node;
    ast_node *body;

    node = ast_create(AST_FUNCTION_DEF);
    gfa_lexer_next(parser->lexer);  /* FUNCTION */

    /* Nom */
    if (gfa_lexer_current_token(parser->lexer) == TOK_IDENTIFIER) {
        node->value.ident =
            os_strdup(parser->lexer->current.value.ident_name);
        node->has_ident = 1;
        /* Enregistrer comme label pour resolution d'appel */
        if (parser->label_count < 256 && node->value.ident) {
            parser->labels[parser->label_count].name = os_strdup(node->value.ident);
            parser->labels[parser->label_count].ast_node_index = 0;
            parser->label_count++;
        }
    }
    gfa_lexer_next(parser->lexer);

    /* (args) optionnels */
    if (gfa_lexer_current_token(parser->lexer) == TOK_LPAREN) {
        int in_var = 0;
        gfa_lexer_next(parser->lexer);
        while (gfa_lexer_current_token(parser->lexer) != TOK_RPAREN &&
               gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
            if (gfa_lexer_current_token(parser->lexer) == TOK_VAR) {
                in_var = 1;
                gfa_lexer_next(parser->lexer);
                if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                    gfa_lexer_next(parser->lexer);
                }
                continue;
            }
            if (gfa_lexer_current_token(parser->lexer) == TOK_IDENTIFIER) {
                ast_node *param;
                param = ast_create_ident(AST_ASSIGN,
                    parser->lexer->current.value.ident_name);
                param->line = in_var;
                ast_add_child(node, param);
            }
            gfa_lexer_next(parser->lexer);
            if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                gfa_lexer_next(parser->lexer);
            }
        }
        if (gfa_lexer_current_token(parser->lexer) == TOK_RPAREN) {
            gfa_lexer_next(parser->lexer);
        }
    }

    /* Corps : jusqu'a ENDFUNC ou fin de fichier */
    body = ast_create(AST_STATEMENT_LIST);
    while (gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
        if (gfa_lexer_current_token(parser->lexer) == TOK_ENDFUNC) {
            gfa_lexer_next(parser->lexer);
            break;
        }
        if (gfa_lexer_current_token(parser->lexer) == TOK_PROCEDURE ||
            gfa_lexer_current_token(parser->lexer) == TOK_FUNCTION) {
            break;
        }
        ast_add_child(body, parse_line(parser));
    }
    ast_set_body(node, body);
    return node;
}

/* ------------------------------------------------------------------ */
/* DEFFN nom(args) = expression                                       */
/* ------------------------------------------------------------------ */

static ast_node *parse_deffn(gfa_parser *parser)
{
    ast_node *node;

    node = ast_create(AST_DEFFN);
    gfa_lexer_next(parser->lexer);  /* DEFFN */

    /* Nom */
    if (gfa_lexer_current_token(parser->lexer) == TOK_IDENTIFIER) {
        node->value.ident =
            os_strdup(parser->lexer->current.value.ident_name);
        node->has_ident = 1;
        if (parser->label_count < 256 && node->value.ident) {
            parser->labels[parser->label_count].name = os_strdup(node->value.ident);
            parser->labels[parser->label_count].ast_node_index = 0;
            parser->label_count++;
        }
    }
    gfa_lexer_next(parser->lexer);

    /* (args) optionnels */
    if (gfa_lexer_current_token(parser->lexer) == TOK_LPAREN) {
        gfa_lexer_next(parser->lexer);
        while (gfa_lexer_current_token(parser->lexer) != TOK_RPAREN &&
               gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
            if (gfa_lexer_current_token(parser->lexer) == TOK_IDENTIFIER) {
                ast_add_child(node,
                    ast_create_ident(AST_ASSIGN,
                        parser->lexer->current.value.ident_name));
            }
            gfa_lexer_next(parser->lexer);
            if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                gfa_lexer_next(parser->lexer);
            }
        }
        if (gfa_lexer_current_token(parser->lexer) == TOK_RPAREN) {
            gfa_lexer_next(parser->lexer);
        }
    }

    /* = expression */
    if (gfa_lexer_current_token(parser->lexer) == TOK_EQ) {
        gfa_lexer_next(parser->lexer);
    }
    ast_set_body(node, parse_expression(parser));

    return node;
}

/* ------------------------------------------------------------------ */
/* FN nom[(args)] = expr  (forme inline)                               */
/* FN nom[(args)] ... corps ... RETURN  (forme multi-lignes,           */
/*    resultat = valeur de la variable nom)                            */
/* ------------------------------------------------------------------ */

static ast_node *parse_fn(gfa_parser *parser)
{
    ast_node *node;
    ast_node *body;
    int multiline = 0;

    node = ast_create(AST_DEFFN);
    gfa_lexer_next(parser->lexer);  /* FN */

    /* Nom (stocke aussi le mode multi-lignes dans node->line) */
    if (gfa_lexer_current_token(parser->lexer) == TOK_IDENTIFIER) {
        node->value.ident =
            os_strdup(parser->lexer->current.value.ident_name);
        node->has_ident = 1;
        if (parser->label_count < 256 && node->value.ident) {
            parser->labels[parser->label_count].name =
                os_strdup(node->value.ident);
            parser->labels[parser->label_count].ast_node_index = 0;
            parser->label_count++;
        }
    }
    gfa_lexer_next(parser->lexer);

    /* (args) optionnels */
    if (gfa_lexer_current_token(parser->lexer) == TOK_LPAREN) {
        gfa_lexer_next(parser->lexer);
        while (gfa_lexer_current_token(parser->lexer) != TOK_RPAREN &&
               gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
            if (gfa_lexer_current_token(parser->lexer) == TOK_IDENTIFIER) {
                ast_add_child(node,
                    ast_create_ident(AST_ASSIGN,
                        parser->lexer->current.value.ident_name));
            }
            gfa_lexer_next(parser->lexer);
            if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                gfa_lexer_next(parser->lexer);
            }
        }
        if (gfa_lexer_current_token(parser->lexer) == TOK_RPAREN) {
            gfa_lexer_next(parser->lexer);
        }
    }

    if (gfa_lexer_current_token(parser->lexer) == TOK_EQ) {
        /* Forme inline : FN nom(x) = expr */
        gfa_lexer_next(parser->lexer);
        ast_set_body(node, parse_expression(parser));
    } else {
        /* Forme multi-lignes : corps jusqu'a RETURN */
        multiline = 1;
        body = ast_create(AST_STATEMENT_LIST);
        while (gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
            if (gfa_lexer_current_token(parser->lexer) == TOK_RETURN) {
                ast_node *ret = ast_create(AST_DEFFN_RET);
                if (node->has_ident && node->value.ident) {
                    ret->value.ident = os_strdup(node->value.ident);
                    ret->has_ident = 1;
                }
                ast_add_child(body, ret);
                gfa_lexer_next(parser->lexer);
                break;
            }
            if (gfa_lexer_current_token(parser->lexer) == TOK_PROCEDURE ||
                gfa_lexer_current_token(parser->lexer) == TOK_FUNCTION ||
                gfa_lexer_current_token(parser->lexer) == TOK_FN) {
                break;
            }
            ast_add_child(body, parse_line(parser));
        }
        ast_set_body(node, body);
    }
    node->line = multiline ? 1 : 0;  /* flag multi-lignes (pas value.int_val) */
    return node;
}

/* ------------------------------------------------------------------ */
/* SOUND ch, freq, dur, vol, env                                      */
/* ------------------------------------------------------------------ */

static ast_node *parse_sound_stmt(gfa_parser *parser)
{
    ast_node *node;
    int i;

    node = ast_create(AST_SOUND);
    gfa_lexer_next(parser->lexer);

    for (i = 0; i < 5; i++) {
        ast_add_child(node, parse_expression(parser));
        if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
            gfa_lexer_next(parser->lexer);
        } else {
            break;
        }
    }

    return node;
}

/* ------------------------------------------------------------------ */
/* LINE x1,y1,x2,y2 / BOX / CIRCLE ...                               */
/* ------------------------------------------------------------------ */

static ast_node *parse_graphics(gfa_parser *parser)
{
    ast_node *node;
    gfa_token_type tok;
    ast_node_type ntype;

    tok = gfa_lexer_current_token(parser->lexer);

    switch (tok) {
        case TOK_LINE_TOK:  ntype = AST_LINE;    break;
        case TOK_BOX:       ntype = AST_BOX;     break;
        case TOK_PBOX:      ntype = AST_PBOX;    break;
        case TOK_CIRCLE_TOK:ntype = AST_CIRCLE;  break;
        case TOK_PCIRCLE:   ntype = AST_PCIRCLE; break;
        default:            ntype = AST_LINE;    break;
    }

    node = ast_create(ntype);
    gfa_lexer_next(parser->lexer);

    /* Lire tous les parametres jusqu'a la fin de ligne */
    while (gfa_lexer_current_token(parser->lexer) != TOK_EOL &&
           gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
        ast_add_child(node, parse_expression(parser));
        if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
            gfa_lexer_next(parser->lexer);
        } else {
            break;
        }
    }

    return node;
}

/* ------------------------------------------------------------------ */
/* Parser d'expressions (precedence standard)                         */
/* ------------------------------------------------------------------ */

/*
 * expression := simple_expr (('='|'<'|'>'|'<='|'>='|'<>') simple_expr)*
 */
static ast_node *parse_comparison(gfa_parser *parser)
{
    ast_node *left;
    gfa_token_type op;

    left = parse_simple_expr(parser);

    op = gfa_lexer_current_token(parser->lexer);
    if (op == TOK_EQ || op == TOK_LT || op == TOK_GT ||
        op == TOK_LE || op == TOK_GE || op == TOK_NE ||
        op == TOK_APPROX_EQ) {
        ast_node *node;
        gfa_lexer_next(parser->lexer);
        node = ast_create(AST_ASSIGN);
        ast_add_child(node, left);
        ast_add_child(node, parse_simple_expr(parser));
        /* Stocker l'operateur */
        node->value.int_val = (long)op;
        return node;
    }

    return left;
}

/*
 * expression := comparison ( (AND|OR|XOR|EQV|IMP) comparison )*
 */
static ast_node *parse_expression(gfa_parser *parser)
{
    ast_node *left;
    gfa_token_type op;

    left = parse_comparison(parser);

    for (;;) {
        op = gfa_lexer_current_token(parser->lexer);
        if (op == TOK_AND_OP || op == TOK_OR_OP ||
            op == TOK_XOR_OP || op == TOK_EQV_OP ||
            op == TOK_IMP_OP) {
            ast_node *node;
            gfa_lexer_next(parser->lexer);
            node = ast_create(AST_ASSIGN);
            ast_add_child(node, left);
            ast_add_child(node, parse_comparison(parser));
            node->value.int_val = (long)op;
            left = node;
            continue;
        }
        break;
    }

    return left;
}

/*
 * simple_expr := term (('+'|'-'|'&') term)*
 */
static ast_node *parse_simple_expr(gfa_parser *parser)
{
    ast_node *left;
    gfa_token_type op;

    left = parse_term(parser);

    for (;;) {
        op = gfa_lexer_current_token(parser->lexer);
        if (op == TOK_PLUS || op == TOK_MINUS ||
            op == TOK_AMPERSAND) {
            ast_node *node;
            gfa_lexer_next(parser->lexer);
            node = ast_create(AST_ASSIGN);
            ast_add_child(node, left);
            ast_add_child(node, parse_term(parser));
            node->value.int_val = (long)op;
            left = node;
            continue;
        }
        break;
    }

    return left;
}

/*
 * term := factor (('*'|'/'|MOD|DIV) factor)*
 */
static ast_node *parse_term(gfa_parser *parser)
{
    ast_node *left;
    gfa_token_type op;

    left = parse_factor(parser);

    for (;;) {
        op = gfa_lexer_current_token(parser->lexer);
        if (op == TOK_STAR || op == TOK_SLASH ||
            op == TOK_MOD_OP || op == TOK_DIV_OP) {
            ast_node *node;
            gfa_lexer_next(parser->lexer);
            node = ast_create(AST_ASSIGN);
            ast_add_child(node, left);
            ast_add_child(node, parse_factor(parser));
            node->value.int_val = (long)op;
            left = node;
            continue;
        }
        break;
    }

    return left;
}

/*
 * factor := ('-'|'+'|NOT) factor | primary ('^' factor)?
 */
static ast_node *parse_factor(gfa_parser *parser)
{
    gfa_token_type op;

    op = gfa_lexer_current_token(parser->lexer);
    if (op == TOK_MINUS || op == TOK_PLUS || op == TOK_NOT_OP ||
        op == TOK_TILDE) {
        ast_node *node;
        gfa_lexer_next(parser->lexer);
        node = ast_create(AST_ASSIGN);
        node->value.int_val = (long)op;
        ast_add_child(node, parse_factor(parser));
        return node;
    }

    /* primary ('^' factor)? */
    {
        ast_node *left;
        left = parse_primary(parser);
        if (gfa_lexer_current_token(parser->lexer) == TOK_CARET) {
            ast_node *node;
            gfa_lexer_next(parser->lexer);
            node = ast_create(AST_ASSIGN);
            node->value.int_val = (long)TOK_CARET;
            ast_add_child(node, left);
            ast_add_child(node, parse_factor(parser));
            return node;
        }
        return left;
    }
}

/*
 * primary := INTEGER | FLOAT | STRING | IDENTIFIER
 *          | '(' expression ')'
 *          | IDENTIFIER '(' args ')'
 *          | builtin_function '(' args ')'
 */
/*
 * parse_gfx_args - Collecte des expressions separees par virgules
 * dans args[] (jusqu'a max). Utilise par les instructions graphiques.
 */
static int parse_gfx_args(gfa_parser *parser, ast_node *node, int max)
{
    int n;

    ast_add_arg(node, parse_expression(parser));
    n = 1;
    while (n < max &&
           gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
        gfa_lexer_next(parser->lexer);
        ast_add_arg(node, parse_expression(parser));
        n++;
    }
    return n;
}

/*
 * parse_builtin_call - Appel de fonction integree generique.
 * Consomme le mot-cle (deja positionne) puis les arguments entre
 * parentheses ou accolades (optionnels). Reutilise pour les
 * instructions-builtin (KILL, SEEK, SPUT, …).
 */
static ast_node *parse_builtin_call(gfa_parser *parser,
                                    gfa_token_type func_tok)
{
    ast_node *call;

    call = ast_create(AST_CALL);
    call->value.int_val = (long)func_tok;
    call->has_ident = 0;
    call->has_str = 0;
    gfa_lexer_next(parser->lexer);

    {
        gfa_token_type open_tok = gfa_lexer_current_token(parser->lexer);
        gfa_token_type close_tok;

        if (open_tok == TOK_LPAREN) {
            close_tok = TOK_RPAREN;
        } else if (open_tok == TOK_LBRACE) {
            close_tok = TOK_RBRACE;
        } else {
            return call;
        }
        gfa_lexer_next(parser->lexer);
        while (gfa_lexer_current_token(parser->lexer) != close_tok &&
               gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
            ast_add_arg(call, parse_expression(parser));
            if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                gfa_lexer_next(parser->lexer);
            }
        }
        if (gfa_lexer_current_token(parser->lexer) == close_tok) {
            gfa_lexer_next(parser->lexer);
        }
    }
    return call;
}

static ast_node *parse_primary(gfa_parser *parser)
{
    gfa_token_type tok;

    tok = gfa_lexer_current_token(parser->lexer);

    switch (tok) {
        case TOK_INTEGER:
            {
                ast_node *node;
                node = ast_create_int(AST_ASSIGN,
                    parser->lexer->current.value.int_value);
                gfa_lexer_next(parser->lexer);
                return node;
            }

        case TOK_FLOAT:
            {
                ast_node *node;
                node = ast_create_float(AST_ASSIGN,
                    parser->lexer->current.value.float_value);
                gfa_lexer_next(parser->lexer);
                return node;
            }

        case TOK_STRING:
            {
                ast_node *node;
                node = ast_create_str(AST_ASSIGN,
                    parser->lexer->current.value.string_value);
                gfa_lexer_next(parser->lexer);
                return node;
            }

        case TOK_HASH:
            {
                /* #n : canal fichier dans une expression (ex: EOF(#1)) */
                ast_node *node;
                gfa_lexer_next(parser->lexer);  /* consommer # */
                node = parse_expression(parser);  /* le numero du canal */
                return node;
            }

        case TOK_MAT:
            {
                /* MAT DET(a) / MAT RANG(a) / MAT NORM(a) en position
                   expression : x = MAT DET(a) */
                ast_node *node;
                gfa_token_type sub_tok;
                const char *sname;
                long sub = -1;

                gfa_lexer_next(parser->lexer);  /* consommer MAT */
                sub_tok = gfa_lexer_current_token(parser->lexer);
                if (sub_tok != TOK_IDENTIFIER) {
                    PARSER_ERROR(parser, "Unexpected token in expression");
                    return NULL;
                }
                sname = parser->lexer->current.value.ident_name;
                if (parse_ident_is(sname, "DET") ||
                    parse_ident_is(sname, "QDET"))
                    sub = MAT_OP_DET;
                else if (parse_ident_is(sname, "RANG"))
                    sub = MAT_OP_RANG;
                else if (parse_ident_is(sname, "NORM"))
                    sub = MAT_OP_NORM;
                else {
                    PARSER_ERROR(parser, "Unexpected token in expression");
                    return NULL;
                }
                gfa_lexer_next(parser->lexer);  /* consommer DET/RANG/… */
                if (gfa_lexer_current_token(parser->lexer) == TOK_LPAREN)
                    gfa_lexer_next(parser->lexer);
                node = ast_create(AST_MAT);
                node->value.int_val = sub;
                node->is_expr = 1;
                node->body = parse_matrix_name(parser);
                if (gfa_lexer_current_token(parser->lexer) == TOK_RPAREN)
                    gfa_lexer_next(parser->lexer);
                return node;
            }

        case TOK_W_COLON:
        case TOK_L_COLON:
            {
                /* W:expr / L:expr : passage de valeur word/long
                   (argument d'appel XBIOS). Les valeurs internes etant
                   sur 32 bits, le cast n'a pas d'effet a l'execution. */
                ast_node *node;
                gfa_lexer_next(parser->lexer);
                node = parse_expression(parser);
                return node;
            }

        case TOK_FN:
            {
                ast_node *call;
                gfa_lexer_next(parser->lexer);  /* consommer FN */
                call = ast_create(AST_FN_CALL);
                /* Nom de la fonction */
                if (gfa_lexer_current_token(parser->lexer) == TOK_IDENTIFIER) {
                    call->value.ident =
                        os_strdup(parser->lexer->current.value.ident_name);
                    call->has_ident = 1;
                    gfa_lexer_next(parser->lexer);
                }
                /* (args) */
                if (gfa_lexer_current_token(parser->lexer) == TOK_LPAREN) {
                    gfa_lexer_next(parser->lexer);
                    while (gfa_lexer_current_token(parser->lexer) != TOK_RPAREN &&
                           gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
                        ast_add_arg(call, parse_expression(parser));
                        if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                            gfa_lexer_next(parser->lexer);
                        }
                    }
                    if (gfa_lexer_current_token(parser->lexer) == TOK_RPAREN) {
                        gfa_lexer_next(parser->lexer);
                    }
                }
                return call;
            }

        case TOK_IDENTIFIER:
            {
                ast_node *ident;
                ident = ast_create_ident(AST_ASSIGN,
                    parser->lexer->current.value.ident_name);
                gfa_lexer_next(parser->lexer);

                /* Appel de fonction ? */
                if (gfa_lexer_current_token(parser->lexer) == TOK_LPAREN) {
                    ast_node *call;
                    call = ast_create(AST_CALL);
                    call->value.ident = ident->value.ident;
                    call->has_ident = ident->has_ident;
                    ident->has_ident = 0;
                    free(ident);

                    gfa_lexer_next(parser->lexer);
                    while (gfa_lexer_current_token(parser->lexer) != TOK_RPAREN &&
                           gfa_lexer_current_token(parser->lexer) != TOK_EOF) {
                        ast_add_arg(call, parse_expression(parser));
                        if (gfa_lexer_current_token(parser->lexer) == TOK_COMMA) {
                            gfa_lexer_next(parser->lexer);
                        }
                    }
                    if (gfa_lexer_current_token(parser->lexer) == TOK_RPAREN) {
                        gfa_lexer_next(parser->lexer);
                    }
                    return call;
                }

                /* Variable simple */
                return ident;
            }

        case TOK_LPAREN:
            {
                ast_node *node;
                gfa_lexer_next(parser->lexer);
                node = parse_expression(parser);
                if (gfa_lexer_current_token(parser->lexer) == TOK_RPAREN) {
                    gfa_lexer_next(parser->lexer);
                }
                return node;
            }

        /* DIM? arr : renvoie les dimensions du tableau en chaine */
        case TOK_DIM_QUESTION:
            {
                ast_node *node, *ident;
                gfa_lexer_next(parser->lexer);
                node = ast_create(AST_DIM_QUESTION);
                if (gfa_lexer_current_token(parser->lexer) == TOK_LPAREN) {
                    gfa_lexer_next(parser->lexer);
                    if (gfa_lexer_current_token(parser->lexer)
                        == TOK_IDENTIFIER) {
                        ident = ast_create_ident(AST_ASSIGN,
                            parser->lexer->current.value.ident_name);
                        gfa_lexer_next(parser->lexer);
                        node->left = ident;
                    }
                    if (gfa_lexer_current_token(parser->lexer)
                        == TOK_RPAREN)
                        gfa_lexer_next(parser->lexer);
                }
                return node;
            }

        /* Fonctions integrees */
        case TOK_ABS: case TOK_ASC: case TOK_LEN: case TOK_VAL:
        case TOK_SIN: case TOK_COS: case TOK_TAN: case TOK_ATN:
        case TOK_EXP: case TOK_LOG: case TOK_LOG10: case TOK_SQR:
        case TOK_INT: case TOK_FIX: case TOK_ROUND: case TOK_FRAC:
        case TOK_SGN: case TOK_RND: case TOK_PEEK: case TOK_DPEEK:
        case TOK_LPEEK: case TOK_INKEY: case TOK_INP: case TOK_CHR_TOK:
        case TOK_STR_TOK: case TOK_HEX_TOK: case TOK_BIN_TOK: case TOK_OCT_TOK:
        case TOK_LEFT_TOK: case TOK_RIGHT_TOK: case TOK_MID_TOK:
        case TOK_INSTR: case TOK_RINSTR:
        case TOK_STRING_TOK: case TOK_SPACE_TOK:
        case TOK_TRIM_TOK: case TOK_UPPER_TOK: case TOK_LCASE_TOK:
        case TOK_LOWER_TOK:
        case TOK_EOF_TOK: case TOK_LOF: case TOK_LOC:
        case TOK_TRUE: case TOK_FALSE: case TOK_PI_TOK:
        case TOK_MOUSEX: case TOK_MOUSEY: case TOK_MOUSEK:
        case TOK__C: case TOK__X: case TOK__Y:
        case TOK_TIMER_TOK: case TOK_DATE_TOK: case TOK_TIME_TOK:
        case TOK_ERR:
        case TOK_CFLOAT: case TOK_CINT:
        case TOK_MIN: case TOK_MAX: case TOK_EVEN: case TOK_ODD:
        case TOK_ASIN: case TOK_ACOS: case TOK_SINQ: case TOK_COSQ:
        case TOK_SINH: case TOK_COSH: case TOK_TANH:
        case TOK_FACT: case TOK_COMBIN: case TOK_VARIAT:
        case TOK_ARRPTR: case TOK_VARPTR:
        case TOK_EXIST: case TOK_DFREE: case TOK_DIR_TOK: case TOK_DIR_TOK2:
        case TOK_FSFIRST: case TOK_FSNEXT:
        case TOK_FNAME: case TOK_FATTR: case TOK_FPOS: case TOK_SIZE_TOK:
        case TOK_FRE: case TOK_HIMEM:
        case TOK_BYTE_TOK: case TOK_CARD: case TOK_WORD_TOK: case TOK_LONG_TOK:
        case TOK_POINT: case TOK_PTST:
        case TOK_GEMDOS: case TOK_BIOS: case TOK_XBIOS:
        case TOK_VOID: case TOK_TILDE:
        case TOK_MALLOC: case TOK_MFREE:
        case TOK_TYPE_TOK: case TOK_TT: case TOK_STE:
        case TOK_OB_X: case TOK_OB_Y: case TOK_OB_W: case TOK_OB_H:
        case TOK_BTST: case TOK_BSET: case TOK_BCLR: case TOK_BCHG:
        case TOK_SHL: case TOK_SHR: case TOK_ROL: case TOK_ROR:
        case TOK_MKI_TOK: case TOK_MKL_TOK: case TOK_MKS_TOK:
        case TOK_MKF_TOK: case TOK_MKD_TOK:
        case TOK_CVI_TOK: case TOK_CVL_TOK: case TOK_CVS_TOK:
        case TOK_CVF_TOK: case TOK_CVD_TOK:
        case TOK_PRED: case TOK_SUCC:
        case TOK_DEG: case TOK_RAD:
        case TOK_SOUND: case TOK_MOUSE: case TOK_SETMOUSE:
        case TOK_CONIN:
        case TOK_STICK: case TOK_STRIG:
        case TOK_PADX: case TOK_PADY: case TOK_PADT:
        case TOK_LPENX: case TOK_LPENY: case TOK_TOUCH:
        case TOK_KEYGET: case TOK_KEYLOOK: case TOK_KEYTEST:
        case TOK_KEYPRESS: case TOK_KEYPAD:
        case TOK_CEIL_TOK: case TOK_TRUNC_TOK:
        case TOK_SINGLE: case TOK_DOUBLE_TOK:
        case TOK_INPUT_TOK: case TOK_INPMID:
        case TOK_VAL_COUNT:
        case TOK_RANDOM:
        case TOK_PAUSE: case TOK_DELAY:
        case TOK_CRSCOL: case TOK_CRSLIN:
        case TOK_CONTRL: case TOK_INTIN: case TOK_INTOUT:
        case TOK_PTSIN: case TOK_PTSOUT:
        case TOK_GINTIN: case TOK_GINTOUT: case TOK_WORK_OUT:
        case TOK_APPL_INIT: case TOK_APPL_EXIT: case TOK_APPL_FIND:
        case TOK_FORM_ALERT: case TOK_MENU_BAR: case TOK_WIND_OPEN:
        case TOK_WIND_CLOSE: case TOK_EVNT_KEYBD:
        case TOK_BGET: case TOK_BPUT:
        case TOK_UCASE: case TOK_INSERT: case TOK_POS:
        case TOK_GEMSYS: case TOK_VDISYS:
        case TOK_SHEL_READ: case TOK_SHEL_WRITE: case TOK_SHEL_GET:
        case TOK_SHEL_PUT: case TOK_SHEL_FIND: case TOK_SHEL_ENVRN:
        case TOK_APPL_READ: case TOK_APPL_WRITE:
        case TOK_APPL_TPLAY: case TOK_APPL_TRECORD:
        case TOK_FORM_BUTTON: case TOK_FORM_CENTER: case TOK_FORM_DIAL:
        case TOK_FORM_DO: case TOK_FORM_ERROR: case TOK_FORM_KEYBD:
        case TOK_FORM_INPUT:
        case TOK_MENU: case TOK_MENU_KILL: case TOK_MENU_OFF:
        case TOK_MENU_ICHECK: case TOK_MENU_IENABLE: case TOK_MENU_REGISTER:
        case TOK_MENU_TEXT: case TOK_MENU_TNORMAL:
        case TOK_WIND_DELETE: case TOK_WIND_FIND:
        case TOK_WIND_CREATE: case TOK_WIND_CALC: case TOK_WIND_GET:
        case TOK_WIND_SET: case TOK_WIND_UPDATE:
        case TOK_EVNT_MOUSE: case TOK_EVNT_MULTI: case TOK_EVNT_MESAG:
        case TOK_EVNT_BUTTON: case TOK_EVNT_TIMER: case TOK_EVNT_DCLICK:
        case TOK_OBJC_ADD: case TOK_OBJC_CHANGE: case TOK_OBJC_DRAW:
        case TOK_OBJC_DELETE: case TOK_OBJC_ADDMOVE: case TOK_OBJC_MOVE:
        case TOK_OBJC_FIND: case TOK_OBJC_OFFSET: case TOK_OBJC_PICK:
        case TOK_OBJC_STATE: case TOK_OBJC_EDIT:
        case TOK_RSRC_LOAD: case TOK_RSRC_FREE: case TOK_RSRC_GADDR:
        case TOK_RSRC_SADDR: case TOK_RSRC_OBFIX:
        case TOK_RCALL: case TOK_RC_COPY: case TOK_RC_INTERSECT:
        case TOK_GRAF_DRAGBOX:
            return parse_builtin_call(parser, tok);

        default:
            PARSER_ERROR(parser, "Unexpected token in expression");
            gfa_lexer_next(parser->lexer);
            return NULL;
    }
}
