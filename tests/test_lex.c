/*
 * test_lex.c - Tests du lexer GFA Basic 3.5
 */
#include "lexer.h"
#include "keywords.h"
#include <stdio.h>
#include <string.h>

static int g_pass = 0, g_fail = 0;
#define TEST(expr, msg) do { \
    if (expr) { g_pass++; printf("  [PASS] %s\n", msg); } \
    else { g_fail++; printf("  [FAIL] %s (line %d)\n", msg, __LINE__); } \
} while(0)

static void test_basic_tokens(void)
{
    gfa_lexer *lex;
    printf("\n--- Basic tokens ---\n");

    lex = gfa_lexer_init("PRINT CLS END");
    TEST(lex != NULL, "Lexer init");

    TEST(gfa_lexer_next(lex) == TOK_PRINT, "PRINT token");
    TEST(gfa_lexer_next(lex) == TOK_CLS, "CLS token");
    TEST(gfa_lexer_next(lex) == TOK_END, "END token");
    TEST(gfa_lexer_next(lex) == TOK_EOF, "EOF");
    gfa_lexer_free(lex);
}

static void test_numbers(void)
{
    gfa_lexer *lex;
    printf("\n--- Numbers ---\n");

    lex = gfa_lexer_init("123 3.14 1.5E-2 &HFF &X1010 &O77");
    gfa_lexer_next(lex);
    TEST(lex->current.type == TOK_INTEGER && lex->current.value.int_value == 123, "Decimal 123");
    gfa_lexer_next(lex);
    TEST(lex->current.type == TOK_FLOAT, "Float 3.14");
    gfa_lexer_next(lex);
    TEST(lex->current.type == TOK_FLOAT, "Float 1.5E-2");
    gfa_lexer_next(lex);
    TEST(lex->current.type == TOK_INTEGER && lex->current.value.int_value == 255, "Hex &HFF");
    gfa_lexer_next(lex);
    TEST(lex->current.type == TOK_INTEGER && lex->current.value.int_value == 10, "Bin &X1010");
    gfa_lexer_next(lex);
    TEST(lex->current.type == TOK_INTEGER && lex->current.value.int_value == 63, "Oct &O77");
    gfa_lexer_free(lex);
}

static void test_strings(void)
{
    gfa_lexer *lex;
    printf("\n--- Strings ---\n");

    lex = gfa_lexer_init("\"hello\" \"world\"");
    gfa_lexer_next(lex);
    TEST(lex->current.type == TOK_STRING && strcmp(lex->current.value.string_value, "hello") == 0, "Simple string");
    gfa_lexer_next(lex);
    TEST(lex->current.type == TOK_STRING && strcmp(lex->current.value.string_value, "world") == 0, "Second string");
    gfa_lexer_free(lex);

    /* Escaped quotes */
    lex = gfa_lexer_init("\"say \"\"hi\"\"\"");
    gfa_lexer_next(lex);
    TEST(lex->current.type == TOK_STRING, "Escaped quotes string type");
    gfa_lexer_free(lex);
}

static void test_comments(void)
{
    gfa_lexer *lex;
    printf("\n--- Comments ---\n");

    lex = gfa_lexer_init("PRINT 1\nREM comment\nPRINT 2\nEND");
    TEST(gfa_lexer_next(lex) == TOK_PRINT, "PRINT before comment");
    TEST(gfa_lexer_next(lex) == TOK_INTEGER, "Integer 1");
    TEST(gfa_lexer_next(lex) == TOK_EOL, "EOL after PRINT 1");
    TEST(gfa_lexer_next(lex) == TOK_REM, "REM comment");
    TEST(gfa_lexer_next(lex) == TOK_EOL, "EOL after REM");
    TEST(gfa_lexer_next(lex) == TOK_PRINT, "PRINT after REM line");
    TEST(gfa_lexer_next(lex) == TOK_INTEGER, "Integer 2");
    TEST(gfa_lexer_next(lex) == TOK_EOL, "EOL after PRINT 2");
    TEST(gfa_lexer_next(lex) == TOK_END, "END token");
    TEST(gfa_lexer_next(lex) == TOK_EOF, "EOF");
    gfa_lexer_free(lex);
    TEST(1, "Comments handled");
}

static void test_identifiers(void)
{
    gfa_lexer *lex;
    printf("\n--- Identifiers ---\n");

    lex = gfa_lexer_init("maVariable x% y$ flag! byte| float#");
    gfa_lexer_next(lex);
    TEST(lex->current.type == TOK_IDENTIFIER && strcmp(lex->current.value.ident_name, "maVariable") == 0, "Identifier");
    gfa_lexer_next(lex);
    TEST(lex->current.type == TOK_IDENTIFIER, "x% identifier");
    gfa_lexer_next(lex);
    TEST(lex->current.type == TOK_IDENTIFIER, "y$ identifier");
    gfa_lexer_free(lex);
}

static void test_abbreviations(void)
{
    gfa_lexer *lex;
    printf("\n--- Abbreviations ---\n");

    lex = gfa_lexer_init("p \"hi\"\ng 100");
    gfa_lexer_set_expand(lex, 1);
    TEST(gfa_lexer_next(lex) == TOK_PRINT, "p -> PRINT");
    TEST(gfa_lexer_next(lex) == TOK_STRING, "string hi");
    TEST(gfa_lexer_next(lex) == TOK_EOL, "EOL after first line");
    TEST(gfa_lexer_next(lex) == TOK_GOTO, "g -> GOTO");
    gfa_lexer_free(lex);
}

static void test_keyword_lookup(void)
{
    printf("\n--- Keyword lookup ---\n");
    TEST(gfa_keyword_lookup("PRINT") == TOK_PRINT, "PRINT found");
    TEST(gfa_keyword_lookup("print") == TOK_PRINT, "print (lowercase) found");
    TEST(gfa_keyword_lookup("Print") == TOK_PRINT, "Print (mixed) found");
    TEST(gfa_keyword_lookup("IF") == TOK_IF, "IF found");
    TEST(gfa_keyword_lookup("FOR") == TOK_FOR, "FOR found");
    TEST(gfa_keyword_lookup("NEXT") == TOK_NEXT, "NEXT found");
    TEST(gfa_keyword_lookup("xyzzy") == TOK_IDENTIFIER, "Unknown -> IDENTIFIER");
}

int main(void)
{
    printf("========================================\n");
    printf(" Tests du Lexer GFA Basic 3.5\n");
    printf("========================================\n");

    test_basic_tokens();
    test_numbers();
    test_strings();
    test_comments();
    test_identifiers();
    test_abbreviations();
    test_keyword_lookup();

    printf("\n========================================\n");
    printf(" Resultat : %d/%d reussis, %d echoues\n", g_pass, g_pass+g_fail, g_fail);
    printf("========================================\n");

    return (g_fail > 0) ? 1 : 0;
}
