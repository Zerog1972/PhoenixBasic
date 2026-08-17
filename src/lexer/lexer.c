/*
 * lexer.c - Implementation de l'analyseur lexical GFA Basic 3.5
 * =============================================================
 * Tokenisation complete du GFA Basic :
 *   - Mots-cles insensibles a la casse
 *   - Nombres decimaux, hexadecimaux (&H), binaires (&X), octaux (&O)
 *   - Chaines de caracteres avec guillemets doubles
 *   - Commentaires REM et apostrophe
 *   - Operateurs et separateurs
 *   - Abreviations de commandes (p -> PRINT, ? -> PRINT)
 *
 * Reference : cahier-des-charges-gfabasic.md, section 3
 */

#include "lexer.h"
#include "keywords.h"
#include "os_layer.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Macros utilitaires                                                 */
/* ------------------------------------------------------------------ */

#define LEXER_BUF_SIZE  256

#define CURRENT_CHAR(lex)  ((lex)->pos < (lex)->src_len ? \
                            (lex)->source[(lex)->pos] : '\0')
#define PEEK_CHAR(lex, off) (((lex)->pos + (off)) < (lex)->src_len ? \
                              (lex)->source[(lex)->pos + (off)] : '\0')
#define ADVANCE(lex)  do { (lex)->pos++; (lex)->column++; } while(0)
#define IS_EOL(c)      ((c) == '\n' || (c) == '\r')

/* ------------------------------------------------------------------ */
/* Tables d'abreviations (1 lettre -> mot-cle)                        */
/* ------------------------------------------------------------------ */

typedef struct {
    char letter;
    const char *keyword;
} gfa_abbrev;

static const gfa_abbrev g_abbrevs[] = {
    {'?', "PRINT"},
    {'p', "PRINT"},
    {'g', "GOTO"},
    {'e', "ELSE"},
    {'w', "WHILE"},
    {'u', "UNTIL"},
    {'n', "NEXT"},
    {'r', "RETURN"},
    {'l', "LET"},
    {'s', "STEP"},
    {'d', "DIM"},
    {'c', "CASE"},
    {'t', "THEN"},
    {'i', "INPUT"},
    {'f', "FOR"},
    {'o', "OPEN"},
    {'q', "QUIT"},
    {'x', "EXIT"},
    {'m', "MOD"},
    {'h', "THEN"},
    {0, NULL}
};

/* ------------------------------------------------------------------ */
/* Forward declarations                                               */
/* ------------------------------------------------------------------ */

static void skip_whitespace(gfa_lexer *lexer);
static gfa_token_type scan_token(gfa_lexer *lexer);
static gfa_token_type scan_number(gfa_lexer *lexer);
static gfa_token_type scan_string(gfa_lexer *lexer);
static gfa_token_type scan_identifier(gfa_lexer *lexer);
static gfa_token_type scan_hex_number(gfa_lexer *lexer);
static gfa_token_type scan_bin_number(gfa_lexer *lexer);
static gfa_token_type scan_octal_number(gfa_lexer *lexer);
static void free_token_value(gfa_token *token);

/* ------------------------------------------------------------------ */
/* Initialisation / Liberation                                        */
/* ------------------------------------------------------------------ */

gfa_lexer *gfa_lexer_init(const char *source)
{
    gfa_lexer *lexer;

    if (source == NULL) return NULL;

    lexer = (gfa_lexer *)malloc(sizeof(gfa_lexer));
    if (lexer == NULL) return NULL;

    memset(lexer, 0, sizeof(gfa_lexer));

    lexer->source    = source;
    lexer->src_len   = (int)strlen(source);
    lexer->pos       = 0;
    lexer->line      = 1;
    lexer->column    = 1;
    lexer->line_start = 0;
    lexer->has_peek  = 0;
    lexer->at_stmt_start = 1;
    lexer->error     = LEX_OK;
    lexer->expand_abbrev = 1;

    /* Initialiser les tokens */
    memset(&lexer->current, 0, sizeof(gfa_token));
    memset(&lexer->peek, 0, sizeof(gfa_token));
    lexer->current.type = TOK_EOF;
    lexer->peek.type    = TOK_EOF;

    return lexer;
}

void gfa_lexer_free(gfa_lexer *lexer)
{
    if (lexer == NULL) return;
    free_token_value(&lexer->current);
    free_token_value(&lexer->peek);
    free(lexer);
}

/* ------------------------------------------------------------------ */
/* Gestion des tokens                                                 */
/* ------------------------------------------------------------------ */

static void free_token_value(gfa_token *token)
{
    if (token == NULL) return;

    if (token->type == TOK_STRING || token->type == TOK_IDENTIFIER ||
        token->type == TOK_LABEL) {
        if (token->value.string_value != NULL) {
            free(token->value.string_value);
            token->value.string_value = NULL;
        }
    }
    if (token->type == TOK_IDENTIFIER || token->type == TOK_LABEL) {
        if (token->value.ident_name != NULL) {
            free(token->value.ident_name);
            token->value.ident_name = NULL;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Avancement dans le source                                          */
/* ------------------------------------------------------------------ */

static void skip_whitespace(gfa_lexer *lexer)
{
    char c;

    for (;;) {
        c = CURRENT_CHAR(lexer);

        /* Espaces et tabulations */
        if (c == ' ' || c == '\t') {
            ADVANCE(lexer);
            continue;
        }

        /* Les sauts de ligne NE sont PAS consommes ici :
           ils sont retournes comme TOK_EOL par scan_token.
           Ceci permet au parser de distinguer IF inline
           (THEN suivi d'une instruction sur la meme ligne)
           de IF multi-lignes (THEN suivi de EOL). */
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Scanner principal                                                  */
/* ------------------------------------------------------------------ */

gfa_token_type gfa_lexer_next(gfa_lexer *lexer)
{
    gfa_token_type type;

    if (lexer == NULL) return TOK_EOF;

    /* Si on a un token en avance, le consommer */
    if (lexer->has_peek) {
        free_token_value(&lexer->current);
        lexer->current = lexer->peek;
        memset(&lexer->peek, 0, sizeof(gfa_token));
        lexer->peek.type = TOK_EOF;
        lexer->has_peek = 0;
        lexer->at_stmt_start =
            (lexer->current.type == TOK_EOL ||
             lexer->current.type == TOK_COLON);
        return lexer->current.type;
    }

    /* Scanner le prochain token */
    skip_whitespace(lexer);

    /* Nettoyer les champs dynamiques du token precedent (eviter
       les ident_name/string_value residuels entre tokens) */
    free_token_value(&lexer->current);

    if (lexer->pos >= lexer->src_len) {
        lexer->current.type = TOK_EOF;
        lexer->current.line = lexer->line;
        lexer->current.column = lexer->column;
        lexer->at_stmt_start = 0;
        return TOK_EOF;
    }

    type = scan_token(lexer);
    lexer->current.type = type;
    lexer->at_stmt_start = (type == TOK_EOL || type == TOK_COLON);

    return type;
}

gfa_token_type gfa_lexer_peek_token(gfa_lexer *lexer)
{
    if (lexer == NULL) return TOK_EOF;

    if (!lexer->has_peek) {
        /* Sauvegarder l'etat */
        int saved_pos, saved_line, saved_col, saved_start;
        gfa_token saved_current;
        lexer_error saved_error;

        saved_pos   = lexer->pos;
        saved_line  = lexer->line;
        saved_col   = lexer->column;
        saved_start = lexer->line_start;
        saved_error = lexer->error;
        saved_current = lexer->current;
        memset(&lexer->current, 0, sizeof(gfa_token));

        /* Scanner le token suivant */
        lexer->peek = lexer->current;
        lexer->peek.type = gfa_lexer_next(lexer);
        {
            /* Transferer vers peek */
            lexer->peek = lexer->current;
        }

        /* Restaurer l'etat : le token scanné appartient maintenant
           a peek (copie superficielle) — ne pas le liberer ici. */
        lexer->current = saved_current;
        lexer->pos     = saved_pos;
        lexer->line    = saved_line;
        lexer->column  = saved_col;
        lexer->line_start = saved_start;
        lexer->error   = saved_error;
        lexer->has_peek = 1;
    }

    return lexer->peek.type;
}

gfa_token_type gfa_lexer_current_token(gfa_lexer *lexer)
{
    if (lexer == NULL) return TOK_EOF;
    return lexer->current.type;
}

int gfa_lexer_expect(gfa_lexer *lexer, gfa_token_type type)
{
    if (lexer == NULL) return 0;

    if (lexer->current.type == type) {
        gfa_lexer_next(lexer);
        return 1;
    }

    sprintf(lexer->error_msg, "Expected '%s', got '%s'",
            gfa_keyword_get_name(type),
            gfa_keyword_get_name(lexer->current.type));
    lexer->error = LEX_ERR_UNEXPECTED_CHAR;
    lexer->error_line = lexer->line;
    lexer->error_column = lexer->column;

    return 0;
}

void gfa_lexer_skip_to_eol(gfa_lexer *lexer)
{
    char c;
    if (lexer == NULL) return;

    c = CURRENT_CHAR(lexer);
    while (c != '\0' && !IS_EOL(c)) {
        ADVANCE(lexer);
        c = CURRENT_CHAR(lexer);
    }
}

/* ------------------------------------------------------------------ */
/* Acces aux erreurs                                                  */
/* ------------------------------------------------------------------ */

lexer_error gfa_lexer_get_error(gfa_lexer *lexer)
{
    if (lexer == NULL) return LEX_OK;
    return lexer->error;
}

const char *gfa_lexer_get_error_msg(gfa_lexer *lexer)
{
    if (lexer == NULL) return "";
    return lexer->error_msg;
}

int gfa_lexer_get_line(gfa_lexer *lexer)
{
    if (lexer == NULL) return 0;
    return lexer->line;
}

int gfa_lexer_get_column(gfa_lexer *lexer)
{
    if (lexer == NULL) return 0;
    return lexer->column;
}

void gfa_lexer_set_expand(gfa_lexer *lexer, int expand)
{
    if (lexer != NULL) {
        lexer->expand_abbrev = expand;
    }
}

/* ------------------------------------------------------------------ */
/* Scanner d'un token individuel                                      */
/* ------------------------------------------------------------------ */

static gfa_token_type scan_token(gfa_lexer *lexer)
{
    char c;
    int start_line, start_col;

    c = CURRENT_CHAR(lexer);
    start_line = lexer->line;
    start_col  = lexer->column;

    /* Fin de source */
    if (c == '\0') {
        lexer->current.line = start_line;
        lexer->current.column = start_col;
        return TOK_EOF;
    }

    /* Commentaire REM */
    if (c == 'R' || c == 'r') {
        char c2, c3;
        c2 = PEEK_CHAR(lexer, 1);
        c3 = PEEK_CHAR(lexer, 2);
        if ((c2 == 'E' || c2 == 'e') && (c3 == 'M' || c3 == 'm')) {
            /* Verifier que ce n'est pas RENAME ou un autre mot-cle */
            char c4;
            c4 = PEEK_CHAR(lexer, 3);
            if (!isalnum((unsigned char)c4) && c4 != '_') {
                /* C'est bien REM */
                gfa_lexer_skip_to_eol(lexer);
                lexer->current.line = start_line;
                lexer->current.column = start_col;
                return TOK_REM;
            }
            /* Sinon, c'est un identifiant qui commence par REM */
        }
    }

    /* Commentaire apostrophe */
    if (c == '\'') {
        ADVANCE(lexer);
        gfa_lexer_skip_to_eol(lexer);
        lexer->current.line = start_line;
        lexer->current.column = start_col;
        return TOK_REM;
    }

    /* Commentaire point d'exclamation (fin de ligne) */
    if (c == '!') {
        ADVANCE(lexer);
        gfa_lexer_skip_to_eol(lexer);
        lexer->current.line = start_line;
        lexer->current.column = start_col;
        return TOK_REM;
    }

    /* Chaine de caracteres */
    if (c == '"') {
        return scan_string(lexer);
    }

    /* Nombres (decimaux, hexa &H, binaire &X, octal &O) */
    if (isdigit((unsigned char)c)) {
        return scan_number(lexer);
    }

    /* &H &X &O - prefixes hexa/binaire/octal */
    if (c == '&') {
        char c2;
        c2 = PEEK_CHAR(lexer, 1);
        if (c2 == 'H' || c2 == 'h') {
            ADVANCE(lexer); ADVANCE(lexer);
            return scan_hex_number(lexer);
        }
        if (c2 == 'X' || c2 == 'x') {
            ADVANCE(lexer); ADVANCE(lexer);
            return scan_bin_number(lexer);
        }
        if (c2 == 'O' || c2 == 'o') {
            ADVANCE(lexer); ADVANCE(lexer);
            return scan_octal_number(lexer);
        }

        /* & seul = operateur de concatenation de chaines
           (ou operateur de concatenation binaire en GFA) */
        ADVANCE(lexer);
        lexer->current.line = start_line;
        lexer->current.column = start_col;
        return TOK_AMPERSAND;
    }

    /* Identifiants et mots-cles */
    if (isalpha((unsigned char)c) || c == '_') {
        return scan_identifier(lexer);
    }

    /* Operateurs et separateurs */
    ADVANCE(lexer);

    switch (c) {
        case '+':
            lexer->current.line = start_line;
            lexer->current.column = start_col;
            return TOK_PLUS;

        case '-':
            lexer->current.line = start_line;
            lexer->current.column = start_col;
            return TOK_MINUS;

        case '*':
            lexer->current.line = start_line;
            lexer->current.column = start_col;
            return TOK_STAR;

        case '/':
            lexer->current.line = start_line;
            lexer->current.column = start_col;
            return TOK_SLASH;

        case '^':
            lexer->current.line = start_line;
            lexer->current.column = start_col;
            return TOK_CARET;

        case '(':
            lexer->current.line = start_line;
            lexer->current.column = start_col;
            return TOK_LPAREN;

        case ')':
            lexer->current.line = start_line;
            lexer->current.column = start_col;
            return TOK_RPAREN;

        case ',':
            lexer->current.line = start_line;
            lexer->current.column = start_col;
            return TOK_COMMA;

        case ';':
            lexer->current.line = start_line;
            lexer->current.column = start_col;
            return TOK_SEMICOLON;

        case ':':
            lexer->current.line = start_line;
            lexer->current.column = start_col;
            return TOK_COLON;

        case '#':
            lexer->current.line = start_line;
            lexer->current.column = start_col;
            return TOK_HASH;

        case '@':
            lexer->current.line = start_line;
            lexer->current.column = start_col;
            return TOK_AT;

        case '{':
            lexer->current.line = start_line;
            lexer->current.column = start_col;
            return TOK_LBRACE;

        case '}':
            lexer->current.line = start_line;
            lexer->current.column = start_col;
            return TOK_RBRACE;

        case '~':
            lexer->current.line = start_line;
            lexer->current.column = start_col;
            return TOK_TILDE;

        case '=':
            if (CURRENT_CHAR(lexer) == '=') {
                ADVANCE(lexer);
                lexer->current.line = start_line;
                lexer->current.column = start_col;
                return TOK_APPROX_EQ;
            }
            lexer->current.line = start_line;
            lexer->current.column = start_col;
            return TOK_EQ;

        case '<':
            if (CURRENT_CHAR(lexer) == '=') {
                ADVANCE(lexer);
                lexer->current.line = start_line;
                lexer->current.column = start_col;
                return TOK_LE;
            }
            if (CURRENT_CHAR(lexer) == '>') {
                ADVANCE(lexer);
                lexer->current.line = start_line;
                lexer->current.column = start_col;
                return TOK_NE;
            }
            lexer->current.line = start_line;
            lexer->current.column = start_col;
            return TOK_LT;

        case '>':
            if (CURRENT_CHAR(lexer) == '=') {
                ADVANCE(lexer);
                lexer->current.line = start_line;
                lexer->current.column = start_col;
                return TOK_GE;
            }
            lexer->current.line = start_line;
            lexer->current.column = start_col;
            return TOK_GT;

        case '\r':
            /* CR : ignorer LF suivant si present (\r\n -> un seul EOL) */
            if (CURRENT_CHAR(lexer) == '\n') ADVANCE(lexer);
            lexer->line++;
            lexer->column = 1;
            lexer->line_start = lexer->pos;
            lexer->current.line = start_line;
            lexer->current.column = start_col;
            return TOK_EOL;

        case '\n':
            lexer->line++;
            lexer->column = 1;
            lexer->line_start = lexer->pos;
            lexer->current.line = start_line;
            lexer->current.column = start_col;
            return TOK_EOL;

        case '\'':
            /* Deja traite plus haut, mais au cas ou */
            lexer->current.line = start_line;
            lexer->current.column = start_col;
            return TOK_APOSTROPHE;

        default:
            /* Caractere inattendu */
            lexer->current.line = start_line;
            lexer->current.column = start_col;
            sprintf(lexer->error_msg, "Unexpected character: '%c' (0x%02X)",
                    c, (unsigned char)c);
            lexer->error = LEX_ERR_UNEXPECTED_CHAR;
            lexer->error_line = start_line;
            lexer->error_column = start_col;
            return TOK_EOF;
    }
}

/* ------------------------------------------------------------------ */
/* Scanner de nombres (decimal)                                       */
/* ------------------------------------------------------------------ */

static gfa_token_type scan_number(gfa_lexer *lexer)
{
    char buf[LEXER_BUF_SIZE];
    int i;
    int is_float;
    char c;
    int start_line, start_col;

    i = 0;
    is_float = 0;
    start_line = lexer->line;
    start_col  = lexer->column;

    /* Partie entiere */
    c = CURRENT_CHAR(lexer);
    while (isdigit((unsigned char)c) && i < LEXER_BUF_SIZE - 1) {
        buf[i++] = c;
        ADVANCE(lexer);
        c = CURRENT_CHAR(lexer);
    }

    /* Partie decimale */
    if (c == '.') {
        is_float = 1;
        buf[i++] = c;
        ADVANCE(lexer);
        c = CURRENT_CHAR(lexer);
        while (isdigit((unsigned char)c) && i < LEXER_BUF_SIZE - 1) {
            buf[i++] = c;
            ADVANCE(lexer);
            c = CURRENT_CHAR(lexer);
        }
    }

    /* Exposant */
    if (c == 'E' || c == 'e') {
        is_float = 1;
        buf[i++] = c;
        ADVANCE(lexer);
        c = CURRENT_CHAR(lexer);
        if (c == '+' || c == '-') {
            buf[i++] = c;
            ADVANCE(lexer);
            c = CURRENT_CHAR(lexer);
        }
        while (isdigit((unsigned char)c) && i < LEXER_BUF_SIZE - 1) {
            buf[i++] = c;
            ADVANCE(lexer);
            c = CURRENT_CHAR(lexer);
        }
    }

    /* Suffixe de type du littéral : %, &, |, !, #, $, ?
       Consommer le caractere et typer la valeur (GFA : 100%, 30&, 255|, 2.5#, 123$). */
    if (c == '%' || c == '&' || c == '|' || c == '!' || c == '#'
        || c == '$' || c == '?') {
        char suffix = c;
        long v;

        ADVANCE(lexer);

        buf[i] = '\0';
        lexer->current.line = start_line;
        lexer->current.column = start_col;

        if (suffix == '$') {
            /* Littéral chaine : "123$" -> "123" */
            lexer->current.value.string_value = os_strdup(buf);
            return TOK_STRING;
        }
        if (suffix == '#' || suffix == '!' || suffix == '?') {
            lexer->current.value.float_value = atof(buf);
            return TOK_FLOAT;
        }
        v = atol(buf);
        if (suffix == '&') {
            unsigned long u = (unsigned long)v & 0xFFFFUL;
            v = (u > 32767UL) ? (long)(u - 65536UL) : (long)u;
        } else if (suffix == '|') {
            v = (long)((unsigned long)v & 0xFFUL);
        }
        lexer->current.value.int_value = v;
        return TOK_INTEGER;
    }

    buf[i] = '\0';

    lexer->current.line = start_line;
    lexer->current.column = start_col;

    if (is_float) {
        lexer->current.value.float_value = atof(buf);
        return TOK_FLOAT;
    } else {
        lexer->current.value.int_value = atol(buf);
        return TOK_INTEGER;
    }
}

/* ------------------------------------------------------------------ */
/* Scanner de chaines                                                 */
/* ------------------------------------------------------------------ */

static gfa_token_type scan_string(gfa_lexer *lexer)
{
    char buf[LEXER_BUF_SIZE];
    int i;
    char c;
    int start_line, start_col;

    i = 0;
    start_line = lexer->line;
    start_col  = lexer->column;

    /* Passer le guillemet ouvrant */
    ADVANCE(lexer);

    c = CURRENT_CHAR(lexer);
    while (c != '\0' && c != '"' && i < LEXER_BUF_SIZE - 1) {
        /* Guillemet echappe : "" -> " */
        if (c == '"' && PEEK_CHAR(lexer, 1) == '"') {
            buf[i++] = '"';
            ADVANCE(lexer);
            ADVANCE(lexer);
            c = CURRENT_CHAR(lexer);
            continue;
        }

        if (IS_EOL(c)) {
            lexer->error = LEX_ERR_UNTERMINATED_STRING;
            lexer->error_line = start_line;
            lexer->error_column = start_col;
            sprintf(lexer->error_msg, "Unterminated string");
            break;
        }

        buf[i++] = c;
        ADVANCE(lexer);
        c = CURRENT_CHAR(lexer);
    }

    buf[i] = '\0';

    /* Passer le guillemet fermant */
    if (c == '"') {
        ADVANCE(lexer);
    }

        lexer->current.line = start_line;
    lexer->current.column = start_col;
    lexer->current.value.string_value = os_strdup(buf);

    return TOK_STRING;
}

/* ------------------------------------------------------------------ */
/* Scanner d'identifiants et mots-cles                                */
/* ------------------------------------------------------------------ */

static gfa_token_type scan_identifier(gfa_lexer *lexer)
{
    char buf[LEXER_BUF_SIZE];
    int i;
    char c;
    int start_line, start_col;
    gfa_token_type keyword_type;

    i = 0;
    start_line = lexer->line;
    start_col  = lexer->column;

    c = CURRENT_CHAR(lexer);
    while ((isalnum((unsigned char)c) || c == '_' || c == '.') &&
           i < LEXER_BUF_SIZE - 1) {
        buf[i++] = c;
        ADVANCE(lexer);
        c = CURRENT_CHAR(lexer);
    }

    /* Verifier les suffixes de type ($, %, &, !, |, #)
       et ? (fonctions type VAL?) */
    if (c == '$' || c == '%' || c == '&' || c == '!' || c == '|' || c == '#' || c == '?') {
        buf[i++] = c;
        ADVANCE(lexer);
    }

    buf[i] = '\0';

    /* "END IF" (avec espace) -> ENDIF. Ne sauter que des espaces
       (pas de retour a la ligne : un IF sur la ligne suivante
       ne fait pas partie du END). */
    if (os_str_iequal(buf, "END")) {
        int p, found;
        p = lexer->pos;
        while (p < lexer->src_len &&
               (lexer->source[p] == ' ' || lexer->source[p] == '\t')) {
            p++;
        }
        found = (p + 1 < lexer->src_len) &&
                toupper((unsigned char)lexer->source[p]) == 'I' &&
                toupper((unsigned char)lexer->source[p + 1]) == 'F' &&
                (p + 2 >= lexer->src_len ||
                 !isalnum((unsigned char)lexer->source[p + 2]));
        if (found) {
            lexer->pos = p + 2;
            strcpy(buf, "ENDIF");
            i = 5;
        }
    }

    lexer->current.line = start_line;
    lexer->current.column = start_col;

    /* Verifier si le token suivant est ':' -> etiquette.
       Ce test DOIT etre avant le lookup des mots-cles car
       un label peut avoir le meme nom qu'un mot-cle (ex: sub:) */
    {
        int saved_pos, saved_col;
        char saved_c;

        saved_pos  = lexer->pos;
        saved_col  = lexer->column;
        saved_c    = CURRENT_CHAR(lexer);

        /* Sauter les espaces */
        while (saved_c == ' ' || saved_c == '\t') {
            saved_pos++;
            saved_col++;
            if (saved_pos < lexer->src_len) {
                saved_c = lexer->source[saved_pos];
            } else {
                saved_c = '\0';
            }
        }

        if (lexer->at_stmt_start && saved_c == ':') {
            /* C'est une etiquette, consommer le ':' */
            lexer->pos     = saved_pos + 1;
            lexer->column  = saved_col + 1;
            lexer->current.value.ident_name = os_strdup(buf);
            return TOK_LABEL;
        }
    }

    /* Verifier si c'est un mot-cle */
    keyword_type = gfa_keyword_lookup(buf);
    if (keyword_type != TOK_IDENTIFIER) {
        return keyword_type;
    }

    /* Verifier les abreviations */
    if (lexer->expand_abbrev && i == 1) {
        int j;
        for (j = 0; g_abbrevs[j].letter != 0; j++) {
            if (tolower((unsigned char)buf[0]) ==
                tolower((unsigned char)g_abbrevs[j].letter)) {
                keyword_type = gfa_keyword_lookup(g_abbrevs[j].keyword);
                if (keyword_type != TOK_IDENTIFIER) {
                    return keyword_type;
                }
            }
        }
    }

    /* Identifiant normal */
    lexer->current.value.ident_name = os_strdup(buf);
    return TOK_IDENTIFIER;
}

/* ------------------------------------------------------------------ */
/* Scanners de nombres speciaux                                       */
/* ------------------------------------------------------------------ */

static gfa_token_type scan_hex_number(gfa_lexer *lexer)
{
    long value;
    char c;
    int digits;
    int start_line, start_col;

    value = 0;
    digits = 0;
    start_line = lexer->line;
    start_col  = lexer->column;

    c = CURRENT_CHAR(lexer);
    while (isxdigit((unsigned char)c)) {
        value = value * 16;
        if (isdigit((unsigned char)c)) {
            value += (long)(c - '0');
        } else {
            value += (long)(toupper((unsigned char)c) - 'A' + 10);
        }
        digits++;
        ADVANCE(lexer);
        c = CURRENT_CHAR(lexer);
    }

    if (digits == 0) {
        lexer->error = LEX_ERR_HEX_DIGIT_EXPECTED;
        lexer->error_line = start_line;
        lexer->error_column = start_col;
    }

    lexer->current.line = start_line;
    lexer->current.column = start_col;
    lexer->current.value.int_value = value;

    return TOK_INTEGER;
}

static gfa_token_type scan_bin_number(gfa_lexer *lexer)
{
    long value;
    char c;
    int digits;
    int start_line, start_col;

    value = 0;
    digits = 0;
    start_line = lexer->line;
    start_col  = lexer->column;

    c = CURRENT_CHAR(lexer);
    while (c == '0' || c == '1') {
        value = (value << 1) | (long)(c - '0');
        digits++;
        ADVANCE(lexer);
        c = CURRENT_CHAR(lexer);
    }

    if (digits == 0) {
        lexer->error = LEX_ERR_BIN_DIGIT_EXPECTED;
    }

    lexer->current.line = start_line;
    lexer->current.column = start_col;
    lexer->current.value.int_value = value;

    return TOK_INTEGER;
}

static gfa_token_type scan_octal_number(gfa_lexer *lexer)
{
    long value;
    char c;
    int digits;
    int start_line, start_col;

    value = 0;
    digits = 0;
    start_line = lexer->line;
    start_col  = lexer->column;

    c = CURRENT_CHAR(lexer);
    while (c >= '0' && c <= '7') {
        value = (value << 3) | (long)(c - '0');
        digits++;
        ADVANCE(lexer);
        c = CURRENT_CHAR(lexer);
    }

    if (digits == 0) {
        lexer->error = LEX_ERR_OCTAL_DIGIT_EXPECTED;
    }

    lexer->current.line = start_line;
    lexer->current.column = start_col;
    lexer->current.value.int_value = value;

    return TOK_INTEGER;
}
