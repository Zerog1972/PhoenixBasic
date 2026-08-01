/*
 * test_parser.c - Tests du parser GFA Basic 3.5
 */
#include "parser.h"
#include <stdio.h>
#include <string.h>

static int g_pass = 0, g_fail = 0;
#define TEST(expr, msg) do { \
    if (expr) { g_pass++; printf("  [PASS] %s\n", msg); } \
    else { g_fail++; printf("  [FAIL] %s (line %d)\n", msg, __LINE__); } \
} while(0)

static void test_simple_stmts(void)
{
    gfa_parser *p;
    ast_node *ast;

    printf("\n--- Simple statements ---\n");

    p = gfa_parser_init("CLS\nEND\n");
    TEST(p != NULL, "Parser init");
    ast = gfa_parser_parse(p);
    TEST(ast != NULL, "Simple program parsed");
    TEST(p->error_count == 0, "No errors");
    /* Ne pas faire ast_free(ast) - gfa_parser_free le fera */
    gfa_parser_free(p);

    p = gfa_parser_init("PRINT \"hello\"\nPRINT 42\n");
    ast = gfa_parser_parse(p);
    TEST(ast != NULL, "PRINT statements parsed");
    TEST(p->error_count == 0, "No errors");
    /* ast_free(ast); -- gfa_parser_free le fera */
    gfa_parser_free(p);

    p = gfa_parser_init("a = 10\nb = a + 20\nPRINT b\n");
    ast = gfa_parser_parse(p);
    TEST(ast != NULL, "Assign and PRINT parsed");
    TEST(p->error_count == 0, "No errors");
    /* ast_free(ast); -- gfa_parser_free le fera */
    gfa_parser_free(p);
}

static void test_control_flow(void)
{
    gfa_parser *p;
    ast_node *ast;

    printf("\n--- Control flow ---\n");

    p = gfa_parser_init("IF a > 10 THEN\n  PRINT \"big\"\nENDIF\n");
    ast = gfa_parser_parse(p);
    TEST(ast != NULL, "IF/THEN/ENDIF parsed");
    TEST(p->error_count == 0, "No errors in IF");
    /* ast_free(ast); -- gfa_parser_free le fera */
    gfa_parser_free(p);

    p = gfa_parser_init("FOR i = 1 TO 10\n  PRINT i\nNEXT i\n");
    ast = gfa_parser_parse(p);
    TEST(ast != NULL, "FOR/NEXT parsed");
    TEST(p->error_count == 0, "No errors in FOR");
    /* ast_free(ast); -- gfa_parser_free le fera */
    gfa_parser_free(p);

    p = gfa_parser_init("WHILE x < 100\n  x = x * 2\nWEND\n");
    ast = gfa_parser_parse(p);
    TEST(ast != NULL, "WHILE/WEND parsed");
    TEST(p->error_count == 0, "No errors in WHILE");
    /* ast_free(ast); -- gfa_parser_free le fera */
    gfa_parser_free(p);

    p = gfa_parser_init("REPEAT\n  x = x + 1\nUNTIL x = 10\n");
    ast = gfa_parser_parse(p);
    TEST(ast != NULL, "REPEAT/UNTIL parsed");
    TEST(p->error_count == 0, "No errors in REPEAT");
    /* ast_free(ast); -- gfa_parser_free le fera */
    gfa_parser_free(p);

    p = gfa_parser_init(
        "SELECT x\n"
        "CASE 1\n  PRINT \"one\"\n"
        "CASE 2\n  PRINT \"two\"\n"
        "DEFAULT\n  PRINT \"other\"\n"
        "ENDSELECT\n");
    ast = gfa_parser_parse(p);
    TEST(ast != NULL, "SELECT/CASE parsed");
    TEST(p->error_count == 0, "No errors in SELECT");
    /* ast_free(ast); -- gfa_parser_free le fera */
    gfa_parser_free(p);
}

static void test_procedures(void)
{
    gfa_parser *p;
    ast_node *ast;

    printf("\n--- Procedures ---\n");

    p = gfa_parser_init(
        "PROCEDURE test(a, b)\n"
        "  PRINT a\n"
        "  PRINT b\n"
        "RETURN\n");
    ast = gfa_parser_parse(p);
    TEST(ast != NULL, "PROCEDURE parsed");
    TEST(p->error_count == 0, "No errors");
    /* ast_free(ast); -- gfa_parser_free le fera */
    gfa_parser_free(p);
}

static void test_graphics(void)
{
    gfa_parser *p;
    ast_node *ast;

    printf("\n--- Graphics ---\n");

    p = gfa_parser_init("LINE 10,20,100,200\nCIRCLE 50,50,30\nBOX 0,0,319,199\n");
    ast = gfa_parser_parse(p);
    TEST(ast != NULL, "Graphics commands parsed");
    TEST(p->error_count == 0, "No errors");
    /* ast_free(ast); -- gfa_parser_free le fera */
    gfa_parser_free(p);
}

static void test_file_io(void)
{
    gfa_parser *p;
    ast_node *ast;

    printf("\n--- File I/O ---\n");

    p = gfa_parser_init("OPEN \"I\",#1,\"test.dat\"\nINPUT #1, a, b\nCLOSE #1\n");
    ast = gfa_parser_parse(p);
    TEST(ast != NULL, "File I/O parsed");
    TEST(p->error_count == 0, "No errors");
    /* ast_free(ast); -- gfa_parser_free le fera */
    gfa_parser_free(p);
}

int main(void)
{
    printf("========================================\n");
    printf(" Tests du Parser GFA Basic 3.5\n");
    printf("========================================\n");

    test_simple_stmts();
    test_control_flow();
    test_procedures();
    test_graphics();
    test_file_io();

    printf("\n========================================\n");
    printf(" Resultat : %d/%d reussis, %d echoues\n", g_pass, g_pass+g_fail, g_fail);
    printf("========================================\n");

    return (g_fail > 0) ? 1 : 0;
}
