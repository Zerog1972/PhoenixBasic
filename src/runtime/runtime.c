/*
 * runtime.c - Implementation du moteur d'execution GFA Basic 3.5
 * ==============================================================
 * Coeur de l'emulateur : boucle d'execution du bytecode,
 * gestion de la pile de valeurs, pile d'appels, contexte.
 *
 * Reference : cahier-des-charges-gfabasic.md, section 7
 */

#include "runtime.h"
#include "token.h"
#include "files.h"
#include "events.h"
#include "sound.h"
#include "tos.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/* Constantes internes                                                */
/* ------------------------------------------------------------------ */

#define RUNTIME_HASH_BUCKETS  256
/* GFA_BYTECODE_INIT_SIZE defini dans runtime.h */
#define RUNTIME_MAX_LINE      999999

/* ------------------------------------------------------------------ */
/* Forward declarations des fonctions d'execution specifiques         */
/* ------------------------------------------------------------------ */

static int execute_instruction(gfa_runtime *rt);
static void runtime_error(gfa_runtime *rt, int code, const char *msg);

/* ------------------------------------------------------------------ */
/* Initialisation / Arret du runtime                                  */
/* ------------------------------------------------------------------ */

gfa_runtime *gfa_runtime_init(void)
{
    gfa_runtime *rt;

    rt = (gfa_runtime *)os_mem_alloc(sizeof(gfa_runtime));
    if (rt == NULL) {
        return NULL;
    }

    os_mem_set(rt, 0, sizeof(gfa_runtime));

    /* Initialiser la table de symboles globale */
    rt->globals = gfa_symbol_table_init(RUNTIME_HASH_BUCKETS);
    if (rt->globals == NULL) {
        os_mem_free(rt);
        return NULL;
    }

    /* Initialiser les modules */
    gfa_files_init();
    gfa_events_init();

    /* Variables reservees */
    {
        gfa_variable *v;

        v = gfa_var_create(rt->globals, "FALSE", GFA_VAR_BOOL);
        if (v != NULL) { v->value.bool_val = 0; v->is_reserved = 1; }

        v = gfa_var_create(rt->globals, "TRUE", GFA_VAR_BOOL);
        if (v != NULL) { v->value.bool_val = (os_byte)255; /* -1 en booleen */
                         v->is_reserved = 1; }

        v = gfa_var_create(rt->globals, "PI", GFA_VAR_FLOAT);
        if (v != NULL) { v->value.float_val = GFA_PI; v->is_reserved = 1; }

        v = gfa_var_create(rt->globals, "_C", GFA_VAR_LONG);
        if (v != NULL) { v->value.long_val = 16; v->is_reserved = 1; }

        v = gfa_var_create(rt->globals, "_X", GFA_VAR_LONG);
        if (v != NULL) { v->value.long_val = 320; v->is_reserved = 1; }

        v = gfa_var_create(rt->globals, "_Y", GFA_VAR_LONG);
        if (v != NULL) { v->value.long_val = 200; v->is_reserved = 1; }

        v = gfa_var_create(rt->globals, "TIMER", GFA_VAR_LONG);
        if (v != NULL) v->is_reserved = 1;
    }

    /* Etat graphique par defaut */
    rt->screen_mode   = 0;
    rt->screen_width  = 320;
    rt->screen_height = 200;
    rt->current_color = 1;
    rt->fill_color    = 1;
    rt->fill_style    = 1;
    rt->fill_pattern  = 1;
    rt->line_style    = 1;
    rt->line_thickness= 1;
    rt->cursor_x      = 1;
    rt->cursor_y      = 1;

    /* Par defaut : OPTION BASE 0 */
    rt->globals->option_base = 0;
    rt->globals->def_type    = '#'; /* Float par defaut */

    rt->running = 0;
    rt->stopped = 0;
    rt->quit_code = 0;
    rt->trace_on = 0;
    rt->current_line = 0;

    return rt;
}

void gfa_runtime_shutdown(gfa_runtime *rt)
{
    if (rt == NULL) return;

    rt->running = 0;

    gfa_files_shutdown();
    gfa_events_shutdown();

    if (rt->program != NULL) {
        gfa_bytecode_free(rt->program);
        rt->program = NULL;
    }

    if (rt->globals != NULL) {
        gfa_symbol_table_free(rt->globals);
        rt->globals = NULL;
    }

    os_mem_free(rt);
}

/* ------------------------------------------------------------------ */
/* Chargement du bytecode                                             */
/* ------------------------------------------------------------------ */

int gfa_runtime_load(gfa_runtime *rt, gfa_bytecode *bc)
{
    if (rt == NULL || bc == NULL) return -1;

    if (rt->program != NULL) {
        gfa_bytecode_free(rt->program);
    }

    rt->program = bc;
    rt->ip      = 0;
    rt->sp      = 0;
    rt->call_depth = 0;
    rt->data_ptr   = 0;
    if (bc->data_values) {
        bc->data_ptr = 0;
    }
    rt->running    = 0;
    rt->stopped    = 0;
    rt->error_code = 0;

    return 0;
}

/* ------------------------------------------------------------------ */
/* Execution principale                                               */
/* ------------------------------------------------------------------ */

int gfa_runtime_execute(gfa_runtime *rt)
{
    int result = 0;

    if (rt == NULL || rt->program == NULL) return -1;

    rt->running = 1;
    rt->stopped = 0;
    rt->ip      = 0;
    rt->sp      = 0;

    while (rt->running && rt->ip < rt->program->length) {
        /* Verifier les evenements entre chaque instruction */
        if (!rt->trace_on) {
            gfa_events_poll();
        }

        result = execute_instruction(rt);
        if (result < 0) {
            /* Erreur fatale ou fin de programme */
            break;
        }
        if (rt->stopped) {
            /* STOP rencontre */
            result = 0;
            break;
        }
    }

    rt->running = 0;
    return result;
}

int gfa_runtime_step(gfa_runtime *rt)
{
    if (rt == NULL || rt->program == NULL) return -1;
    if (rt->ip >= rt->program->length) return -1;

    return execute_instruction(rt);
}

void gfa_runtime_stop(gfa_runtime *rt)
{
    if (rt != NULL) {
        rt->stopped = 1;
        rt->running = 0;
    }
}

void gfa_runtime_continue(gfa_runtime *rt)
{
    if (rt != NULL && rt->stopped) {
        rt->stopped = 0;
        rt->running = 1;
        /* Reprendre apres le STOP */
        if (rt->ip < rt->program->length) {
            gfa_runtime_execute(rt);
        }
    }
}

int gfa_runtime_get_error(gfa_runtime *rt)
{
    if (rt == NULL) return -1;
    return rt->error_code;
}

/* ------------------------------------------------------------------ */
/* Execution d'une instruction                                        */
/* ------------------------------------------------------------------ */

static int execute_instruction(gfa_runtime *rt)
{
    gfa_instruction *inst;
    gfa_opcode op;
    os_int32 operand;
    int result;
    gfa_value *v1, *v2;
    gfa_variable *var;
    const char *str;
    double fval;
    os_int32 lval;
    int bval;

    if (rt->ip >= rt->program->length) {
        rt->running = 0;
        return 0;
    }

    inst    = &rt->program->code[rt->ip];
    op      = inst->opcode;
    operand = inst->operand.int_val;
    result  = 0;

    switch (op) {

        /* ---------------------------------------------------------- */
        /* Empilement / Depilement                                    */
        /* ---------------------------------------------------------- */

        case OP_NOP:
            break;

        case OP_PUSH_CONST:
            gfa_value_push_float(rt, inst->operand.float_val);
            break;

        case OP_PUSH_VAR:
            var = (gfa_variable *)inst->operand.ptr_val;
            if (var == NULL) {
                runtime_error(rt, 42, "Variable not found");
                return -1;
            }
            switch (var->type) {
                case GFA_VAR_BOOL:
                    gfa_value_push_bool(rt, (var->value.bool_val != 0) ? -1 : 0);
                    break;
                case GFA_VAR_BYTE:
                    gfa_value_push_long(rt, (os_int32)var->value.byte_val);
                    break;
                case GFA_VAR_WORD:
                    gfa_value_push_long(rt, (os_int32)var->value.word_val);
                    break;
                case GFA_VAR_LONG:
                    gfa_value_push_long(rt, var->value.long_val);
                    break;
                case GFA_VAR_FLOAT:
                    gfa_value_push_float(rt, var->value.float_val);
                    break;
                case GFA_VAR_STRING:
                    if (var->value.str.data != NULL) {
                        gfa_value_push_string(rt,
                            gfa_str_new(var->value.str.data), 1);
                    } else {
                        gfa_value_push_string(rt, gfa_str_new(""), 1);
                    }
                    break;
                default:
                    gfa_value_push_long(rt, 0);
                    break;
            }
            break;

        case OP_PUSH_STRING:
            str = NULL;
            if (operand >= 0 && operand < rt->program->str_count) {
                str = rt->program->strings[operand];
            }
            gfa_value_push_string(rt, gfa_str_new(str != NULL ? str : ""), 1);
            break;

        case OP_POP:
            gfa_value_discard(rt, 1);
            break;

        case OP_POP_STORE:
            var = (gfa_variable *)inst->operand.ptr_val;
            if (var != NULL && rt->sp > 0) {
                v1 = gfa_value_peek(rt, 0);
                if (v1 != NULL) {
                    switch (var->type) {
                        case GFA_VAR_BOOL:
                            var->value.bool_val = (os_byte)(gfa_value_to_bool(v1) ? 255 : 0);
                            break;
                        case GFA_VAR_BYTE:
                            var->value.byte_val = (os_byte)(gfa_value_to_long(v1) & 0xFF);
                            break;
                        case GFA_VAR_WORD:
                            var->value.word_val = (os_int16)(gfa_value_to_long(v1) & 0xFFFF);
                            break;
                        case GFA_VAR_LONG:
                            var->value.long_val = gfa_value_to_long(v1);
                            break;
                        case GFA_VAR_FLOAT:
                            var->value.float_val = gfa_value_to_float(v1);
                            break;
                        case GFA_VAR_STRING:
                            gfa_var_set_from_string(var,
                                (v1->type == GFA_VAL_STRING) ? v1->data.s : "");
                            break;
                        default:
                            break;
                    }
                }
                gfa_value_discard(rt, 1);
            }
            break;

        case OP_ARRAY_LOAD:
            /* Stack: [indices...] ; pop indices, read array element, push value */
            var = (gfa_variable *)inst->operand.ptr_val;
            if (var != NULL && var->type == GFA_VAR_ARRAY && var->value.arr.data) {
                int ndim = var->value.arr.num_dims;
                if (ndim >= 1 && rt->sp >= ndim) {
                    int indices[7];
                    int di;
                    double *base;
                    long flat_index = 0;
                    int stride = 1;
                    for (di = ndim - 1; di >= 0; di--) {
                        gfa_value *idx = gfa_value_pop(rt);
                        if (idx) {
                            indices[di] = (int)gfa_value_to_long(idx);
                            os_mem_free(idx);
                        }
                        flat_index += indices[di] * stride;
                        stride *= var->value.arr.dim_sizes[di];
                    }
                    base = (double *)var->value.arr.data;
                    gfa_value_push_float(rt, base[flat_index]);
                }
            }
            break;

        case OP_ARRAY_STORE:
            /* Stack: [indices...] [value] ; pop value, indices, store */
            var = (gfa_variable *)inst->operand.ptr_val;
            if (var != NULL && var->type == GFA_VAR_ARRAY && var->value.arr.data) {
                int ndim = var->value.arr.num_dims;
                if (ndim >= 1 && rt->sp >= ndim + 1) {
                    int indices[7];
                    int di;
                    double *base;
                    long flat_index = 0;
                    int stride = 1;
                    gfa_value *val = gfa_value_pop(rt);
                    for (di = ndim - 1; di >= 0; di--) {
                        gfa_value *idx = gfa_value_pop(rt);
                        if (idx) {
                            indices[di] = (int)gfa_value_to_long(idx);
                            os_mem_free(idx);
                        }
                        flat_index += indices[di] * stride;
                        stride *= var->value.arr.dim_sizes[di];
                    }
                    base = (double *)var->value.arr.data;
                    if (val) {
                        base[flat_index] = gfa_value_to_float(val);
                        os_mem_free(val);
                    }
                }
            }
            break;

        case OP_DUP:
            if (rt->sp > 0 && rt->sp < GFA_VALUE_STACK_SIZE) {
                rt->value_stack[rt->sp] = rt->value_stack[rt->sp - 1];
                /* Si c'est une chaine, dupliquer la chaine */
                if (rt->value_stack[rt->sp].type == GFA_VAL_STRING &&
                    rt->value_stack[rt->sp - 1].data.s != NULL) {
                    rt->value_stack[rt->sp].data.s =
                        gfa_str_new(rt->value_stack[rt->sp - 1].data.s);
                    rt->value_stack[rt->sp].owns_string = 1;
                }
                rt->sp++;
            }
            break;

        case OP_SWAP:
            if (rt->sp >= 2) {
                gfa_value tmp;
                tmp = rt->value_stack[rt->sp - 1];
                rt->value_stack[rt->sp - 1] = rt->value_stack[rt->sp - 2];
                rt->value_stack[rt->sp - 2] = tmp;
            }
            break;

        /* ---------------------------------------------------------- */
        /* Arithmetique                                               */
        /* ---------------------------------------------------------- */

        case OP_ADD:
            if (rt->sp >= 2) {
                v2 = gfa_value_pop(rt);
                v1 = gfa_value_peek(rt, 0);
                if (v1 != NULL && v2 != NULL) {
                    if (v1->type == GFA_VAL_STRING || v2->type == GFA_VAL_STRING) {
                        /* Concatenation de chaines */
                        char *s1, *s2, *result_str;
                        s1 = (v1->type == GFA_VAL_STRING) ? v1->data.s : gfa_str_float(gfa_value_to_float(v1));
                        s2 = (v2->type == GFA_VAL_STRING) ? v2->data.s : gfa_str_float(gfa_value_to_float(v2));
                        result_str = gfa_str_concat(s1, s2);
                        if (v1->type != GFA_VAL_STRING) os_mem_free(s1);
                        if (v2->type != GFA_VAL_STRING) os_mem_free(s2);
                        gfa_value_discard(rt, 1);
                        gfa_value_push_string(rt, result_str, 1);
                    } else {
                        fval = gfa_value_to_float(v1) + gfa_value_to_float(v2);
                        gfa_value_discard(rt, 1);
                        gfa_value_push_float(rt, fval);
                    }
                }
                if (v2 != NULL && v2->owns_string && v2->data.s != NULL) {
                    os_mem_free(v2->data.s);
                }
                os_mem_free(v2);
            }
            break;

        case OP_SUB:
            if (rt->sp >= 2) {
                v2 = gfa_value_pop(rt); v1 = gfa_value_pop(rt);
                if (v1 && v2) {
                    gfa_value_push_float(rt, gfa_value_to_float(v1) - gfa_value_to_float(v2));
                }
                gfa_value_discard(rt, 0); /* deja depile */
                os_mem_free(v1); os_mem_free(v2);
            }
            break;

        case OP_MUL:
            if (rt->sp >= 2) {
                v2 = gfa_value_pop(rt); v1 = gfa_value_pop(rt);
                if (v1 && v2) {
                    gfa_value_push_float(rt, gfa_value_to_float(v1) * gfa_value_to_float(v2));
                }
                os_mem_free(v1); os_mem_free(v2);
            }
            break;

        case OP_DIV:
            if (rt->sp >= 2) {
                v2 = gfa_value_pop(rt); v1 = gfa_value_pop(rt);
                if (v1 && v2) {
                    double denom = gfa_value_to_float(v2);
                    if (fabs(denom) < 1.0e-15) {
                        runtime_error(rt, 0, "Division by zero");
                        os_mem_free(v1); os_mem_free(v2);
                        return -1;
                    }
                    gfa_value_push_float(rt, gfa_value_to_float(v1) / denom);
                }
                os_mem_free(v1); os_mem_free(v2);
            }
            break;

        case OP_MOD:
            if (rt->sp >= 2) {
                v2 = gfa_value_pop(rt); v1 = gfa_value_pop(rt);
                if (v1 && v2) {
                    lval = gfa_value_to_long(v2);
                    if (lval == 0) {
                        runtime_error(rt, 0, "Division by zero");
                        os_mem_free(v1); os_mem_free(v2);
                        return -1;
                    }
                    gfa_value_push_long(rt, gfa_value_to_long(v1) % lval);
                }
                os_mem_free(v1); os_mem_free(v2);
            }
            break;

        case OP_INT_DIV:
            if (rt->sp >= 2) {
                v2 = gfa_value_pop(rt); v1 = gfa_value_pop(rt);
                if (v1 && v2) {
                    lval = gfa_value_to_long(v2);
                    if (lval == 0) {
                        runtime_error(rt, 0, "Division by zero");
                        os_mem_free(v1); os_mem_free(v2);
                        return -1;
                    }
                    gfa_value_push_long(rt, gfa_value_to_long(v1) / lval);
                }
                os_mem_free(v1); os_mem_free(v2);
            }
            break;

        case OP_POW:
            if (rt->sp >= 2) {
                v2 = gfa_value_pop(rt); v1 = gfa_value_pop(rt);
                if (v1 && v2) {
                    gfa_value_push_float(rt, pow(gfa_value_to_float(v1),
                                                  gfa_value_to_float(v2)));
                }
                os_mem_free(v1); os_mem_free(v2);
            }
            break;

        case OP_NEG:
            if (rt->sp >= 1) {
                v1 = gfa_value_peek(rt, 0);
                if (v1 != NULL) {
                    fval = -gfa_value_to_float(v1);
                    gfa_value_discard(rt, 1);
                    gfa_value_push_float(rt, fval);
                }
            }
            break;

        /* ---------------------------------------------------------- */
        /* Comparaisons                                               */
        /* ---------------------------------------------------------- */

        case OP_EQ:
            if (rt->sp >= 2) {
                v2 = gfa_value_pop(rt); v1 = gfa_value_pop(rt);
                if (v1 && v2) {
                    if (v1->type == GFA_VAL_STRING && v2->type == GFA_VAL_STRING) {
                        bval = (strcmp(v1->data.s ? v1->data.s : "",
                                       v2->data.s ? v2->data.s : "") == 0);
                    } else {
                        bval = (gfa_value_to_float(v1) == gfa_value_to_float(v2));
                    }
                    gfa_value_push_bool(rt, bval ? -1 : 0);
                }
                os_mem_free(v1); os_mem_free(v2);
            }
            break;

        case OP_NE:
            if (rt->sp >= 2) {
                v2 = gfa_value_pop(rt); v1 = gfa_value_pop(rt);
                if (v1 && v2) {
                    if (v1->type == GFA_VAL_STRING && v2->type == GFA_VAL_STRING) {
                        bval = (strcmp(v1->data.s ? v1->data.s : "",
                                       v2->data.s ? v2->data.s : "") != 0);
                    } else {
                        bval = (gfa_value_to_float(v1) != gfa_value_to_float(v2));
                    }
                    gfa_value_push_bool(rt, bval ? -1 : 0);
                }
                os_mem_free(v1); os_mem_free(v2);
            }
            break;

        case OP_LT:
            if (rt->sp >= 2) {
                v2 = gfa_value_pop(rt); v1 = gfa_value_pop(rt);
                if (v1 && v2) {
                    bval = (gfa_value_to_float(v1) < gfa_value_to_float(v2));
                    gfa_value_push_bool(rt, bval ? -1 : 0);
                }
                os_mem_free(v1); os_mem_free(v2);
            }
            break;

        case OP_LE:
            if (rt->sp >= 2) {
                v2 = gfa_value_pop(rt); v1 = gfa_value_pop(rt);
                if (v1 && v2) {
                    bval = (gfa_value_to_float(v1) <= gfa_value_to_float(v2));
                    gfa_value_push_bool(rt, bval ? -1 : 0);
                }
                os_mem_free(v1); os_mem_free(v2);
            }
            break;

        case OP_GT:
            if (rt->sp >= 2) {
                v2 = gfa_value_pop(rt); v1 = gfa_value_pop(rt);
                if (v1 && v2) {
                    bval = (gfa_value_to_float(v1) > gfa_value_to_float(v2));
                    gfa_value_push_bool(rt, bval ? -1 : 0);
                }
                os_mem_free(v1); os_mem_free(v2);
            }
            break;

        case OP_GE:
            if (rt->sp >= 2) {
                v2 = gfa_value_pop(rt); v1 = gfa_value_pop(rt);
                if (v1 && v2) {
                    bval = (gfa_value_to_float(v1) >= gfa_value_to_float(v2));
                    gfa_value_push_bool(rt, bval ? -1 : 0);
                }
                os_mem_free(v1); os_mem_free(v2);
            }
            break;

        /* ---------------------------------------------------------- */
        /* Logique bit a bit                                          */
        /* ---------------------------------------------------------- */

        case OP_AND:
            if (rt->sp >= 2) {
                v2 = gfa_value_pop(rt); v1 = gfa_value_pop(rt);
                if (v1 && v2) {
                    gfa_value_push_long(rt, gfa_value_to_long(v1) & gfa_value_to_long(v2));
                }
                os_mem_free(v1); os_mem_free(v2);
            }
            break;

        case OP_OR:
            if (rt->sp >= 2) {
                v2 = gfa_value_pop(rt); v1 = gfa_value_pop(rt);
                if (v1 && v2) {
                    gfa_value_push_long(rt, gfa_value_to_long(v1) | gfa_value_to_long(v2));
                }
                os_mem_free(v1); os_mem_free(v2);
            }
            break;

        case OP_XOR:
            if (rt->sp >= 2) {
                v2 = gfa_value_pop(rt); v1 = gfa_value_pop(rt);
                if (v1 && v2) {
                    gfa_value_push_long(rt, gfa_value_to_long(v1) ^ gfa_value_to_long(v2));
                }
                os_mem_free(v1); os_mem_free(v2);
            }
            break;

        case OP_NOT:
            if (rt->sp >= 1) {
                v1 = gfa_value_pop(rt);
                if (v1 != NULL) {
                    gfa_value_push_long(rt, ~gfa_value_to_long(v1));
                }
                os_mem_free(v1);
            }
            break;

        case OP_EQV:
            if (rt->sp >= 2) {
                v2 = gfa_value_pop(rt); v1 = gfa_value_pop(rt);
                if (v1 && v2) {
                    gfa_value_push_long(rt, ~(gfa_value_to_long(v1) ^ gfa_value_to_long(v2)));
                }
                os_mem_free(v1); os_mem_free(v2);
            }
            break;

        case OP_IMP:
            if (rt->sp >= 2) {
                v2 = gfa_value_pop(rt); v1 = gfa_value_pop(rt);
                if (v1 && v2) {
                    gfa_value_push_long(rt, (~gfa_value_to_long(v1)) | gfa_value_to_long(v2));
                }
                os_mem_free(v1); os_mem_free(v2);
            }
            break;

        /* ---------------------------------------------------------- */
        /* Controle de flux                                           */
        /* ---------------------------------------------------------- */

        case OP_JMP:
            rt->ip = (int)operand;
            return 0;  /* Ne pas incrementer ip */

        case OP_JMP_IF_FALSE:
            if (rt->sp > 0) {
                v1 = gfa_value_pop(rt);
                if (v1 != NULL && !gfa_value_to_bool(v1)) {
                    rt->ip = (int)operand;
                    os_mem_free(v1);
                    return 0;
                }
                os_mem_free(v1);
            }
            break;

        case OP_JMP_IF_TRUE:
            if (rt->sp > 0) {
                v1 = gfa_value_pop(rt);
                if (v1 != NULL && gfa_value_to_bool(v1)) {
                    rt->ip = (int)operand;
                    os_mem_free(v1);
                    return 0;
                }
                os_mem_free(v1);
            }
            break;

        case OP_CALL:
            if (rt->call_depth < GFA_MAX_CALL_DEPTH) {
                gfa_call_frame *frame;
                frame = &rt->call_stack[rt->call_depth++];
                frame->return_ip = rt->ip + 1;
                frame->return_sp = rt->sp;
                frame->is_gosub  = 1;
                frame->proc_index = 0;
                frame->saved_count = 0;
                rt->ip = (int)operand;
                return 0;
            }
            runtime_error(rt, 93, "Stack overflow");
            return -1;

        case OP_RET:
            if (rt->call_depth > 0) {
                gfa_call_frame *frame;
                int i;
                frame = &rt->call_stack[--rt->call_depth];
                rt->ip = frame->return_ip;
                while (rt->sp > frame->return_sp && rt->sp > 0) {
                    gfa_value_discard(rt, 1);
                }
                /* Restore saved local variables (reverse order) */
                for (i = frame->saved_count - 1; i >= 0; i--) {
                    gfa_variable *var = frame->saved_vars[i];
                    if (var && frame->saved_vals[i]) {
                        gfa_value *saved = frame->saved_vals[i];
                        switch (var->type) {
                            case GFA_VAR_BOOL:
                                var->value.bool_val = (os_byte)(gfa_value_to_bool(saved) ? 255 : 0);
                                break;
                            case GFA_VAR_BYTE:
                                var->value.byte_val = (os_byte)(gfa_value_to_long(saved) & 0xFF);
                                break;
                            case GFA_VAR_WORD:
                                var->value.word_val = (os_int16)(gfa_value_to_long(saved) & 0xFFFF);
                                break;
                            case GFA_VAR_LONG:
                                var->value.long_val = gfa_value_to_long(saved);
                                break;
                            case GFA_VAR_FLOAT:
                                var->value.float_val = gfa_value_to_float(saved);
                                break;
                            case GFA_VAR_STRING:
                                if (saved->type == GFA_VAL_STRING && saved->data.s)
                                    gfa_var_set_from_string(var, saved->data.s);
                                break;
                            default: break;
                        }
                        os_mem_free(saved);
                        frame->saved_vals[i] = NULL;
                    }
                }
                frame->saved_count = 0;
                return 0;
            }
            /* Pas de frame : fin du programme */
            rt->running = 0;
            return 0;

        case OP_SAVE_LOCAL:
            /* Pop top of value stack as new value for var,
               save old value of var in call frame */
            {
                gfa_variable *var = (gfa_variable *)inst->operand.ptr_val;
                gfa_call_frame *frame;
                if (rt->call_depth > 0 && var != NULL) {
                    frame = &rt->call_stack[rt->call_depth - 1];
                    if (frame->saved_count < 16) {
                        int idx = frame->saved_count++;
                        frame->saved_vars[idx] = var;
                        /* Save current value */
                        switch (var->type) {
                            case GFA_VAR_BOOL:
                                gfa_value_push_bool(rt, (var->value.bool_val != 0) ? -1 : 0);
                                break;
                            case GFA_VAR_BYTE:
                                gfa_value_push_long(rt, (os_int32)var->value.byte_val);
                                break;
                            case GFA_VAR_WORD:
                                gfa_value_push_long(rt, (os_int32)var->value.word_val);
                                break;
                            case GFA_VAR_LONG:
                                gfa_value_push_long(rt, var->value.long_val);
                                break;
                            case GFA_VAR_FLOAT:
                                gfa_value_push_float(rt, var->value.float_val);
                                break;
                            case GFA_VAR_STRING:
                                if (var->value.str.data)
                                    gfa_value_push_string(rt,
                                        gfa_str_new(var->value.str.data), 1);
                                else
                                    gfa_value_push_string(rt, gfa_str_new(""), 1);
                                break;
                            default:
                                gfa_value_push_long(rt, 0);
                                break;
                        }
                        /* Save current value into frame (copy, not pointer!) */
                        {
                            if (rt->sp > 0) {
                                gfa_value *v = gfa_value_peek(rt, 0);
                                if (v) {
                                    gfa_value *copy;
                                    copy = (gfa_value *)os_mem_alloc(sizeof(gfa_value));
                                    if (copy) {
                                        os_mem_copy(copy, v, sizeof(gfa_value));
                                        if (v->type == GFA_VAL_STRING && v->data.s) {
                                            copy->data.s = gfa_str_new(v->data.s);
                                            copy->owns_string = 1;
                                        }
                                        frame->saved_vals[idx] = copy;
                                    }
                                }
                                gfa_value_discard(rt, 1);
                            }
                        }
                    }
                }
                /* Now pop the arg from caller and assign to var */
                if (rt->sp > 0) {
                    gfa_value *arg = gfa_value_peek(rt, 0);
                    if (arg && var) {
                        switch (var->type) {
                            case GFA_VAR_BOOL:
                                var->value.bool_val = (os_byte)(gfa_value_to_bool(arg) ? 255 : 0);
                                break;
                            case GFA_VAR_BYTE:
                                var->value.byte_val = (os_byte)(gfa_value_to_long(arg) & 0xFF);
                                break;
                            case GFA_VAR_WORD:
                                var->value.word_val = (os_int16)(gfa_value_to_long(arg) & 0xFFFF);
                                break;
                            case GFA_VAR_LONG:
                                var->value.long_val = gfa_value_to_long(arg);
                                break;
                            case GFA_VAR_FLOAT:
                                var->value.float_val = gfa_value_to_float(arg);
                                break;
                            case GFA_VAR_STRING:
                                if (arg->type == GFA_VAL_STRING && arg->data.s)
                                    gfa_var_set_from_string(var, arg->data.s);
                                break;
                            default: break;
                        }
                    }
                    gfa_value_discard(rt, 1);
                }
            }
            break;

        case OP_BIND_REF:
            /* Pop top of value stack and assign to var (no save, VAR param).
               Modification persists after function returns. */
            {
                gfa_variable *var = (gfa_variable *)inst->operand.ptr_val;
                if (rt->sp > 0 && var != NULL) {
                    gfa_value *arg = gfa_value_peek(rt, 0);
                    if (arg) {
                        switch (var->type) {
                            case GFA_VAR_BOOL:
                                var->value.bool_val = (os_byte)(gfa_value_to_bool(arg) ? 255 : 0);
                                break;
                            case GFA_VAR_BYTE:
                                var->value.byte_val = (os_byte)(gfa_value_to_long(arg) & 0xFF);
                                break;
                            case GFA_VAR_WORD:
                                var->value.word_val = (os_int16)(gfa_value_to_long(arg) & 0xFFFF);
                                break;
                            case GFA_VAR_LONG:
                                var->value.long_val = gfa_value_to_long(arg);
                                break;
                            case GFA_VAR_FLOAT:
                                var->value.float_val = gfa_value_to_float(arg);
                                break;
                            case GFA_VAR_STRING:
                                if (arg->type == GFA_VAL_STRING && arg->data.s)
                                    gfa_var_set_from_string(var, arg->data.s);
                                break;
                            default: break;
                        }
                    }
                    gfa_value_discard(rt, 1);
                }
            }
            break;

        case OP_CALL_BUILTIN:
            /* Fonction integree : operand.int_val = type de token de la fonction.
               Les parametres sont sur la pile (le dernier push = dernier arg). */
            {
                gfa_token_type func_tok;
                gfa_value *arg1, *arg2, *arg3;
                double result_f;
                os_int32 result_l;
                const char *result_s;

                func_tok = (gfa_token_type)operand;
                arg1 = arg2 = arg3 = NULL;
                result_f = 0.0;
                result_l = 0;
                result_s = NULL;

                switch (func_tok) {
                    /* Maths - 1 argument */
                    case TOK_ABS: case TOK_SGN: case TOK_INT: case TOK_FRAC:
                    case TOK_FIX: case TOK_ROUND: case TOK_CEIL_TOK: case TOK_TRUNC_TOK:
                    case TOK_SQR: case TOK_EXP: case TOK_LOG: case TOK_LOG10:
                    case TOK_SIN: case TOK_COS: case TOK_TAN: case TOK_ATN:
                    case TOK_ASIN: case TOK_ACOS: case TOK_SINH: case TOK_COSH: case TOK_TANH:
                    case TOK_SINQ: case TOK_COSQ:
                    case TOK_DEG: case TOK_RAD: case TOK_CFLOAT:
                    case TOK_RND: case TOK_FACT: case TOK_PRED: case TOK_SUCC:
                        arg1 = gfa_value_pop(rt);
                        if (arg1) {
                            double x = gfa_value_to_float(arg1);
                            switch (func_tok) {
                                case TOK_ABS:   result_f = gfa_abs(x); break;
                                case TOK_SGN:   result_f = gfa_sgn(x); break;
                                case TOK_INT:   result_f = gfa_int(x); break;
                                case TOK_FRAC:  result_f = gfa_frac(x); break;
                                case TOK_FIX:   result_f = gfa_fix(x); break;
                                case TOK_ROUND: result_f = gfa_round(x); break;
                                case TOK_CEIL_TOK: result_f = gfa_ceil(x); break;
                                case TOK_TRUNC_TOK: result_f = gfa_trunc(x); break;
                                case TOK_SQR:   result_f = gfa_sqr(x); break;
                                case TOK_EXP:   result_f = gfa_exp(x); break;
                                case TOK_LOG:   result_f = gfa_log(x); break;
                                case TOK_LOG10: result_f = gfa_log10(x); break;
                                case TOK_SIN:   result_f = gfa_sin(x); break;
                                case TOK_COS:   result_f = gfa_cos(x); break;
                                case TOK_TAN:   result_f = gfa_tan(x); break;
                                case TOK_ATN:   result_f = gfa_atn(x); break;
                                case TOK_ASIN:  result_f = gfa_asin(x); break;
                                case TOK_ACOS:  result_f = gfa_acos(x); break;
                                case TOK_SINH:  result_f = gfa_sinh(x); break;
                                case TOK_COSH:  result_f = gfa_cosh(x); break;
                                case TOK_TANH:  result_f = gfa_tanh(x); break;
                                case TOK_SINQ:  result_f = gfa_sinq(x); break;
                                case TOK_COSQ:  result_f = gfa_cosq(x); break;
                                case TOK_DEG:   result_f = gfa_deg(x); break;
                                case TOK_RAD:   result_f = gfa_rad(x); break;
                                case TOK_RND:   result_f = gfa_rnd(x); break;
                                case TOK_FACT:  result_f = gfa_fact((int)x); break;
                                case TOK_CFLOAT: result_f = x; break;
                                case TOK_PRED:  result_f = (double)gfa_pred((os_int32)x); break;
                                case TOK_SUCC:  result_f = (double)gfa_succ((os_int32)x); break;
                                default: break;
                            }
                            gfa_value_push_float(rt, result_f);
                            os_mem_free(arg1);
                        }
                        break;

                    /* Maths - 2 arguments */
                    case TOK_MIN: case TOK_MAX:
                    case TOK_COMBIN: case TOK_VARIAT:
                        arg2 = gfa_value_pop(rt);
                        arg1 = gfa_value_pop(rt);
                        if (arg1 && arg2) {
                            double a = gfa_value_to_float(arg1);
                            double b = gfa_value_to_float(arg2);
                            switch (func_tok) {
                                case TOK_MIN: result_f = gfa_min(a, b); break;
                                case TOK_MAX: result_f = gfa_max(a, b); break;
                                case TOK_COMBIN: result_f = gfa_combin((int)a, (int)b); break;
                                case TOK_VARIAT: result_f = gfa_variat((int)a, (int)b); break;
                                default: break;
                            }
                            gfa_value_push_float(rt, result_f);
                        }
                        os_mem_free(arg1); os_mem_free(arg2);
                        break;

                    /* Maths - tests */
                    case TOK_EVEN: case TOK_ODD:
                        arg1 = gfa_value_pop(rt);
                        if (arg1) {
                            os_int32 x = gfa_value_to_long(arg1);
                            result_l = (func_tok == TOK_EVEN) ? gfa_even(x) : gfa_odd(x);
                            gfa_value_push_long(rt, result_l ? -1 : 0);
                            os_mem_free(arg1);
                        }
                        break;

                    /* Chaines */
                    case TOK_LEN: case TOK_ASC: case TOK_VAL:
                    case TOK_UPPER_TOK: case TOK_LCASE_TOK: case TOK_LOWER_TOK:
                    case TOK_TRIM_TOK:
                        arg1 = gfa_value_pop(rt);
                        if (arg1) {
                            const char *s;
                            if (arg1->type == GFA_VAL_STRING && arg1->data.s)
                                s = arg1->data.s;
                            else if (arg1->owns_string && arg1->data.s)
                                s = arg1->data.s;
                            else
                                s = "";
                            switch (func_tok) {
                                case TOK_LEN: result_f = (double)gfa_len(s); gfa_value_push_float(rt, result_f); break;
                                case TOK_ASC: result_f = (double)gfa_asc(s); gfa_value_push_float(rt, result_f); break;
                                case TOK_VAL: result_f = gfa_val(s); gfa_value_push_float(rt, result_f); break;
                                case TOK_UPPER_TOK: result_s = gfa_upper(s); gfa_value_push_string(rt, result_s, 1); break;
                                case TOK_LCASE_TOK: case TOK_LOWER_TOK: result_s = gfa_lower(s); gfa_value_push_string(rt, result_s, 1); break;
                                case TOK_TRIM_TOK: result_s = gfa_trim(s); gfa_value_push_string(rt, result_s, 1); break;
                                default: break;
                            }
                            os_mem_free(arg1);
                        }
                        break;

                    /* Chaines - 2 ou 3 args */
                    case TOK_LEFT_TOK: case TOK_RIGHT_TOK:
                    case TOK_INSTR: case TOK_RINSTR:
                        arg2 = gfa_value_pop(rt);
                        arg1 = gfa_value_pop(rt);
                        if (arg1 && arg2) {
                            const char *s;
                            if (arg1->type == GFA_VAL_STRING && arg1->data.s)
                                s = arg1->data.s;
                            else if (arg1->owns_string && arg1->data.s)
                                s = arg1->data.s;
                            else
                                s = "";
                            if (func_tok == TOK_INSTR) {
                                result_f = (double)gfa_instr(1, s, (arg2->type == GFA_VAL_STRING && arg2->data.s) ? arg2->data.s : "");
                                gfa_value_push_float(rt, result_f);
                            } else if (func_tok == TOK_RINSTR) {
                                result_f = (double)gfa_rinstr(-1, s, (arg2->type == GFA_VAL_STRING && arg2->data.s) ? arg2->data.s : "");
                                gfa_value_push_float(rt, result_f);
                            } else {
                                int n = (int)gfa_value_to_long(arg2);
                                if (func_tok == TOK_LEFT_TOK) result_s = gfa_left(s, n);
                                else if (func_tok == TOK_RIGHT_TOK) result_s = gfa_right(s, n);
                                else if (func_tok == TOK_MID_TOK) {
                                    /* MID needs a 3rd arg (length). Pop it from stack. */
                                    gfa_value *arg3 = gfa_value_pop(rt);
                                    int len = (arg3) ? (int)gfa_value_to_long(arg3) : 1;
                                    if (arg3) os_mem_free(arg3);
                                    result_s = gfa_mid(s, n, len);
                                }
                                gfa_value_push_string(rt, result_s, 1);
                            }
                        }
                        os_mem_free(arg1); os_mem_free(arg2);
                        break;

                    case TOK_STRING_TOK:
                        arg2 = gfa_value_pop(rt);
                        arg1 = gfa_value_pop(rt);
                        if (arg1 && arg2) {
                            int n = (int)gfa_value_to_long(arg1);
                            if (arg2->type == GFA_VAL_STRING)
                                result_s = gfa_string(n, arg2->data.s);
                            else
                                result_s = gfa_string_char(n, (int)gfa_value_to_long(arg2));
                            gfa_value_push_string(rt, result_s, 1);
                        }
                        os_mem_free(arg1); os_mem_free(arg2);
                        break;

                    case TOK_SPACE_TOK:
                        arg1 = gfa_value_pop(rt);
                        if (arg1) {
                            result_s = gfa_space((int)gfa_value_to_long(arg1));
                            gfa_value_push_string(rt, result_s, 1);
                            os_mem_free(arg1);
                        }
                        break;

                    case TOK_STR_TOK:
                        arg1 = gfa_value_pop(rt);
                        if (arg1) {
                            result_s = gfa_str_float(gfa_value_to_float(arg1));
                            gfa_value_push_string(rt, result_s, 1);
                            os_mem_free(arg1);
                        }
                        break;

                    case TOK_BIN_TOK:
                        arg1 = gfa_value_pop(rt);
                        if (arg1) {
                            result_s = gfa_bin(gfa_value_to_long(arg1), 8);
                            gfa_value_push_string(rt, result_s, 1);
                            os_mem_free(arg1);
                        }
                        break;

                    case TOK_HEX_TOK:
                        arg1 = gfa_value_pop(rt);
                        if (arg1) {
                            result_s = gfa_hex(gfa_value_to_long(arg1), 8);
                            gfa_value_push_string(rt, result_s, 1);
                            os_mem_free(arg1);
                        }
                        break;

                    case TOK_OCT_TOK:
                        arg1 = gfa_value_pop(rt);
                        if (arg1) {
                            result_s = gfa_oct(gfa_value_to_long(arg1), 11);
                            gfa_value_push_string(rt, result_s, 1);
                            os_mem_free(arg1);
                        }
                        break;

                    /* Chaines - 3 args (MID) */
                    case TOK_MID_TOK:
                        /* Stack: [string, pos, len]. Pop in reverse. */
                        {
                            gfa_value *a3, *a2, *a1;
                            a3 = gfa_value_pop(rt); /* len */
                            a2 = gfa_value_pop(rt); /* pos */
                            a1 = gfa_value_pop(rt); /* string */
                            if (a1 && a2 && a3) {
                                const char *s = (a1->type == GFA_VAL_STRING && a1->data.s) ? a1->data.s : "";
                                int pos = (int)gfa_value_to_long(a2);
                                int len = (int)gfa_value_to_long(a3);
                                result_s = gfa_mid(s, pos, len);
                                gfa_value_push_string(rt, result_s, 1);
                            }
                            if (a1) os_mem_free(a1);
                            if (a2) os_mem_free(a2);
                            if (a3) os_mem_free(a3);
                        }
                        break;

                    case TOK_CHR_TOK:
                        arg1 = gfa_value_pop(rt);
                        if (arg1) {
                            result_s = gfa_str_new(gfa_chr((int)gfa_value_to_long(arg1)));
                            gfa_value_push_string(rt, result_s, 1);
                            os_mem_free(arg1);
                        }
                        break;

                    /* Constantes */
                    case TOK_INKEY:
                        {
                            int c = os_con_input_key();
                            if (c >= 0) {
                                char s[2]; s[0] = (char)c; s[1] = '\0';
                                gfa_value_push_string(rt, gfa_str_new(s), 1);
                            } else {
                                gfa_value_push_string(rt, gfa_str_new(""), 1);
                            }
                        }
                        break;

                    case TOK__DATA:
                        /* READ: push next DATA value */
                        if (rt->program && rt->program->data_ptr < rt->program->data_count) {
                            gfa_value_push_float(rt, rt->program->data_values[rt->program->data_ptr++]);
                        } else {
                            gfa_value_push_float(rt, 0.0); /* Out of data */
                        }
                        break;

                    case TOK_RESTORE:
                        /* RESTORE: reset DATA pointer */
                        if (rt->program) rt->program->data_ptr = 0;
                        break;

                    case TOK_TRUE:  gfa_value_push_long(rt, -1); break;
                    case TOK_FALSE: gfa_value_push_long(rt, 0); break;
                    case TOK_PI_TOK: gfa_value_push_float(rt, GFA_PI); break;

                    /* Non implemente */
                    default:
                        gfa_value_push_long(rt, 0);
                        break;
                }
            }
            break;

        /* ---------------------------------------------------------- */
        /* Instructions GFA specifiques                               */
        /* ---------------------------------------------------------- */

        case OP_PRINT:
            if (rt->sp > 0) {
                v1 = gfa_value_pop(rt);
                if (v1 != NULL) {
                    if (v1->type == GFA_VAL_STRING) {
                        os_con_output_string(v1->data.s ? v1->data.s : "");
                    } else {
                        char *s;
                        s = gfa_str_float(gfa_value_to_float(v1));
                        os_con_output_string(s);
                        os_mem_free(s);
                    }
                }
                os_mem_free(v1);
            }
            break;

        case OP_PRINT_NL:
            os_con_output_char('\n');
            break;

        case OP_BLOAD:
            /* Stack: [filename$] [addr] ; load file into buffer */
            if (rt->sp >= 2) {
                gfa_value *ad;
                gfa_value *fn;
                static char bload_buf[65536];
                os_file_handle fh;
                os_int32 size, nread;
                ad = gfa_value_pop(rt);
                fn = gfa_value_pop(rt);
                if (fn && fn->type == GFA_VAL_STRING && fn->data.s) {
                    fh = os_file_open(fn->data.s, 'I', 0);
                    if (fh != NULL) {
                        size = os_file_size(fh);
                        if (size > 65536) size = 65536;
                        if (size < 0) size = 0;
                        nread = os_file_read(fh, bload_buf, size);
                        os_file_close(fh);
                        gfa_value_push_long(rt, nread);
                    } else {
                        gfa_value_push_long(rt, -1);
                    }
                } else {
                    gfa_value_push_long(rt, -1);
                }
                if (fn) os_mem_free(fn);
                if (ad) os_mem_free(ad);
            }
            break;

        case OP_BSAVE:
            /* Stack: [filename$] [start] [end] ; save buffer to file */
            if (rt->sp >= 3) {
                gfa_value *en;
                gfa_value *st;
                gfa_value *fn;
                static char bsave_buf[65536];
                os_file_handle fh;
                os_int32 start, end, len, written;
                en = gfa_value_pop(rt);
                st = gfa_value_pop(rt);
                fn = gfa_value_pop(rt);
                if (fn && st && en && fn->type == GFA_VAL_STRING && fn->data.s) {
                    start = (os_int32)gfa_value_to_long(st);
                    end   = (os_int32)gfa_value_to_long(en);
                    if (end < start) { os_int32 tmp = start; start = end; end = tmp; }
                    len = end - start;
                    if (len > 65536) len = 65536;
                    fh = os_file_open(fn->data.s, 'O', 0);
                    if (fh != NULL && len > 0) {
                        written = os_file_write(fh, bsave_buf + start, len);
                        os_file_close(fh);
                        gfa_value_push_long(rt, (os_int32)written);
                    } else {
                        gfa_value_push_long(rt, -1);
                    }
                } else {
                    gfa_value_push_long(rt, -1);
                }
                if (fn) os_mem_free(fn);
                if (st) os_mem_free(st);
                if (en) os_mem_free(en);
            }
            break;

        case OP_BGET:
            /* Stack: [channel] [addr] [count] ; read file -> buffer */
            if (rt->sp >= 3) {
                gfa_value *co, *ad, *ch;
                int chan, count, n;
                static char bget_buf[4096];
                co = gfa_value_pop(rt);
                ad = gfa_value_pop(rt);
                ch = gfa_value_pop(rt);
                if (ch && ad && co) {
                    chan  = (int)gfa_value_to_long(ch);
                    count = (int)gfa_value_to_long(co);
                    if (count > 4096) count = 4096;
                    n = gfa_bget(chan, bget_buf, count);
                    gfa_value_push_long(rt, (os_int32)n);
                } else {
                    gfa_value_push_long(rt, (os_int32)-1);
                }
                if (ch) os_mem_free(ch);
                if (ad) os_mem_free(ad);
                if (co) os_mem_free(co);
            }
            break;

        case OP_BPUT:
            /* Stack: [channel] [addr] [count] ; write buffer -> file */
            if (rt->sp >= 3) {
                gfa_value *co, *ad, *ch;
                int chan, count, n;
                static char bput_buf[4096];
                co = gfa_value_pop(rt);
                ad = gfa_value_pop(rt);
                ch = gfa_value_pop(rt);
                if (ch && ad && co) {
                    chan  = (int)gfa_value_to_long(ch);
                    count = (int)gfa_value_to_long(co);
                    if (count > 4096) count = 4096;
                    n = gfa_bput(chan, bput_buf, count);
                    gfa_value_push_long(rt, (os_int32)n);
                } else {
                    gfa_value_push_long(rt, (os_int32)-1);
                }
                if (ch) os_mem_free(ch);
                if (ad) os_mem_free(ad);
                if (co) os_mem_free(co);
            }
            break;

        case OP_PRINT_CHAN:
            /* Stack: [channel] [value] ; write value to file channel */
            if (rt->sp >= 2) {
                gfa_value *chan_val;
                int channel;
                v1 = gfa_value_pop(rt);  /* value */
                chan_val = gfa_value_pop(rt);  /* channel */
                if (v1 != NULL && chan_val != NULL) {
                    channel = (int)gfa_value_to_long(chan_val);
                    if (v1->type == GFA_VAL_STRING) {
                        gfa_print_channel(channel,
                            v1->data.s ? v1->data.s : "");
                    } else {
                        char *s;
                        s = gfa_str_float(gfa_value_to_float(v1));
                        gfa_print_channel(channel, s);
                        os_mem_free(s);
                    }
                    gfa_print_channel(channel, "\n");
                }
                if (v1) os_mem_free(v1);
                if (chan_val) os_mem_free(chan_val);
            }
            break;

        case OP_INPUT_FILE:
            /* Stack: [channel] ; pop channel, read from file into var */
            if (rt->sp > 0) {
                gfa_value *chan_val;
                int channel;
                var = (gfa_variable *)inst->operand.ptr_val;
                chan_val = gfa_value_pop(rt);
                if (chan_val != NULL && var != NULL) {
                    char line[256];
                    channel = (int)gfa_value_to_long(chan_val);
                    if (gfa_input_channel(channel, line,
                            (int)sizeof(line)) >= 0) {
                        if (var->type == GFA_VAR_STRING) {
                            gfa_var_set_from_string(var, line);
                        } else {
                            gfa_var_set_from_float(var, gfa_val(line));
                        }
                    }
                }
                if (chan_val) os_mem_free(chan_val);
            }
            break;

        case OP_CLS:
            os_con_clear();
            rt->cursor_x = 1;
            rt->cursor_y = 1;
            break;

        case OP_COLOR:
            /* COLOR fg [, bg] : args on stack */
            if (rt->sp >= 1) {
                v1 = gfa_value_pop(rt);
                if (v1) { rt->current_color = (int)gfa_value_to_long(v1); os_mem_free(v1); }
                if (rt->sp > 0) { v2 = gfa_value_pop(rt); if (v2) os_mem_free(v2); }
            }
            break;

        case OP_LINE_GFX:
        case OP_BOX_GFX:
        case OP_PBOX_GFX:
        case OP_CIRCLE_GFX:
            /* Graphics: params x1,y1,x2,y2 or x,y,r on stack. Emit ANSI/placeholder */
            {
                int x1, y1, x2, y2;
                (void)x1; (void)y1; (void)x2; (void)y2;
                x1 = y1 = x2 = y2 = 0;
                if (rt->sp >= 2) {
                    v2 = gfa_value_pop(rt); v1 = gfa_value_pop(rt);
                    if (v1) { x1 = (int)gfa_value_to_long(v1); os_mem_free(v1); }
                    if (v2) { y1 = (int)gfa_value_to_long(v2); os_mem_free(v2); }
                }
                if (rt->sp >= 2) {
                    v2 = gfa_value_pop(rt); v1 = gfa_value_pop(rt);
                    if (v1) { x2 = (int)gfa_value_to_long(v1); os_mem_free(v1); }
                    if (v2) { y2 = (int)gfa_value_to_long(v2); os_mem_free(v2); }
                }
                /* Emit ANSI placeholder */
                if (inst->opcode == OP_LINE_GFX)
                    os_con_output_string("[LINE]");
                else if (inst->opcode == OP_CIRCLE_GFX)
                    os_con_output_string("[CIRCLE]");
                else
                    os_con_output_string("[RECT]");
            }
            break;

        case OP_SOUND:
            /* SOUND ch, freq, dur, vol, env */
            {
                int ch, freq, dur, vol, env;
                ch = freq = dur = vol = env = 0;
                if (rt->sp >= 5) {
                    gfa_value *vals[5]; int k;
                    for (k = 4; k >= 0; k--) vals[k] = gfa_value_pop(rt);
                    if (vals[0]) ch = (int)gfa_value_to_long(vals[0]);
                    if (vals[1]) freq = (int)gfa_value_to_long(vals[1]);
                    if (vals[2]) dur = (int)gfa_value_to_long(vals[2]);
                    if (vals[3]) vol = (int)gfa_value_to_long(vals[3]);
                    if (vals[4]) env = (int)gfa_value_to_long(vals[4]);
                    for (k = 0; k < 5; k++) if (vals[k]) os_mem_free(vals[k]);
                }
                gfa_sound(ch, freq, dur, vol, env);
            }
            break;

        case OP_BEEP:
            gfa_beep();
            break;

        case OP_INPUT:
            var = (gfa_variable *)inst->operand.ptr_val;
            if (var != NULL) {
                char line[256];
                if (fgets(line, (int)sizeof(line), stdin) != NULL) {
                    int len;
                    len = (int)strlen(line);
                    if (len > 0 && line[len - 1] == '\n') {
                        line[len - 1] = '\0';
                        len--;
                    }
                    if (var->type == GFA_VAR_STRING) {
                        gfa_var_set_from_string(var, line);
                    } else {
                        gfa_var_set_from_float(var, gfa_val(line));
                    }
                }
            }
            break;

        case OP_LINE_INPUT:
            var = (gfa_variable *)inst->operand.ptr_val;
            if (var != NULL) {
                char line[256];
                int i, c;
                i = 0;
                while (i < 255 && (c = getchar()) != EOF && c != '\n') {
                    line[i++] = (char)c;
                }
                line[i] = '\0';
                gfa_var_set_from_string(var, line);
            }
            break;

        case OP_LOCATE:
            /* Les coordonnees sont sur la pile : y, x */
            if (rt->sp >= 2) {
                v2 = gfa_value_pop(rt); /* x */
                v1 = gfa_value_pop(rt); /* y */
                if (v1 && v2) {
                    rt->cursor_x = (int)gfa_value_to_long(v2);
                    rt->cursor_y = (int)gfa_value_to_long(v1);
                    os_con_cursor_goto(rt->cursor_x, rt->cursor_y);
                }
                os_mem_free(v1); os_mem_free(v2);
            }
            break;

        case OP_OPEN_FILE:
            /* OPEN: stack has [mode$, channel, filename[, reclen]] */
            {
                char mode_str[8];
                int channel, reclen;
                const char *fname;
                gfa_value *vr, *vf, *vc, *vm;
                vr = vf = vc = vm = NULL;
                reclen = 0;
                /* Pop optional reclen first if 4 values on stack */
                if (rt->sp >= 4) {
                    vr = gfa_value_pop(rt);
                }
                /* Pop filename, channel, mode */
                if (rt->sp >= 3) {
                    vf = gfa_value_pop(rt);
                    vc = gfa_value_pop(rt);
                    vm = gfa_value_pop(rt);
                }
                if (vm && vc && vf) {
                    if (vm->type == GFA_VAL_STRING && vm->data.s && vm->data.s[0])
                        mode_str[0] = vm->data.s[0];
                    else
                        mode_str[0] = (char)gfa_value_to_long(vm);
                    mode_str[1] = '\0';
                    channel = (int)gfa_value_to_long(vc);
                    fname = (vf->type == GFA_VAL_STRING && vf->data.s) ? vf->data.s : "";
                    reclen = vr ? (int)gfa_value_to_long(vr) : 0;
                    gfa_open(mode_str, channel, fname, reclen);
                }
                if (vm) os_mem_free(vm);
                if (vc) os_mem_free(vc);
                if (vf) os_mem_free(vf);
                if (vr) os_mem_free(vr);
            }
            break;

        case OP_CLOSE_FILE:
            /* CLOSE: channel on stack */
            if (rt->sp >= 1) {
                v1 = gfa_value_pop(rt);
                if (v1) {
                    gfa_close((int)gfa_value_to_long(v1));
                    os_mem_free(v1);
                }
            }
            break;

        case OP_PEEK:
            if (rt->sp >= 1) { v1 = gfa_value_pop(rt); if (v1) { gfa_value_push_long(rt, gfa_value_to_long(v1)); os_mem_free(v1); } }
            break;

        case OP_POKE:
            if (rt->sp >= 2) { v2 = gfa_value_pop(rt); v1 = gfa_value_pop(rt); if (v1) os_mem_free(v1); if (v2) os_mem_free(v2); }
            break;

        case OP_DPEEK:
            if (rt->sp >= 1) { v1 = gfa_value_pop(rt); if (v1) { gfa_value_push_long(rt, gfa_value_to_long(v1)); os_mem_free(v1); } }
            break;

        case OP_DPOKE:
            if (rt->sp >= 2) { v2 = gfa_value_pop(rt); v1 = gfa_value_pop(rt); if (v1) os_mem_free(v1); if (v2) os_mem_free(v2); }
            break;

        case OP_LPEEK:
            if (rt->sp >= 1) { v1 = gfa_value_pop(rt); if (v1) { gfa_value_push_long(rt, gfa_value_to_long(v1)); os_mem_free(v1); } }
            break;

        case OP_LPOKE:
            if (rt->sp >= 2) { v2 = gfa_value_pop(rt); v1 = gfa_value_pop(rt); if (v1) os_mem_free(v1); if (v2) os_mem_free(v2); }
            break;

        case OP_ON_ERROR:
            /* operand = string index for error label */
            { int str_idx = (int)operand; (void)str_idx; }
            break;

        case OP_EVERY:
            if (rt->sp >= 1) {
                v1 = gfa_value_pop(rt);
                if (v1) { gfa_every(gfa_value_to_long(v1), 0); os_mem_free(v1); }
            }
            break;

        case OP_AFTER:
            if (rt->sp >= 1) {
                v1 = gfa_value_pop(rt);
                if (v1) { gfa_after(gfa_value_to_long(v1), 0); os_mem_free(v1); }
            }
            break;

        case OP_END:
            rt->running = 0;
            return 0;

        case OP_STOP:
            rt->stopped = 1;
            rt->running = 0;
            return 0;

        case OP_QUIT:
            if (rt->sp > 0) {
                v1 = gfa_value_pop(rt);
                if (v1 != NULL) {
                    rt->quit_code = (int)gfa_value_to_long(v1);
                }
                os_mem_free(v1);
            }
            os_sys_quit(rt->quit_code);
            rt->running = 0;
            return 0;

        case OP_TRON:
            rt->trace_on = 1;
            break;

        case OP_TROFF:
            rt->trace_on = 0;
            break;

        case OP_ERROR:
            /* Stack: [error_code] ; raise error */
            if (rt->sp > 0) {
                v1 = gfa_value_pop(rt);
                if (v1) {
                    gfa_error_raise((int)gfa_value_to_long(v1));
                    os_mem_free(v1);
                }
            }
            break;

        case OP_GEMDOS:
            /* Stack: [fn] [arg1] [arg2] ; call GEMDOS */
            if (rt->sp >= 3) {
                os_int32 fn, arg1, arg2;
                v2 = gfa_value_pop(rt);
                v1 = gfa_value_pop(rt);
                v0 = gfa_value_pop(rt);
                if (v0 && v1 && v2) {
                    fn = gfa_value_to_long(v0);
                    arg1 = gfa_value_to_long(v1);
                    arg2 = gfa_value_to_long(v2);
                    gfa_value_push_long(rt, gfa_gemdos(fn, arg1, arg2));
                }
                if (v0) os_mem_free(v0);
                if (v1) os_mem_free(v1);
                if (v2) os_mem_free(v2);
            }
            break;

        case OP_BIOS:
            /* Stack: [fn] [arg1] [arg2] ; call BIOS */
            if (rt->sp >= 3) {
                os_int32 fn, arg1, arg2;
                v2 = gfa_value_pop(rt);
                v1 = gfa_value_pop(rt);
                v0 = gfa_value_pop(rt);
                if (v0 && v1 && v2) {
                    fn = gfa_value_to_long(v0);
                    arg1 = gfa_value_to_long(v1);
                    arg2 = gfa_value_to_long(v2);
                    gfa_value_push_long(rt, gfa_bios(fn, arg1, arg2));
                }
                if (v0) os_mem_free(v0);
                if (v1) os_mem_free(v1);
                if (v2) os_mem_free(v2);
            }
            break;

        case OP_XBIOS:
            /* Stack: [fn] [arg1] [arg2] ; call XBIOS */
            if (rt->sp >= 3) {
                os_int32 fn, arg1, arg2;
                v2 = gfa_value_pop(rt);
                v1 = gfa_value_pop(rt);
                v0 = gfa_value_pop(rt);
                if (v0 && v1 && v2) {
                    fn = gfa_value_to_long(v0);
                    arg1 = gfa_value_to_long(v1);
                    arg2 = gfa_value_to_long(v2);
                    gfa_value_push_long(rt, gfa_xbios(fn, arg1, arg2));
                }
                if (v0) os_mem_free(v0);
                if (v1) os_mem_free(v1);
                if (v2) os_mem_free(v2);
            }
            break;

        case OP_LABEL:
        case OP_LINE_NUM:
            /* Marqueurs, ne rien faire */
            break;

        default:
            /* Opcode inconnu */
            runtime_error(rt, 9, "Function or command not yet implemented");
            return -1;
    }

    /* Avancer le pointeur d'instruction (sauf si deja modifie) */
    rt->ip++;

    return result;
}

/* ------------------------------------------------------------------ */
/* Gestion des erreurs                                                */
/* ------------------------------------------------------------------ */

static void runtime_error(gfa_runtime *rt, int code, const char *msg)
{
    (void)msg;

    if (rt == NULL) return;

    rt->error_code = code;

    if (rt->trace_on && code != 0) {
        os_con_output_string("Error ");
        {
            char buf[32];
            sprintf(buf, "%d", code);
            os_con_output_string(buf);
        }
        os_con_output_string(" at line ");
        {
            char buf[32];
            sprintf(buf, "%d", rt->current_line);
            os_con_output_string(buf);
        }
        os_con_output_char('\n');
    }

    /* Declencher ON ERROR GOSUB si actif */
    gfa_error_raise(code);
}
