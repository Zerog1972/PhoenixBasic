/*
 * runtime.c - Implementation du moteur d'execution GFA Basic 3.5
 * ==============================================================
 * Coeur de l'emulateur : boucle d'execution du bytecode,
 * gestion de la pile de valeurs, pile d'appels, contexte.
 *
 * Reference : cahier-des-charges-gfabasic.md, section 7
 */

#include "runtime.h"
#include "vmem.h"
#include "token.h"
#include "files.h"
#include "events.h"
#include "sound.h"
#include "tos.h"
#include "gfx.h"
#include "strings.h"
#include "matrix.h"
#include "vm_internal.h"

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
static int vm_dispatch(gfa_runtime *rt, int r);


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
    vmem_init();  /* memoire virtuelle pour PEEK/POKE/BYTE{}/... */

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

void gfa_runtime_set_chain_fn(gfa_runtime *rt, int (*fn)(const char *path))
{
    if (rt == NULL) return;
    rt->chain_fn = fn;
}

void gfa_runtime_shutdown(gfa_runtime *rt)
{
    if (rt == NULL) return;

    rt->running = 0;

    vmem_shutdown();
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
    /* Also support RESUME from error handler */
    if (rt != NULL && rt->resume_ip >= 0 && rt->running == 0) {
        rt->running = 1;
        rt->stopped = 0;
        rt->ip = rt->resume_ip;
        rt->error_code = 0;
        rt->fatal_error = 0;
        gfa_error_clear();
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

/*
 * vm_dispatch - Traduit le code retour de vm_exec_builtin /
 * vm_exec_statement (voir vm_internal.h) en valeur retournee par
 * execute_instruction.
 */
static int vm_dispatch(gfa_runtime *rt, int r)
{
    if (r < 0) return r;                       /* erreur fatale   */
    if (r == VM_ADV) { rt->ip++; return 0; }   /* break : ip++    */
    if (r == VM_RET0) return 0;                /* ip deja gere    */
    return r - 1;                              /* code de sortie  */
}

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
    if (getenv("GFA_DBG_IP")) {
        fprintf(stderr, "DBGIP ip=%d op=%d\n", rt->ip, (int)op);
        fflush(stderr);
    }
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
                if (!runtime_error(rt, 42, "Variable not found"))
                    return -1;
                return 0;
            }
            if (var->type != GFA_VAR_ARRAY) rt->last_var = var;
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
                        gfa_value_push_string_len(rt,
                            gfa_str_dup_n(var->value.str.data,
                                          (int)var->value.str.length),
                            (os_int32)var->value.str.length, 1);
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
                if (var->type != GFA_VAR_ARRAY) rt->last_var = var;
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
                            if (v1->type == GFA_VAL_STRING &&
                                v1->str_len > 0 && v1->data.s) {
                                /* Chaine binaire a longueur explicite */
                                gfa_var_set_from_string_len(
                                    var, v1->data.s, v1->str_len);
                            } else {
                                gfa_var_set_from_string(var,
                                    (v1->type == GFA_VAL_STRING)
                                        ? v1->data.s : "");
                            }
                            break;
                        default:
                            break;
                    }
                }
                gfa_value_discard(rt, 1);
            }
            break;

        case OP_ARRAY_LOAD:
            /* Stack: [indices...] ; pop les indices, lire l'element,
               push la valeur. operand2.index2 = nombre d'indices :
               garantit UN resultat meme si la variable n'est pas un
               tableau (ex : "MAT b = a" avant DIM b). */
            {
                int ndim, di;
                int indices[7];
                int in_range, valid, sp_ok;
                double *base;
                long flat_index = 0;
                int stride = 1;
                os_int32 arr_base = 0;

                var = (gfa_variable *)inst->operand.ptr_val;
                valid = (var != NULL && var->type == GFA_VAR_ARRAY &&
                         var->value.arr.data != NULL);
                ndim = 0;
                if (inst->has_operand2 && inst->operand2.index2 >= 0)
                    ndim = inst->operand2.index2;
                if (valid && var->value.arr.num_dims >= 1)
                    ndim = (ndim > 0) ? ndim : var->value.arr.num_dims;
                if (ndim < 0) ndim = 0;
                if (ndim > 7) ndim = 7;
                if (valid)
                    arr_base = var->value.arr.base;
                in_range = 1;
                sp_ok = 1;
                for (di = ndim - 1; di >= 0; di--) {
                    gfa_value *idx;
                    if (rt->sp >= 1) {
                        idx = gfa_value_pop(rt);
                        if (idx != NULL) {
                            int raw = (int)gfa_value_to_long(idx);
                            os_mem_free(idx);
                            indices[di] = (valid && arr_base != 0)
                                ? (raw - (int)arr_base) : raw;
                            if (valid && arr_base != 0 &&
                                (indices[di] < 0 ||
                                 indices[di] >=
                                  (int)var->value.arr.dim_sizes[di]))
                                in_range = 0;
                        } else {
                            indices[di] = 0;
                        }
                    } else {
                        indices[di] = 0;
                        sp_ok = 0;
                    }
                }
                if (valid && in_range && sp_ok &&
                    ndim == var->value.arr.num_dims) {
                    base = (double *)var->value.arr.data;
                    flat_index = 0;
                    stride = 1;
                    for (di = 0; di < ndim; di++) {
                        flat_index += indices[di] * stride;
                        stride *= var->value.arr.dim_sizes[di];
                    }
                    /* Pousser selon le type logique des elements */
                    {
                        double fv = base[flat_index];
                        gfa_var_type et = var->value.arr.elem_type;
                        if (et == GFA_VAR_BYTE || et == GFA_VAR_WORD ||
                            et == GFA_VAR_LONG)
                            gfa_value_push_long(rt, (os_int32)(long)fv);
                        else
                            gfa_value_push_float(rt, fv);
                    }
                } else {
                    gfa_value_push_float(rt, 0.0);
                }
            }
            break;

        case OP_ARRAY_STORE:
            /* Stack: [indices...] [value] ; pop value + indices, store.
               operand2.index2 = nombre d'indices (garantit un depilement
               propre meme si la variable n'est pas un tableau). */
            {
                int ndim, di;
                int indices[7];
                int in_range, valid, sp_ok;
                double *base;
                long flat_index = 0;
                int stride = 1;
                os_int32 arr_base = 0;
                gfa_value *val = NULL;

                var = (gfa_variable *)inst->operand.ptr_val;
                valid = (var != NULL && var->type == GFA_VAR_ARRAY &&
                         var->value.arr.data != NULL);
                ndim = 0;
                if (inst->has_operand2 && inst->operand2.index2 >= 0)
                    ndim = inst->operand2.index2;
                if (valid && var->value.arr.num_dims >= 1)
                    ndim = (ndim > 0) ? ndim : var->value.arr.num_dims;
                if (ndim < 0) ndim = 0;
                if (ndim > 7) ndim = 7;
                if (valid)
                    arr_base = var->value.arr.base;
                in_range = 1;
                sp_ok = 1;
                if (rt->sp >= 1) {
                    val = gfa_value_pop(rt);
                } else {
                    sp_ok = 0;
                }
                for (di = ndim - 1; di >= 0; di--) {
                    gfa_value *idx;
                    if (rt->sp >= 1) {
                        idx = gfa_value_pop(rt);
                        if (idx != NULL) {
                            int raw = (int)gfa_value_to_long(idx);
                            os_mem_free(idx);
                            indices[di] = (valid && arr_base != 0)
                                ? (raw - (int)arr_base) : raw;
                            if (valid && arr_base != 0 &&
                                (indices[di] < 0 ||
                                 indices[di] >=
                                  (int)var->value.arr.dim_sizes[di]))
                                in_range = 0;
                        } else {
                            indices[di] = 0;
                        }
                    } else {
                        indices[di] = 0;
                        sp_ok = 0;
                    }
                }
                if (valid && in_range && sp_ok && val != NULL &&
                    ndim == var->value.arr.num_dims) {
                    base = (double *)var->value.arr.data;
                    flat_index = 0;
                    stride = 1;
                    for (di = 0; di < ndim; di++) {
                        flat_index += indices[di] * stride;
                        stride *= var->value.arr.dim_sizes[di];
                    }
                    /* Conversion selon le type logique des elements */
                    {
                        double fv = gfa_value_to_float(val);
                        gfa_var_type et = var->value.arr.elem_type;
                        if (et == GFA_VAR_BYTE) {
                            long lv = gfa_value_to_long(val);
                            fv = (double)((unsigned long)lv & 0xFFUL);
                        } else if (et == GFA_VAR_WORD) {
                            unsigned long u =
                                (unsigned long)gfa_value_to_long(val)
                                & 0xFFFFUL;
                            fv = (u > 32767UL)
                                ? (double)(long)(u - 65536UL)
                                : (double)(long)u;
                        }
                        base[flat_index] = fv;
                    }
                }
                if (val != NULL) os_mem_free(val);
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
            /*
             * Echange les deux valeurs au sommet de la pile.
             * Note: si les deux valeurs sont des chaines avec owns_string=1
             * et partagent le meme pointeur (ex: viennent de la meme
             * variable), le swap est sans danger car les pointeurs sont
             * simplement echanges (pas de double-free).
             * Structure copy is safe because we swap the entire gfa_value
             * structs, not just the string pointers.
             */
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
                        if (!runtime_error(rt, 11, "Division by zero")) {
                            os_mem_free(v1); os_mem_free(v2);
                            return -1;
                        }
                        os_mem_free(v1); os_mem_free(v2);
                        return 0;
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
                        if (!runtime_error(rt, 11, "Division by zero")) {
                            os_mem_free(v1); os_mem_free(v2);
                            return -1;
                        }
                        os_mem_free(v1); os_mem_free(v2);
                        return 0;
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
                        if (!runtime_error(rt, 11, "Division by zero")) {
                            os_mem_free(v1); os_mem_free(v2);
                            return -1;
                        }
                        os_mem_free(v1); os_mem_free(v2);
                        return 0;
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

        case OP_APPROX_EQ:
            if (rt->sp >= 2) {
                v2 = gfa_value_pop(rt); v1 = gfa_value_pop(rt);
                if (v1 && v2) {
                    if (v1->type == GFA_VAL_STRING && v2->type == GFA_VAL_STRING) {
                        bval = (strcmp(v1->data.s ? v1->data.s : "",
                                       v2->data.s ? v2->data.s : "") == 0);
                    } else {
                        double diff = gfa_value_to_float(v1) - gfa_value_to_float(v2);
                        bval = fabs(diff) < 1.0e-10;
                    }
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
            /* Garde : operande negatif = saut non patché (label introuvable). */
            if (operand < 0) {
                if (!runtime_error(rt, 9, "Jump to undefined label"))
                    return -1;
                return 0;
            }
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
            if (getenv("GFA_DBG_IP")) {
                fprintf(stderr, "DBGCALL2: inst->operand.int_val=%d\n",
                        (int)inst->operand.int_val);
                fflush(stderr);
            }
            /* Garde : un operande negatif = saut non patché (label introuvable). */
            if (operand < 0) {
                if (!runtime_error(rt, 9, "Jump to undefined label"))
                    return -1;
                return 0;
            }
            if (rt->call_depth < GFA_MAX_CALL_DEPTH) {
                gfa_call_frame *frame;
                frame = &rt->call_stack[rt->call_depth++];
                frame->return_ip = rt->ip + 1;
                frame->return_sp = rt->sp;
                frame->is_gosub  = 1;
                frame->proc_index = 0;
                frame->saved_count = 0;
                rt->ip = (int)operand;
                if (getenv("GFA_DBG_IP")) {
                    fprintf(stderr, "DBGCALLIP: operand=%d newip=%d\n",
                            (int)operand, rt->ip);
                    fflush(stderr);
                }
                return 0;
            }
            if (!runtime_error(rt, 93, "Stack overflow"))
                return -1;
            return 0;

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
                                    gfa_value_push_string_len(rt,
                                        gfa_str_dup_n(
                                            var->value.str.data,
                                            (int)var->value.str.length),
                                        (os_int32)var->value.str.length, 1);
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

        /* ---------------------------------------------------------- */
        /* Fonctions integrees (vm_builtin.c)                         */
        /* ---------------------------------------------------------- */
        case OP_CALL_BUILTIN:
            return vm_dispatch(rt, vm_exec_builtin(rt, inst, operand));
        /* ---------------------------------------------------------- */
        /* Instructions GFA specifiques (vm_statement.c)              */
        /* ---------------------------------------------------------- */
        case OP_PRINT: case OP_PRINT_AT: case OP_PRINT_USING: case OP_PRINT_NL:
        case OP_BLOAD: case OP_BSAVE: case OP_BGET: case OP_BPUT:
        case OP_PRINT_CHAN: case OP_INPUT_FILE: case OP_CLS: case OP_OPENW:
        case OP_CLOSEW: case OP_COLOR: case OP_LINE_GFX: case OP_BOX_GFX:
        case OP_PBOX_GFX: case OP_CIRCLE_GFX: case OP_WINDOW_GFX: case OP_SOUND:
        case OP_BEEP: case OP_INPUT: case OP_LINE_INPUT: case OP_LINE_INPUT_FILE:
        case OP_LOCATE: case OP_OPEN_FILE: case OP_CLOSE_FILE: case OP_PEEK:
        case OP_POKE: case OP_DPEEK: case OP_DPOKE: case OP_LPEEK:
        case OP_LPOKE: case OP_SPOKE: case OP_SDPOKE: case OP_SLPOKE:
        case OP_ON_ERROR: case OP_EVERY: case OP_AFTER: case OP_END:
        case OP_STOP: case OP_QUIT: case OP_TRON: case OP_TROFF:
        case OP_ERROR: case OP_FATAL: case OP_RESUME: case OP_GEMDOS:
        case OP_BIOS: case OP_XBIOS: case OP_ERASE_VAR: case OP_ARRAYFILL:
        case OP_DIM_QUESTION: case OP_CLEAR_ALL: case OP_QSORT: case OP_SSORT:
        case OP_INSERT_ELEM: case OP_DELETE_ELEM: case OP_PLOT_GFX: case OP_TEXT_GFX:
        case OP_POLY_GFX: case OP_FILL_GFX: case OP_GETBIT_GFX: case OP_PUTBIT_GFX:
        case OP_SETCOLOR: case OP_MODE_GFX: case OP_CLIP_GFX: case OP_WINDOW_STMT:
        case OP_DRAW_TURTLE: case OP_DRAW_QUERY: case OP_MAT_CLR: case OP_MAT_ONE:
        case OP_MAT_CPY: case OP_MAT_ADD: case OP_MAT_SUB: case OP_MAT_MUL:
        case OP_MAT_TRANS: case OP_MAT_INV: case OP_MAT_DET: case OP_MAT_RANG:
        case OP_MAT_NORM: case OP_MAT_SET: case OP_MAT_PRINT: case OP_MAT_READ:
        case OP_MAT_INPUT: case OP_MAT_DET_EXPR: case OP_MAT_RANG_EXPR: case OP_MAT_NORM_EXPR:
        case OP_ELLIPSE_GFX: case OP_ACHAR_GFX: case OP_DIM: case OP_ON_GOTO:
        case OP_ON_GOSUB: case OP_LABEL: case OP_LINE_NUM:
            return vm_dispatch(rt, vm_exec_statement(rt, inst, operand));

        default:
            /* Opcode inconnu */
            if (!runtime_error(rt, 9, "Function or command not yet implemented"))
                return -1;
            return 0;
    }

    /* Avancer le pointeur d'instruction (sauf si deja modifie) */
    rt->ip++;

    return result;
}

/* ------------------------------------------------------------------ */
/* Gestion des erreurs                                                */
/* ------------------------------------------------------------------ */

/*
 * runtime_error - Raise a runtime error.
 * Returns 1 if execution jumped to an ON ERROR handler, 0 otherwise.
 * When jumping: saves resume_ip, pushes call frame, sets ip to handler.
 * Caller should return 0 after a successful jump, or -1 otherwise.
 */
int runtime_error(gfa_runtime *rt, int code, const char *msg)
{
    if (rt == NULL) return 0;

    rt->error_code = code;

    if (rt->trace_on && code != 0) {
        os_con_output_string("Error ");
        {
            char buf[32];
            sprintf(buf, "%d", code);
            os_con_output_string(buf);
        }
        os_con_output_string(": ");
        if (msg != NULL) {
            os_con_output_string(msg);
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

    /* If ON ERROR is active and we have a valid error label, jump to it */
    if (rt->on_error_active && rt->error_label >= 0 && !rt->fatal_error) {
        gfa_call_frame *frame;

        /* Save resume IP (current instruction that caused the error) */
        rt->resume_ip = rt->ip;

        /* Push a call frame so RETURN goes back */
        if (rt->call_depth < GFA_MAX_CALL_DEPTH) {
            frame = &rt->call_stack[rt->call_depth++];
            frame->return_ip = rt->ip + 1;
            frame->return_sp = rt->sp;
            frame->is_gosub  = 1;
            frame->proc_index = 0;
            frame->saved_count = 0;
        }

        /* Jump to error handler.
         * The caller (OP_ERROR etc.) returns 0, skipping post-switch rt->ip++,
         * so we set ip directly to the label address. */
        rt->ip = rt->error_label;

        return 1;  /* Successfully jumped to handler */
    }

    return 0;  /* No handler, program will stop */
}

/* ================================================================== */
/* Turtle (DRAW) - execution d'une chaine de commandes                */
/* ================================================================== */

/*
 * gfa_turtle_exec - Interprete la chaine de commandes GFA "DRAW".
 *   FD n / BK n   : avance/recule (echelle SX/SY appliquee)
 *   LT a / RT a   : tourne a gauche/droite de a degres
 *   TT a          : oriente a l'angle absolu a
 *   MA x,y        : deplace (sans tracé) aux coords absolues
 *   DA x,y        : trace jusqu'aux coords absolues
 *   MR x,y        : deplacement relatif
 *   DR x,y        : tracé relatif
 *   PU / PD       : pen up / pen down
 *   CO c          : couleur
 *   SX n / SY n   : echelle du mouvement
 * L'angle 0 = droite (+x), sens trigonométrique, y ecran vers le bas.
 */
void gfa_turtle_exec(gfa_runtime *rt, const char *prog)
{
    double sx = 1.0, sy = 1.0;
    const char *p;

    if (rt == NULL || prog == NULL) return;

    /* Initialisation du turtle au centre de l'ecran (a la premiere
       utilisation). */
    if (rt->turtle_x == 0 && rt->turtle_y == 0 &&
        rt->turtle_angle == 0 && rt->turtle_pen_down == 0 &&
        rt->turtle_color == 0) {
        rt->turtle_x = rt->screen_width / 2;
        rt->turtle_y = rt->screen_height / 2;
        rt->turtle_angle = 90;
        rt->turtle_color = rt->current_color;
    }

    p = prog;
    while (*p != '\0') {
        char cmd[5];
        double n = 0.0, m = 0.0;

        /* Sauter les espaces */
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;

        /* Lire le mot (lettres) */
        {
            int ci;
            ci = 0;
            while ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
                   *p == '_' || *p == '~') {
                char c = *p++;
                if (c >= 'a' && c <= 'z') c = (char)(c - 32);
                if (ci < 4) cmd[ci] = c;
                ci++;
            }
            cmd[4] = '\0';
        }

        /* Lire les nombres separes par virgules/espaces */
        n = 0.0; m = 0.0;
        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        if (*p == '-' || *p == '+' || (*p >= '0' && *p <= '9')) {
            char *end = NULL;
            n = strtod(p, &end);
            if (end != p) p = end;
            while (*p == ' ' || *p == '\t' || *p == ',') p++;
            if (*p == '-' || *p == '+' || (*p >= '0' && *p <= '9')) {
                m = strtod(p, &end);
                if (end != p) p = end;
            }
        }

        if (cmd[0] == '\0') break;

        if (cmd[0] == 'F' && cmd[1] == 'D' && cmd[2] == '\0') {
            double a = (rt->turtle_angle * 3.141592653) / 180.0;
            double nx = rt->turtle_x + n * sx * cos(a);
            double ny = rt->turtle_y - n * sy * sin(a);
            if (rt->turtle_pen_down)
                gfx_turtle_line(rt->turtle_x, rt->turtle_y,
                                (int)nx, (int)ny, rt->turtle_color);
            rt->turtle_x = (int)nx;
            rt->turtle_y = (int)ny;
        } else if (cmd[0] == 'B' && cmd[1] == 'K' && cmd[2] == '\0') {
            double a = (rt->turtle_angle * 3.141592653) / 180.0;
            double nx = rt->turtle_x - n * sx * cos(a);
            double ny = rt->turtle_y + n * sy * sin(a);
            if (rt->turtle_pen_down)
                gfx_turtle_line(rt->turtle_x, rt->turtle_y,
                                (int)nx, (int)ny, rt->turtle_color);
            rt->turtle_x = (int)nx;
            rt->turtle_y = (int)ny;
        } else if (cmd[0] == 'L' && cmd[1] == 'T' && cmd[2] == '\0') {
            rt->turtle_angle += (int)n;
        } else if (cmd[0] == 'R' && cmd[1] == 'T' && cmd[2] == '\0') {
            rt->turtle_angle -= (int)n;
        } else if (cmd[0] == 'T' && cmd[1] == 'T' && cmd[2] == '\0') {
            rt->turtle_angle = (int)n;
        } else if (cmd[0] == 'M' && cmd[1] == 'A' && cmd[2] == '\0') {
            rt->turtle_x = (int)n;
            rt->turtle_y = (int)m;
        } else if (cmd[0] == 'D' && cmd[1] == 'A' && cmd[2] == '\0') {
            if (rt->turtle_pen_down)
                gfx_turtle_line(rt->turtle_x, rt->turtle_y,
                                (int)n, (int)m, rt->turtle_color);
            rt->turtle_x = (int)n;
            rt->turtle_y = (int)m;
        } else if (cmd[0] == 'M' && cmd[1] == 'R' && cmd[2] == '\0') {
            rt->turtle_x += (int)n;
            rt->turtle_y += (int)m;
        } else if (cmd[0] == 'D' && cmd[1] == 'R' && cmd[2] == '\0') {
            if (rt->turtle_pen_down)
                gfx_turtle_line(rt->turtle_x, rt->turtle_y,
                                rt->turtle_x + (int)n,
                                rt->turtle_y + (int)m, rt->turtle_color);
            rt->turtle_x += (int)n;
            rt->turtle_y += (int)m;
        } else if (cmd[0] == 'P' && cmd[1] == 'U' && cmd[2] == '\0') {
            rt->turtle_pen_down = 0;
        } else if (cmd[0] == 'P' && cmd[1] == 'D' && cmd[2] == '\0') {
            rt->turtle_pen_down = 1;
        } else if (cmd[0] == 'C' && cmd[1] == 'O' && cmd[2] == '\0') {
            rt->turtle_color = (int)n;
        } else if (cmd[0] == 'S' && cmd[1] == 'X' && cmd[2] == '\0') {
            sx = (n != 0.0) ? n : 1.0;
        } else if (cmd[0] == 'S' && cmd[1] == 'Y' && cmd[2] == '\0') {
            sy = (n != 0.0) ? n : 1.0;
        }
        /* Autres commandes : ignorees */
    }
}

/* ================================================================== */
/* Tri de tableaux (QSORT / SSORT)                                    */
/* ================================================================== */

static int gfa_qsort_partition(double *a, int lo, int hi)
{
    double pivot = a[(lo + hi) / 2];
    int i = lo - 1;
    int j = hi + 1;

    for (;;) {
        do { i++; } while (a[i] < pivot);
        do { j--; } while (a[j] > pivot);
        if (i >= j) return j;
        {
            double t = a[i];
            a[i] = a[j];
            a[j] = t;
        }
    }
}

static void gfa_qsort_rec(double *a, int lo, int hi)
{
    int j;

    if (lo >= hi) return;
    j = gfa_qsort_partition(a, lo, hi);
    gfa_qsort_rec(a, lo, j);
    gfa_qsort_rec(a, j + 1, hi);
}

void gfa_array_quicksort(double *arr, int lo, int hi)
{
    if (arr == NULL || lo < 0 || hi > 4096) return;
    gfa_qsort_rec(arr, lo, hi);
}

void gfa_array_shellsort(double *arr, int lo, int hi)
{
    int gap, i, j;
    if (arr == NULL || lo < 0 || hi > 4096) return;
    for (gap = (hi - lo) / 2; gap > 0; gap /= 2) {
        for (i = lo + gap; i <= hi; i++) {
            double tmp = arr[i];
            for (j = i; j >= lo + gap; j -= gap) {
                if (arr[j - gap] > tmp)
                    arr[j] = arr[j - gap];
                else
                    break;
            }
            arr[j] = tmp;
        }
    }
}
