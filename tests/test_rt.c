/*
 * test_rt.c - Tests du moteur d'execution GFA Basic 3.5
 * =====================================================
 * Valide : runtime lifecycle, variables, value stack, bytecode,
 *          execution, conditions, arrays, et nouvelles fonctions.
 * Mise a jour : 7 juin 2026.
 */

#include "runtime.h"
#include "token.h"
#include "files.h"
#include "events.h"
#include "sound.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

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
    double *check;

    printf("\n--- Arrays ---\n");

    rt = gfa_runtime_init();
    sizes[0] = 3; sizes[1] = 4;

    arr = gfa_var_array_create(rt->globals, "matrice",
                                GFA_VAR_FLOAT, 2, sizes, 0);
    TEST_ASSERT(arr != NULL, "2D array created");
    TEST_ASSERT(arr->type == GFA_VAR_ARRAY, "Type is ARRAY");
    TEST_ASSERT(arr->value.arr.num_dims == 2, "2 dimensions");
    TEST_ASSERT(arr->value.arr.total_elements == 12, "3*4 = 12 elements");

    indices[0] = 1; indices[1] = 2;
    elem = (double *)gfa_var_array_get_element(arr, indices);
    TEST_ASSERT(elem != NULL, "Element access returns non-NULL");
    *elem = 99.5;
    check = (double *)gfa_var_array_get_element(arr, indices);
    TEST_ASSERT(*check == 99.5, "Array element stored/retrieved");

    gfa_var_array_fill(arr, 7.0);
    indices[0] = 0; indices[1] = 0;
    check = (double *)gfa_var_array_get_element(arr, indices);
    TEST_ASSERT(check != NULL && *check == 7.0, "ARRAYFILL sets all elements");

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

    /* SINH(0) = 0 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 0.0);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_SINH);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r")) == 0.0,
                "SINH(0) = 0");
    gfa_runtime_shutdown(rt);

    /* COSH(0) = 1 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 0.0);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_COSH);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r")) == 1.0,
                "COSH(0) = 1");
    gfa_runtime_shutdown(rt);

    /* TANH(0) = 0 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 0.0);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_TANH);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r")) == 0.0,
                "TANH(0) = 0");
    gfa_runtime_shutdown(rt);

    /* SIN(0) = 0 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 0.0);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_SIN);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    {
        double v = gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r"));
        TEST_ASSERT(v > -0.001 && v < 0.001, "SIN(0) = 0");
    }
    gfa_runtime_shutdown(rt);

    /* COS(0) = 1 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 0.0);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_COS);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    {
        double v = gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r"));
        TEST_ASSERT(v > 0.999 && v < 1.001, "COS(0) = 1");
    }
    gfa_runtime_shutdown(rt);

    /* TAN(0) = 0 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 0.0);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_TAN);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    {
        double v = gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r"));
        TEST_ASSERT(v > -0.001 && v < 0.001, "TAN(0) = 0");
    }
    gfa_runtime_shutdown(rt);

    /* ATN(0) = 0 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 0.0);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_ATN);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    {
        double v = gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r"));
        TEST_ASSERT(v > -0.001 && v < 0.001, "ATN(0) = 0");
    }
    gfa_runtime_shutdown(rt);

    /* ASIN(0) = 0 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 0.0);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_ASIN);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    {
        double v = gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r"));
        TEST_ASSERT(v > -0.001 && v < 0.001, "ASIN(0) = 0");
    }
    gfa_runtime_shutdown(rt);

    /* ACOS(1) = 0 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)1);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_ACOS);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    {
        double v = gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r"));
        TEST_ASSERT(v > -0.001 && v < 0.001, "ACOS(1) = 0");
    }
    gfa_runtime_shutdown(rt);

    /* SINQ(0) = 0 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 0.0);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_SINQ);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    {
        double v = gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r"));
        TEST_ASSERT(v > -0.001 && v < 0.001, "SINQ(0) = 0");
    }
    gfa_runtime_shutdown(rt);

    /* COSQ(1) = 1 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)1);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_COSQ);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    {
        double v = gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r"));
        TEST_ASSERT(v > 0.999 && v < 1.001, "COSQ(1) = 1");
    }
    gfa_runtime_shutdown(rt);

    /* EXP(0) = 1 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 0.0);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_EXP);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    {
        double v = gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r"));
        TEST_ASSERT(v > 0.999 && v < 1.001, "EXP(0) = 1");
    }
    gfa_runtime_shutdown(rt);

    /* LOG(1) = 0 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)1);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_LOG);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    {
        double v = gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r"));
        TEST_ASSERT(v > -0.001 && v < 0.001, "LOG(1) = 0");
    }
    gfa_runtime_shutdown(rt);

    /* LOG10(1) = 0 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)1);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_LOG10);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    {
        double v = gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r"));
        TEST_ASSERT(v > -0.001 && v < 0.001, "LOG10(1) = 0");
    }
    gfa_runtime_shutdown(rt);

    /* SGN(-5) = -1 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, -5.0);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_SGN);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r")) == -1.0,
                "SGN(-5) = -1");
    gfa_runtime_shutdown(rt);

    /* SGN(5) = 1 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)5);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_SGN);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r")) == 1.0,
                "SGN(5) = 1");
    gfa_runtime_shutdown(rt);

    /* INT(3.7) = 3 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 3.7);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_INT);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r")) == 3.0,
                "INT(3.7) = 3");
    gfa_runtime_shutdown(rt);

    /* FRAC(3.7) = 0.7 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 3.7);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_FRAC);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    {
        double v = gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r"));
        TEST_ASSERT(v > 0.699 && v < 0.701, "FRAC(3.7) = 0.7");
    }
    gfa_runtime_shutdown(rt);

    /* FIX(-3.7) = -3 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, -3.7);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_FIX);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r")) == -3.0,
                "FIX(-3.7) = -3");
    gfa_runtime_shutdown(rt);

    /* ROUND(3.5) = 4 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 3.5);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_ROUND);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r")) == 4.0,
                "ROUND(3.5) = 4");
    gfa_runtime_shutdown(rt);

    /* CEIL(3.2) = 4 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 3.2);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_CEIL_TOK);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r")) == 4.0,
                "CEIL(3.2) = 4");
    gfa_runtime_shutdown(rt);

    /* TRUNC(3.7) = 3 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 3.7);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_TRUNC_TOK);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r")) == 3.0,
                "TRUNC(3.7) = 3");
    gfa_runtime_shutdown(rt);

    /* PRED(5) = 4 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)5);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_PRED);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_long(gfa_var_lookup(rt->globals, "r")) == 4,
                "PRED(5) = 4");
    gfa_runtime_shutdown(rt);

    /* SUCC(5) = 6 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)5);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_SUCC);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_long(gfa_var_lookup(rt->globals, "r")) == 6,
                "SUCC(5) = 6");
    gfa_runtime_shutdown(rt);

    /* FACT(5) = 120 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)5);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_FACT);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_long(gfa_var_lookup(rt->globals, "r")) == 120,
                "FACT(5) = 120");
    gfa_runtime_shutdown(rt);

    /* RND(1) >= 0 and <= 1 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)1);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_RND);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    {
        double v = gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r"));
        TEST_ASSERT(v >= 0.0 && v <= 1.0, "RND(1) is in [0,1]");
    }
    gfa_runtime_shutdown(rt);

    /* DEG(3.14159) approx 180 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 3.14159);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_DEG);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    {
        double v = gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r"));
        TEST_ASSERT(v > 179.9 && v < 180.1, "DEG(3.14159) approx 180");
    }
    gfa_runtime_shutdown(rt);

    /* RAD(180) approx 3.14159 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)180);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_RAD);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    {
        double v = gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r"));
        TEST_ASSERT(v > 3.14 && v < 3.15, "RAD(180) approx 3.14159");
    }
    gfa_runtime_shutdown(rt);

    /* CFLOAT(5) = 5.0 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)5);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_CFLOAT);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r")) == 5.0,
                "CFLOAT(5) = 5.0");
    gfa_runtime_shutdown(rt);

    /* EVEN(4) != 0 (TRUE) */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_LONG);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)4);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_EVEN);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_long(gfa_var_lookup(rt->globals, "r")) != 0,
                "EVEN(4) is TRUE (nonzero)");
    gfa_runtime_shutdown(rt);

    /* ODD(3) != 0 (TRUE) */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_LONG);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)3);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_ODD);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_long(gfa_var_lookup(rt->globals, "r")) != 0,
                "ODD(3) is TRUE (nonzero)");
    gfa_runtime_shutdown(rt);

    /* MIN(3,7) = 3 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)3);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)7);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_MIN);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r")) == 3.0,
                "MIN(3,7) = 3");
    gfa_runtime_shutdown(rt);

    /* MAX(3,7) = 7 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)3);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)7);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_MAX);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r")) == 7.0,
                "MAX(3,7) = 7");
    gfa_runtime_shutdown(rt);

    /* COMBIN(5,2) = 10 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)5);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)2);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_COMBIN);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r")) == 10.0,
                "COMBIN(5,2) = 10");
    gfa_runtime_shutdown(rt);

    /* VARIAT(5) = 120 (factorielle — sémantique GFA 1 argument) */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)5);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_VARIAT);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r")) == 120.0,
                "VARIAT(5) = 120");
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
/* Test : Fonctions chaines etendues                                  */
/* ------------------------------------------------------------------ */

static void test_builtin_strings_extended(void)
{
    gfa_runtime *rt;
    gfa_bytecode *bc;

    printf("\n--- Built-in string functions (extended) ---\n");

    /* CHR$(65) = "A" */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r$", GFA_VAR_STRING);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)65);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_CHR_TOK);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r$"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    {
        const char *s = gfa_var_get_as_string(gfa_var_lookup(rt->globals, "r$"));
        TEST_ASSERT(s != NULL && strcmp(s, "A") == 0, "CHR$(65) = 'A'");
    }
    gfa_runtime_shutdown(rt);

    /* VAL("123") = 123 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_str(bc, OP_PUSH_STRING, "123");
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_VAL);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r")) == 123.0,
                "VAL('123') = 123");
    gfa_runtime_shutdown(rt);

    /* RIGHT$("hello",2) = "lo" */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r$", GFA_VAR_STRING);
    gfa_bytecode_emit_str(bc, OP_PUSH_STRING, "hello");
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)2);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_RIGHT_TOK);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r$"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    {
        const char *s = gfa_var_get_as_string(gfa_var_lookup(rt->globals, "r$"));
        TEST_ASSERT(s != NULL && strcmp(s, "lo") == 0, "RIGHT$('hello',2) = 'lo'");
    }
    gfa_runtime_shutdown(rt);

    /* MID$("hello",2,3) = "ell" */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r$", GFA_VAR_STRING);
    gfa_bytecode_emit_str(bc, OP_PUSH_STRING, "hello");
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)2);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)3);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_MID_TOK);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r$"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    {
        const char *s = gfa_var_get_as_string(gfa_var_lookup(rt->globals, "r$"));
        TEST_ASSERT(s != NULL && strcmp(s, "ell") == 0, "MID$('hello',2,3) = 'ell'");
    }
    gfa_runtime_shutdown(rt);

    /* INSTR("hello","ll") = 3 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_str(bc, OP_PUSH_STRING, "hello");
    gfa_bytecode_emit_str(bc, OP_PUSH_STRING, "ll");
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_INSTR);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r")) == 3.0,
                "INSTR('hello','ll') = 3");
    gfa_runtime_shutdown(rt);

    /* RINSTR("hello","l") = 4 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_FLOAT);
    gfa_bytecode_emit_str(bc, OP_PUSH_STRING, "hello");
    gfa_bytecode_emit_str(bc, OP_PUSH_STRING, "l");
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_RINSTR);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_float(gfa_var_lookup(rt->globals, "r")) == 4.0,
                "RINSTR('hello','l') = 4");
    gfa_runtime_shutdown(rt);

    /* LCASE$("HELLO") = "hello" */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r$", GFA_VAR_STRING);
    gfa_bytecode_emit_str(bc, OP_PUSH_STRING, "HELLO");
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_LCASE_TOK);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r$"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    {
        const char *s = gfa_var_get_as_string(gfa_var_lookup(rt->globals, "r$"));
        TEST_ASSERT(s != NULL && strcmp(s, "hello") == 0, "LCASE$('HELLO') = 'hello'");
    }
    gfa_runtime_shutdown(rt);

    /* TRIM$("  hello  ") = "hello" */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r$", GFA_VAR_STRING);
    gfa_bytecode_emit_str(bc, OP_PUSH_STRING, "  hello  ");
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_TRIM_TOK);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r$"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    {
        const char *s = gfa_var_get_as_string(gfa_var_lookup(rt->globals, "r$"));
        TEST_ASSERT(s != NULL && strcmp(s, "hello") == 0, "TRIM$('  hello  ') = 'hello'");
    }
    gfa_runtime_shutdown(rt);

    /* STR$(123) = "123" */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r$", GFA_VAR_STRING);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)123);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_STR_TOK);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r$"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    {
        const char *s = gfa_var_get_as_string(gfa_var_lookup(rt->globals, "r$"));
        TEST_ASSERT(s != NULL && strcmp(s, "123") == 0, "STR$(123) = '123'");
    }
    gfa_runtime_shutdown(rt);

    /* BIN$(5) */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r$", GFA_VAR_STRING);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)5);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_BIN_TOK);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r$"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    {
        const char *s = gfa_var_get_as_string(gfa_var_lookup(rt->globals, "r$"));
        TEST_ASSERT(s != NULL && strlen(s) > 0, "BIN$(5) returns non-empty string");
    }
    gfa_runtime_shutdown(rt);

    /* HEX$(255) */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r$", GFA_VAR_STRING);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)255);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_HEX_TOK);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r$"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    {
        const char *s = gfa_var_get_as_string(gfa_var_lookup(rt->globals, "r$"));
        TEST_ASSERT(s != NULL && strlen(s) > 0, "HEX$(255) returns non-empty string");
    }
    gfa_runtime_shutdown(rt);

    /* OCT$(8) */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r$", GFA_VAR_STRING);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)8);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_OCT_TOK);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r$"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    {
        const char *s = gfa_var_get_as_string(gfa_var_lookup(rt->globals, "r$"));
        TEST_ASSERT(s != NULL && strlen(s) > 0, "OCT$(8) returns non-empty string");
    }
    gfa_runtime_shutdown(rt);

    /* SPACE$(5) = "     " */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r$", GFA_VAR_STRING);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)5);
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_SPACE_TOK);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r$"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    {
        const char *s = gfa_var_get_as_string(gfa_var_lookup(rt->globals, "r$"));
        TEST_ASSERT(s != NULL && strcmp(s, "     ") == 0, "SPACE$(5) = '     '");
    }
    gfa_runtime_shutdown(rt);

    /* STRING$(3,"A") = "AAA" */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r$", GFA_VAR_STRING);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, (double)3);
    gfa_bytecode_emit_str(bc, OP_PUSH_STRING, "A");
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_STRING_TOK);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r$"));
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    {
        const char *s = gfa_var_get_as_string(gfa_var_lookup(rt->globals, "r$"));
        TEST_ASSERT(s != NULL && strcmp(s, "AAA") == 0, "STRING$(3,'A') = 'AAA'");
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
/* Test : IO files (files.h)                                          */
/* ------------------------------------------------------------------ */

static void test_io(void)
{
    char buf[256];
    int ret;
    int count;

    printf("\n--- IO files ---\n");

    /* Lifecycle */
    gfa_files_init();
    count = gfa_files_get_count();
    TEST_ASSERT(count == 0, "gfa_files_init: no files open");

    /* Open for output, write, close */
    ret = gfa_open("O", 1, "test_io.tmp", 0);
    TEST_ASSERT(ret == 0, "gfa_open('O', 1, ...) returns 0");
    count = gfa_files_get_count();
    TEST_ASSERT(count == 1, "1 file open after open");

    ret = gfa_print_channel(1, "hello");
    TEST_ASSERT(ret == 0, "gfa_print_channel returns 0");

    gfa_close(1);
    count = gfa_files_get_count();
    TEST_ASSERT(count == 0, "0 files open after close");

    /* Open for input, read, close */
    ret = gfa_open("I", 1, "test_io.tmp", 0);
    TEST_ASSERT(ret == 0, "gfa_open('I', 1, ...) returns 0");

    ret = gfa_input_channel(1, buf, (int)sizeof(buf));
    TEST_ASSERT(ret > 0, "gfa_input_channel reads data");
    TEST_ASSERT(strcmp(buf, "hello") == 0, "Read back 'hello'");

    gfa_close(1);

    /* EOF test on empty file */
    {
        gfa_open("I", 2, "test_io.tmp", 0);
        gfa_input_channel(2, buf, (int)sizeof(buf));  /* consume remaining */
        ret = gfa_eof(2);
        TEST_ASSERT(ret == -1, "gfa_eof returns -1 (TRUE) after reading all");
        gfa_close(2);
    }

    /* EXIST */
    ret = gfa_exist("test_io.tmp");
    TEST_ASSERT(ret == -1, "gfa_exist('test_io.tmp') returns -1 (TRUE)");

    /* KILL */
    gfa_kill("test_io.tmp");
    ret = gfa_exist("test_io.tmp");
    TEST_ASSERT(ret == 0, "gfa_exist returns 0 (FALSE) after kill");

    /* EXIST on nonexistent */
    ret = gfa_exist("nonexistent_file_12345");
    TEST_ASSERT(ret == 0, "gfa_exist('nonexistent') returns 0 (FALSE)");

    gfa_files_shutdown();
    TEST_ASSERT(1, "gfa_files_shutdown completes");
}

/* ------------------------------------------------------------------ */
/* Test : Memory (DATA/READ/RESTORE, PEEK/POKE via bytecode)          */
/* ------------------------------------------------------------------ */

static void test_memory(void)
{
    gfa_runtime *rt;
    gfa_bytecode *bc;

    printf("\n--- Memory (DATA/READ/RESTORE, PEEK/POKE) ---\n");

    /* Test DATA/READ/RESTORE via bytecode */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();

    gfa_var_create(rt->globals, "a", GFA_VAR_FLOAT);
    gfa_var_create(rt->globals, "b", GFA_VAR_FLOAT);
    gfa_var_create(rt->globals, "c", GFA_VAR_FLOAT);

    {
        int i;
        bc->data_values = (double *)os_mem_alloc(3 * sizeof(double));
        for (i = 0; i < 3; i++) {
            bc->data_values[i] = (double)((i + 1) * 10);
        }
        bc->data_count = 3;
        bc->data_ptr = 0;
    }

    /* READ a (should get 10) */
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK__DATA);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "a"));
    /* READ b (should get 20) */
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK__DATA);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "b"));
    /* READ c (should get 30) */
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK__DATA);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "c"));
    /* RESTORE */
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK_RESTORE);
    /* READ a again (should get 10 again) */
    gfa_bytecode_emit_int(bc, OP_CALL_BUILTIN, (os_int32)TOK__DATA);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "a"));
    gfa_bytecode_emit(bc, OP_END);

    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);

    TEST_ASSERT(gfa_var_get_as_float(gfa_var_lookup(rt->globals, "a")) == 10.0,
                "READ a = 10 (first value)");
    TEST_ASSERT(gfa_var_get_as_float(gfa_var_lookup(rt->globals, "b")) == 20.0,
                "READ b = 20 (second value)");
    TEST_ASSERT(gfa_var_get_as_float(gfa_var_lookup(rt->globals, "c")) == 30.0,
                "READ c = 30 (third value)");

    gfa_runtime_shutdown(rt);

    /* Test PEEK/POKE via bytecode */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();

    gfa_var_create(rt->globals, "addr", GFA_VAR_LONG);
    gfa_var_create(rt->globals, "val", GFA_VAR_LONG);
    gfa_var_create(rt->globals, "result", GFA_VAR_LONG);

    /* POKE 12345, 42 (address 12345, value 42) */
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 12345.0);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 42.0);
    gfa_bytecode_emit(bc, OP_POKE);
    /* PEEK(12345) -> pushes value */
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 12345.0);
    gfa_bytecode_emit(bc, OP_PEEK);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "result"));
    gfa_bytecode_emit(bc, OP_END);

    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);

    TEST_ASSERT(gfa_var_get_as_long(gfa_var_lookup(rt->globals, "result")) == 42,
                "PEEK(12345) renvoie la valeur ecrite par POKE");

    gfa_runtime_shutdown(rt);

    /* Test DPEEK/DPOKE, LPEEK/LPOKE */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();

    gfa_var_create(rt->globals, "r", GFA_VAR_LONG);

    /* DPOKE 1000, 65 */
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 1000.0);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 65.0);
    gfa_bytecode_emit(bc, OP_DPOKE);
    /* DPEEK(1000) -> pushes 1000 */
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 1000.0);
    gfa_bytecode_emit(bc, OP_DPEEK);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);

    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_long(gfa_var_lookup(rt->globals, "r")) == 65,
                "DPEEK(1000) renvoie le mot ecrit par DPOKE");
    gfa_runtime_shutdown(rt);

    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_LONG);

    /* LPOKE 2000, 123456 */
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 2000.0);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 123456.0);
    gfa_bytecode_emit(bc, OP_LPOKE);
    /* LPEEK(2000) -> pushes 2000 */
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 2000.0);
    gfa_bytecode_emit(bc, OP_LPEEK);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);

    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_long(gfa_var_lookup(rt->globals, "r")) == 123456,
                "LPEEK(2000) renvoie le long ecrit par LPOKE");
    gfa_runtime_shutdown(rt);

    /* Test SPOKE/SDPOKE/SLPOKE */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_LONG);

    /* SPOKE 500, 77 puis lecture PEEK(500) -> 77 */
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 500.0);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 77.0);
    gfa_bytecode_emit(bc, OP_SPOKE);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 500.0);
    gfa_bytecode_emit(bc, OP_PEEK);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);

    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_long(gfa_var_lookup(rt->globals, "r")) == 77,
                "SPOKE 500,77 puis PEEK(500) renvoie 77");
    gfa_runtime_shutdown(rt);

    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_LONG);

    /* SDPOKE 2000, 42 puis lecture DPEEK(2000) -> 42 */
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 2000.0);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 42.0);
    gfa_bytecode_emit(bc, OP_SDPOKE);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 2000.0);
    gfa_bytecode_emit(bc, OP_DPEEK);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);

    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_long(gfa_var_lookup(rt->globals, "r")) == 42,
                "SDPOKE 2000,42 puis DPEEK(2000) renvoie 42");
    gfa_runtime_shutdown(rt);

    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_var_create(rt->globals, "r", GFA_VAR_LONG);

    /* SLPOKE 3000, 1234567 puis lecture LPEEK(3000) -> 1234567 */
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 3000.0);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 1234567.0);
    gfa_bytecode_emit(bc, OP_SLPOKE);
    gfa_bytecode_emit_float(bc, OP_PUSH_CONST, 3000.0);
    gfa_bytecode_emit(bc, OP_LPEEK);
    emit_ptr(bc, OP_POP_STORE, gfa_var_lookup(rt->globals, "r"));
    gfa_bytecode_emit(bc, OP_END);

    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(gfa_var_get_as_long(gfa_var_lookup(rt->globals, "r")) == 1234567,
                "SLPOKE 3000,1234567 puis LPEEK(3000) renvoie 1234567");
    gfa_runtime_shutdown(rt);
}

/* ------------------------------------------------------------------ */
/* Test : Evenements (events.h)                                       */
/* ------------------------------------------------------------------ */

static void test_events(void)
{
    int err;

    printf("\n--- Events ---\n");

    /* Lifecycle */
    gfa_events_init();
    TEST_ASSERT(1, "gfa_events_init completes");

    /* Break */
    gfa_break_enable();
    /* break_check returns OS_FALSE (no actual break pending) */
    TEST_ASSERT(gfa_break_check() == OS_FALSE,
                "gfa_break_check() returns OS_FALSE");

    /* Error get/clear */
    err = gfa_error_get();
    TEST_ASSERT(err == 0, "gfa_error_get() returns 0 initially");

    gfa_error_raise(42);
    err = gfa_error_get();
    TEST_ASSERT(err == 42, "gfa_error_get() returns 42 after raise");

    gfa_error_clear();
    err = gfa_error_get();
    TEST_ASSERT(err == 0, "gfa_error_get() returns 0 after clear");

    /* Error string */
    {
        const char *msg;
        msg = gfa_error_get_string(0);
        TEST_ASSERT(msg != NULL, "gfa_error_get_string(0) returns non-NULL");
        msg = gfa_error_get_string(99);
        TEST_ASSERT(msg != NULL, "gfa_error_get_string(99) returns non-NULL");
    }

    /* AFTER + AFTER STOP */
    {
        int ret;
        ret = gfa_after(50, 1);
        TEST_ASSERT(ret == 0, "gfa_after(50, 1) returns 0");
        gfa_after_stop();
        TEST_ASSERT(1, "gfa_after_stop completes");
    }

    /* EVERY (short test) */
    {
        int ret;
        ret = gfa_every(100, 2);
        TEST_ASSERT(ret == 0, "gfa_every(100, 2) returns 0");
        gfa_every_stop();
        TEST_ASSERT(1, "gfa_every_stop completes");
    }

    /* Events poll */
    {
        int triggered;
        triggered = gfa_events_poll();
        TEST_ASSERT(triggered == 0, "gfa_events_poll() returns 0 with no active events");
    }

    gfa_events_shutdown();
    TEST_ASSERT(1, "gfa_events_shutdown completes");
}

/* ------------------------------------------------------------------ */
/* Test : Son (sound.h)                                               */
/* ------------------------------------------------------------------ */

static void test_sound(void)
{
    printf("\n--- Sound ---\n");

    /* Lifecycle */
    {
        int ret;
        ret = gfa_sound_init();
        TEST_ASSERT(ret == 0, "gfa_sound_init() returns 0");
    }

    /* BEEP */
    gfa_beep();
    TEST_ASSERT(1, "gfa_beep() completes");

    /* Sound on channel 0 */
    gfa_sound(0, 440, 100, 10, 0);
    TEST_ASSERT(1, "gfa_sound(0, 440, 100, 10, 0) completes");

    /* Stop channel 0 */
    gfa_sound_stop(0);
    TEST_ASSERT(1, "gfa_sound_stop(0) completes");

    /* Sound on channels 1 and 2 */
    gfa_sound(1, 880, 50, 8, 0);
    gfa_sound(2, 1760, 30, 6, 0);
    TEST_ASSERT(1, "gfa_sound on channels 1,2 completes");

    /* Stop all */
    gfa_sound_stop_all();
    TEST_ASSERT(1, "gfa_sound_stop_all() completes");

    /* Get channel state after stop */
    {
        int freq, vol;
        freq = gfa_sound_get_channel_freq(0);
        vol = gfa_sound_get_channel_volume(0);
        /* freq retains YM register value (only volume/mixer cleared) */
        TEST_ASSERT(freq > 0, "gfa_sound_get_channel_freq(0) returns period > 0 (register not cleared)");
        TEST_ASSERT(vol == 0, "gfa_sound_get_channel_volume(0) returns 0 after stop");
    }

    /* Sound poll */
    gfa_sound_poll();
    TEST_ASSERT(1, "gfa_sound_poll() completes");

    gfa_sound_shutdown();
    TEST_ASSERT(1, "gfa_sound_shutdown() completes");
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
    test_builtin_strings_extended();
    test_io();
    test_memory();
    test_events();
    test_sound();
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
