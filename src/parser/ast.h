/*
 * ast.h - Arbre syntaxique abstrait (AST) GFA Basic 3.5
 * ======================================================
 * Definit les types de noeuds de l'AST pour la representation
 * intermediaire entre le parser et le generateur de bytecode.
 *
 * Reference : cahier-des-charges-gfabasic.md, section 5
 */

#ifndef GFA_AST_H
#define GFA_AST_H

#include "token.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Types de noeuds                                                    */
/* ------------------------------------------------------------------ */

typedef enum {
    /* Programme */
    AST_PROGRAM,          /* Liste d'instructions (racine)           */
    AST_LINE_NUMBER,      /* Numero de ligne optionnel               */

    /* Assignation */
    AST_ASSIGN,           /* var = expression                        */

    /* Controle de flux */
    AST_IF,               /* IF ... THEN ... [ELSE ...] ENDIF        */
    AST_FOR,              /* FOR var = start TO end [STEP s] ... NEXT*/
    AST_WHILE,            /* WHILE cond ... WEND                     */
    AST_REPEAT,           /* REPEAT ... UNTIL cond                   */
    AST_DO_LOOP,          /* DO ... LOOP [WHILE|UNTIL cond]          */
    AST_EXIT_IF,          /* EXIT IF cond                            */
    AST_GOTO,             /* GOTO label                              */
    AST_GOSUB,            /* GOSUB label                             */
    AST_RETURN,           /* RETURN                                  */
    AST_ON_GOTO_GOSUB,    /* ON expr GOTO/GOSUB label1, label2, ...  */
    AST_SELECT,           /* SELECT expr ... CASE ... ENDSELECT      */
    AST_CASE,             /* CASE expr                               */
    AST_DEFAULT_CASE,     /* DEFAULT                                 */
    AST_STOP,             /* STOP                                    */
    AST_END,              /* END                                     */
    AST_QUIT,             /* QUIT                                    */

    /* Definitions */
    AST_LET,              /* [LET] var = expr                        */
    AST_DIM,              /* DIM arr(dim1, ...)                      */
    AST_ERASE,            /* ERASE arr                               */
    AST_CLEAR,            /* CLEAR                                   */
    AST_OPTION_BASE,      /* OPTION BASE n                           */
    AST_PROCEDURE,        /* PROCEDURE nom[(args)]                   */
    AST_FUNCTION_DEF,     /* FUNCTION nom[(args)]                    */
    AST_ENDFUNC,          /* ENDFUNC                                 */
    AST_LOCAL,            /* LOCAL var1, var2, ...                   */
    AST_DEFFN,            /* DEFFN f(x) = expr                       */
    AST_DEFFN_RET,        /* RETURN d'un FN multi-lignes : pousse la
                             valeur de la variable nom puis OP_RET    */
    AST_FN_CALL,          /* FN f(args)                              */
    AST_DEFBIT, AST_DEFBYT, AST_DEFWRD, AST_DEFNUM,
    AST_DEFFLT, AST_DEFSTR, AST_DEFDBL,

    /* Donnees */
    AST_DATA,             /* DATA val1, val2, ...                    */
    AST_READ,             /* READ var1, var2, ...                    */
    AST_RESTORE,          /* RESTORE [label]                         */

    /* I/O */
    AST_PRINT,            /* PRINT [#n,] expr...                     */
    AST_PRINT_AT,         /* PRINT AT(x,y); expr                     */
    AST_PRINT_USING,      /* PRINT USING fmt$; expr                  */
    AST_PRINT_SEP,        /* Separateur PRINT : 0 = ';' , 1 = ','    */
    AST_INPUT,            /* INPUT [prompt;] var...                  */
    AST_LINE_INPUT,       /* LINE INPUT [prompt;] var$               */
    AST_CLS,              /* CLS                                     */
    AST_LOCATE,           /* LOCATE x, y                             */
    AST_VTAB,             /* VTAB line                               */

    /* Fichiers */
    AST_OPEN,             /* OPEN mode$, #n, "fichier"               */
    AST_CLOSE,            /* CLOSE [#n]                              */
    AST_OPENW,            /* OPENW n                                 */
    AST_CLOSEW,           /* CLOSEW n                                */

    /* Graphisme */
    AST_COLOR,            /* COLOR fg [, bg]                         */
    AST_LINE,             /* LINE x1,y1,x2,y2                        */
    AST_CIRCLE,           /* CIRCLE x,y,r                            */
    AST_BOX,              /* BOX x1,y1,x2,y2                         */
    AST_PBOX,             /* PBOX x1,y1,x2,y2                        */
    AST_PCIRCLE,          /* PCIRCLE x,y,r                           */

    /* Son */
    AST_SOUND,            /* SOUND ch, freq, dur, vol, env           */
    AST_BEEP,             /* BEEP                                    */

    /* Evenements */
    AST_EVERY,            /* EVERY t GOSUB label                     */
    AST_AFTER,            /* AFTER t GOSUB label                     */
    AST_ON_ERROR,         /* ON ERROR GOSUB label                    */
    AST_ON_BREAK,         /* ON BREAK GOSUB label                    */
    AST_ERROR,            /* ERROR n                                 */
    AST_FATAL,            /* FATAL n (like ERROR but blocks RESUME)  */
    AST_RESUME,           /* RESUME [NEXT]                           */
    AST_SETTIME,          /* SETTIME time$ [, date$]                 */
    AST_SWAP,             /* SWAP var1, var2                         */

    /* Memoire */
    AST_PEEK,             /* PEEK(addr)                              */
    AST_POKE,             /* POKE addr, val                          */
    AST_DPEEK,            /* DPEEK(addr)                             */
    AST_DPOKE,            /* DPOKE addr, val                         */
    AST_LPEEK,            /* LPEEK(addr)                             */
    AST_LPOKE,            /* LPOKE addr, val                         */
    AST_SPOKE,            /* SPOKE addr, val                         */
    AST_SDPOKE,           /* SDPOKE addr, val                        */
    AST_SLPOKE,           /* SLPOKE addr, val                        */

    /* Debug */
    AST_TRON,             /* TRON                                    */
    AST_TROFF,            /* TROFF                                   */
    AST_REM,              /* REM / ' / ! commentaire                 */

    /* Divers */
    AST_CALL,             /* Appel de procedure/builtin              */
    AST_VOID,             /* VOID expr                               */
    AST_TILDE,            /* ~expr                                   */
    AST_BLOAD,            /* BLOAD "fichier", adresse                 */
    AST_BSAVE,            /* BSAVE "fichier", debut, fin              */
    AST_BGET,             /* BGET #n, adresse, nb                    */
    AST_BPUT,             /* BPUT #n, adresse, nb                    */

    /* Labels */
    AST_LABEL,            /* etiquette:                              */

    /* Definitions graphiques */
    AST_DEFFILL, AST_DEFLINE, AST_DEFTEXT, AST_DEFMOUSE, AST_DEFMARK,

    /* Nouvelles instructions (2026-08) */
    AST_MAT,            /* MAT … : value.int_val = sous-op,
                           left = cible, body = src1, cond = src2,
                           step = valeur scalaire */
    AST_QSORT_STMT,     /* QSORT arr(), lo, hi                    */
    AST_INSERT_ELEM,    /* INSERT x(i), val                       */
    AST_DELETE_ELEM,    /* DELETE x(i)                            */
    AST_DRAW,           /* DRAW "prog" (turtle) ou DRAW(n)        */
    AST_WINDOW,         /* CLEARW/TITLEW/INFOW/TOPW/GETSIZE/MWOUT */
    AST_GFX_STMT,       /* HLINE/PELLIPSE/POLYLINE/ATEXT/…        */

    /* Noeud sentinelle pour les listes */
    AST_STATEMENT_LIST,

    AST_COUNT
} ast_node_type;

/* ------------------------------------------------------------------ */
/* Structure de noeud                                                 */
/* ------------------------------------------------------------------ */

typedef struct ast_node {
    ast_node_type type;
    int           line;          /* Ligne dans le source             */

    /*
     * Flags pour indiquer quels membres de l'union sont actifs.
     * Necessaire car int_val/float_val peuvent avoir le meme motif
     * binaire qu'un pointeur valide, causant des double-free.
     */
    int           has_str;       /* 1 si value.str_val est allouee   */
    int           has_ident;     /* 1 si value.ident est allouee     */

    /* Valeurs possibles */
    union {
        long    int_val;
        double  float_val;
        char   *str_val;        /* Chaine (allouee)                 */
        char   *ident;          /* Identifiant (alloue)             */
    } value;

    /* Enfants (liste chainee pour simplicite) */
    struct ast_node *left;       /* Premier enfant                   */
    struct ast_node *right;      /* Frere suivant                    */

    /* Pour les structures conditionnelles/boucles */
    struct ast_node *cond;       /* Condition                        */
    struct ast_node *body;       /* Corps                            */
    struct ast_node *else_body;  /* Branche ELSE                     */
    struct ast_node *step;       /* STEP pour FOR                    */
    struct ast_node *cases;      /* Liste de CASE pour SELECT        */

    /* Pour les appels de fonction */
    struct ast_node **args;      /* Tableau d'arguments              */
    int               arg_count;

} ast_node;

/* ------------------------------------------------------------------ */
/* API AST                                                            */
/* ------------------------------------------------------------------ */

/*
 * ast_create - Cree un noeud AST du type donne.
 */
ast_node *ast_create(ast_node_type type);

/*
 * ast_create_int - Cree un noeud avec une valeur entiere.
 */
ast_node *ast_create_int(ast_node_type type, long value);

/*
 * ast_create_float - Cree un noeud avec une valeur flottante.
 */
ast_node *ast_create_float(ast_node_type type, double value);

/*
 * ast_create_str - Cree un noeud avec une valeur chaine.
 */
ast_node *ast_create_str(ast_node_type type, const char *value);

/*
 * ast_create_ident - Cree un noeud identifiant.
 */
ast_node *ast_create_ident(ast_node_type type, const char *name);

/*
 * ast_add_child - Ajoute un enfant a un noeud.
 */
void ast_add_child(ast_node *parent, ast_node *child);

/*
 * ast_set_cond - Definit la condition d'un noeud (IF, WHILE...).
 */
void ast_set_cond(ast_node *node, ast_node *cond);

/*
 * ast_set_body - Definit le corps d'un noeud.
 */
void ast_set_body(ast_node *node, ast_node *body);

/*
 * ast_set_else - Definit la branche ELSE.
 */
void ast_set_else(ast_node *node, ast_node *else_body);

/*
 * ast_set_step - Definit le pas d'une boucle FOR.
 */
void ast_set_step(ast_node *node, ast_node *step);

/*
 * ast_add_arg - Ajoute un argument a un noeud d'appel.
 */
void ast_add_arg(ast_node *node, ast_node *arg);

/*
 * ast_free - Libere recursivement un arbre AST.
 */
void ast_free(ast_node *node);

/*
 * ast_dump - Affiche un arbre AST (debug).
 */
void ast_dump(ast_node *node, int indent);

/*
 * ast_node_type_name - Retourne le nom d'un type de noeud.
 */
const char *ast_node_type_name(ast_node_type type);

#ifdef __cplusplus
}
#endif

#endif /* GFA_AST_H */
