/*
 * ast.c - Implementation de l'AST GFA Basic 3.5
 * ==============================================
 * Creation, manipulation et liberation des noeuds de l'arbre
 * syntaxique abstrait.
 */

#include "ast.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Noms des types de noeuds (debug)                                   */
/* ------------------------------------------------------------------ */

static const char *g_ast_names[] = {
    "PROGRAM", "LINE_NUMBER",
    "ASSIGN",
    "IF", "FOR", "WHILE", "REPEAT", "DO_LOOP", "EXIT_IF",
    "GOTO", "GOSUB", "RETURN", "ON_GOTO_GOSUB",
    "SELECT", "CASE", "DEFAULT_CASE",
    "STOP", "END", "QUIT",
    "LET", "DIM", "ERASE", "CLEAR", "OPTION_BASE",
    "PROCEDURE", "FUNCTION_DEF", "ENDFUNC", "LOCAL",
    "DEFFN", "FN_CALL",
    "DEFBIT", "DEFBYT", "DEFWRD", "DEFNUM", "DEFFLT", "DEFSTR", "DEFDBL",
    "DATA", "READ", "RESTORE",
    "PRINT", "PRINT_AT", "PRINT_USING", "INPUT", "LINE_INPUT",
    "CLS", "LOCATE", "VTAB",
    "OPEN", "CLOSE", "OPENW", "CLOSEW",
    "COLOR", "LINE", "CIRCLE", "BOX", "PBOX", "PCIRCLE",
    "SOUND", "BEEP",
    "EVERY", "AFTER", "ON_ERROR", "ON_BREAK", "ERROR",
    "PEEK", "POKE", "DPEEK", "DPOKE", "LPEEK", "LPOKE",
    "TRON", "TROFF", "REM",
    "CALL", "VOID", "TILDE",
    "LABEL",
    "STATEMENT_LIST",
    "COUNT"
};

const char *ast_node_type_name(ast_node_type type)
{
    if (type < 0 || type >= AST_COUNT) return "UNKNOWN";
    return g_ast_names[type];
}

/* ------------------------------------------------------------------ */
/* Creation de noeuds                                                 */
/* ------------------------------------------------------------------ */

ast_node *ast_create(ast_node_type type)
{
    ast_node *node;

    node = (ast_node *)calloc(1, sizeof(ast_node));
    if (node == NULL) return NULL;

    node->type = type;
    node->line = 0;
    node->left = NULL;
    node->right = NULL;
    node->cond = NULL;
    node->body = NULL;
    node->else_body = NULL;
    node->step = NULL;
    node->cases = NULL;
    node->args = NULL;
    node->arg_count = 0;

    return node;
}

ast_node *ast_create_int(ast_node_type type, long value)
{
    ast_node *node;
    node = ast_create(type);
    if (node != NULL) {
        node->value.float_val = (double)value;
        node->has_str   = 0;
        node->has_ident = 0;
    }
    return node;
}

ast_node *ast_create_float(ast_node_type type, double value)
{
    ast_node *node;
    node = ast_create(type);
    if (node != NULL) {
        node->value.float_val = value;
        node->has_str   = 0;
        node->has_ident = 0;
    }
    return node;
}

ast_node *ast_create_str(ast_node_type type, const char *value)
{
    ast_node *node;
    node = ast_create(type);
    if (node != NULL) {
        node->value.str_val = (value != NULL) ? strdup(value) : NULL;
        node->has_str   = (value != NULL) ? 1 : 0;
        node->has_ident = 0;
    }
    return node;
}

ast_node *ast_create_ident(ast_node_type type, const char *name)
{
    ast_node *node;
    node = ast_create(type);
    if (node != NULL) {
        node->value.ident = (name != NULL) ? strdup(name) : NULL;
        node->has_str   = 0;
        node->has_ident = (name != NULL) ? 1 : 0;
    }
    return node;
}

/* ------------------------------------------------------------------ */
/* Manipulation des enfants                                           */
/* ------------------------------------------------------------------ */

void ast_add_child(ast_node *parent, ast_node *child)
{
    ast_node *prev;

    if (parent == NULL || child == NULL) return;

    if (parent->left == NULL) {
        parent->left = child;
        return;
    }

    /* Ajouter a la fin de la liste des freres */
    prev = parent->left;
    while (prev->right != NULL) {
        prev = prev->right;
    }
    prev->right = child;
}

void ast_set_cond(ast_node *node, ast_node *cond)
{
    if (node != NULL) {
        node->cond = cond;
    }
}

void ast_set_body(ast_node *node, ast_node *body)
{
    if (node != NULL) {
        node->body = body;
    }
}

void ast_set_else(ast_node *node, ast_node *else_body)
{
    if (node != NULL) {
        node->else_body = else_body;
    }
}

void ast_set_step(ast_node *node, ast_node *step)
{
    if (node != NULL) {
        node->step = step;
    }
}

void ast_add_arg(ast_node *node, ast_node *arg)
{
    if (node == NULL || arg == NULL) return;

    node->args = (ast_node **)realloc(node->args,
        (size_t)(node->arg_count + 1) * sizeof(ast_node *));
    if (node->args != NULL) {
        node->args[node->arg_count] = arg;
        node->arg_count++;
    }
}

/* ------------------------------------------------------------------ */
/* Liberation                                                         */
/* ------------------------------------------------------------------ */

void ast_free(ast_node *node)
{
    int i;
    ast_node *child, *next;

    if (node == NULL) return;

    /* Liberer les chaines */
    if (node->has_str && node->value.str_val != NULL) {
        free(node->value.str_val);
        node->value.str_val = NULL;
        node->has_str = 0;
    }
    if (node->has_ident && node->value.ident != NULL) {
        free(node->value.ident);
        node->value.ident = NULL;
        node->has_ident = 0;
    }

    /*
     * Liberer les enfants. On parcourt la liste des freres (right)
     * manuellement pour eviter les double-free quand un noeud est
     * reference a la fois comme enfant (left) et comme corps (body).
     */
    child = node->left;
    while (child != NULL) {
        next = child->right;
        child->right = NULL;  /* Eviter la recursion sur les freres */
        ast_free(child);
        child = next;
    }

    /* Liberer les sous-arbres speciaux */
    ast_free(node->cond);
    ast_free(node->body);
    ast_free(node->else_body);
    ast_free(node->step);
    ast_free(node->cases);

    /* Liberer les arguments */
    if (node->args != NULL) {
        for (i = 0; i < node->arg_count; i++) {
            ast_free(node->args[i]);
        }
        free(node->args);
        node->args = NULL;
    }

    free(node);
}

/* ------------------------------------------------------------------ */
/* Affichage debug                                                    */
/* ------------------------------------------------------------------ */

void ast_dump(ast_node *node, int indent)
{
    int i;

    if (node == NULL) return;

    for (i = 0; i < indent; i++) printf("  ");
    printf("%s", ast_node_type_name(node->type));

    if (node->value.int_val != 0) {
        printf(" (%ld)", node->value.int_val);
    }
    if (node->value.float_val != 0.0) {
        printf(" (%g)", node->value.float_val);
    }
    if (node->value.str_val != NULL) {
        printf(" \"%s\"", node->value.str_val);
    }
    if (node->value.ident != NULL) {
        printf(" %s", node->value.ident);
    }
    printf("\n");

    /* Condition */
    if (node->cond != NULL) {
        for (i = 0; i < indent + 1; i++) printf("  ");
        printf("[COND]\n");
        ast_dump(node->cond, indent + 2);
    }

    /* Corps */
    if (node->body != NULL) {
        for (i = 0; i < indent + 1; i++) printf("  ");
        printf("[BODY]\n");
        ast_dump(node->body, indent + 2);
    }

    /* ELSE */
    if (node->else_body != NULL) {
        for (i = 0; i < indent + 1; i++) printf("  ");
        printf("[ELSE]\n");
        ast_dump(node->else_body, indent + 2);
    }

    /* STEP */
    if (node->step != NULL) {
        for (i = 0; i < indent + 1; i++) printf("  ");
        printf("[STEP]\n");
        ast_dump(node->step, indent + 2);
    }

    /* Arguments */
    if (node->args != NULL) {
        int k;
        for (i = 0; i < node->arg_count; i++) {
            for (k = 0; k < indent + 1; k++) printf("  ");
            printf("[ARG %d]\n", i);
            ast_dump(node->args[i], indent + 2);
        }
    }

    /* Freres */
    if (node->right != NULL) {
        ast_dump(node->right, indent);
    }

    /* Enfants */
    if (node->left != NULL && node->left != node->body &&
        node->left != node->cond && node->left != node->else_body &&
        node->left != node->step && node->left != node->cases) {
        ast_dump(node->left, indent + 1);
    }
}
