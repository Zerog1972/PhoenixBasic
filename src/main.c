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

#include "os_layer.h"
#include "parser.h"
#include "codegen.h"
#include "runtime.h"

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
    return (result != 0) ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* Mode interactif (REPL simplifie)                                   */
/* ------------------------------------------------------------------ */

static void repl_mode(void)
{
    char line[1024];
    char program[65536];
    int prog_len;
    int running;

    printf("GFA Basic 3.5 Emulator (C89)\n");
    printf("Type 'RUN' to execute, 'LIST' to show, 'NEW' to clear, 'QUIT' to exit.\n");
    printf("Enter program lines or commands:\n\n");

    prog_len = 0;
    program[0] = '\0';
    running = 1;

    while (running) {
        printf("] ");
        fflush(stdout);

        if (fgets(line, (int)sizeof(line), stdin) == NULL) {
            break;
        }

        /* Enlever le \n final */
        {
            int len;
            len = (int)strlen(line);
            if (len > 0 && line[len - 1] == '\n') {
                line[len - 1] = '\0';
                len--;
            }
        }

        /* Commandes speciales */
        if (strcmp(line, "QUIT") == 0 || strcmp(line, "quit") == 0) {
            running = 0;
            continue;
        }

        if (strcmp(line, "RUN") == 0 || strcmp(line, "run") == 0) {
            if (prog_len > 0) {
                printf("\n");
                run_program(program);
                printf("\nProgram end.\n\n");
            } else {
                printf("No program in memory.\n");
            }
            continue;
        }

        if (strcmp(line, "LIST") == 0 || strcmp(line, "list") == 0) {
            printf("%s", program);
            continue;
        }

        if (strcmp(line, "NEW") == 0 || strcmp(line, "new") == 0) {
            prog_len = 0;
            program[0] = '\0';
            printf("Program cleared.\n");
            continue;
        }

        if (strcmp(line, "CLS") == 0 || strcmp(line, "cls") == 0) {
            os_con_clear();
            continue;
        }

        /* Ligne de programme : on l'ajoute au buffer */
        if (prog_len + (int)strlen(line) + 2 < (int)sizeof(program)) {
            if (prog_len > 0) {
                program[prog_len++] = '\n';
            }
            strcpy(program + prog_len, line);
            prog_len += (int)strlen(line);
            program[prog_len] = '\0';
        } else {
            printf("Program too long.\n");
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
