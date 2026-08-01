/*
 * main.c - Point d'entree de l'emulateur GFA Basic 3.5
 * =====================================================
 * Usage : gfabasic [fichier.bas]
 *
 * Sans argument : mode interactif (REPL simplifie)
 * Avec un fichier .bas : le charge, le compile et l'execute
 *
 * Reference : cahier-des-charges-gfabasic.md, section 2.1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#include <termios.h>
#else
#include <conio.h>
#endif

#include "os_layer.h"
#include "keywords.h"
#include "parser.h"
#include "codegen.h"
#include "runtime.h"
#include "gfx.h"

/* ------------------------------------------------------------------ */
/* Charge un fichier en memoire                                       */
/* ------------------------------------------------------------------ */

static char *load_file(const char *filename)
{
    FILE *fp;
    long size;
    char *buffer;

    fp = fopen(filename, "rb");
    if (fp == NULL) {
        fprintf(stderr, "Error: cannot open file '%s'\n", filename);
        return NULL;
    }

    /* Determiner la taille */
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size < 0) {
        fclose(fp);
        return NULL;
    }

    buffer = (char *)malloc((size_t)(size + 1));
    if (buffer == NULL) {
        fclose(fp);
        fprintf(stderr, "Error: out of memory\n");
        return NULL;
    }

    if (fread(buffer, 1, (size_t)size, fp) != (size_t)size) {
        free(buffer);
        fclose(fp);
        fprintf(stderr, "Error: cannot read file\n");
        return NULL;
    }

    buffer[size] = '\0';
    fclose(fp);
    return buffer;
}

/* ------------------------------------------------------------------ */
/* Execute un programme GFA Basic                                     */
/* ------------------------------------------------------------------ */

static int run_program(const char *source)
{
    gfa_parser *parser;
    ast_node *ast;
    gfa_runtime *rt;
    gfa_bytecode *bc;
    int result;

    /* Initialiser le runtime */
    rt = gfa_runtime_init();
    if (rt == NULL) {
        fprintf(stderr, "Error: cannot initialize runtime\n");
        return 1;
    }

    /* Initialiser le mode graphique C89 (framebuffer ANSI, 640x400) */
    gfx_init(640, 400);

    /* Parser le source */
    parser = gfa_parser_init(source);
    if (parser == NULL) {
        fprintf(stderr, "Error: cannot initialize parser\n");
        gfa_runtime_shutdown(rt);
        return 1;
    }

    ast = gfa_parser_parse(parser);
    if (ast == NULL || parser->error_count > 0) {
        fprintf(stderr, "Parse error: %s (line %d)\n",
                gfa_parser_get_error(parser),
                gfa_lexer_get_line(parser->lexer));
        gfa_parser_free(parser);
        gfa_runtime_shutdown(rt);
        return 1;
    }

    /* Compiler en bytecode */
    result = gfa_codegen_compile(rt->globals, ast, &bc,
                                  (gfa_label_info *)parser->labels,
                                  parser->label_count);
    if (result != 0 || bc == NULL) {
        fprintf(stderr, "Error: code generation failed\n");
        gfa_parser_free(parser);
        gfa_runtime_shutdown(rt);
        return 1;
    }

    /* Liberer le parser (l'AST est libere avec) */
    gfa_parser_free(parser);

    /* Charger et executer le bytecode */
    result = gfa_runtime_load(rt, bc);
    if (result != 0) {
        fprintf(stderr, "Error: cannot load bytecode\n");
        gfa_runtime_shutdown(rt);
        return 1;
    }

    printf("Running program (%d bytecode instructions)...\n\n", bc->length);

    result = gfa_runtime_execute(rt);
    if (result != 0) {
        fprintf(stderr, "\nRuntime error %d\n", gfa_runtime_get_error(rt));
    }

    gfa_runtime_shutdown(rt);
    gfx_shutdown();
    return (result != 0) ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* Mode interactif (REPL) - editeur ligne GFA Basic 3.5               */
/* ------------------------------------------------------------------ */

#define MAX_LINES      4096
#define MAX_LINE_LEN   256

/* strieq portable pour C89 */
static int strieq(const char *a, const char *b)
{
    if (a == NULL || b == NULL) return (a == b);
    while (*a && *b) {
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b))
            return 0;
        a++; b++;
    }
    return (*a == *b);
}

static int strnicmp_local(const char *a, const char *b, int n)
{
    int i;
    if (a == NULL || b == NULL) return (a == b) ? 0 : -1;
    for (i = 0; i < n; i++) {
        if (a[i] == '\0' && b[i] == '\0') return 0;
        if (a[i] == '\0') return -1;
        if (b[i] == '\0') return 1;
        if (toupper((unsigned char)a[i]) != toupper((unsigned char)b[i]))
            return (int)(unsigned char)a[i] - (int)(unsigned char)b[i];
    }
    return 0;
}

/* Ligne de programme */
typedef struct {
    char  text[MAX_LINE_LEN];
} prog_line;

static prog_line g_lines[MAX_LINES];
static int g_line_count = 0;

/* Copie bornee de texte dans une ligne (toujours NUL-terminee) */
static void copy_line_text(char *dst, const char *src, int maxlen)
{
    int i;
    for (i = 0; i < maxlen - 1 && src[i] != '\0'; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

/* Supprimer des lignes par plage d'index */
static void delete_lines(int from, int to)
{
    int i;
    if (from < 1) from = 1;
    if (to > g_line_count) to = g_line_count;
    for (i = from - 1; i < to && i < g_line_count; i++) {
        int j;
        for (j = i; j < g_line_count - 1; j++)
            g_lines[j] = g_lines[j + 1];
        g_line_count--;
        i--; to--;
    }
}

/* Lister les lignes */
static int is_block_opener(const char *line)
{
    if (strnicmp_local(line, "IF", 2) == 0 && (line[2] == ' ' || line[2] == '\0')) return 1;
    if (strnicmp_local(line, "FOR", 3) == 0 && (line[3] == ' ' || line[3] == '\0')) return 1;
    if (strnicmp_local(line, "WHILE", 5) == 0 && (line[5] == ' ' || line[5] == '\0')) return 1;
    if (strnicmp_local(line, "REPEAT", 6) == 0 && (line[6] == ' ' || line[6] == '\0')) return 1;
    if (strnicmp_local(line, "DO", 2) == 0 && (line[2] == ' ' || line[2] == '\0')) return 1;
    if (strnicmp_local(line, "SELECT", 6) == 0 && (line[6] == ' ' || line[6] == '\0')) return 1;
    if (strnicmp_local(line, "PROCEDURE", 9) == 0 && (line[9] == ' ' || line[9] == '\0')) return 1;
    if (strnicmp_local(line, "FUNCTION", 8) == 0 && (line[8] == ' ' || line[8] == '\0')) return 1;
    if (strnicmp_local(line, "DEFFN", 5) == 0 && (line[5] == ' ' || line[5] == '\0')) return 1;
    if (strnicmp_local(line, "CASE", 4) == 0 && (line[4] == ' ' || line[4] == '\0')) return 1;
    if (strnicmp_local(line, "DEFAULT", 7) == 0 && (line[7] == ' ' || line[7] == '\0')) return 1;
    if (strnicmp_local(line, "ELSE", 4) == 0 && (line[4] == ' ' || line[4] == '\0')) return 1;
    return 0;
}

static int is_block_closer(const char *line)
{
    if (strnicmp_local(line, "ENDIF", 5) == 0 && (line[5] == ' ' || line[5] == '\0')) return 1;
    if (strnicmp_local(line, "NEXT", 4) == 0 && (line[4] == ' ' || line[4] == '\0')) return 1;
    if (strnicmp_local(line, "WEND", 4) == 0 && (line[4] == ' ' || line[4] == '\0')) return 1;
    if (strnicmp_local(line, "UNTIL", 5) == 0 && (line[5] == ' ' || line[5] == '\0')) return 1;
    if (strnicmp_local(line, "LOOP", 4) == 0 && (line[4] == ' ' || line[4] == '\0')) return 1;
    if (strnicmp_local(line, "ENDSELECT", 9) == 0 && (line[9] == ' ' || line[9] == '\0')) return 1;
    if (strnicmp_local(line, "ELSE", 4) == 0 && (line[4] == ' ' || line[4] == '\0')) return 1;
    if (strnicmp_local(line, "RETURN", 6) == 0 && (line[6] == ' ' || line[6] == '\0')) return 1;
    if (strnicmp_local(line, "ENDFUNC", 7) == 0 && (line[7] == ' ' || line[7] == '\0')) return 1;
    return 0;
}

/* Formateur : mots-cles en majuscules */
static void format_line(char *buffer, int bufsize)
{
    char in[MAX_LINE_LEN];
    int i, o;
    char kw[MAX_LINE_LEN];
    int kw_len;

    strncpy(in, buffer, (size_t)bufsize - 1);
    in[bufsize - 1] = '\0';
    i = 0; o = 0;
    while (in[i] != '\0' && o < bufsize - 1) {
        if (in[i] == '"') {
            buffer[o++] = in[i++];
            while (in[i] != '\0' && o < bufsize - 1) {
                buffer[o++] = in[i];
                if (in[i] == '"' && (i == 0 || in[i-1] != '\\')) { i++; break; }
                i++;
            }
            continue;
        }
        if (isalpha((unsigned char)in[i]) || in[i] == '_') {
            kw_len = 0;
            while ((isalnum((unsigned char)in[i]) || in[i] == '_' ||
                    in[i] == '$' || in[i] == '%' || in[i] == '&' ||
                    in[i] == '!' || in[i] == '|' || in[i] == '#') &&
                   kw_len < MAX_LINE_LEN - 1)
                kw[kw_len++] = in[i++];
            kw[kw_len] = '\0';
            if (gfa_keyword_lookup(kw) != TOK_IDENTIFIER) {
                int j;
                for (j = 0; j < kw_len && o < bufsize - 1; j++)
                    buffer[o++] = toupper((unsigned char)kw[j]);
            } else {
                int j;
                for (j = 0; j < kw_len && o < bufsize - 1; j++)
                    buffer[o++] = kw[j];
            }
            continue;
        }
        buffer[o++] = in[i++];
    }
    buffer[o] = '\0';
}

/* Lister avec indentation */
static void list_lines(int from, int to)
{
    int i, level;
    if (from < 1) from = 1;
    if (to > g_line_count) to = g_line_count;
    level = 0;
    for (i = from - 1; i < to && i < g_line_count; i++) {
        const char *text = g_lines[i].text;
        int indent = level;
        int ws;
        while (*text == ' ') text++;
        if (*text == '\0') { printf("\n"); continue; }
        {
            char fk[64];
            int fi = 0;
            while (isalpha((unsigned char)text[fi]) && fi < 63) {
                fk[fi] = toupper((unsigned char)text[fi]);
                fi++;
            }
            fk[fi] = '\0';
            if (is_block_closer(fk) && indent > 0) indent--;
        }
        printf("%d] ", i + 1);
        for (ws = 0; ws < indent * 2; ws++) putchar(' ');
        printf("%s\n", text);
        {
            char fk[64];
            int fi = 0;
            while (isalpha((unsigned char)text[fi]) && fi < 63) {
                fk[fi] = toupper((unsigned char)text[fi]);
                fi++;
            }
            fk[fi] = '\0';
            if (is_block_closer(fk)) level--;
            if (level < 0) level = 0;
            if (is_block_opener(fk)) level++;
        }
    }
}

/* Assembler le programme en un buffer source */
static char *build_source(void)
{
    static char buf[131072];
    int pos;
    int i;
    pos = 0;
    for (i = 0; i < g_line_count; i++) {
        int len;
        len = (int)strlen(g_lines[i].text);
        if (pos + len + 1 >= (int)sizeof(buf)) {
            break;  /* Buffer plein : on stoppe */
        }
        strcpy(buf + pos, g_lines[i].text);
        pos += len;
        buf[pos++] = '\n';
    }
    buf[pos] = '\0';
    return buf;
}

/* Charger un fichier source dans l'editeur */
static int load_file_into_editor(const char *filename)
{
    char *source;
    char *p, *line_start;
    char text_buf[MAX_LINE_LEN];

    source = load_file(filename);
    if (source == NULL) return -1;

    g_line_count = 0;
    p = source;
    while (*p && g_line_count < MAX_LINES) {
        line_start = p;
        while (*p && *p != '\n' && *p != '\r') p++;
        {
            int len = (int)(p - line_start);
            if (len > 0 && len < MAX_LINE_LEN) {
                memcpy(text_buf, line_start, (size_t)len);
                text_buf[len] = '\0';
                copy_line_text(g_lines[g_line_count].text, text_buf, MAX_LINE_LEN);
                format_line(g_lines[g_line_count].text, MAX_LINE_LEN);
                g_line_count++;
            }
        }
        if (*p == '\r') p++;
        if (*p == '\n') p++;
    }
    free(source);
    return 0;
}

/* Sauvegarder le programme */
static void save_program(const char *filename, int ascii)
{
    FILE *fp;
    int i;
    (void)ascii;  /* Pour l'instant meme format (tokenise non gere) */
    fp = fopen(filename, "w");
    if (fp == NULL) {
        printf("Error: cannot write '%s'\n", filename);
        return;
    }
    for (i = 0; i < g_line_count; i++)
        fprintf(fp, "%s\n", g_lines[i].text);
    fclose(fp);
    printf("Saved %d lines to %s\n", g_line_count, filename);
}

/* Renumeroter les lignes */
/* Renumeroter : sans objet (pas de numeros de ligne en GFA Basic 3.5) */
static void renum_lines(void)
{
    printf("RENUM not needed - GFA Basic uses labels, not line numbers.\n");
}

/* Parser une commande de ligne LIST/DELETE avec plage */
static int parse_range(const char *cmd, int *from, int *to)
{
    const char *dash;
    dash = strchr(cmd, '-');
    if (dash != NULL) {
        /* Plage avec tiret */
        if (dash == cmd) {
            /* "-to" */
            if (sscanf(cmd + 1, "%d", to) == 1) {
                *from = 0;
                return 1;
            }
        } else if (*(dash + 1) == '\0') {
            /* "from-" */
            if (sscanf(cmd, "%d", from) == 1) {
                *to = 999999;
                return 1;
            }
        } else {
            /* "from-to" */
            if (sscanf(cmd, "%d-%d", from, to) == 2) {
                return 1;
            }
        }
        return 0;
    }
    /* Ligne unique */
    if (sscanf(cmd, "%d", from) == 1) {
        *to = *from;
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Editeur de ligne en mode raw : affiche le texte, laisse modifier   */
/* avec backspace, fleches, home, fin.                                */
/* ------------------------------------------------------------------ */

static void line_editor(char *buffer, int bufsize)
{
    int pos, len, i, c, next, arrow, dummy, back;
#if defined(_WIN32) || defined(_WIN64)
    /* Windows : lecture simple sans mode raw */
    (void)pos; (void)len; (void)i; (void)c; (void)next; (void)arrow;
    (void)dummy; (void)back;
    if (fgets(buffer, bufsize, stdin) != NULL) {
        int llen = (int)strlen(buffer);
        while (llen > 0 && (buffer[llen-1]=='\n'||buffer[llen-1]=='\r'))
            buffer[--llen] = '\0';
    } else {
        buffer[0] = '\0';
    }
    return;
#else
    struct termios oldt, newt;

    /* Si stdin n'est pas un terminal, utiliser fgets */
    if (!isatty(STDIN_FILENO)) {
        char line[1024];
        if (fgets(line, (int)sizeof(line), stdin) != NULL) {
            int llen = (int)strlen(line);
            while (llen > 0 && (line[llen-1]=='\n'||line[llen-1]=='\r'))
                line[--llen] = '\0';
            if (line[0] != '\0')
                strncpy(buffer, line, (size_t)bufsize - 1);
            else
                buffer[0] = '\0';
        }
        return;
    }

    pos = (int)strlen(buffer);
    len = pos;

    /* Mode raw */
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    newt.c_cc[VMIN] = 1;
    newt.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    /* Afficher le texte existant */
    for (i = 0; i < len; i++) putchar(buffer[i]);
    fflush(stdout);

    while (1) {
        c = getchar();

        if (c == '\n' || c == '\r') {
            /* Valider */
            buffer[len] = '\0';
            break;
        }
        if (c == 27) { /* ESC pour les sequences et annulation */
            next = getchar();
            if (next == '[') {
                arrow = getchar();
                if (arrow == 'D' && pos > 0) { /* Gauche */
                    pos--;
                    printf("\033[D");
                    fflush(stdout);
                } else if (arrow == 'C' && pos < len) { /* Droite */
                    pos++;
                    printf("\033[C");
                    fflush(stdout);
                } else if (arrow == 'H') { /* Home */
                    printf("\033[%dD", pos);
                    pos = 0;
                    fflush(stdout);
                } else if (arrow == 'F') { /* End */
                    printf("\033[%dC", len - pos);
                    pos = len;
                    fflush(stdout);
                } else if (arrow == '3') { /* Delete */
                    dummy = getchar(); /* ~ */
                    (void)dummy;
                    if (pos < len) {
                        for (i = pos; i < len - 1; i++) buffer[i] = buffer[i + 1];
                        len--;
                        printf("\033[P");
                        fflush(stdout);
                    }
                }
            } else { /* ESC seul = annuler */
                buffer[0] = '\0';
                break;
            }
        } else if (c == 127 || c == 8) { /* Backspace */
            if (pos > 0) {
                for (i = pos - 1; i < len - 1; i++) buffer[i] = buffer[i + 1];
                len--;
                pos--;
                printf("\033[D\033[P");
                fflush(stdout);
            }
        } else if (c >= 32 && c <= 126 && len < bufsize - 1) { /* Caractere imprimable */
            for (i = len; i > pos; i--) buffer[i] = buffer[i - 1];
            buffer[pos] = (char)c;
            len++;
            pos++;
            /* Re-afficher la fin de ligne */
            for (i = pos - 1; i < len; i++) putchar(buffer[i]);
            /* Reculer le curseur */
            {
                back = len - pos;
                while (back-- > 0) printf("\033[D");
            }
            fflush(stdout);
        }
    }

    /* Nouvelle ligne */
    putchar('\n');
    fflush(stdout);

    /* Restaurer le mode */
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    buffer[len] = '\0';
#endif
}

static void repl_mode(void)
{
    char line[1024];
    int running;
    int cmd_mode = 0;  /* 0=edition, 1=commandes */
    

    printf("GFA Basic 3.5 Emulator (C89) - Interactive mode\n");
    printf("Edit mode: type code. Empty line = switch to commands.\n");
    printf("Commands: RUN LIST DELETE EDIT INSERT NEW LOAD SAVE CLS QUIT\n\n");

    g_line_count = 0;
    running = 1;

    while (running) {
        if (cmd_mode) {
            printf("> ");
        } else {
            printf("%d] ", g_line_count + 1);
        }
        fflush(stdout);

        if (fgets(line, (int)sizeof(line), stdin) == NULL) {
            printf("\n");
            break;
        }

        /* Enlever le \n final */
        {
            int len;
            len = (int)strlen(line);
            while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
                line[len - 1] = '\0';
                len--;
            }
        }

        /* Ligne vide : basculer mode edition/commande */
        if (line[0] == '\0') {
            if (cmd_mode) {
                cmd_mode = 0;
                printf("Edit mode.\n");
            } else {
                cmd_mode = 1;
                printf("Command mode.\n");
            }
            continue;
        }

        /* expanded = 0; */

        /* --- En mode commande : interpreter les commandes --- */
        if (cmd_mode) {

        /* --- Commandes (insensibles a la casse) --- */
        if (strieq(line, "quit") || strieq(line, "bye")) {
            running = 0;
            continue;
        }

        if (strieq(line, "cls")) {
            os_con_clear();
            continue;
        }

        if (strieq(line, "new")) {
            g_line_count = 0;
            cmd_mode = 0; printf("Program cleared. Edit mode.\n");
            continue;
        }

        if (strieq(line, "list")) {
            list_lines(0, 999999);
            continue;
        }

        if (strnicmp_local(line, "list ", 5) == 0) {
            int from, to;
            const char *range = line + 5;
            if (parse_range(range, &from, &to))
                list_lines(from, to);
            else
                printf("Syntax: LIST [from[-to]]\n");
            continue;
        }

        if (strieq(line, "tron")) {
            if (g_line_count < MAX_LINES) {
                strcpy(g_lines[g_line_count].text, "TRON");
                g_line_count++;
            }
            printf("Trace ON\n");
            continue;
        }

        if (strieq(line, "troff")) {
            if (g_line_count < MAX_LINES) {
                strcpy(g_lines[g_line_count].text, "TROFF");
                g_line_count++;
            }
            printf("Trace OFF\n");
            continue;
        }

        if (strieq(line, "cont")) {
            printf("CONT not supported.\n");
            continue;
        }

        if (strnicmp_local(line, "delete ", 7) == 0) {
            int from, to;
            const char *range = line + 7;
            if (parse_range(range, &from, &to)) {
                delete_lines(from, to);
                printf("Deleted lines %d-%d.\n", from, to);
            } else {
                printf("Syntax: DELETE position[-to]\n");
            }
            continue;
        }

        if (strnicmp_local(line, "edit ", 5) == 0) {
            int n;
            if (sscanf(line + 5, "%d", &n) == 1 && n >= 1 && n <= g_line_count) {
                char buf[MAX_LINE_LEN];
                int edited = 0;
                strncpy(buf, g_lines[n - 1].text, MAX_LINE_LEN - 1);
                buf[MAX_LINE_LEN - 1] = '\0';
                printf("%d] ", n);
                fflush(stdout);
                line_editor(buf, MAX_LINE_LEN);
                if (buf[0] != '\0') {
                    copy_line_text(g_lines[n - 1].text, buf, MAX_LINE_LEN);
                    format_line(g_lines[n - 1].text, MAX_LINE_LEN);
                    edited = 1;
                    printf("Line %d updated.\n", n);
                } else {
                    printf("Line %d unchanged.\n", n);
                }
                if (edited) {
                    cmd_mode = 0;
                    printf("Edit mode.\n");
                }
            } else if (g_line_count == 0) {
                printf("No lines to edit.\n");
            } else {
                printf("Position 1-%d expected.\n", g_line_count);
            }
            continue;
        }

        if (strnicmp_local(line, "insert ", 7) == 0) {
            int n;
            if (sscanf(line + 7, "%d", &n) == 1 && n >= 0 && n <= g_line_count + 1) {
                char buf[MAX_LINE_LEN];
                if (g_line_count >= MAX_LINES) {
                    printf("Program full.\n");
                    continue;
                }
                /* INSERT 0 = avant ligne 1, INSERT 1 = avant ligne 1, INSERT 4 = avant ligne 4 */
                if (n == 0) n = 1;
                /* Decaler les lignes a partir de n-1 */
                {
                    int i;
                    for (i = g_line_count; i >= n; i--)
                        g_lines[i] = g_lines[i - 1];
                }
                g_lines[n - 1].text[0] = '\0';
                g_line_count++;
                printf("%d] ", n);
                fflush(stdout);
                if (fgets(buf, (int)sizeof(buf), stdin) != NULL) {
                    int len = (int)strlen(buf);
                    while (len > 0 && (buf[len-1]=='\n'||buf[len-1]=='\r'))
                        buf[--len] = '\0';
                    /* Annuler si vide ou que des espaces */
                    {
                        int only_spaces = 1;
                        int ci;
                        for (ci = 0; ci < len; ci++) {
                            if (buf[ci] != ' ') {
                                only_spaces = 0;
                                break;
                            }
                        }
                        if (len == 0 || only_spaces) {
                            /* Annuler l'insertion : restaurer l'etat precedent */
                            for (ci = n - 1; ci < g_line_count - 1; ci++)
                                g_lines[ci] = g_lines[ci + 1];
                            g_line_count--;
                        } else {
                            copy_line_text(g_lines[n - 1].text, buf, MAX_LINE_LEN);
                            format_line(g_lines[n - 1].text, MAX_LINE_LEN);
                        }
                    }
                } else {
                    /* EOF : annuler */
                    {
                        int ci;
                        for (ci = n - 1; ci < g_line_count - 1; ci++)
                            g_lines[ci] = g_lines[ci + 1];
                        g_line_count--;
                    }
                }
            } else {
                printf("Position 0-%d expected.\n", g_line_count + 1);
            }
            continue;
        }

        if (strnicmp_local(line, "renum", 5) == 0) {
            renum_lines();
            continue;
        }

        if (strnicmp_local(line, "load ", 5) == 0) {
            char fname[256];
            if (sscanf(line + 5, "%255s", fname) == 1) {
                int flen = (int)strlen(fname);
                if (flen >= 2 && fname[0] == '\"' && fname[flen-1] == '\"') {
                    fname[flen-1] = '\0';
                    memmove(fname, fname + 1, (size_t)flen - 1);
                }
                printf("Loading %s...\n", fname);
                if (load_file_into_editor(fname) == 0)
                    printf("Loaded %d lines.\n", g_line_count);
                else
                    printf("Error loading file.\n");
            } else {
                printf("Syntax: LOAD \"filename\"\n");
            }
            continue;
        }

        if (strnicmp_local(line, "save", 4) == 0) {
            char fname[256];
            int is_ascii = 0;
            const char *p = line + 4;
            while (*p == ' ') p++;
            if ((*p == 'a' || *p == 'A') && (*(p+1) == ',') ) {
                is_ascii = 1;
                p += 2;
                while (*p == ' ') p++;
            }
            if (sscanf(p, "%255s", fname) == 1) {
                int flen = (int)strlen(fname);
                if (flen >= 2 && fname[0] == '\"' && fname[flen-1] == '\"') {
                    fname[flen-1] = '\0';
                    memmove(fname, fname + 1, (size_t)flen - 1);
                }
                save_program(fname, is_ascii);
            } else {
                printf("Syntax: SAVE [A,] \"filename\"\n");
            }
            continue;
        }

        if (strieq(line, "run")) {
            char *source;
            if (g_line_count == 0) {
                printf("No program in memory.\n");
                continue;
            }
            source = build_source();
            printf("\n");
            run_program(source);
            printf("\nProgram end.\n\n");
            continue;
        }

        if (strnicmp_local(line, "run ", 4) == 0) {
            char fname[256];
            const char *p = line + 4;
            while (*p == ' ') p++;
            if (*p == '\"') {
                int flen = 0;
                p++;
                while (*p && *p != '\"' && flen < 255)
                    fname[flen++] = *p++;
                fname[flen] = '\0';
                if (load_file_into_editor(fname) == 0)
                    printf("Loaded %d lines.\n", g_line_count);
            /* Commande inconnue en mode commande */
            printf("Unknown command. Try LIST, RUN, EDIT, NEW...\n");
            continue;
            }
            if (g_line_count > 0) {
                char *source = build_source();
                printf("\n");
                run_program(source);
                printf("\nProgram end.\n\n");
            }
            continue;
        }

        }
        /* --- Mode edition : ajouter au programme --- */
        {
            if (g_line_count < MAX_LINES) {
                copy_line_text(g_lines[g_line_count].text, line, MAX_LINE_LEN);
                format_line(g_lines[g_line_count].text, MAX_LINE_LEN);
                g_line_count++;
            } else {
                printf("Program full.\n");
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    int result;

    setbuf(stderr, NULL);
    setbuf(stdout, NULL);
    os_init();

    if (argc < 2) {
        /* Mode interactif */
        repl_mode();
        result = 0;
    } else {
        /* Charger et executer un fichier */
        char *source;
        const char *filename;

        filename = argv[1];
        printf("GFA Basic 3.5 Emulator\n");
        printf("Loading %s...\n", filename);

        source = load_file(filename);
        if (source == NULL) {
            os_shutdown();
            return 1;
        }

        result = run_program(source);
        free(source);
    }

    os_shutdown();
    return result;
}
