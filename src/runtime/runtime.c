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
#include "gfx.h"
#include "strings.h"

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
static int runtime_error(gfa_runtime *rt, int code, const char *msg);
static char *format_using(const char *fmt, double value);

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
 * format_using - GFA Basic PRINT USING format engine.
 * Parses a format string and formats a numeric value accordingly.
 *
 * Supported patterns:
 *   #          digit placeholder
 *   .          decimal point
 *   ,          thousands separator (in output)
 *   **         fill leading with asterisks
 *   $$         floating dollar sign
 *   **$        fill leading with * and floating $
 *   + / -      sign indicator (leading + or trailing -)
 *   ^^^^       exponential format (E+00)
 *   _          escape next character (literal)
 *   other      literal character
 */
static char *format_using(const char *fmt, double value)
{
    char buf[256];
    int buflen;
    char number_part[128];
    int num_len;
    int leading_stars;
    int floating_dollar;
    int star_fill;
    int int_digits;
    int frac_digits;
    int exp_format;
    int i;
    int fmt_len;
    int pos;
    int dot_pos;
    int trail_minus;
    int lead_plus;
    double abs_val;
    int is_neg;
    char *result;
    int outpos;

    if (fmt == NULL || fmt[0] == '\0') {
        return gfa_str_float(value);
    }

    fmt_len = (int)strlen(fmt);
    leading_stars = 0;
    floating_dollar = 0;
    star_fill = 0;
    int_digits = 0;
    frac_digits = 0;
    exp_format = 0;
    trail_minus = 0;
    lead_plus = 0;
    dot_pos = -1;
    buflen = 0;

    /* Parse format string to count digits and flags */
    i = 0;
    pos = 0;
    while (i < fmt_len && pos < 128) {
        char c;
        c = fmt[i];

        if (c == '_' && i + 1 < fmt_len) {
            /* Escape: next char is literal */
            number_part[pos++] = fmt[i + 1];
            i += 2;
            continue;
        }

        if (c == '^') {
            /* Caret sequence for exponential */
            if (i + 3 < fmt_len && fmt[i+1] == '^' &&
                fmt[i+2] == '^' && fmt[i+3] == '^') {
                exp_format = 1;
                i += 4;
                continue;
            }
            number_part[pos++] = c;
            i++;
            continue;
        }

        /* Check for ** prefix */
        if (i == 0 && c == '*' && i + 1 < fmt_len && fmt[i+1] == '*') {
            number_part[pos++] = '*'; number_part[pos++] = '*';
            leading_stars = 2;
            star_fill = 1;
            i += 2;
            continue;
        }

        /* Check for $$ prefix */
        if (c == '$' && i + 1 < fmt_len && fmt[i+1] == '$') {
            number_part[pos++] = '$'; number_part[pos++] = '$';
            floating_dollar = 2;
            i += 2;
            continue;
        }

        /* Check for **$ prefix */
        if (c == '*' && i + 2 < fmt_len && fmt[i+1] == '*' &&
            fmt[i+2] == '$') {
            number_part[pos++] = '*'; number_part[pos++] = '*';
            number_part[pos++] = '$';
            leading_stars = 2;
            star_fill = 1;
            floating_dollar = 1;
            i += 3;
            continue;
        }

        /* Leading + sign */
        if (i == 0 && c == '+') {
            lead_plus = 1;
            number_part[pos++] = c;
            i++;
            continue;
        }

        /* Trailing - sign (at end of format) */
        if (c == '-') {
            /* Check if it's at the end or followed only by trailing chars */
            {
                int after;
                after = i + 1;
                while (after < fmt_len && fmt[after] != '#') after++;
                if (after == fmt_len) {
                    trail_minus = 1;
                    i++;
                    continue;
                }
            }
        }

        if (c == '#') {
            if (dot_pos < 0) {
                int_digits++;
            } else {
                frac_digits++;
            }
            number_part[pos++] = c;
            i++;
            continue;
        }

        if (c == '.') {
            dot_pos = pos;
            number_part[pos++] = c;
            i++;
            continue;
        }

        if (c == ',') {
            i++;
            continue;
        }

        /* Any other character: literal */
        number_part[pos++] = c;
        i++;
    }
    number_part[pos] = '\0';
    num_len = pos;

    /* Handle exponential format */
    if (exp_format) {
        char exp_buf[64];
        int exp_val;
        double mant;
        int mant_sign;
        mant = value;
        exp_val = 0;
        mant_sign = (mant >= 0) ? 1 : -1;
        if (mant == 0.0) {
            exp_val = 0;
        } else {
            mant = (mant < 0) ? -mant : mant;
            while (mant >= 10.0) { mant /= 10.0; exp_val++; }
            while (mant < 1.0 && mant > 0.0) { mant *= 10.0; exp_val--; }
        }
        if (mant_sign < 0) mant = -mant;
        sprintf(exp_buf, "%.*fE%+03d", frac_digits > 0 ? frac_digits : 4,
                mant, exp_val);
        result = gfa_str_new(exp_buf);
        if (result == NULL) return NULL;
        return result;
    }

    /* Build formatted output */
    {
        char raw_num[512];

        abs_val = (value < 0) ? -value : value;
        is_neg = (value < 0) ? 1 : 0;

        /* Safety clamp: prevent buffer overflow from excessive fraction digits */
        if (frac_digits > 99) frac_digits = 99;

        /* Build formatted number: separate integer and fractional parts */
        {
            char int_digits_raw[64];
            char frac_digits_raw[64];
            int int_len;
            int frac_len;
            int int_idx;
            int frac_idx;
            int ip;
            int past_decimal;
            int dollar_pos;
            int fmtpos;

            if (frac_digits > 0) {
                sprintf(raw_num, "%.*f", frac_digits, abs_val);
            } else {
                long int_part_long;
                int_part_long = (long)abs_val;
                sprintf(raw_num, "%ld", int_part_long);
            }

            /* Split at decimal point and extract digits */
            {
                int raw_len;
                int dot_idx;
                raw_len = (int)strlen(raw_num);
                dot_idx = -1;
                for (ip = 0; ip < raw_len; ip++) {
                    if (raw_num[ip] == '.') { dot_idx = ip; break; }
                }

                /* Extract integer digits */
                int_len = 0;
                for (ip = 0; ip < (dot_idx >= 0 ? dot_idx : raw_len); ip++) {
                    if (raw_num[ip] >= '0' && raw_num[ip] <= '9') {
                        int_digits_raw[int_len++] = raw_num[ip];
                    }
                }
                int_digits_raw[int_len] = '\0';

                /* Extract fractional digits */
                frac_len = 0;
                if (dot_idx >= 0) {
                    for (ip = dot_idx + 1; ip < raw_len; ip++) {
                        if (raw_num[ip] >= '0' && raw_num[ip] <= '9') {
                            frac_digits_raw[frac_len++] = raw_num[ip];
                        }
                    }
                }
                /* Pad fractional part with zeros on right */
                while (frac_len < frac_digits) {
                    frac_digits_raw[frac_len++] = '0';
                }
                frac_digits_raw[frac_len] = '\0';
            }

            /* Walk format template and fill output buffer */
            outpos = 0;
            int_idx = 0;
            frac_idx = 0;
            past_decimal = 0;
            dollar_pos = -1;

            /* Calculate integer padding: how many spaces before first digit */
            {
                int int_pad;
                int_pad = int_digits - int_len;
                if (int_pad < 0) int_pad = 0;

                for (fmtpos = 0; fmtpos < num_len && outpos < 250; fmtpos++) {
                    char fc;
                    fc = number_part[fmtpos];

                    if (fc == '.') {
                        past_decimal = 1;
                        buf[outpos++] = '.';
                    } else if (fc == '#') {
                        if (!past_decimal) {
                            /* Integer position */
                            if (int_pad > 0) {
                                buf[outpos++] = ' ';
                                int_pad--;
                            } else if (int_idx < int_len) {
                                buf[outpos++] = int_digits_raw[int_idx++];
                            } else {
                                buf[outpos++] = ' ';
                            }
                        } else {
                            /* Fractional position */
                            if (frac_idx < frac_len) {
                                buf[outpos++] = frac_digits_raw[frac_idx++];
                            } else {
                                buf[outpos++] = '0';
                            }
                        }
                    } else if (fc == '*' && star_fill) {
                        buf[outpos++] = '*';
                    } else if (fc == '$' && floating_dollar > 0) {
                        buf[outpos++] = '$';
                        dollar_pos = outpos - 1;
                    } else {
                        buf[outpos++] = fc;
                    }
                }
            }

            /* Star fill: replace leading * positions */
            if (star_fill) {
                /* Replace stars with spaces or digits */
                int star_count;
                star_count = leading_stars;
                if (floating_dollar) star_count = 2;  /* **$ = 2 stars */

                /* Stars fill from the rightmost star position */
                for (i = 0; i < outpos && star_count > 0; i++) {
                    if (buf[i] == '*') {
                        buf[i] = ' ';
                        star_count--;
                    }
                }
            }

            /* Floating dollar: move $ next to first digit */
            if (floating_dollar) {
                /* Find first non-space, non-star digit */
                int first_dig;
                first_dig = -1;
                for (i = 0; i < outpos; i++) {
                    if (buf[i] >= '0' && buf[i] <= '9') {
                        first_dig = i;
                        break;
                    }
                }
                if (first_dig >= 1 && dollar_pos >= 0) {
                    /* Shift right part to make room for $ before first digit */
                    buf[dollar_pos] = ' ';
                    buf[first_dig - 1] = '$';
                }
            }
        }
        buf[buflen > 0 ? buflen : outpos] = '\0';
        buflen = (outpos > 0) ? outpos : buflen;
    }

    /* Handle sign */
    {
        char final_buf[256];
        int fi;
        fi = 0;

        /* For leading + format, replace + with - if negative */
        for (i = 0; i < buflen && fi < 250; i++) {
            if (is_neg && buf[i] == '+' && lead_plus) {
                final_buf[fi++] = '-';
            } else {
                final_buf[fi++] = buf[i];
            }
        }

        /* For trailing minus format, append sign at end */
        if (trail_minus && fi < 249) {
            if (is_neg)
                final_buf[fi++] = '-';
            else
                final_buf[fi++] = ' ';
        }

        final_buf[fi] = '\0';
        result = gfa_str_new(final_buf);
    }

    return result;
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
                        if (!runtime_error(rt, 0, "Division by zero")) {
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
                        if (!runtime_error(rt, 0, "Division by zero")) {
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
                        if (!runtime_error(rt, 0, "Division by zero")) {
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
            if (!runtime_error(rt, 93, "Stack overflow"))
                return -1;
            return 0;

        case OP_RET:
            fprintf(stderr, "DEBUG OP_RET: call_depth=%d\n", rt->call_depth);
            if (rt->call_depth > 0) {
                gfa_call_frame *frame;
                int i;
                frame = &rt->call_stack[--rt->call_depth];
                fprintf(stderr, "DEBUG OP_RET: return_ip=%d\n", frame->return_ip);
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
                char *result_s;

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

                    /* Bits - 2 arguments */
                    case TOK_BTST: case TOK_BSET: case TOK_BCLR: case TOK_BCHG:
                    case TOK_SHL: case TOK_SHR: case TOK_ROL: case TOK_ROR:
                        arg2 = gfa_value_pop(rt);
                        arg1 = gfa_value_pop(rt);
                        if (arg1 && arg2) {
                            os_int32 a = gfa_value_to_long(arg1);
                            os_int32 b = gfa_value_to_long(arg2);
                            switch (func_tok) {
                                case TOK_BTST: result_l = (os_int32)gfa_btst(a, (int)b); break;
                                case TOK_BSET: result_l = (os_int32)gfa_bset(a, (int)b); break;
                                case TOK_BCLR: result_l = (os_int32)gfa_bclr(a, (int)b); break;
                                case TOK_BCHG: result_l = (os_int32)gfa_bchg(a, (int)b); break;
                                case TOK_SHL:  result_l = (os_int32)gfa_shl(a, (int)b); break;
                                case TOK_SHR:  result_l = (os_int32)gfa_shr(a, (int)b); break;
                                case TOK_ROL:  result_l = (os_int32)gfa_rol(a, (int)b); break;
                                case TOK_ROR:  result_l = (os_int32)gfa_ror(a, (int)b); break;
                                default: break;
                            }
                            gfa_value_push_long(rt, result_l);
                        }
                        os_mem_free(arg1); os_mem_free(arg2);
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

                    /* Conversion binaire <-> chaine (donnees typees) */
                    case TOK_MKI_TOK:
                        arg1 = gfa_value_pop(rt);
                        if (arg1) {
                            result_s = gfa_mki(gfa_value_to_long(arg1));
                            gfa_value_push_string(rt, result_s, 1);
                            if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                            os_mem_free(arg1);
                        }
                        break;

                    case TOK_MKL_TOK:
                        arg1 = gfa_value_pop(rt);
                        if (arg1) {
                            result_s = gfa_mkl(gfa_value_to_long(arg1));
                            gfa_value_push_string(rt, result_s, 1);
                            if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                            os_mem_free(arg1);
                        }
                        break;

                    case TOK_MKS_TOK:
                        arg1 = gfa_value_pop(rt);
                        if (arg1) {
                            result_s = gfa_mks(gfa_value_to_float(arg1));
                            gfa_value_push_string(rt, result_s, 1);
                            if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                            os_mem_free(arg1);
                        }
                        break;

                    case TOK_MKF_TOK:
                        arg1 = gfa_value_pop(rt);
                        if (arg1) {
                            result_s = gfa_mkf(gfa_value_to_float(arg1));
                            gfa_value_push_string(rt, result_s, 1);
                            if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                            os_mem_free(arg1);
                        }
                        break;

                    case TOK_MKD_TOK:
                        arg1 = gfa_value_pop(rt);
                        if (arg1) {
                            result_s = gfa_mkd(gfa_value_to_float(arg1));
                            gfa_value_push_string(rt, result_s, 1);
                            if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                            os_mem_free(arg1);
                        }
                        break;

                    case TOK_CVI_TOK:
                        arg1 = gfa_value_pop(rt);
                        if (arg1) {
                            const char *s = (arg1->type == GFA_VAL_STRING && arg1->data.s) ? arg1->data.s : "";
                            gfa_value_push_float(rt, (double)(int)gfa_cvi(s));
                            if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                            os_mem_free(arg1);
                        }
                        break;

                    case TOK_CVL_TOK:
                        arg1 = gfa_value_pop(rt);
                        if (arg1) {
                            const char *s = (arg1->type == GFA_VAL_STRING && arg1->data.s) ? arg1->data.s : "";
                            gfa_value_push_long(rt, gfa_cvl(s));
                            if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                            os_mem_free(arg1);
                        }
                        break;

                    case TOK_CVS_TOK:
                        arg1 = gfa_value_pop(rt);
                        if (arg1) {
                            const char *s = (arg1->type == GFA_VAL_STRING && arg1->data.s) ? arg1->data.s : "";
                            gfa_value_push_float(rt, gfa_cvs(s));
                            if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                            os_mem_free(arg1);
                        }
                        break;

                    case TOK_CVF_TOK:
                        arg1 = gfa_value_pop(rt);
                        if (arg1) {
                            const char *s = (arg1->type == GFA_VAL_STRING && arg1->data.s) ? arg1->data.s : "";
                            gfa_value_push_float(rt, gfa_cvf(s));
                            if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                            os_mem_free(arg1);
                        }
                        break;

                    case TOK_CVD_TOK:
                        arg1 = gfa_value_pop(rt);
                        if (arg1) {
                            const char *s = (arg1->type == GFA_VAL_STRING && arg1->data.s) ? arg1->data.s : "";
                            gfa_value_push_float(rt, gfa_cvd(s));
                            if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
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

                    /* AES (GEM) - implementes minimalement */
                    case TOK_APPL_INIT:
                        gfa_value_push_long(rt, 1);  /* Fake app ID */
                        break;
                    case TOK_APPL_EXIT:
                        gfa_value_push_long(rt, 0);
                        break;
                    case TOK_BGET:
                        /* BGET(#chan, addr, count) returns bytes read */
                        if (rt->sp >= 3) {
                            gfa_value *c, *a, *ch;
                            int count, n, chan;
                            static char bgetbuf2[4096];
                            c = gfa_value_pop(rt);
                            a = gfa_value_pop(rt);
                            ch = gfa_value_pop(rt);
                            if (ch && a && c) {
                                chan = (int)gfa_value_to_long(ch);
                                count = (int)gfa_value_to_long(c);
                                if (count > 4096) count = 4096;
                                n = gfa_bget(chan, (void *)bgetbuf2, count);
                                gfa_value_push_long(rt, (os_int32)n);
                            } else gfa_value_push_long(rt, -1);
                            if (ch) os_mem_free(ch);
                            if (a) os_mem_free(a);
                            if (c) os_mem_free(c);
                        } else gfa_value_push_long(rt, -1);
                        break;

                    case TOK_BPUT:
                        /* BPUT(#chan, addr, count) returns bytes written */
                        if (rt->sp >= 3) {
                            gfa_value *c, *a, *ch;
                            int count, n, chan;
                            static char bputbuf2[4096];
                            c = gfa_value_pop(rt);
                            a = gfa_value_pop(rt);
                            ch = gfa_value_pop(rt);
                            if (ch && a && c) {
                                chan = (int)gfa_value_to_long(ch);
                                count = (int)gfa_value_to_long(c);
                                if (count > 4096) count = 4096;
                                n = gfa_bput(chan, (const void *)bputbuf2, count);
                                gfa_value_push_long(rt, (os_int32)n);
                            } else gfa_value_push_long(rt, -1);
                            if (ch) os_mem_free(ch);
                            if (a) os_mem_free(a);
                            if (c) os_mem_free(c);
                        } else gfa_value_push_long(rt, -1);
                        break;

                    case TOK_FORM_ALERT:
                        /* FORM_ALERT(button, string) : pop args, show on console */
                        if (rt->sp >= 2) {
                            gfa_value *s = gfa_value_pop(rt);
                            gfa_value *b = gfa_value_pop(rt);
                            if (s && s->type == GFA_VAL_STRING) {
                                os_con_output_string("\n[ALERT] ");
                                os_con_output_string(s->data.s ? s->data.s : "");
                                os_con_output_string("\n");
                            }
                            if (s) os_mem_free(s);
                            if (b) os_mem_free(b);
                        }
                        gfa_value_push_long(rt, 1);  /* Default button */
                        break;
                    case TOK_MENU_BAR:
                    case TOK_WIND_OPEN:
                    case TOK_WIND_CLOSE:
                    case TOK_EVNT_KEYBD:
                    case TOK_EVNT_MOUSE:
                    case TOK_GRAF_DRAGBOX:
                        gfa_value_push_long(rt, 0);
                        break;

                    /* ========================================== */
                    /* Priorite A - fonctions runtime uniquement  */
                    /* ========================================== */

                    /* VAL? - compter les caracteres valides */
                    case TOK_VAL_COUNT:
                        arg1 = gfa_value_pop(rt);
                        if (arg1) {
                            const char *s = (arg1->type == GFA_VAL_STRING && arg1->data.s) ? arg1->data.s : "";
                            result_f = (double)gfa_val_count(s);
                            gfa_value_push_float(rt, result_f);
                            if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                            os_mem_free(arg1);
                        }
                        break;

                    /* INPUT$ - lire depuis un fichier ou stdin */
                    case TOK_INPUT_TOK:
                        /* INPUT$(count) ou INPUT$(#chan, count) */
                        if (rt->sp >= 2) {
                            arg2 = gfa_value_pop(rt);
                            arg1 = gfa_value_pop(rt);
                            if (arg1 && arg2) {
                                int count = (int)gfa_value_to_long(arg2);
                                int chan = (int)gfa_value_to_long(arg1);
                                if (count > 256) count = 256;
                                if (count < 0) count = 0;
                                {
                                    char *buf = (char *)os_mem_alloc((size_t)(count + 1));
                                    if (buf) {
                                        int nread;
                                        nread = gfa_input_channel(chan, buf, count);
                                        buf[nread] = '\0';
                                        result_s = gfa_str_new(buf);
                                        os_mem_free(buf);
                                        gfa_value_push_string(rt, result_s, 1);
                                    } else {
                                        gfa_value_push_string(rt, gfa_str_new(""), 1);
                                    }
                                }
                            }
                            if (arg1) { if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s); os_mem_free(arg1); }
                            if (arg2) { if (arg2->owns_string && arg2->data.s) os_mem_free(arg2->data.s); os_mem_free(arg2); }
                        } else if (rt->sp >= 1) {
                            /* INPUT$(count) lit depuis stdin */
                            arg1 = gfa_value_pop(rt);
                            if (arg1) {
                                int count = (int)gfa_value_to_long(arg1);
                                if (count > 256) count = 256;
                                if (count < 0) count = 0;
                                if (count > 0) {
                                    char *buf = (char *)os_mem_alloc((size_t)(count + 1));
                                    if (buf) {
                                        int i;
                                        for (i = 0; i < count; i++) {
                                            buf[i] = (char)os_con_input_char();
                                        }
                                        buf[count] = '\0';
                                        result_s = gfa_str_new(buf);
                                        os_mem_free(buf);
                                        gfa_value_push_string(rt, result_s, 1);
                                    } else {
                                        gfa_value_push_string(rt, gfa_str_new(""), 1);
                                    }
                                } else {
                                    gfa_value_push_string(rt, gfa_str_new(""), 1);
                                }
                                if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                                os_mem_free(arg1);
                            }
                        } else {
                            gfa_value_push_string(rt, gfa_str_new(""), 1);
                        }
                        break;

                    /* INPMID$ - INSTR + MID$ combine */
                    case TOK_INPMID:
                        /* Stack: [haystack, needle, start] -> pop en ordre inverse */
                        if (rt->sp >= 3) {
                            gfa_value *a3, *a2, *a1;
                            a3 = gfa_value_pop(rt); /* start */
                            a2 = gfa_value_pop(rt); /* needle */
                            a1 = gfa_value_pop(rt); /* haystack */
                            if (a1 && a2 && a3) {
                                const char *hs = (a1->type == GFA_VAL_STRING && a1->data.s) ? a1->data.s : "";
                                const char *nd = (a2->type == GFA_VAL_STRING && a2->data.s) ? a2->data.s : "";
                                int start = (int)gfa_value_to_long(a3);
                                int pos = gfa_instr(start, hs, nd);
                                if (pos > 0) {
                                    result_s = gfa_mid(hs, pos, (int)strlen(hs) - pos + 1);
                                    gfa_value_push_string(rt, result_s, 1);
                                } else {
                                    gfa_value_push_string(rt, gfa_str_new(""), 1);
                                }
                            }
                            if (a1) { if (a1->owns_string && a1->data.s) os_mem_free(a1->data.s); os_mem_free(a1); }
                            if (a2) { if (a2->owns_string && a2->data.s) os_mem_free(a2->data.s); os_mem_free(a2); }
                            if (a3) { if (a3->owns_string && a3->data.s) os_mem_free(a3->data.s); os_mem_free(a3); }
                        } else if (rt->sp >= 2) {
                            gfa_value *a2, *a1;
                            a2 = gfa_value_pop(rt); /* needle */
                            a1 = gfa_value_pop(rt); /* haystack */
                            if (a1 && a2) {
                                const char *hs = (a1->type == GFA_VAL_STRING && a1->data.s) ? a1->data.s : "";
                                const char *nd = (a2->type == GFA_VAL_STRING && a2->data.s) ? a2->data.s : "";
                                int pos = gfa_instr(1, hs, nd);
                                if (pos > 0) {
                                    result_s = gfa_mid(hs, pos, (int)strlen(hs) - pos + 1);
                                    gfa_value_push_string(rt, result_s, 1);
                                } else {
                                    gfa_value_push_string(rt, gfa_str_new(""), 1);
                                }
                            }
                            if (a1) { if (a1->owns_string && a1->data.s) os_mem_free(a1->data.s); os_mem_free(a1); }
                            if (a2) { if (a2->owns_string && a2->data.s) os_mem_free(a2->data.s); os_mem_free(a2); }
                        } else {
                            gfa_value_push_string(rt, gfa_str_new(""), 1);
                        }
                        break;

                    /* DIR$ - premier fichier du repertoire */
                    case TOK_DIR_TOK:
                    case TOK_DIR_TOK2:
                        {
                            const char *pattern = "*.*";
                            if (rt->sp >= 1) {
                                arg1 = gfa_value_pop(rt);
                                if (arg1 && arg1->type == GFA_VAL_STRING && arg1->data.s) {
                                    pattern = arg1->data.s;
                                }
                            }
                            {
                                os_file_info info;
                                os_mem_set(&info, 0, sizeof(info));
                                if (os_dir_first(pattern, 0, &info) == 0 && info.name[0] != '\0') {
                                    gfa_value_push_string(rt, gfa_str_new(info.name), 1);
                                } else {
                                    gfa_value_push_string(rt, gfa_str_new(""), 1);
                                }
                            }
                            if (arg1) { if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s); os_mem_free(arg1); }
                        }
                        break;

                    /* DFREE - espace disque libre (octets) */
                    case TOK_DFREE:
                        /* Pop argument optionnel (drive) */
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1) {
                                result_l = os_fs_free((int)gfa_value_to_long(arg1));
                                if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                                os_mem_free(arg1);
                            } else {
                                result_l = os_fs_free(0);
                            }
                        } else {
                            result_l = os_fs_free(0);
                        }
                        gfa_value_push_long(rt, result_l);
                        break;

                    /* TYPE - type de variable (0=float, 1=string) */
                    case TOK_TYPE_TOK:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1) {
                                if (arg1->type == GFA_VAL_STRING)
                                    gfa_value_push_float(rt, 1.0);
                                else
                                    gfa_value_push_float(rt, 0.0);
                                if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                                os_mem_free(arg1);
                            } else {
                                gfa_value_push_float(rt, 0.0);
                            }
                        } else {
                            gfa_value_push_float(rt, 0.0);
                        }
                        break;

                    /* PAUSE - attendre une touche */
                    case TOK_PAUSE:
                        /* PAUSE delay (en 1/50s) */
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1) {
                                int delay_ms = (int)(gfa_value_to_float(arg1) * 20.0);
                                if (delay_ms < 0) delay_ms = 0;
                                os_time_delay((os_int32)delay_ms);
                                if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                                os_mem_free(arg1);
                            }
                        }
                        gfa_value_push_long(rt, 0);
                        break;

                    /* DELAY - pause en millisecondes */
                    case TOK_DELAY:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1) {
                                int delay_ms = (int)gfa_value_to_long(arg1);
                                if (delay_ms < 0) delay_ms = 0;
                                os_time_delay((os_int32)delay_ms);
                                if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                                os_mem_free(arg1);
                            }
                        }
                        gfa_value_push_long(rt, 0);
                        break;

                    /* RANDOMIZE - initialiser le generateur aleatoire */
                    case TOK_RANDOMIZE:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1) {
                                gfa_randomize((os_int32)gfa_value_to_long(arg1));
                                if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                                os_mem_free(arg1);
                            }
                        } else {
                            gfa_randomize(1);
                        }
                        gfa_value_push_long(rt, 0);
                        break;

                    /* Stubs: MOUSE, STICK/STRIG, PAD, LPEN, TOUCH, SPRITE, KEY* */
                    case TOK_MOUSE:     case TOK_MOUSEX:    case TOK_MOUSEY:
                    case TOK_MOUSEK:    case TOK_SETMOUSE:
                    case TOK_STICK:     case TOK_STRIG:     case TOK_PADX:
                    case TOK_PADY:      case TOK_PADT:      case TOK_LPENX:
                    case TOK_LPENY:     case TOK_TOUCH:
                    case TOK_STICK_TOK: case TOK_STRIG_TOK: case TOK_PAD_TOK:
                    case TOK_TOUCH_TOK: case TOK_LPEN_TOK:  case TOK_SPRITE:
                    case TOK_KEYDEF:    case TOK_KEYGET:    case TOK_KEYLOOK:
                    case TOK_KEYTEST:   case TOK_KEYPRESS:  case TOK_KEYPAD:
                        gfa_value_push_long(rt, 0);
                        break;

                    /* TIMER - nombre de ticks depuis boot (1 tick = 1/200s) */
                    case TOK_TIMER_TOK:
                        gfa_value_push_long(rt, os_time_ticks());
                        break;

                    /* DATE$ - date systeme */
                    case TOK_DATE_TOK:
                        {
                            const char *d = os_time_get_date(0);
                            gfa_value_push_string(rt, gfa_str_new(d ? d : ""), 1);
                        }
                        break;

                    /* TIME$ - heure systeme */
                    case TOK_TIME_TOK:
                        {
                            const char *t = os_time_get_time();
                            gfa_value_push_string(rt, gfa_str_new(t ? t : ""), 1);
                        }
                        break;

                    /* _C / _X / _Y - couleur courante, curseur X, curseur Y */
                    case TOK__C:
                        gfa_value_push_long(rt, (os_int32)rt->current_color);
                        break;
                    case TOK__X:
                        gfa_value_push_long(rt, (os_int32)rt->cursor_x);
                        break;
                    case TOK__Y:
                        gfa_value_push_long(rt, (os_int32)rt->cursor_y);
                        break;

                    /* BYTE{} / CARD{} / WORD{} / LONG{} / SINGLE{} / DOUBLE{} */
                    /* Typed memory access (PEEK-like) */
                    case TOK_BYTE_TOK:
                    case TOK_CARD:
                    case TOK_WORD_TOK:
                    case TOK_LONG_TOK:
                    case TOK_SINGLE:
                    case TOK_DOUBLE_TOK:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1) {
                                gfa_value_push_long(rt, 0);
                                if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                                os_mem_free(arg1);
                            } else {
                                gfa_value_push_long(rt, 0);
                            }
                        } else {
                            gfa_value_push_long(rt, 0);
                        }
                        break;

                    /* HIMEM / FRE() - memoire */
                    case TOK_HIMEM:
                        /* HIMEM retourne l'adresse haute memoire */
                        gfa_value_push_long(rt, (os_int32)(16 * 1024 * 1024));
                        break;
                    case TOK_FRE:
                        /* FRE() - stub: toujours retourne 256 Mo.
                         * FRE(-1) devrait forcer le garbage collector. */
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1) { if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s); os_mem_free(arg1); }
                        }
                        gfa_value_push_long(rt, (os_int32)(256 * 1024 * 1024));
                        break;

                    /* EXIST - teste existence fichier/repertoire */
                    case TOK_EXIST:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1) {
                                const char *name = (arg1->type == GFA_VAL_STRING && arg1->data.s) ? arg1->data.s : "";
                                result_l = (os_int32)os_fs_exist(name);
                                gfa_value_push_long(rt, result_l);
                                if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                                os_mem_free(arg1);
                            } else {
                                gfa_value_push_long(rt, 0);
                            }
                        } else {
                            gfa_value_push_long(rt, 0);
                        }
                        break;

                    /* SETTIME time$ [, date$] — règle date et heure système */
                    case TOK_SETTIME:
                        /* time$ est obligatoire, date$ optionnel sur la pile */
                        if (rt->sp >= 1) {
                            arg2 = gfa_value_pop(rt);
                            if (rt->sp >= 1) {
                                arg1 = gfa_value_pop(rt);
                            } else {
                                arg1 = arg2;
                                arg2 = NULL;
                            }
                            if (arg1) {
                                const char *time_str = (arg1->type == GFA_VAL_STRING && arg1->data.s) ? arg1->data.s : "";
                                const char *date_str = (arg2 && arg2->type == GFA_VAL_STRING && arg2->data.s) ? arg2->data.s : NULL;
                                result_l = (os_int32)os_time_set_datetime(time_str, date_str);
                                gfa_value_push_long(rt, result_l);
                                if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                                os_mem_free(arg1);
                                if (arg2) {
                                    if (arg2->owns_string && arg2->data.s) os_mem_free(arg2->data.s);
                                    os_mem_free(arg2);
                                }
                            } else {
                                if (arg2) {
                                    if (arg2->owns_string && arg2->data.s) os_mem_free(arg2->data.s);
                                    os_mem_free(arg2);
                                }
                                gfa_value_push_long(rt, 0);
                            }
                        } else {
                            gfa_value_push_long(rt, 0);
                        }
                        break;

                    /* ~ (tilde, NOT bitwise) */
                    case TOK_TILDE:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1) {
                                result_l = ~gfa_value_to_long(arg1);
                                gfa_value_push_long(rt, result_l);
                                if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                                os_mem_free(arg1);
                            }
                        }
                        break;

                    /* Stubs: VOID, STE/TT, OB_X/Y/W/H */
                    case TOK_VOID:
                    case TOK_STE:    /* 0 = ST, pas STE */
                    case TOK_TT:     /* 0 = pas TT/Falcon */
                    case TOK_OB_X:   case TOK_OB_Y:
                    case TOK_OB_W:   case TOK_OB_H:
                        gfa_value_push_long(rt, 0);
                        break;

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
                        if (s != NULL) {
                            os_con_output_string(s);
                            os_mem_free(s);
                        }
                    }
                    if (v1->owns_string && v1->data.s != NULL) {
                        os_mem_free(v1->data.s);
                    }
                }
                os_mem_free(v1);
            }
            break;

        case OP_PRINT_AT:
            /* Stack: [x] [y] [value] — top = value, then y, then x */
            if (rt->sp >= 3) {
                gfa_value *v_exp, *v_y, *v_x;
                v_exp = gfa_value_pop(rt);
                v_y   = gfa_value_pop(rt);
                v_x   = gfa_value_pop(rt);
                if (v_x != NULL && v_y != NULL) {
                    rt->cursor_x = (int)gfa_value_to_long(v_x);
                    rt->cursor_y = (int)gfa_value_to_long(v_y);
                    os_con_cursor_goto(rt->cursor_x, rt->cursor_y);
                }
                if (v_exp != NULL) {
                    if (v_exp->type == GFA_VAL_STRING) {
                        os_con_output_string(v_exp->data.s ? v_exp->data.s : "");
                    } else {
                        char *s;
                        s = gfa_str_float(gfa_value_to_float(v_exp));
                        if (s != NULL) {
                            os_con_output_string(s);
                            os_mem_free(s);
                        }
                    }
                }
                os_con_output_char('\n');
                if (v_x != NULL && v_x->owns_string && v_x->data.s != NULL) {
                    os_mem_free(v_x->data.s);
                }
                if (v_y != NULL && v_y->owns_string && v_y->data.s != NULL) {
                    os_mem_free(v_y->data.s);
                }
                if (v_exp != NULL && v_exp->owns_string && v_exp->data.s != NULL) {
                    os_mem_free(v_exp->data.s);
                }
                if (v_x)   os_mem_free(v_x);
                if (v_y)   os_mem_free(v_y);
                if (v_exp) os_mem_free(v_exp);
            }
            break;

        case OP_PRINT_USING:
            /* Stack: [format$] [value] — top = value, then format$ */
            if (rt->sp >= 2) {
                gfa_value *v_val, *v_fmt;
                v_val = gfa_value_pop(rt);
                v_fmt = gfa_value_pop(rt);
                if (v_fmt != NULL && v_val != NULL) {
                    const char *fmt_str;
                    char *formatted;
                    fmt_str = (v_fmt->type == GFA_VAL_STRING && v_fmt->data.s)
                              ? v_fmt->data.s : "";
                    formatted = format_using(fmt_str, gfa_value_to_float(v_val));
                    if (formatted != NULL) {
                        os_con_output_string(formatted);
                        os_mem_free(formatted);
                    }
                }
                os_con_output_char('\n');
                if (v_val != NULL && v_val->owns_string && v_val->data.s != NULL) {
                    os_mem_free(v_val->data.s);
                }
                if (v_fmt != NULL && v_fmt->owns_string && v_fmt->data.s != NULL) {
                    os_mem_free(v_fmt->data.s);
                }
                if (v_val) os_mem_free(v_val);
                if (v_fmt) os_mem_free(v_fmt);
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
                        if (s != NULL) {
                            gfa_print_channel(channel, s);
                            os_mem_free(s);
                        }
                    }
                    gfa_print_channel(channel, "\n");
                }
                if (v1 != NULL && v1->owns_string && v1->data.s != NULL) {
                    os_mem_free(v1->data.s);
                }
                if (chan_val != NULL && chan_val->owns_string && chan_val->data.s != NULL) {
                    os_mem_free(chan_val->data.s);
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

        case OP_OPENW:
            /* OPENW n : open window n (clear screen) */
            gfx_clear();
            gfx_update();
            break;

        case OP_CLOSEW:
            /* CLOSEW : no-op in SDL2 */
            break;

        case OP_COLOR:
            /* COLOR fg [, bg] : args on stack */
            if (rt->sp >= 1) {
                int fg, bg;
                v2 = NULL;
                v1 = gfa_value_pop(rt);
                fg = v1 ? (int)gfa_value_to_long(v1) : 1;
                if (rt->sp > 0) {
                    v2 = gfa_value_pop(rt);
                    bg = v2 ? (int)gfa_value_to_long(v2) : 0;
                } else bg = 0;
                gfx_color(fg, bg);
                gfx_update();
                rt->current_color = fg;
                if (v1) os_mem_free(v1);
                if (v2) os_mem_free(v2);
            }
            break;

        case OP_LINE_GFX:
        case OP_BOX_GFX:
        case OP_PBOX_GFX:
        case OP_CIRCLE_GFX:
            /* Graphics via SDL2 */
            {
                int x1, y1, x2, y2, r;
                gfa_value *v3, *v4;
                v1 = v2 = v3 = v4 = NULL;
                x1 = y1 = x2 = y2 = r = 0;
                if (inst->opcode == OP_CIRCLE_GFX) {
                    /* CIRCLE x,y,r [fill] */
                    if (rt->sp >= 3) {
                        v3 = gfa_value_pop(rt);
                        v2 = gfa_value_pop(rt);
                        v1 = gfa_value_pop(rt);
                        if (v1) x1 = (int)gfa_value_to_long(v1);
                        if (v2) y1 = (int)gfa_value_to_long(v2);
                        if (v3) r  = (int)gfa_value_to_long(v3);
                    }
                    if (rt->sp > 0) {
                        v4 = gfa_value_pop(rt);
                    }
                    if (r > 0) {
                        if (v4 && gfa_value_to_long(v4) != 0)
                            gfx_fill_circle(x1, y1, r);
                        else
                            gfx_circle(x1, y1, r);
                        gfx_update();
                    }
                } else {
                    if (rt->sp >= 4) {
                        v4 = gfa_value_pop(rt);
                        v3 = gfa_value_pop(rt);
                        v2 = gfa_value_pop(rt);
                        v1 = gfa_value_pop(rt);
                        if (v1) x1 = (int)gfa_value_to_long(v1);
                        if (v2) y1 = (int)gfa_value_to_long(v2);
                        if (v3) x2 = (int)gfa_value_to_long(v3);
                        if (v4) y2 = (int)gfa_value_to_long(v4);
                    }
                    if (inst->opcode == OP_LINE_GFX) {
                        gfx_line(x1, y1, x2, y2);
                    } else if (inst->opcode == OP_PBOX_GFX) {
                        gfx_fill_box(x1, y1, x2, y2);
                    } else {
                        gfx_box(x1, y1, x2, y2);
                    }
                    gfx_update();
                }
                if (v1) os_mem_free(v1);
                if (v2) os_mem_free(v2);
                if (v3) os_mem_free(v3);
                if (v4) os_mem_free(v4);
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
            /* STUB: POKE addr, byte - memory write not yet implemented */
            if (rt->sp >= 2) { v2 = gfa_value_pop(rt); v1 = gfa_value_pop(rt); if (v1) { if (v1->owns_string && v1->data.s) os_mem_free(v1->data.s); os_mem_free(v1); } if (v2) { if (v2->owns_string && v2->data.s) os_mem_free(v2->data.s); os_mem_free(v2); } }
            break;

        case OP_DPEEK:
            if (rt->sp >= 1) { v1 = gfa_value_pop(rt); if (v1) { gfa_value_push_long(rt, gfa_value_to_long(v1)); os_mem_free(v1); } }
            break;

        case OP_DPOKE:
            /* STUB: DPOKE addr, word - memory write not yet implemented */
            if (rt->sp >= 2) { v2 = gfa_value_pop(rt); v1 = gfa_value_pop(rt); if (v1) { if (v1->owns_string && v1->data.s) os_mem_free(v1->data.s); os_mem_free(v1); } if (v2) { if (v2->owns_string && v2->data.s) os_mem_free(v2->data.s); os_mem_free(v2); } }
            break;

        case OP_LPEEK:
            if (rt->sp >= 1) { v1 = gfa_value_pop(rt); if (v1) { gfa_value_push_long(rt, gfa_value_to_long(v1)); os_mem_free(v1); } }
            break;

        case OP_LPOKE:
            /* STUB: LPOKE addr, long - memory write not yet implemented */
            if (rt->sp >= 2) { v2 = gfa_value_pop(rt); v1 = gfa_value_pop(rt); if (v1) { if (v1->owns_string && v1->data.s) os_mem_free(v1->data.s); os_mem_free(v1); } if (v2) { if (v2->owns_string && v2->data.s) os_mem_free(v2->data.s); os_mem_free(v2); } }
            break;

        case OP_SPOKE:
            /* STUB: SPOKE addr, byte - memory write not yet implemented */
            if (rt->sp >= 2) { v2 = gfa_value_pop(rt); v1 = gfa_value_pop(rt); if (v1) { if (v1->owns_string && v1->data.s) os_mem_free(v1->data.s); os_mem_free(v1); } if (v2) { if (v2->owns_string && v2->data.s) os_mem_free(v2->data.s); os_mem_free(v2); } }
            break;

        case OP_SDPOKE:
            /* STUB: SDPOKE addr, word - memory write not yet implemented */
            if (rt->sp >= 2) { v2 = gfa_value_pop(rt); v1 = gfa_value_pop(rt); if (v1) { if (v1->owns_string && v1->data.s) os_mem_free(v1->data.s); os_mem_free(v1); } if (v2) { if (v2->owns_string && v2->data.s) os_mem_free(v2->data.s); os_mem_free(v2); } }
            break;

        case OP_SLPOKE:
            /* STUB: SLPOKE addr, long - memory write not yet implemented */
            if (rt->sp >= 2) { v2 = gfa_value_pop(rt); v1 = gfa_value_pop(rt); if (v1) { if (v1->owns_string && v1->data.s) os_mem_free(v1->data.s); os_mem_free(v1); } if (v2) { if (v2->owns_string && v2->data.s) os_mem_free(v2->data.s); os_mem_free(v2); } }
            break;

        case OP_ON_ERROR:
            /* operand = resolved IP address of error handler label */
            fprintf(stderr, "DEBUG OP_ON_ERROR: operand=%ld error_label=%d\n",
                    (long)operand, (int)operand);
            rt->error_label = (int)operand;
            rt->on_error_active = (operand >= 0) ? 1 : 0;
            gfa_on_error_gosub((int)operand);
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
            fprintf(stderr, "DEBUG OP_END at ip=%d, running=%d\n", rt->ip, rt->running);
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
                    int code = (int)gfa_value_to_long(v1);
                    os_mem_free(v1);
                    if (!runtime_error(rt, code, gfa_error_get_string(code))) {
                        return -1;
                    }
                    /* runtime_error jumped to handler, resume_ip is set */
                    return 0;
                }
            }
            break;

        case OP_FATAL:
            /* Stack: [error_code] ; raise fatal error (blocks RESUME) */
            if (rt->sp > 0) {
                v1 = gfa_value_pop(rt);
                if (v1) {
                    int code = (int)gfa_value_to_long(v1);
                    os_mem_free(v1);
                    rt->fatal_error = 1;
                    if (!runtime_error(rt, code, gfa_error_get_string(code))) {
                        return -1;
                    }
                    return 0;
                }
            }
            break;

        case OP_RESUME:
            /* operand: 0 = RESUME, 1 = RESUME NEXT */
            if (rt->fatal_error) {
                runtime_error(rt, 6, "RESUME after FATAL not allowed");
                return -1;
            }
            if (rt->resume_ip >= 0) {
                rt->ip = rt->resume_ip + ((operand != 0) ? 1 : 0);
                rt->error_code = 0;
                rt->fatal_error = 0;
                gfa_error_clear();
                return 0;
            }
            runtime_error(rt, 8, "RESUME without error handler");
            return -1;

        case OP_GEMDOS:
            /* Stack: [fn] [arg1] [arg2] ; call GEMDOS */
            if (rt->sp >= 3) {
                gfa_value *a3, *a2, *a1;
                os_int32 fn, arg1, arg2;
                a3 = gfa_value_pop(rt);
                a2 = gfa_value_pop(rt);
                a1 = gfa_value_pop(rt);
                if (a1 && a2 && a3) {
                    fn = gfa_value_to_long(a1);
                    arg1 = gfa_value_to_long(a2);
                    arg2 = gfa_value_to_long(a3);
                    gfa_value_push_long(rt, gfa_gemdos(fn, arg1, arg2));
                }
                if (a1) os_mem_free(a1);
                if (a2) os_mem_free(a2);
                if (a3) os_mem_free(a3);
            }
            break;

        case OP_BIOS:
            if (rt->sp >= 3) {
                gfa_value *a3, *a2, *a1;
                os_int32 fn, arg1, arg2;
                a3 = gfa_value_pop(rt);
                a2 = gfa_value_pop(rt);
                a1 = gfa_value_pop(rt);
                if (a1 && a2 && a3) {
                    fn = gfa_value_to_long(a1);
                    arg1 = gfa_value_to_long(a2);
                    arg2 = gfa_value_to_long(a3);
                    gfa_value_push_long(rt, gfa_bios(fn, arg1, arg2));
                }
                if (a1) os_mem_free(a1);
                if (a2) os_mem_free(a2);
                if (a3) os_mem_free(a3);
            }
            break;

        case OP_XBIOS:
            if (rt->sp >= 3) {
                gfa_value *a3, *a2, *a1;
                os_int32 fn, arg1, arg2;
                a3 = gfa_value_pop(rt);
                a2 = gfa_value_pop(rt);
                a1 = gfa_value_pop(rt);
                if (a1 && a2 && a3) {
                    fn = gfa_value_to_long(a1);
                    arg1 = gfa_value_to_long(a2);
                    arg2 = gfa_value_to_long(a3);
                    gfa_value_push_long(rt, gfa_xbios(fn, arg1, arg2));
                }
                if (a1) os_mem_free(a1);
                if (a2) os_mem_free(a2);
                if (a3) os_mem_free(a3);
            }
            break;

        case OP_ON_GOTO:
        case OP_ON_GOSUB:
            /* Stack: [label_count] [label_str_idx...] [index] */
            if (rt->sp >= 3) {
                gfa_value *idx_val, *cnt_val;
                int label_count, i, idx, target_str_idx, found;
                int label_str_indices[32];
                cnt_val = gfa_value_pop(rt);
                label_count = cnt_val ? (int)gfa_value_to_long(cnt_val) : 0;
                if (cnt_val) os_mem_free(cnt_val);
                if (label_count < 1 || label_count > 32) label_count = 0;
                for (i = 0; i < label_count && rt->sp > 0; i++) {
                    gfa_value *lv = gfa_value_pop(rt);
                    label_str_indices[i] = lv ? (int)gfa_value_to_long(lv) : -1;
                    if (lv) os_mem_free(lv);
                }
                idx_val = gfa_value_pop(rt);
                idx = idx_val ? (int)gfa_value_to_long(idx_val) : 0;
                if (idx_val) os_mem_free(idx_val);
                if (label_count > 0 && idx >= 1 && idx <= label_count) {
                    target_str_idx = label_str_indices[label_count - idx];
                    if (target_str_idx >= 0 && rt->program) {
                        found = 0;
                        for (i = 0; i < rt->program->length && !found; i++) {
                            if (rt->program->code[i].opcode == OP_LABEL
                                && rt->program->code[i].operand.int_val
                                   == (os_int32)target_str_idx) {
                                if (inst->opcode == OP_ON_GOSUB
                                    && rt->call_depth < GFA_MAX_CALL_DEPTH) {
                                    gfa_call_frame *f;
                                    f = &rt->call_stack[rt->call_depth++];
                                    f->return_ip = rt->ip + 1;
                                    f->return_sp = rt->sp;
                                    f->is_gosub  = 1;
                                    f->proc_index = 0;
                                    f->saved_count = 0;
                                }
                                rt->ip = i;
                                found = 1;
                            }
                        }
                    }
                }
            }
            break;

        case OP_LABEL:
        case OP_LINE_NUM:
            /* Marqueurs, ne rien faire */
            break;

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
static int runtime_error(gfa_runtime *rt, int code, const char *msg)
{
    if (rt == NULL) return 0;

    fprintf(stderr, "DEBUG runtime_error: code=%d on_error_active=%d error_label=%d fatal=%d\n",
            code, rt->on_error_active, rt->error_label, rt->fatal_error);

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
        fprintf(stderr, "DEBUG runtime_error: jumping to handler at ip=%d, resume_ip=%d\n",
                rt->error_label, rt->ip);

        /* Push a call frame so RETURN goes back */
        if (rt->call_depth < GFA_MAX_CALL_DEPTH) {
            frame = &rt->call_stack[rt->call_depth++];
            frame->return_ip = rt->ip + 1;
            frame->return_sp = rt->sp;
            frame->is_gosub  = 1;
            frame->proc_index = 0;
            frame->saved_count = 0;
            fprintf(stderr, "DEBUG runtime_error: pushed call frame, return_ip=%d\n", frame->return_ip);
        }

        /* Jump to error handler.
         * The caller (OP_ERROR etc.) returns 0, skipping post-switch rt->ip++,
         * so we set ip directly to the label address. */
        rt->ip = rt->error_label;
        fprintf(stderr, "DEBUG runtime_error: set ip=%d\n", rt->ip);

        return 1;  /* Successfully jumped to handler */
    }

    return 0;  /* No handler, program will stop */
}
