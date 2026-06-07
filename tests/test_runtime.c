/*
 * test_runtime.c - Tests du moteur d'execution GFA Basic 3.5
 * ==========================================================
 * Valide : runtime lifecycle, variables, value stack, bytecode,
 *          execution, conditions, arrays, et nouvelles fonctions.
 * Mise a jour : 7 juin 2026.
 */

#include "runtime.h"
#include "token.h"
#include <stdio.h>
#include <string.h>

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(expr, msg) do { \
    g_tests_run++; \
    if (expr) { \
        g_tests_passed++; \
        printf("  [PASS] %s\n", msg); \
    } else { \
        g_tests_failed++; \
        printf("  [FAIL] %s (line %d)\n", msg, __LINE__); \
    } \
} while(0)

/* Helper: emit ptr_val operand */
static int emit_ptr(gfa_bytecode *bc, gfa_opcode op, void *ptr)
{
    int idx = gfa_bytecode_emit(bc, op);
    if (idx >= 0) bc->code[idx].operand.ptr_val = ptr;
    return idx;
}

/* ------------------------------------------------------------------ */
/* Test : Creation / destruction du runtime                           */
/* ------------------------------------------------------------------ */

static void test_runtime_lifecycle(void)
{
    gfa_runtime *rt;

    printf("\n--- Runtime lifecycle ---\n");

    rt = gfa_runtime_init();
    TEST_ASSERT(rt != NULL, "gfa_runtime_init returns non-NULL");
    TEST_ASSERT(rt->globals != NULL, "Symbol table created");
    TEST_ASSERT(rt->sp == 0, "Value stack empty");
    TEST_ASSERT(rt->call_depth == 0, "Call stack empty");
    TEST_ASSERT(rt->running == 0, "Not running");

    gfa_runtime_shutdown(rt);
    TEST_ASSERT(1, "gfa_runtime_shutdown completes");
}

/* ------------------------------------------------------------------ */
/* Test : Table de symboles et variables                              */
/* ------------------------------------------------------------------ */

static void test_variables(void)
{
    gfa_runtime *rt;
    gfa_variable *var;
    gfa_symbol_table *table;

    printf("\n--- Variables et symboles ---\n");

    rt = gfa_runtime_init();
    TEST_ASSERT(rt != NULL, "Runtime created");

    table = rt->globals;

    /* Creation de variables */
    var = gfa_var_create(table, "test_float", GFA_VAR_FLOAT);
    TEST_ASSERT(var != NULL, "Float variable created");
    gfa_var_set_from_float(var, 3.14159);
    {
        double v = gfa_var_get_as_float(var);
        TEST_ASSERT(v > 3.14 && v < 3.15, "Float value stored/retrieved");
    }

    var = gfa_var_create(table, "test_long%", GFA_VAR_LONG);
    TEST_ASSERT(var != NULL, "Long variable created");
    gfa_var_set_from_long(var, 42);
    TEST_ASSERT(gfa_var_get_as_long(var) == 42, "Long value stored/retrieved");

    var = gfa_var_create(table, "test_str$", GFA_VAR_STRING);
    TEST_ASSERT(var != NULL, "String variable created");
    gfa_var_set_from_string(var, "Hello GFA");
    {
        const char *s = gfa_var_get_as_string(var);
        TEST_ASSERT(s != NULL && strcmp(s, "Hello GFA") == 0,
                    "String value stored/retrieved");
    }

    var = gfa_var_create(table, "test_bool!", GFA_VAR_BOOL);
    TEST_ASSERT(var != NULL, "Boolean variable created");
    gfa_var_set_from_float(var, -1.0);
    TEST_ASSERT(gfa_var_get_as_long(var) == -1, "Boolean TRUE = -1");

    /* Variables reservees */
    var = gfa_var_lookup(table, "TRUE");
    TEST_ASSERT(var != NULL, "TRUE exists");
    TEST_ASSERT(gfa_var_get_as_long(var) == -1, "TRUE == -1");

    var = gfa_var_lookup(table, "FALSE");
    TEST_ASSERT(var != NULL, "FALSE exists");
    TEST_ASSERT(gfa_var_get_as_long(var) == 0, "FALSE == 0");

    var = gfa_var_lookup(table, "PI");
    TEST_ASSERT(var != NULL, "PI exists");
    {
        double pi = gfa_var_get_as_float(var);
        TEST_ASSERT(pi > 3.14 && pi < 3.15, "PI ~= 3.14159");
    }

    /* Lookup inexistant */
    var = gfa_var_lookup(table, "NONEXISTENT");
    TEST_ASSERT(var == NULL, "Lookup nonexistent returns NULL");

    gfa_runtime_shutdown(rt);
}

/* ------------------------------------------------------------------ */
/* Test : Pile de valeurs                                             */
/* ------------------------------------------------------------------ */

static void test_value_stack(void)
{
    gfa_runtime *rt;
    gfa_value *val;

    printf("\n--- Value stack ---\n");

    rt = gfa_runtime_init();
    TEST_ASSERT(rt != NULL, "Runtime created");

    gfa_value_push_long(rt, 10);
    TEST_ASSERT(rt->sp == 1, "Stack has 1 element after push_long");

    gfa_value_push_float(rt, 3.14);
    TEST_ASSERT(rt->sp == 2, "Stack has 2 elements after push_float");

    gfa_value_push_string(rt, gfa_str_new("test"), 1);
    TEST_ASSERT(rt->sp == 3, "Stack has 3 elements after push_string");

    val = gfa_value_pop(rt);
    TEST_ASSERT(val != NULL, "Pop returns non-NULL");
    TEST_ASSERT(val->type == GFA_VAL_STRING, "Popped value is string");
    TEST_ASSERT(strcmp(val->data.s, "test") == 0, "Popped string matches");
    os_mem_free(val);

    val = gfa_value_pop(rt);
    TEST_ASSERT(val != NULL, "Pop #2 returns non-NULL");
    TEST_ASSERT(val->type == GFA_VAL_FLOAT, "Popped value is float");
    os_mem_free(val);

    val = gfa_value_pop(rt);
    TEST_ASSERT(val != NULL, "Pop #3 returns non-NULL");
    TEST_ASSERT(val->type == GFA_VAL_LONG, "Popped value is long");
    TEST_ASSERT(val->data.l == 10, "Popped long == 10");
    os_mem_free(val);

    TEST_ASSERT(rt->sp == 0, "Stack empty after pops");

    /* Conversion de types */
    gfa_value_push_bool(rt, -1);
    val = gfa_value_peek(rt, 0);
    TEST_ASSERT(gfa_value_to_float(val) == -1.0, "Bool to float: -1");
    TEST_ASSERT(gfa_value_to_long(val) == -1, "Bool to long: -1");
    TEST_ASSERT(gfa_value_to_bool(val) == -1, "Bool to bool: -1");
    gfa_value_discard(rt, 1);

    gfa_runtime_shutdown(rt);
}

/* ------------------------------------------------------------------ */
/* Test : Bytecode                                                    */
/* ------------------------------------------------------------------ */

static void test_bytecode(void)
{
    gfa_bytecode *bc;
    int idx;

    printf("\n--- Bytecode ---\n");

    bc = gfa_bytecode_create();
    TEST_ASSERT(bc != NULL, "Bytecode created");
    TEST_ASSERT(bc->length == 0, "Bytecode starts empty");

    idx = gfa_bytecode_emit(bc, OP_NOP);
    TEST_ASSERT(idx == 0, "First emit returns index 0");

    idx = gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)42);
    TEST_ASSERT(idx == 1, "emit_int returns index 1");
    TEST_ASSERT(bc->code[1].operand.float_val == 42.0, "Operand stored");

    gfa_bytecode_add_string(bc, "hello");
    gfa_bytecode_add_string(bc, "world");
    TEST_ASSERT(bc->str_count == 2, "String table has 2 entries");

    gfa_bytecode_patch(bc, 0, 999);
    TEST_ASSERT(bc->code[0].operand.int_val == 999, "Bytecode patched");

    gfa_bytecode_free(bc);
}

/* ------------------------------------------------------------------ */
/* Test : Execution simple (10 + 20 = 30)                             */
/* ------------------------------------------------------------------ */

static void test_simple_execution(void)
{
    gfa_runtime *rt;
    gfa_bytecode *bc;

    printf("\n--- Simple execution ---\n");

    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();

    gfa_var_create(rt->globals, "a", GFA_VAR_FLOAT);

    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)10);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)20);
    gfa_bytecode_emit(bc, OP_ADD);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "a"));
    gfa_bytecode_emit(bc, OP_END);

    TEST_ASSERT(bc->length == 5, "Bytecode has 5 instructions");
    TEST_ASSERT(gfa_runtime_load(rt, bc) == 0, "Bytecode loaded");
    TEST_ASSERT(gfa_runtime_execute(rt) == 0, "Execution completed");

    {
        gfa_variable *var = gfa_var_lookup(rt->globals, "a");
        TEST_ASSERT(var != NULL, "Variable 'a' exists");
        TEST_ASSERT(gfa_var_get_as_float(var) == 30.0, "10 + 20 = 30");
    }

    gfa_runtime_shutdown(rt);
}

/* ------------------------------------------------------------------ */
/* Test : Conditions (5 > 3)                                          */
/* ------------------------------------------------------------------ */

static void test_conditions(void)
{
    gfa_runtime *rt;
    gfa_bytecode *bc;
    int skip_target;

    printf("\n--- Conditions ---\n");

    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();

    gfa_var_create(rt->globals, "result", GFA_VAR_FLOAT);

    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)5);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)3);
    gfa_bytecode_emit(bc, OP_GT);
    skip_target = gfa_bytecode_emit_int(bc, OP_JMP_IF_FALSE, 0);

    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)1);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "result"));

    {
        int end_jmp = gfa_bytecode_emit_int(bc, OP_JMP, 0);
        gfa_bytecode_patch(bc, skip_target, (os_int32)bc->length);
        gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)0);
        emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "result"));
        gfa_bytecode_patch(bc, end_jmp, (os_int32)bc->length);
    }
    gfa_bytecode_emit(bc, OP_END);

    TEST_ASSERT(gfa_runtime_load(rt, bc) == 0, "Bytecode loaded");
    TEST_ASSERT(gfa_runtime_execute(rt) == 0, "Condition execution completed");

    {
        gfa_variable *var = gfa_var_lookup(rt->globals, "result");
        TEST_ASSERT(var != NULL, "Variable result exists");
        TEST_ASSERT(gfa_var_get_as_float(var) == 1.0, "5 > 3 => result = 1 (TRUE)");
    }

    gfa_runtime_shutdown(rt);
}

/* ------------------------------------------------------------------ */
/* Test : Tableaux                                                    */
/* ------------------------------------------------------------------ */

static void test_arrays(void)
{
    gfa_runtime *rt;
    gfa_variable *arr;
    os_int32 sizes[2];
    int indices[2];
    double *elem;

    printf("\n--- Arrays ---\n");

    rt = gfa_runtime_init();
    sizes[0] = 3; sizes[1] = 4;

    arr = gfa_var_array_create(rt->globals, "matrice",
                                GFA_VAR_FLOAT, 2, sizes);
    TEST_ASSERT(arr != NULL, "2D array created");
    TEST_ASSERT(arr->type == GFA_VAR_ARRAY, "Type is ARRAY");
    TEST_ASSERT(arr->value.arr.num_dims == 2, "2 dimensions");
    TEST_ASSERT(arr->value.arr.total_elements == 12, "3*4 = 12 elements");

    indices[0] = 1; indices[1] = 2;
    elem = (double *)gfa_var_array_get_element(arr, indices);
    TEST_ASSERT(elem != NULL, "Element access returns non-NULL");
    *elem = 99.5;
    {
        double *check = (double *)gfa_var_array_get_element(arr, indices);
        TEST_ASSERT(*check == 99.5, "Array element stored/retrieved");
    }

    gfa_var_array_fill(arr, 7.0);
    {
        indices[0] = 0; indices[1] = 0;
        double *check = (double *)gfa_var_array_get_element(arr, indices);
        TEST_ASSERT(check != NULL && *check == 7.0, "ARRAYFILL sets all elements");
    }

    TEST_ASSERT(gfa_var_array_count(arr) == 12, "DIM? returns 12");

    gfa_runtime_shutdown(rt);
}

/* ------------------------------------------------------------------ */
/* Test : Fonctions mathematiques (OP_CALL_BUILTIN)                   */
/* ------------------------------------------------------------------ */

static void test_builtin_math(void)
{
    gfa_runtime *rt;
    gfa_bytecode *bc;

    printf("\n--- Built-in math functions ---\n");

    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();

    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);

    /* ABS(-5) = 5 */
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, -5.0);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_ABS);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);

    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r")) == 5.0,
                "ABS(-5) = 5");
    gfa_runtime_shutdown(rt);

    /* SQR(16) = 4 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)16);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_SQR);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r")) == 4.0,
                "SQR(16) = 4");
    gfa_runtime_shutdown(rt);
}

/* ------------------------------------------------------------------ */
/* Test : Fonctions chaines (OP_CALL_BUILTIN)                        */
/* ------------------------------------------------------------------ */

static void test_builtin_strings(void)
{
    gfa_runtime *rt;
    gfa_bytecode *bc;

    printf("\n--- Built-in string functions ---\n");

    /* LEN("hello") = 5 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_str(bc, OP_PUSH_STRING, "hello");
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_LEN);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r")) == 5.0,
                "LEN('hello') = 5");
    gfa_runtime_shutdown(rt);

    /* ASC("A") = 65 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_str(bc, OP_PUSH_STRING, "A");
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_ASC);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r")) == 65.0,
                "ASC('A') = 65");
    gfa_runtime_shutdown(rt);

    /* LEFT("hello", 2) returns string */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r$", GFA_VAR_STRING);
    gfa_bytecode_emit_str(bc, OP_PUSH_STRING, "hello");
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)2);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_LEFT_TOK);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r$"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    {
        const char *s = gfa_var_get_as_string(gfa_var_lookup(rt->globals, "r$"));
        TEST_ASSERT(s != NULL && strcmp(s, "he") == 0, "LEFT('hello',2) = 'he'");
    }
    gfa_runtime_shutdown(rt);

    /* UPPER("hello") = "HELLO" */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r$", GFA_VAR_STRING);
    gfa_bytecode_emit_str(bc, OP_PUSH_STRING, "hello");
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_UPPER_TOK);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r$"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    {
        const char *s = gfa_var_get_as_string(gfa_var_lookup(rt->globals, "r$"));
        TEST_ASSERT(s != NULL && strcmp(s, "HELLO") == 0, "UPPER('hello') = 'HELLO'");
    }
    gfa_runtime_shutdown(rt);
}

/* ------------------------------------------------------------------ */
/* Test : END complet (sans crash)                                    */
/* ------------------------------------------------------------------ */

static void test_end_to_end(void)
{
    gfa_runtime *rt;
    gfa_bytecode *bc;

    printf("\n--- End-to-end ---\n");

    /* Programme: a=3; b=4; c=a+b; d=c*2 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "a", GFA_VAR_FLOAT);
    gfa_var_create(rt->globals, "b", GFA_VAR_FLOAT);
    gfa_var_create(rt->globals, "c", GFA_VAR_FLOAT);
    gfa_var_create(rt->globals, "d", GFA_VAR_FLOAT);

    /* a = 3 */
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)3);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "a"));
    /* b = 4 */
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)4);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "b"));
    /* c = a + b */
    emit_ptr(bc, OP_PUSH_VAR, gfa_var_lookup(rt->globals, "a"));
    emit_ptr(bc, OP_PUSH_VAR, gfa_var_lookup(rt->globals, "b"));
    gfa_bytecode_emit(bc, OP_ADD);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "c"));
    /* d = c * 2 */
    emit_ptr(bc, OP_PUSH_VAR, gfa_var_lookup(rt->globals, "c"));
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)2);
    gfa_bytecode_emit(bc, OP_MUL);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "d"));
    gfa_bytecode_emit(bc, OP_END);

    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);

    TEST_ASSERT(gfa_var_get_as_float(gfa_var_lookup(rt->globals, "a")) == 3.0, "a = 3");
    TEST_ASSERT(gfa_var_get_as_float(gfa_var_lookup(rt->globals, "b")) == 4.0, "b = 4");
    TEST_ASSERT(gfa_var_get_as_float(gfa_var_lookup(rt->globals, "c")) == 7.0, "c = 7");
    TEST_ASSERT(gfa_var_get_as_float(gfa_var_lookup(rt->globals, "d")) == 14.0, "d = 14");

    gfa_runtime_shutdown(rt);
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("========================================\n");
    printf(" Tests du Runtime GFA Basic 3.5\n");
    printf("========================================\n");

    g_tests_run    = 0;
    g_tests_passed = 0;
    g_tests_failed = 0;

    test_runtime_lifecycle();
    test_variables();
    test_value_stack();
    test_bytecode();
    test_simple_execution();
    test_conditions();
    test_arrays();
    test_builtin_math();
    test_builtin_strings();
    test_end_to_end();

    printf("\n========================================\n");
    printf(" Resultat : %d/%d reussis, %d echoues\n",
           g_tests_passed, g_tests_run, g_tests_failed);
    printf("========================================\n");

    if (g_tests_failed > 0) {
        printf("\n*** %d TEST(S) ECHOUES ***\n", g_tests_failed);
        return 1;
    }

    printf("\n*** TOUS LES TESTS ONT REUSSI ***\n");
    return 0;
}
