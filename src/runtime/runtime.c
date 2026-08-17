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

/* Etat de l'iteration de repertoire (FSFIRST/FSNEXT/FNAME/...) */
static os_file_info g_fs_info;
static int          g_fs_state = 0;  /* 0=aucune 1=en cours 2=terminée  */
static os_int32     g_fs_pos   = 0;

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

/*
 * gfa_keybuf_pop - Retire le premier code du tampon clavier emule.
 * Retourne 0 si le tampon est vide.
 */
static int gfa_keybuf_pop(gfa_runtime *rt)
{
    int code;
    int i;
    if (rt->keybuf_count <= 0) return 0;
    code = rt->keybuf[0];
    for (i = 1; i < rt->keybuf_count; i++) {
        rt->keybuf[i - 1] = rt->keybuf[i];
    }
    rt->keybuf_count--;
    return code;
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

        case OP_CALL_BUILTIN:
            /* Fonction integree : operand.int_val = type de token de la fonction.
               Les parametres sont sur la pile (le dernier push = dernier arg). */
            {
                gfa_token_type func_tok;
                gfa_value *arg1, *arg2, *arg3;
                double result_f;
                os_int32 result_l;
                char *result_s;
                int sp_before, argc_call, expected_sp;

                sp_before = rt->sp;
                argc_call = inst->has_operand2 ? inst->operand2.int_val2 : 0;

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
                    case TOK_VARIAT:
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
                                case TOK_VARIAT:
                                    {
                                        /* VARIAT(n) = n! ; VARIAT(n,k) = n!/(n-k)!
                                           arg1 deja depile = dernier argument */
                                        int n, k;
                                        if (argc_call >= 2) {
                                            gfa_value *an = gfa_value_pop(rt);
                                            k = (int)x;  /* arg1 = k */
                                            if (an) {
                                                n = (int)gfa_value_to_float(an);
                                                if (an->owns_string && an->data.s) os_mem_free(an->data.s);
                                                os_mem_free(an);
                                            } else {
                                                n = k;
                                            }
                                            result_f = gfa_variat(n, k);
                                        } else {
                                            result_f = gfa_variat((int)x, (int)x);
                                        }
                                    }
                                    break;
                                case TOK_CFLOAT: result_f = x; break;
                                case TOK_PRED:  result_f = (double)gfa_pred((os_int32)x); break;
                                case TOK_SUCC:  result_f = (double)gfa_succ((os_int32)x); break;
                                default: break;
                            }
                            gfa_value_push_float(rt, result_f);
                            os_mem_free(arg1);
                        }
                        break;

                    /* RAND(n) / RANDOM(n) : entier aleatoire dans [0, n-1].
                       Sans argument : entier aleatoire dans [0, 32767]. */
                    case TOK_RANDOM:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1) {
                                double n = gfa_value_to_float(arg1);
                                if (n > 1.0) {
                                    result_l = (os_int32)gfa_rnd(n);
                                } else {
                                    result_l = 0;
                                }
                                os_mem_free(arg1);
                            }
                        } else {
                            result_l = (os_int32)gfa_rnd(32768.0);
                        }
                        gfa_value_push_long(rt, result_l);
                        break;

                    /* Maths - 2 arguments */
                    case TOK_MIN: case TOK_MAX:
                    case TOK_COMBIN:
                        arg2 = gfa_value_pop(rt);
                        arg1 = gfa_value_pop(rt);
                        if (arg1 && arg2) {
                            double a = gfa_value_to_float(arg1);
                            double b = gfa_value_to_float(arg2);
                            switch (func_tok) {
                                case TOK_MIN: result_f = gfa_min(a, b); break;
                                case TOK_MAX: result_f = gfa_max(a, b); break;
                                case TOK_COMBIN: result_f = gfa_combin((int)a, (int)b); break;
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
                                case TOK_LEN:
                                    if (arg1->type == GFA_VAL_STRING &&
                                        arg1->str_len > 0)
                                        result_f = (double)arg1->str_len;
                                    else
                                        result_f = (double)gfa_len(s);
                                    gfa_value_push_float(rt, result_f);
                                    break;
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
                            gfa_value_push_string_len(rt, result_s, 2, 1);
                            if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                            os_mem_free(arg1);
                        }
                        break;

                    case TOK_MKL_TOK:
                        arg1 = gfa_value_pop(rt);
                        if (arg1) {
                            result_s = gfa_mkl(gfa_value_to_long(arg1));
                            gfa_value_push_string_len(rt, result_s, 4, 1);
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
                            gfa_value_push_string_len(rt, result_s, 6, 1);
                            if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                            os_mem_free(arg1);
                        }
                        break;

                    case TOK_MKD_TOK:
                        arg1 = gfa_value_pop(rt);
                        if (arg1) {
                            result_s = gfa_mkd(gfa_value_to_float(arg1));
                            gfa_value_push_string_len(rt, result_s, 8, 1);
                            if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                            os_mem_free(arg1);
                        }
                        break;

                    case TOK_CVI_TOK:
                        arg1 = gfa_value_pop(rt);
                        if (arg1) {
                            char tmp[2 + 1];
                            int bi;
                            os_int32 slen;
                            for (bi = 0; bi <= 2; bi++) tmp[bi] = 0;
                            slen = 0;
                            if (arg1->type == GFA_VAL_STRING &&
                                arg1->data.s) {
                                slen = (arg1->str_len > 0)
                                    ? arg1->str_len
                                    : (os_int32)strlen(arg1->data.s);
                                for (bi = 0; bi < 2 && bi < slen; bi++)
                                    tmp[bi] = arg1->data.s[bi];
                            } else {
                                tmp[0] = 0;
                            }
                            gfa_value_push_float(rt, (double)(int)gfa_cvi(tmp));
                            if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                            os_mem_free(arg1);
                        }
                        break;

                    case TOK_CVL_TOK:
                        arg1 = gfa_value_pop(rt);
                        if (arg1) {
                            char tmp[4 + 1];
                            int bi;
                            os_int32 slen;
                            for (bi = 0; bi <= 4; bi++) tmp[bi] = 0;
                            slen = 0;
                            if (arg1->type == GFA_VAL_STRING &&
                                arg1->data.s) {
                                slen = (arg1->str_len > 0)
                                    ? arg1->str_len
                                    : (os_int32)strlen(arg1->data.s);
                                for (bi = 0; bi < 4 && bi < slen; bi++)
                                    tmp[bi] = arg1->data.s[bi];
                            } else {
                                tmp[0] = 0;
                            }
                            gfa_value_push_long(rt, gfa_cvl(tmp));
                            if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                            os_mem_free(arg1);
                        }
                        break;

                    case TOK_CVS_TOK:
                        arg1 = gfa_value_pop(rt);
                        if (arg1) {
                            char tmp[6 + 1];
                            int bi;
                            os_int32 slen;
                            for (bi = 0; bi <= 6; bi++) tmp[bi] = 0;
                            slen = 0;
                            if (arg1->type == GFA_VAL_STRING &&
                                arg1->data.s) {
                                slen = (arg1->str_len > 0)
                                    ? arg1->str_len
                                    : (os_int32)strlen(arg1->data.s);
                                for (bi = 0; bi < 6 && bi < slen; bi++)
                                    tmp[bi] = arg1->data.s[bi];
                            } else {
                                tmp[0] = 0;
                            }
                            gfa_value_push_float(rt, gfa_cvs(tmp));
                            if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                            os_mem_free(arg1);
                        }
                        break;

                    case TOK_CVF_TOK:
                        arg1 = gfa_value_pop(rt);
                        if (arg1) {
                            char tmp[6 + 1];
                            int bi;
                            os_int32 slen;
                            for (bi = 0; bi <= 6; bi++) tmp[bi] = 0;
                            slen = 0;
                            if (arg1->type == GFA_VAL_STRING &&
                                arg1->data.s) {
                                slen = (arg1->str_len > 0)
                                    ? arg1->str_len
                                    : (os_int32)strlen(arg1->data.s);
                                for (bi = 0; bi < 6 && bi < slen; bi++)
                                    tmp[bi] = arg1->data.s[bi];
                            } else {
                                tmp[0] = 0;
                            }
                            gfa_value_push_float(rt, gfa_cvf(tmp));
                            if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                            os_mem_free(arg1);
                        }
                        break;

                    case TOK_CVD_TOK:
                        arg1 = gfa_value_pop(rt);
                        if (arg1) {
                            char tmp[8 + 1];
                            int bi;
                            os_int32 slen;
                            for (bi = 0; bi <= 8; bi++) tmp[bi] = 0;
                            slen = 0;
                            if (arg1->type == GFA_VAL_STRING &&
                                arg1->data.s) {
                                slen = (arg1->str_len > 0)
                                    ? arg1->str_len
                                    : (os_int32)strlen(arg1->data.s);
                                for (bi = 0; bi < 8 && bi < slen; bi++)
                                    tmp[bi] = arg1->data.s[bi];
                            } else {
                                tmp[0] = 0;
                            }
                            gfa_value_push_float(rt, gfa_cvd(tmp));
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
                                        if (nread < 0) nread = 0; /* EOF/erreur : chaine vide */
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
                        /* PAUSE delay (en 1/50s) : attend une touche pendant
                           delay ticks (20 ms/tick), renvoie le code ASCII de
                           la touche, ou 0 en cas de timeout. */
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1) {
                                int delay_ms = (int)(gfa_value_to_float(arg1) * 20.0);
                                int waited = 0;
                                int key = 0;
                                if (delay_ms < 0) delay_ms = 0;
                                /* Tampon emule (KEYPRESS) d'abord */
                                if (rt->keybuf_count > 0) {
                                    key = gfa_keybuf_pop(rt);
                                } else {
                                    while (waited < delay_ms && key == 0) {
                                        int c = os_con_input_key();
                                        if (c > 0) {
                                            key = c;
                                        } else {
                                            os_time_delay(10);
                                            waited += 10;
                                        }
                                    }
                                }
                                gfa_value_push_long(rt, (os_int32)key);
                                if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                                os_mem_free(arg1);
                            }
                        } else {
                            gfa_value_push_long(rt, 0);
                        }
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

                    /* MOUSE / JOYSTICK / PAD / PCE : pas de peripherique
                       en mode console — retourne 0 (stub honnete). */
                    case TOK_MOUSE:     case TOK_MOUSEX:    case TOK_MOUSEY:
                    case TOK_MOUSEK:    case TOK_SETMOUSE:
                    case TOK_STICK:     case TOK_STRIG:     case TOK_PADX:
                    case TOK_PADY:      case TOK_PADT:      case TOK_LPENX:
                    case TOK_LPENY:     case TOK_TOUCH:
                    case TOK_STICK_TOK: case TOK_STRIG_TOK: case TOK_PAD_TOK:
                    case TOK_TOUCH_TOK: case TOK_LPEN_TOK:  case TOK_SPRITE:
                        gfa_value_push_long(rt, 0);
                        break;

                    /* KEYDEF - redefinition de touche (inexploite en console) */
                    case TOK_KEYDEF:
                        if (rt->sp >= 2) {
                            arg2 = gfa_value_pop(rt);
                            arg1 = gfa_value_pop(rt);
                            if (arg1) { if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s); os_mem_free(arg1); }
                            if (arg2) { if (arg2->owns_string && arg2->data.s) os_mem_free(arg2->data.s); os_mem_free(arg2); }
                        } else if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1) { if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s); os_mem_free(arg1); }
                        }
                        gfa_value_push_long(rt, 0);
                        break;

                    /* KEYGET - lit et consomme la prochaine touche */
                    case TOK_KEYGET:
                        {
                            int key = gfa_keybuf_pop(rt);
                            if (key == 0) {
                                int c = os_con_input_key();
                                if (c > 0) key = c;
                            }
                            gfa_value_push_long(rt, (os_int32)key);
                        }
                        break;

                    /* CONIN - lit un caractere console sans attente.
                       Retourne la chaine ("" si aucun caractere). */
                    case TOK_CONIN:
                        {
                            int c = os_con_input_key();
                            if (c > 0) {
                                char s[2]; s[0] = (char)c; s[1] = '\0';
                                gfa_value_push_string(rt, gfa_str_new(s), 1);
                            } else {
                                gfa_value_push_string(rt, gfa_str_new(""), 1);
                            }
                        }
                        break;

                    /* CONOUT / CONOUTI - ecriture console sans saut de ligne */
                    case TOK_CONOUT:
                    case TOK_CONOUTI:
                        if (rt->sp >= 1) {
                            gfa_value *s = gfa_value_pop(rt);
                            if (s && s->type == GFA_VAL_STRING && s->data.s)
                                os_con_output_string(s->data.s);
                            if (s) {
                                if (s->owns_string && s->data.s) os_mem_free(s->data.s);
                                os_mem_free(s);
                            }
                        }
                        gfa_value_push_long(rt, 0);
                        break;

                    /* KEY n, chaine$ - definit un buffer de touches
                       (emule : conserve la derniere definition). */
                    case TOK_KEY:
                        {
                            gfa_value *v1 = 0, *v2 = 0;
                            if (rt->sp >= 2) {
                                v2 = gfa_value_pop(rt);
                                v1 = gfa_value_pop(rt);
                            } else if (rt->sp >= 1) {
                                v1 = gfa_value_pop(rt);
                            }
                            if (v1 && v2 && v2->type == GFA_VAL_STRING &&
                                v2->data.s)
                                os_con_output_string(v2->data.s);
                            if (v1) {
                                if (v1->owns_string && v1->data.s) os_mem_free(v1->data.s);
                                os_mem_free(v1);
                            }
                            if (v2) {
                                if (v2->owns_string && v2->data.s) os_mem_free(v2->data.s);
                                os_mem_free(v2);
                            }
                            gfa_value_push_long(rt, 0);
                        }
                        break;

                    /* ON KEY GOSUB n - activeur (emule : resultat 0) */
                    case TOK_ON_KEY:
                        if (rt->sp >= 1) {
                            gfa_value *k = gfa_value_pop(rt);
                            if (k) os_mem_free(k);
                        }
                        gfa_value_push_long(rt, 0);
                        break;

                    /* DMACONTROL / DMASOUND / SYSTEM - emules, resultat 0 */
                    case TOK_DMACONTROL:
                    case TOK_DMASOUND:
                    case TOK_SYSTEM:
                        if (rt->sp >= 1) {
                            gfa_value *sv = gfa_value_pop(rt);
                            if (sv) {
                                if (sv->owns_string && sv->data.s) os_mem_free(sv->data.s);
                                os_mem_free(sv);
                            }
                        }
                        gfa_value_push_long(rt, 0);
                        break;

                    /* KEYTEST - 1 si une touche est disponible (sans consommer) */
                    case TOK_KEYTEST:
                        gfa_value_push_long(rt,
                            (rt->keybuf_count > 0 || os_con_key_available()) ? 1 : 0);
                        break;

                    /* KEYLOOK - code de la touche en tete du tampon emule
                       (approximation console : pas de detection "maintenue") */
                    case TOK_KEYLOOK:
                        gfa_value_push_long(rt,
                            rt->keybuf_count > 0 ? (os_int32)rt->keybuf[0] : 0);
                        break;

                    /* KEYPRESS - simule une touche (pousse dans le tampon) */
                    case TOK_KEYPRESS:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1) {
                                int code;
                                if (arg1->type == GFA_VAL_STRING &&
                                    arg1->data.s && arg1->data.s[0]) {
                                    code = (unsigned char)arg1->data.s[0];
                                } else {
                                    code = (int)gfa_value_to_long(arg1);
                                }
                                if (rt->keybuf_count < 32) {
                                    rt->keybuf[rt->keybuf_count++] = code;
                                }
                                if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                                os_mem_free(arg1);
                            }
                        }
                        gfa_value_push_long(rt, 0);
                        break;

                    /* KEYPAD key - 1 si la touche donnee est en tete du tampon */
                    case TOK_KEYPAD:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1) {
                                int code = (int)gfa_value_to_long(arg1);
                                gfa_value_push_long(rt,
                                    (rt->keybuf_count > 0 && rt->keybuf[0] == code)
                                        ? 1 : 0);
                                if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                                os_mem_free(arg1);
                            } else {
                                gfa_value_push_long(rt, 0);
                            }
                        } else {
                            gfa_value_push_long(rt, 0);
                        }
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

                    /* ERR - code du dernier ERROR/FATAL declenche (0 sinon) */
                    case TOK_ERR:
                        gfa_value_push_long(rt, gfa_error_get());
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
                    /* Lecture memoire typée via vmem (big-endian 68000). */
                    case TOK_BYTE_TOK:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1) {
                                gfa_value_push_long(rt, (os_int32)vmem_read_byte((os_int32)gfa_value_to_long(arg1)));
                                if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                                os_mem_free(arg1);
                            }
                        }
                        break;
                    case TOK_CARD:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1) {
                                gfa_value_push_long(rt, (os_int32)vmem_read_card((os_int32)gfa_value_to_long(arg1)));
                                if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                                os_mem_free(arg1);
                            }
                        }
                        break;
                    case TOK_WORD_TOK:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1) {
                                gfa_value_push_long(rt, (os_int32)vmem_read_word((os_int32)gfa_value_to_long(arg1)));
                                if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                                os_mem_free(arg1);
                            }
                        }
                        break;
                    case TOK_LONG_TOK:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1) {
                                gfa_value_push_long(rt, vmem_read_long((os_int32)gfa_value_to_long(arg1)));
                                if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                                os_mem_free(arg1);
                            }
                        }
                        break;
                    case TOK_SINGLE:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1) {
                                gfa_value_push_float(rt, (double)vmem_read_single((os_int32)gfa_value_to_long(arg1)));
                                if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                                os_mem_free(arg1);
                            }
                        }
                        break;
                    case TOK_DOUBLE_TOK:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1) {
                                gfa_value_push_float(rt, vmem_read_double((os_int32)gfa_value_to_long(arg1)));
                                if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                                os_mem_free(arg1);
                            }
                        }
                        break;

                    /* PEEK / DPEEK / LPEEK en forme fonction (PEEK(addr)) */
                    case TOK_PEEK:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1) {
                                gfa_value_push_long(rt, (os_int32)vmem_read_byte((os_int32)gfa_value_to_long(arg1)));
                                if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                                os_mem_free(arg1);
                            }
                        }
                        break;
                    case TOK_DPEEK:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1) {
                                gfa_value_push_long(rt, (os_int32)vmem_read_card((os_int32)gfa_value_to_long(arg1)));
                                if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                                os_mem_free(arg1);
                            }
                        }
                        break;
                    case TOK_LPEEK:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1) {
                                gfa_value_push_long(rt, vmem_read_long((os_int32)gfa_value_to_long(arg1)));
                                if (arg1->owns_string && arg1->data.s) os_mem_free(arg1->data.s);
                                os_mem_free(arg1);
                            }
                        }
                        break;

                    /* HIMEM / FRE() - memoire */
                    case TOK_HIMEM:
                        /* HIMEM - taille de la region memoire emulee */
                        gfa_value_push_long(rt, vmem_size());
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
                                /* GFA : TRUE(-1) / FALSE(0) */
                                result_l = (os_int32)gfa_exist(name);
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

                    /* --- Fonctions fichiers (canal en argument) --- */
                    case TOK_EOF_TOK:
                        /* EOF(n) : fin de fichier canal n.
                           EOF sans arg : fin d'iteration FSFIRST. */
                        if (argc_call >= 1) {
                            arg1 = gfa_value_pop(rt);
                            result_l = (os_int32)gfa_eof(
                                (int)gfa_value_to_long(arg1));
                            gfa_value_push_long(rt, result_l);
                            if (arg1) os_mem_free(arg1);
                        } else {
                            gfa_value_push_long(rt,
                                g_fs_state != 1 ? 1 : 0);
                        }
                        break;
                    case TOK_LOF:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            result_l = (os_int32)gfa_lof(
                                (int)gfa_value_to_long(arg1));
                            gfa_value_push_long(rt, result_l);
                            if (arg1) os_mem_free(arg1);
                        } else {
                            gfa_value_push_long(rt, 0);
                        }
                        break;
                    case TOK_LOC:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            result_l = (os_int32)gfa_loc(
                                (int)gfa_value_to_long(arg1));
                            gfa_value_push_long(rt, result_l);
                            if (arg1) os_mem_free(arg1);
                        } else {
                            gfa_value_push_long(rt, 0);
                        }
                        break;

                    /* --- Instructions fichiers (statement) --- */
                    case TOK_KILL:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1 && arg1->type == GFA_VAL_STRING &&
                                arg1->data.s) {
                                gfa_kill(arg1->data.s);
                            }
                            if (arg1) os_mem_free(arg1);
                        }
                        gfa_value_push_long(rt, 0);
                        break;
                    case TOK_NAME:   /* NAME "a" AS "b" : pile [ancien][nouveau] */
                        if (rt->sp >= 2) {
                            arg2 = gfa_value_pop(rt);
                            arg1 = gfa_value_pop(rt);
                            if (arg1 && arg1->type == GFA_VAL_STRING &&
                                arg1->data.s &&
                                arg2 && arg2->type == GFA_VAL_STRING &&
                                arg2->data.s) {
                                gfa_name_file(arg1->data.s, arg2->data.s);
                            }
                            if (arg1) os_mem_free(arg1);
                            if (arg2) os_mem_free(arg2);
                        }
                        gfa_value_push_long(rt, 0);
                        break;
                    case TOK_MKDIR:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1 && arg1->type == GFA_VAL_STRING &&
                                arg1->data.s) {
                                os_dir_mkdir(arg1->data.s);
                            }
                            if (arg1) os_mem_free(arg1);
                        }
                        gfa_value_push_long(rt, 0);
                        break;
                    case TOK_RMDIR:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1 && arg1->type == GFA_VAL_STRING &&
                                arg1->data.s) {
                                os_dir_rmdir(arg1->data.s);
                            }
                            if (arg1) os_mem_free(arg1);
                        }
                        gfa_value_push_long(rt, 0);
                        break;
                    case TOK_CHDIR:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1 && arg1->type == GFA_VAL_STRING &&
                                arg1->data.s) {
                                os_dir_chdir(arg1->data.s);
                            }
                            if (arg1) os_mem_free(arg1);
                        }
                        gfa_value_push_long(rt, 0);
                        break;
                    case TOK_CHDRIVE:
                        /* Pas de changement de lecteur sur l'hote */
                        gfa_value_push_long(rt, 0);
                        break;
                    case TOK_FILES:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1 && arg1->type == GFA_VAL_STRING &&
                                arg1->data.s) {
                                os_file_info fi;
                                if (os_dir_first(arg1->data.s, 0, &fi)) {
                                    do {
                                        os_con_output_string(fi.name);
                                        os_con_output_string("\n");
                                    } while (os_dir_next(&fi));
                                }
                            }
                            if (arg1) os_mem_free(arg1);
                        }
                        gfa_value_push_long(rt, 0);
                        break;
                    case TOK_FGETDTA:
                    case TOK_FSETDTA:
                        gfa_value_push_long(rt, 0);
                        break;
                    case TOK_FSFIRST:
                        /* FSFIRST "masque" : demarre l'iteration. */
                        if (rt->sp >= 1) {
                            gfa_value *p;
                            int rc;
                            p = gfa_value_pop(rt);
                            if (p != NULL && p->type == GFA_VAL_STRING &&
                                p->data.s != NULL) {
                                rc = os_dir_first(p->data.s, 0,
                                                  &g_fs_info);
                                if (rc == 0) {
                                    g_fs_state = 1;
                                    g_fs_pos = 1;
                                } else {
                                    g_fs_state = 2;
                                    g_fs_pos = 0;
                                }
                            } else {
                                rc = OS_ERR_FILE_NOT_FOUND;
                                g_fs_state = 2;
                                g_fs_pos = 0;
                            }
                            gfa_value_push_long(rt, (os_int32)rc);
                            if (p != NULL) os_mem_free(p);
                        } else {
                            gfa_value_push_long(rt, 0);
                        }
                        break;
                    case TOK_FSNEXT:
                        if (g_fs_state == 1) {
                            int rc;
                            rc = os_dir_next(&g_fs_info);
                            if (rc == 0) {
                                g_fs_pos++;
                                gfa_value_push_long(rt, 0);
                            } else {
                                g_fs_state = 2;
                                gfa_value_push_long(rt, (os_int32)rc);
                            }
                        } else {
                            gfa_value_push_long(rt,
                                (os_int32)OS_ERR_NO_MORE_FILES);
                        }
                        break;
                    case TOK_FNAME:
                        /* FNAME() : nom du fichier courant (8.3). */
                        gfa_value_push_string(rt, gfa_str_new(
                            g_fs_state == 1 ? g_fs_info.name : ""), 1);
                        break;
                    case TOK_FATTR:
                        gfa_value_push_long(rt,
                            g_fs_state == 1 ?
                            (os_int32)g_fs_info.attr : 0);
                        break;
                    case TOK_FPOS:
                        gfa_value_push_long(rt,
                            g_fs_state == 1 ? g_fs_pos : 0);
                        break;
                    case TOK_SIZE_TOK:
                        /* SIZE() : taille du fichier courant. */
                        gfa_value_push_long(rt,
                            g_fs_state == 1 ? g_fs_info.size : 0);
                        break;
                    case TOK_SEEK:
                        if (rt->sp >= 2) {
                            arg2 = gfa_value_pop(rt);
                            arg1 = gfa_value_pop(rt);
                            result_l = (os_int32)gfa_seek(
                                (int)gfa_value_to_long(arg1),
                                (long)gfa_value_to_long(arg2));
                            gfa_value_push_long(rt, result_l);
                            if (arg1) os_mem_free(arg1);
                            if (arg2) os_mem_free(arg2);
                        } else {
                            gfa_value_push_long(rt, 0);
                        }
                        break;
                    case TOK_RELSEEK:
                        if (rt->sp >= 2) {
                            arg2 = gfa_value_pop(rt);
                            arg1 = gfa_value_pop(rt);
                            result_l = (os_int32)gfa_relseek(
                                (int)gfa_value_to_long(arg1),
                                (long)gfa_value_to_long(arg2));
                            gfa_value_push_long(rt, result_l);
                            if (arg1) os_mem_free(arg1);
                            if (arg2) os_mem_free(arg2);
                        } else {
                            gfa_value_push_long(rt, 0);
                        }
                        break;
                    case TOK_FIELD:
                        /* FIELD #n, len AS var$ : pile [var$][len][chan] */
                        if (rt->sp >= 3) {
                            char *fname;
                            arg3 = gfa_value_pop(rt);  /* var$ */
                            arg2 = gfa_value_pop(rt);  /* len  */
                            arg1 = gfa_value_pop(rt);  /* chan */
                            fname = (arg3 && arg3->type == GFA_VAL_STRING &&
                                     arg3->data.s) ? arg3->data.s : NULL;
                            if (fname != NULL) {
                                int fr, bsz;
                                char *buf;
                                gfa_variable *fv;
                                fr = gfa_field((int)gfa_value_to_long(arg1),
                                               (int)gfa_value_to_long(arg2),
                                               fname);
                                if (fr == 0) {
                                    buf = gfa_field_buffer(
                                        (int)gfa_value_to_long(arg1),
                                        &bsz);
                                    fv = gfa_var_lookup(rt->globals, fname);
                                    if (buf != NULL && fv != NULL &&
                                        fv->type == GFA_VAR_STRING &&
                                        bsz > 0) {
                                        char tmp[256];
                                        int ci;
                                        for (ci = 0; ci < bsz && ci < 255;
                                             ci++) {
                                            if (buf[ci] == '\0') break;
                                            tmp[ci] = buf[ci];
                                        }
                                        tmp[ci] = '\0';
                                        /* Supprimer les fins de ligne
                                           eventuelles (ecrites par
                                           PRINT #). */
                                        if (ci > 0 && tmp[ci - 1] == '\n')
                                            tmp[--ci] = '\0';
                                        if (ci > 0 && tmp[ci - 1] == '\r')
                                            tmp[--ci] = '\0';
                                        gfa_var_set_from_string(fv, tmp);
                                    }
                                }
                            }
                            if (arg1) os_mem_free(arg1);
                            if (arg2) os_mem_free(arg2);
                            if (arg3) os_mem_free(arg3);
                        }
                        gfa_value_push_long(rt, 0);
                        break;
                    case TOK_LSET:
                    case TOK_RSET:
                        /* LSET/RSET var$ = expr : assignation simple
                           (geree par le parser). Neant ici. */
                        while (rt->sp > 0) gfa_value_discard(rt, 1);
                        gfa_value_push_long(rt, 0);
                        break;
                    case TOK_GET:
                        /* GET #n[, pos] : lit un enregistrement dans le
                           buffer du canal (random ou sequentiel). */
                        {
                            long gpos = -1;
                            if (argc_call >= 2) {
                                arg2 = gfa_value_pop(rt);
                                gpos = (long)gfa_value_to_long(arg2);
                                if (arg2) os_mem_free(arg2);
                            }
                            if (rt->sp >= 1) {
                                arg1 = gfa_value_pop(rt);
                                result_l = (os_int32)gfa_get_channel(
                                    (int)gfa_value_to_long(arg1), gpos);
                                gfa_value_push_long(rt, result_l);
                                if (arg1) os_mem_free(arg1);
                            } else {
                                gfa_value_push_long(rt, -1);
                            }
                        }
                        break;
                    case TOK_PUT:
                        /* PUT #n[, pos] : ecrit le buffer du canal. */
                        {
                            long ppos = -1;
                            if (argc_call >= 2) {
                                arg2 = gfa_value_pop(rt);
                                ppos = (long)gfa_value_to_long(arg2);
                                if (arg2) os_mem_free(arg2);
                            }
                            if (rt->sp >= 1) {
                                arg1 = gfa_value_pop(rt);
                                result_l = (os_int32)gfa_put_channel(
                                    (int)gfa_value_to_long(arg1), ppos);
                                gfa_value_push_long(rt, result_l);
                                if (arg1) os_mem_free(arg1);
                            } else {
                                gfa_value_push_long(rt, -1);
                            }
                        }
                        break;
                    case TOK_SGET:
                        /* SGET #n : lit un octet dans la variable courante */
                        if (rt->sp >= 1) {
                            int ch_byte;
                            arg1 = gfa_value_pop(rt);
                            ch_byte = gfa_sget(
                                (int)gfa_value_to_long(arg1));
                            if (ch_byte >= 0 && rt->last_var != NULL &&
                                rt->last_var->type == GFA_VAR_FLOAT) {
                                rt->last_var->value.float_val =
                                    (double)ch_byte;
                            }
                            gfa_value_push_long(rt, (os_int32)ch_byte);
                            if (arg1) os_mem_free(arg1);
                        } else {
                            gfa_value_push_long(rt, -1);
                        }
                        break;
                    case TOK_SPUT:
                        /* SPUT #n, val : ecrit un octet */
                        if (rt->sp >= 2) {
                            arg2 = gfa_value_pop(rt);
                            arg1 = gfa_value_pop(rt);
                            gfa_sput((int)gfa_value_to_long(arg1),
                                     (int)gfa_value_to_long(arg2));
                            if (arg1) os_mem_free(arg1);
                            if (arg2) os_mem_free(arg2);
                        }
                        gfa_value_push_long(rt, 0);
                        break;

                    /* --- Memoire dynamique (vmem) --- */
                    case TOK_MALLOC:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            result_l = vmem_malloc(
                                (os_int32)gfa_value_to_long(arg1));
                            gfa_value_push_long(rt, result_l);
                            if (arg1) os_mem_free(arg1);
                        } else {
                            gfa_value_push_long(rt, 0);
                        }
                        break;
                    case TOK_MFREE:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            vmem_free((os_int32)gfa_value_to_long(arg1));
                            if (arg1) os_mem_free(arg1);
                        }
                        gfa_value_push_long(rt, 0);
                        break;
                    case TOK_BMOVE:
                        /* BMOVE dst, src, len */
                        if (rt->sp >= 3) {
                            arg3 = gfa_value_pop(rt);
                            arg2 = gfa_value_pop(rt);
                            arg1 = gfa_value_pop(rt);
                            vmem_copy((os_int32)gfa_value_to_long(arg1),
                                      (os_int32)gfa_value_to_long(arg2),
                                      (os_int32)gfa_value_to_long(arg3));
                            if (arg1) os_mem_free(arg1);
                            if (arg2) os_mem_free(arg2);
                            if (arg3) os_mem_free(arg3);
                        }
                        gfa_value_push_long(rt, 0);
                        break;

                    /* --- Console : position curseur --- */
                    case TOK_POS:
                        gfa_value_push_long(rt,
                            (os_int32)os_con_cursor_get_x());
                        break;
                    case TOK_CRSCOL:
                        gfa_value_push_long(rt,
                            (os_int32)os_con_cursor_get_x());
                        break;
                    case TOK_CRSLIN:
                        gfa_value_push_long(rt,
                            (os_int32)os_con_cursor_get_y());
                        break;
                    case TOK_VTAB:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            os_con_cursor_goto(1,
                                (int)gfa_value_to_long(arg1));
                            if (arg1) os_mem_free(arg1);
                        }
                        gfa_value_push_long(rt, 0);
                        break;
                    case TOK_HTAB:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            os_con_cursor_goto(
                                (int)gfa_value_to_long(arg1),
                                os_con_cursor_get_y());
                            if (arg1) os_mem_free(arg1);
                        }
                        gfa_value_push_long(rt, 0);
                        break;

                    /* --- Ports I/O --- */
                    case TOK_INP:
                        if (rt->sp >= 1) {
                            int port;
                            arg1 = gfa_value_pop(rt);
                            port = (int)gfa_value_to_long(arg1);
                            if (arg1) os_mem_free(arg1);
                            if (port == 28) {
                                int ch = os_con_input_key();
                                gfa_value_push_long(rt,
                                    (os_int32)(ch >= 0 ? ch : 0));
                            } else {
                                gfa_value_push_long(rt, 0);
                            }
                        } else {
                            gfa_value_push_long(rt, 0);
                        }
                        break;
                    case TOK_OUT:
                        /* OUT? port, val */
                        if (rt->sp >= 2) {
                            int port;
                            arg2 = gfa_value_pop(rt);
                            arg1 = gfa_value_pop(rt);
                            port = (int)gfa_value_to_long(arg1);
                            if (port == 28) {
                                os_con_output_char(
                                    (int)gfa_value_to_long(arg2));
                            } else if (port >= 0 && port <= 3) {
                                /* ports son : ignore (voir SOUND) */
                            }
                            if (arg1) os_mem_free(arg1);
                            if (arg2) os_mem_free(arg2);
                        }
                        gfa_value_push_long(rt, 0);
                        break;

                    /* --- UCASE$ (alias UPPER$) --- */
                    case TOK_UCASE:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1 && arg1->type == GFA_VAL_STRING &&
                                arg1->data.s) {
                                result_s = gfa_upper(arg1->data.s);
                                if (result_s)
                                    gfa_value_push_string(rt, result_s, 1);
                            }
                            if (arg1) os_mem_free(arg1);
                            if (!result_s) gfa_value_push_long(rt, 0);
                        } else {
                            gfa_value_push_long(rt, 0);
                        }
                        break;

                    /* --- Tableaux de controle VDI/AES (adresses vmem) --- */
                    case TOK_CONTRL:   /* = ADDRIN/ADDROUT (memes tokens) */
                        gfa_value_push_long(rt, 0x1000);
                        break;
                    case TOK_INTIN:
                        gfa_value_push_long(rt, 0x1200);
                        break;
                    case TOK_INTOUT:
                        gfa_value_push_long(rt, 0x1280);
                        break;
                    case TOK_PTSIN:
                        gfa_value_push_long(rt, 0x1300);
                        break;
                    case TOK_PTSOUT:
                        gfa_value_push_long(rt, 0x1380);
                        break;
                    case TOK_GINTIN:
                        gfa_value_push_long(rt, 0x1400);
                        break;
                    case TOK_GINTOUT:
                        gfa_value_push_long(rt, 0x1480);
                        break;
                    case TOK_WORK_OUT:
                        gfa_value_push_long(rt, 0x1500);
                        break;

                    /* --- POINT / PTST : lecture pixel --- */
                    case TOK_POINT:
                    case TOK_PTST:
                        if (rt->sp >= 2) {
                            arg2 = gfa_value_pop(rt);
                            arg1 = gfa_value_pop(rt);
                            result_l = (os_int32)gfx_get_pixel(
                                (int)gfa_value_to_long(arg1),
                                (int)gfa_value_to_long(arg2));
                            gfa_value_push_long(rt, result_l);
                            if (arg1) os_mem_free(arg1);
                            if (arg2) os_mem_free(arg2);
                        } else {
                            gfa_value_push_long(rt, 0);
                        }
                        break;

                    /* --- INSERT(a$, b$) : insertion de b$ au centre de a$ --- */
                    case TOK_INSERT:
                        if (rt->sp >= 2) {
                            arg2 = gfa_value_pop(rt);  /* b$ */
                            arg1 = gfa_value_pop(rt);  /* a$ */
                            {
                                const char *sa = (arg1 &&
                                    arg1->type == GFA_VAL_STRING &&
                                    arg1->data.s) ? arg1->data.s : "";
                                const char *sb = (arg2 &&
                                    arg2->type == GFA_VAL_STRING &&
                                    arg2->data.s) ? arg2->data.s : "";
                                int la = (int)strlen(sa);
                                int lb = (int)strlen(sb);
                                int mid = la / 2;
                                int il;
                                result_s = (char *)os_mem_alloc(
                                    (size_t)(la + lb) + 1);
                                if (result_s != NULL) {
                                    for (il = 0; il < mid; il++)
                                        result_s[il] = sa[il];
                                    memcpy(result_s + mid, sb, (size_t)lb);
                                    memcpy(result_s + mid + lb, sa + mid,
                                           (size_t)(la - mid));
                                    result_s[la + lb] = '\0';
                                }
                            }
                            if (arg1) os_mem_free(arg1);
                            if (arg2) os_mem_free(arg2);
                            if (result_s) {
                                gfa_value_push_string(rt, result_s, 1);
                            } else {
                                gfa_value_push_long(rt, 0);
                            }
                        } else {
                            gfa_value_push_long(rt, 0);
                        }
                        break;

                    /* --- Shell (emulation hote) --- */
                    case TOK_SHEL_ENVRN:
                        if (rt->sp >= 1) {
                            arg1 = gfa_value_pop(rt);
                            if (arg1 && arg1->type == GFA_VAL_STRING &&
                                arg1->data.s) {
                                const char *env = os_get_env(arg1->data.s);
                                if (env != NULL) {
                                    result_s = gfa_str_new(env);
                                    if (result_s)
                                        gfa_value_push_string(rt, result_s, 1);
                                }
                            }
                            if (arg1) os_mem_free(arg1);
                            if (!result_s) gfa_value_push_long(rt, 0);
                        } else {
                            gfa_value_push_long(rt, 0);
                        }
                        break;
                    case TOK_SHEL_FIND:
                        gfa_value_push_long(rt, 1); /* le shell hote existe */
                        break;
                    case TOK_SHEL_READ:
                    case TOK_SHEL_WRITE:
                    case TOK_SHEL_GET:
                    case TOK_SHEL_PUT:
                        gfa_value_push_long(rt, 0);
                        break;

                    /* --- WAVE : son avec enveloppe (hote) --- */
                    case TOK_WAVE:
                        /* WAVE ch, env, forme, periode, duree, freq */
                        if (rt->sp >= 6) {
                            arg3 = gfa_value_pop(rt);   /* freq   */
                            {
                                gfa_value *dv = gfa_value_pop(rt);  /* duree  */
                                gfa_value *pv = gfa_value_pop(rt);  /* periode*/
                                gfa_value *fv = gfa_value_pop(rt);  /* forme  */
                                gfa_value *ev = gfa_value_pop(rt);  /* env    */
                                gfa_value *cv = gfa_value_pop(rt);  /* canal  */
                                (void)pv; (void)fv;
                                gfa_sound(
                                    cv ? (int)gfa_value_to_long(cv) : 0,
                                    arg3 ? (int)gfa_value_to_long(arg3) : 0,
                                    dv ? (int)(gfa_value_to_float(dv) * 1000.0) : 0,
                                    50,
                                    ev ? (int)gfa_value_to_long(ev) : 0);
                                if (dv) os_mem_free(dv);
                                if (pv) os_mem_free(pv);
                                if (fv) os_mem_free(fv);
                                if (ev) os_mem_free(ev);
                                if (cv) os_mem_free(cv);
                            }
                            if (arg3) os_mem_free(arg3);
                        }
                        gfa_value_push_long(rt, 0);
                        break;

                    /* --- GEMSYS / VDISYS : appels generiques (stubs hote) --- */
                    case TOK_GEMSYS:
                    case TOK_VDISYS:
                        gfa_value_push_long(rt, 0);
                        break;

                    /* --- Stubs AES sans peripherique (retournent 0) --- */
                    case TOK_APPL_FIND:
                    case TOK_APPL_READ:
                    case TOK_APPL_WRITE:
                    case TOK_APPL_TPLAY:
                    case TOK_APPL_TRECORD:
                    case TOK_FORM_BUTTON:
                    case TOK_FORM_CENTER:
                    case TOK_FORM_DIAL:
                    case TOK_FORM_DO:
                    case TOK_FORM_ERROR:
                    case TOK_FORM_KEYBD:
                    case TOK_FORM_INPUT:
                    case TOK_MENU:
                    case TOK_MENU_KILL:
                    case TOK_MENU_OFF:
                    case TOK_MENU_ICHECK:
                    case TOK_MENU_IENABLE:
                    case TOK_MENU_REGISTER:
                    case TOK_MENU_TEXT:
                    case TOK_MENU_TNORMAL:
                    case TOK_WIND_DELETE:
                    case TOK_WIND_FIND:
                    case TOK_WIND_CREATE:
                    case TOK_WIND_CALC:
                    case TOK_WIND_GET:
                    case TOK_WIND_SET:
                    case TOK_WIND_UPDATE:
                    case TOK_EVNT_MULTI:
                    case TOK_EVNT_MESAG:
                    case TOK_EVNT_BUTTON:
                    case TOK_EVNT_TIMER:
                    case TOK_EVNT_DCLICK:
                    case TOK_RSRC_LOAD:
                    case TOK_RSRC_FREE:
                    case TOK_RSRC_GADDR:
                    case TOK_RSRC_SADDR:
                    case TOK_RSRC_OBFIX:
                    case TOK_RCALL:
                    case TOK_RC_COPY:
                    case TOK_RC_INTERSECT:
                    case TOK_OBJC_ADD:
                    case TOK_OBJC_CHANGE:
                    case TOK_OBJC_DRAW:
                    case TOK_OBJC_DELETE:
                    case TOK_OBJC_ADDMOVE:
                    case TOK_OBJC_MOVE:
                    case TOK_OBJC_FIND:
                    case TOK_OBJC_OFFSET:
                    case TOK_OBJC_PICK:
                    case TOK_OBJC_STATE:
                    case TOK_OBJC_EDIT:
                        gfa_value_push_long(rt, 0);
                        break;

                    /* Non implemente */
                    default:
                        gfa_value_push_long(rt, 0);
                        break;
                }

                /* Garantie : le built-in a consomme exactement argc_call
                   arguments et laisse exactement UN resultat sur la pile
                   (certains cas ne popent pas leurs args ou ne pushent rien). */
                if (inst->has_operand2) {
                    expected_sp = sp_before - argc_call + 1;
                    while (rt->sp > expected_sp) gfa_value_discard(rt, 1);
                    if (rt->sp < expected_sp) gfa_value_push_long(rt, 0);
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
            /* Pile: [filename$] [addr] ; charge le fichier en memoire
               virtuelle a l'adresse donnee (defaut 32768 = $8000). */
            if (rt->sp >= 2) {
                gfa_value *ad;
                gfa_value *fn;
                static char bload_buf[65536];
                os_file_handle fh;
                os_int32 size, nread, addr, i;
                ad = gfa_value_pop(rt);
                fn = gfa_value_pop(rt);
                addr = (os_int32)gfa_value_to_long(ad);
                if (addr == 0) addr = 32768;
                if (fn != NULL && fn->type == GFA_VAL_STRING && fn->data.s) {
                    fh = os_file_open(fn->data.s, 'I', 0);
                    if (fh != NULL) {
                        size = os_file_size(fh);
                        if (size > 65536) size = 65536;
                        if (size < 0) size = 0;
                        nread = os_file_read(fh, bload_buf, size);
                        os_file_close(fh);
                        if (nread < 0) nread = 0;
                        if (vmem_addr_valid(addr)) {
                            os_int32 vsize = vmem_size();
                            if (addr + nread > vsize) nread = vsize - addr;
                            for (i = 0; i < nread; i++)
                                vmem_write_byte((os_int32)(addr + i),
                                                (unsigned char)bload_buf[i]);
                        } else {
                            nread = 0;
                        }
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
            /* Pile: [filename$] [debut] [fin] ; sauve la memoire
               virtuelle [debut..fin[ dans le fichier. */
            if (rt->sp >= 3) {
                gfa_value *en;
                gfa_value *st;
                gfa_value *fn;
                static char bsave_buf[65536];
                os_file_handle fh;
                os_int32 start, end, len, written, i;
                en = gfa_value_pop(rt);
                st = gfa_value_pop(rt);
                fn = gfa_value_pop(rt);
                if (fn != NULL && st != NULL && en != NULL &&
                    fn->type == GFA_VAL_STRING && fn->data.s) {
                    start = (os_int32)gfa_value_to_long(st);
                    end   = (os_int32)gfa_value_to_long(en);
                    if (end < start) { os_int32 tmp = start; start = end; end = tmp; }
                    len = end - start;
                    if (len > 65536) len = 65536;
                    if (vmem_addr_valid(start)) {
                        os_int32 vsize = vmem_size();
                        if (start + len > vsize) len = vsize - start;
                        for (i = 0; i < len; i++)
                            bsave_buf[i] =
                                (char)vmem_read_byte((os_int32)(start + i));
                    } else {
                        len = 0;
                    }
                    fh = os_file_open(fn->data.s, 'O', 0);
                    if (fh != NULL && len > 0) {
                        written = os_file_write(fh, bsave_buf, len);
                        os_file_close(fh);
                        gfa_value_push_long(rt, (os_int32)written);
                    } else {
                        if (fh) os_file_close(fh);
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
            /* Pile: [canal] [addr] [count] ; peripherique -> vmem */
            if (rt->sp >= 3) {
                gfa_value *co, *ad, *ch;
                int chan, count, n;
                static char bget_buf[4096];
                os_int32 addr, i;
                co = gfa_value_pop(rt);
                ad = gfa_value_pop(rt);
                ch = gfa_value_pop(rt);
                if (ch != NULL && ad != NULL && co != NULL) {
                    chan  = (int)gfa_value_to_long(ch);
                    count = (int)gfa_value_to_long(co);
                    addr  = (os_int32)gfa_value_to_long(ad);
                    if (count > 4096) count = 4096;
                    n = gfa_bget(chan, bget_buf, count);
                    if (n > 0 && vmem_addr_valid(addr)) {
                        os_int32 vsize = vmem_size();
                        if (addr + n > vsize) n = vsize - addr;
                        for (i = 0; i < n; i++)
                            vmem_write_byte((os_int32)(addr + i),
                                            (unsigned char)bget_buf[i]);
                    }
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
            /* Pile: [canal] [addr] [count] ; vmem -> peripherique */
            if (rt->sp >= 3) {
                gfa_value *co, *ad, *ch;
                int chan, count, n;
                static char bput_buf[4096];
                os_int32 addr, i;
                co = gfa_value_pop(rt);
                ad = gfa_value_pop(rt);
                ch = gfa_value_pop(rt);
                if (ch != NULL && ad != NULL && co != NULL) {
                    chan  = (int)gfa_value_to_long(ch);
                    count = (int)gfa_value_to_long(co);
                    addr  = (os_int32)gfa_value_to_long(ad);
                    if (count > 4096) count = 4096;
                    if (vmem_addr_valid(addr)) {
                        os_int32 vsize = vmem_size();
                        if (addr + count > vsize) count = vsize - addr;
                        for (i = 0; i < count; i++)
                            bput_buf[i] =
                                (char)vmem_read_byte((os_int32)(addr + i));
                        n = gfa_bput(chan, bput_buf, count);
                    } else {
                        n = -1;
                    }
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
            /* CLOSEW : no-op (pas de gestionnaire de fenetres) */
            break;

        case OP_COLOR:
            /* COLOR fg [, bg] : pile [fg][bg] (bg en haut) */
            if (rt->sp >= 1) {
                int fg, bg;
                v2 = NULL;
                v1 = NULL;
                if (rt->sp >= 2) {
                    v2 = gfa_value_pop(rt);  /* bg */
                    v1 = gfa_value_pop(rt);  /* fg */
                } else {
                    v1 = gfa_value_pop(rt);  /* fg seul */
                }
                fg = v1 ? (int)gfa_value_to_long(v1) : 1;
                bg = v2 ? (int)gfa_value_to_long(v2) : 0;
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
            /* Graphics via gfx.c (framebuffer ANSI C89) */
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

        case OP_WINDOW_GFX:
            /* WINDOW (x0,y0), (x1,y1) : pile [x0][y0][x1][y1] */
            {
                gfa_value *wv1, *wv2, *wv3, *wv4;
                wv1 = wv2 = wv3 = wv4 = NULL;
                if (rt->sp >= 4) {
                    wv4 = gfa_value_pop(rt);
                    wv3 = gfa_value_pop(rt);
                    wv2 = gfa_value_pop(rt);
                    wv1 = gfa_value_pop(rt);
                    gfx_window((int)gfa_value_to_long(wv1),
                               (int)gfa_value_to_long(wv2),
                               (int)gfa_value_to_long(wv3),
                               (int)gfa_value_to_long(wv4));
                    gfx_update();
                }
                if (wv1) os_mem_free(wv1);
                if (wv2) os_mem_free(wv2);
                if (wv3) os_mem_free(wv3);
                if (wv4) os_mem_free(wv4);
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

        case OP_LINE_INPUT_FILE:
            /* Pile: [canal] ; lit une ligne depuis le canal fichier. */
            {
                gfa_variable *vv = (gfa_variable *)inst->operand.ptr_val;
                char *ln;
                if (vv != NULL && rt->sp >= 1) {
                    gfa_value *cv = gfa_value_pop(rt);
                    int ch = (int)gfa_value_to_long(cv);
                    if (cv) os_mem_free(cv);
                    ln = gfa_line_input_channel(ch);
                    if (ln != NULL) {
                        gfa_var_set_from_string(vv, ln);
                        os_mem_free(ln);
                    }
                } else {
                    while (rt->sp > 0) gfa_value_discard(rt, 1);
                }
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
            if (rt->sp >= 1) {
                v1 = gfa_value_pop(rt);
                if (v1) {
                    gfa_value_push_long(rt, (os_int32)vmem_read_byte((os_int32)gfa_value_to_long(v1)));
                    os_mem_free(v1);
                }
            }
            break;

        case OP_POKE:
            /* PEEK/POKE emule la memoire physique ST via vmem (big-endian). */
            if (rt->sp >= 2) {
                v2 = gfa_value_pop(rt); /* valeur (top) */
                v1 = gfa_value_pop(rt); /* adresse */
                if (v1 && v2) {
                    vmem_write_byte((os_int32)gfa_value_to_long(v1),
                                    (unsigned char)gfa_value_to_long(v2));
                }
                if (v1) { if (v1->owns_string && v1->data.s) os_mem_free(v1->data.s); os_mem_free(v1); }
                if (v2) { if (v2->owns_string && v2->data.s) os_mem_free(v2->data.s); os_mem_free(v2); }
            }
            break;

        case OP_DPEEK:
            if (rt->sp >= 1) {
                v1 = gfa_value_pop(rt);
                if (v1) {
                    gfa_value_push_long(rt, (os_int32)vmem_read_card((os_int32)gfa_value_to_long(v1)));
                    os_mem_free(v1);
                }
            }
            break;

        case OP_DPOKE:
            if (rt->sp >= 2) {
                v2 = gfa_value_pop(rt);
                v1 = gfa_value_pop(rt);
                if (v1 && v2) {
                    vmem_write_word((os_int32)gfa_value_to_long(v1),
                                    (unsigned short)gfa_value_to_long(v2));
                }
                if (v1) { if (v1->owns_string && v1->data.s) os_mem_free(v1->data.s); os_mem_free(v1); }
                if (v2) { if (v2->owns_string && v2->data.s) os_mem_free(v2->data.s); os_mem_free(v2); }
            }
            break;

        case OP_LPEEK:
            if (rt->sp >= 1) {
                v1 = gfa_value_pop(rt);
                if (v1) {
                    gfa_value_push_long(rt, vmem_read_long((os_int32)gfa_value_to_long(v1)));
                    os_mem_free(v1);
                }
            }
            break;

        case OP_LPOKE:
            if (rt->sp >= 2) {
                v2 = gfa_value_pop(rt);
                v1 = gfa_value_pop(rt);
                if (v1 && v2) {
                    vmem_write_long((os_int32)gfa_value_to_long(v1),
                                    (os_int32)gfa_value_to_long(v2));
                }
                if (v1) { if (v1->owns_string && v1->data.s) os_mem_free(v1->data.s); os_mem_free(v1); }
                if (v2) { if (v2->owns_string && v2->data.s) os_mem_free(v2->data.s); os_mem_free(v2); }
            }
            break;

        case OP_SPOKE:
            /* SPOKE = POKE (variante GFA) */
            if (rt->sp >= 2) {
                v2 = gfa_value_pop(rt);
                v1 = gfa_value_pop(rt);
                if (v1 && v2) {
                    vmem_write_byte((os_int32)gfa_value_to_long(v1),
                                    (unsigned char)gfa_value_to_long(v2));
                }
                if (v1) { if (v1->owns_string && v1->data.s) os_mem_free(v1->data.s); os_mem_free(v1); }
                if (v2) { if (v2->owns_string && v2->data.s) os_mem_free(v2->data.s); os_mem_free(v2); }
            }
            break;

        case OP_SDPOKE:
            /* SDPOKE = DPOKE (variante GFA) */
            if (rt->sp >= 2) {
                v2 = gfa_value_pop(rt);
                v1 = gfa_value_pop(rt);
                if (v1 && v2) {
                    vmem_write_word((os_int32)gfa_value_to_long(v1),
                                    (unsigned short)gfa_value_to_long(v2));
                }
                if (v1) { if (v1->owns_string && v1->data.s) os_mem_free(v1->data.s); os_mem_free(v1); }
                if (v2) { if (v2->owns_string && v2->data.s) os_mem_free(v2->data.s); os_mem_free(v2); }
            }
            break;

        case OP_SLPOKE:
            /* SLPOKE = LPOKE (variante GFA) */
            if (rt->sp >= 2) {
                v2 = gfa_value_pop(rt);
                v1 = gfa_value_pop(rt);
                if (v1 && v2) {
                    vmem_write_long((os_int32)gfa_value_to_long(v1),
                                    (os_int32)gfa_value_to_long(v2));
                }
                if (v1) { if (v1->owns_string && v1->data.s) os_mem_free(v1->data.s); os_mem_free(v1); }
                if (v2) { if (v2->owns_string && v2->data.s) os_mem_free(v2->data.s); os_mem_free(v2); }
            }
            break;

        case OP_ON_ERROR:
            /* operand = IP resolu (si >= 0) ; sinon operand2.int_val2
               = index de chaine du label, resolu ici par scan des OP_LABEL. */
            {
                int str_idx = -1;
                int label_ip = -1;
                int i;

                if (operand >= 0) {
                    label_ip = operand;
                } else if (inst->has_operand2
                           && inst->operand2.int_val2 >= 0
                           && rt->program) {
                    str_idx = inst->operand2.int_val2;
                    {
                        const char *target =
                            (str_idx >= 0 && str_idx < rt->program->str_count)
                                ? rt->program->strings[str_idx] : NULL;
                        for (i = 0; i < rt->program->length; i++) {
                            const char *lbl = NULL;
                            int s2 = rt->program->code[i].operand.int_val;
                            if (rt->program->code[i].opcode != OP_LABEL)
                                continue;
                            if (s2 >= 0 && s2 < rt->program->str_count)
                                lbl = rt->program->strings[s2];
                            if (target != NULL && lbl != NULL
                                && (s2 == str_idx
                                    || os_str_iequal(target, lbl))) {
                                label_ip = i;
                                break;
                            }
                        }
                    }
                }
                rt->error_label = label_ip;
                rt->on_error_active = (label_ip >= 0) ? 1 : 0;
                gfa_on_error_gosub(label_ip);
            }
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

        /* ---------------------------------------------------------- */
        /* Tableaux : ERASE / CLEAR / tri / INSERT / DELETE           */
        /* ---------------------------------------------------------- */

        case OP_ERASE_VAR:
            {
                gfa_variable *ev = (gfa_variable *)inst->operand.ptr_val;
                if (ev != NULL && ev->type == GFA_VAR_ARRAY) {
                    if (ev->value.arr.data != NULL) {
                        os_mem_free(ev->value.arr.data);
                        ev->value.arr.data = NULL;
                    }
                    if (ev->value.arr.dim_sizes != NULL) {
                        os_mem_free(ev->value.arr.dim_sizes);
                        ev->value.arr.dim_sizes = NULL;
                    }
                    ev->value.arr.num_dims = 0;
                    ev->value.arr.total_elements = 0;
                    ev->value.arr.is_matrix = 0;
                }
            }
            break;

        case OP_ARRAYFILL:
            {
                gfa_variable *av = (gfa_variable *)inst->operand.ptr_val;
                gfa_value *fv = (rt->sp >= 1) ? gfa_value_pop(rt) : NULL;
                if (av != NULL && av->type == GFA_VAR_ARRAY
                    && av->value.arr.data != NULL) {
                    double *dp = (double *)av->value.arr.data;
                    int i;
                    int n = (int)av->value.arr.total_elements;
                    double val = (fv != NULL) ? gfa_value_to_float(fv) : 0.0;
                    for (i = 0; i < n; i++) dp[i] = val;
                }
                if (fv) {
                    if (fv->owns_string && fv->data.s) os_mem_free(fv->data.s);
                    os_mem_free(fv);
                }
            }
            break;

        case OP_DIM_QUESTION:
            {
                gfa_variable *dv = (gfa_variable *)inst->operand.ptr_val;
                char buf[64];
                const char *out = "0";
                if (dv != NULL && dv->type == GFA_VAR_ARRAY
                    && dv->value.arr.data != NULL) {
                    int i;
                    int pos = 0;
                    buf[0] = '\0';
                    for (i = 0; i < dv->value.arr.num_dims && i < 7; i++) {
                        char part[16];
                        sprintf(part, "%d", (int)dv->value.arr.dim_sizes[i]);
                        if (i > 0) {
                            if (pos < 50) { buf[pos++] = ','; buf[pos] = '\0'; }
                        }
                        {
                            int pl = (int)strlen(part);
                            if (pos + pl < 63) {
                                os_mem_copy(&buf[pos], part, (size_t)pl);
                                pos += pl;
                                buf[pos] = '\0';
                            }
                        }
                    }
                    out = buf;
                }
                gfa_value_push_string(rt, gfa_str_new(out), 1);
            }
            break;

        case OP_CLEAR_ALL:
            {
                int bi, bj;
                /* Reset des valeurs en place (les pointeurs du codegen
                   restent valides). */
                for (bi = 0; bi < rt->globals->num_buckets; bi++) {
                    gfa_variable *gv = rt->globals->buckets[bi];
                    for (bj = 0; gv != NULL && bj < 4096; bj++, gv = gv->next) {
                        if (gv->is_reserved) continue;
                        switch (gv->type) {
                            case GFA_VAR_BOOL:  gv->value.bool_val = 0; break;
                            case GFA_VAR_BYTE:  gv->value.byte_val = 0; break;
                            case GFA_VAR_WORD:  gv->value.word_val = 0; break;
                            case GFA_VAR_LONG:  gv->value.long_val = 0; break;
                            case GFA_VAR_FLOAT: gv->value.float_val = 0.0; break;
                            case GFA_VAR_STRING:
                                if (gv->value.str.data != NULL) {
                                    gv->value.str.data[0] = '\0';
                                    gv->value.str.length = 0;
                                }
                                break;
                            case GFA_VAR_ARRAY:
                                if (gv->value.arr.data != NULL)
                                    os_mem_set(gv->value.arr.data, 0,
                                        (size_t)gv->value.arr.total_elements *
                                        (size_t)gv->value.arr.element_size);
                                break;
                            default: break;
                        }
                    }
                }
                rt->data_ptr = 0; /* CLEAR reinitialise aussi DATA */
            }
            break;

        case OP_QSORT:
        case OP_SSORT:
            {
                gfa_variable *qv = (gfa_variable *)inst->operand.ptr_val;
                if (qv != NULL && qv->type == GFA_VAR_ARRAY &&
                    qv->value.arr.data != NULL && rt->sp >= 2) {
                    gfa_value *hv = gfa_value_pop(rt);
                    gfa_value *lv = gfa_value_pop(rt);
                    int lo, hi, total;
                    double *arr;
                    lo = (int)gfa_value_to_long(lv);
                    hi = (int)gfa_value_to_long(hv);
                    if (lv) os_mem_free(lv);
                    if (hv) os_mem_free(hv);
                    total = (int)qv->value.arr.total_elements;
                    if (lo < 0) lo = 0;
                    if (hi >= total) hi = total - 1;
                    arr = (double *)qv->value.arr.data;
                    if (lo < hi && arr != NULL) {
                        if (inst->opcode == OP_QSORT)
                            gfa_array_quicksort(arr, lo, hi);
                        else
                            gfa_array_shellsort(arr, lo, hi);
                    }
                } else {
                    while (rt->sp > 0) gfa_value_discard(rt, 1);
                }
            }
            break;

        case OP_INSERT_ELEM:
            {
                gfa_variable *iv = (gfa_variable *)inst->operand.ptr_val;
                if (iv != NULL && iv->type == GFA_VAR_ARRAY &&
                    iv->value.arr.data != NULL && rt->sp >= 2) {
                    gfa_value *vv = gfa_value_pop(rt);
                    gfa_value *xv = gfa_value_pop(rt);
                    int idx, total, i2;
                    double *arr;
                    idx = (int)gfa_value_to_long(xv);
                    if (xv) os_mem_free(xv);
                    if (vv) os_mem_free(vv);
                    total = (int)iv->value.arr.total_elements;
                    arr = (double *)iv->value.arr.data;
                    if (idx >= 0 && idx < total && vv != NULL) {
                        for (i2 = total - 1; i2 > idx; i2--)
                            arr[i2] = arr[i2 - 1];
                        arr[idx] = gfa_value_to_float(vv);
                    }
                } else {
                    while (rt->sp > 0) gfa_value_discard(rt, 1);
                }
            }
            break;

        case OP_DELETE_ELEM:
            {
                gfa_variable *dv = (gfa_variable *)inst->operand.ptr_val;
                if (dv != NULL && dv->type == GFA_VAR_ARRAY &&
                    dv->value.arr.data != NULL && rt->sp >= 1) {
                    gfa_value *xv = gfa_value_pop(rt);
                    int idx, total, i2;
                    double *arr;
                    idx = (int)gfa_value_to_long(xv);
                    if (xv) os_mem_free(xv);
                    total = (int)dv->value.arr.total_elements;
                    arr = (double *)dv->value.arr.data;
                    if (idx >= 0 && idx < total) {
                        for (i2 = idx; i2 < total - 1; i2++)
                            arr[i2] = arr[i2 + 1];
                        arr[total - 1] = 0.0;
                    }
                } else {
                    while (rt->sp > 0) gfa_value_discard(rt, 1);
                }
            }
            break;

        /* ---------------------------------------------------------- */
        /* Graphismes VDI etendus (ANSI)                              */
        /* ---------------------------------------------------------- */

        case OP_PLOT_GFX:
            if (rt->sp >= 2) {
                v2 = gfa_value_pop(rt);
                v1 = gfa_value_pop(rt);
                gfx_plot((int)gfa_value_to_long(v1),
                         (int)gfa_value_to_long(v2));
                if (v1) os_mem_free(v1);
                if (v2) os_mem_free(v2);
                gfx_update();
            }
            break;

        case OP_TEXT_GFX:
            /* Stack: [x][y][texte$] */
            if (rt->sp >= 3) {
                v2 = gfa_value_pop(rt);
                {
                    gfa_value *vy = gfa_value_pop(rt);
                    gfa_value *vx = gfa_value_pop(rt);
                    if (v2 && v2->type == GFA_VAL_STRING && v2->data.s)
                        gfx_text((int)gfa_value_to_long(vx),
                                 (int)gfa_value_to_long(vy), v2->data.s);
                    if (vx) os_mem_free(vx);
                    if (vy) os_mem_free(vy);
                }
                if (v2) os_mem_free(v2);
                gfx_update();
            }
            break;

        case OP_POLY_GFX:
            /* Stack: [n][xy0..xy2n-1] ; operand.int_val = 0 ligne,
               1 polygone plein, 2 bezier, 3 marqueurs.
               Si operand.ptr_val != NULL : les points sont lus dans
               le tableau (POLYLINE n, xy&()). */
            {
                int n, i2;
                static int pts[128];
                gfa_variable *av;
                int from_arr;
                if (rt->sp >= 1) {
                    v1 = gfa_value_pop(rt);  /* n */
                    n = v1 ? (int)gfa_value_to_long(v1) : 0;
                    if (v1) os_mem_free(v1);
                    av = (gfa_variable *)inst->operand.ptr_val;
                    from_arr = (av != NULL && av->type == GFA_VAR_ARRAY &&
                                 av->value.arr.data != NULL);
                    if (n > 0 && n <= 32) {
                        if (from_arr) {
                            int total;
                            double *arr;
                            total = (int)av->value.arr.total_elements;
                            arr = (double *)av->value.arr.data;
                            if (2 * n > total) n = total / 2;
                            for (i2 = 0; i2 < 2 * n; i2++)
                                pts[i2] = (int)arr[i2];
                        } else if (rt->sp >= (2 * n)) {
                            for (i2 = 2 * n - 1; i2 >= 0; i2--) {
                                gfa_value *pv = gfa_value_pop(rt);
                                pts[i2] = pv ? (int)gfa_value_to_long(pv)
                                             : 0;
                                if (pv) os_mem_free(pv);
                            }
                        } else {
                            n = 0;
                        }
                        if (n > 0) {
                            if (inst->operand.int_val == 1) {
                                gfx_polygon(n, pts, 1);
                            } else if (inst->operand.int_val == 2) {
                                gfx_bezier(n, pts);
                            } else if (inst->operand.int_val == 3) {
                                for (i2 = 0; i2 < 2 * n; i2 += 2)
                                    gfx_plot(pts[i2], pts[i2 + 1]);
                            } else {
                                gfx_polyline(n, pts);
                            }
                            gfx_update();
                        }
                    } else if (!from_arr) {
                        while (rt->sp > 0) gfa_value_discard(rt, 1);
                    }
                }
            }
            break;

        case OP_FILL_GFX:
            /* Stack: [limite][y][x] */
            if (rt->sp >= 3) {
                int border;
                v2 = gfa_value_pop(rt);   /* limite (ou -1) */
                {
                    gfa_value *vy = gfa_value_pop(rt);
                    gfa_value *vx = gfa_value_pop(rt);
                    border = (int)gfa_value_to_long(v2);
                    gfx_flood_fill((int)gfa_value_to_long(vx),
                                   (int)gfa_value_to_long(vy), border);
                    if (vx) os_mem_free(vx);
                    if (vy) os_mem_free(vy);
                }
                if (v2) os_mem_free(v2);
                gfx_update();
            }
            break;

        case OP_GETBIT_GFX:
            /* Stack: [x2][y2][x1][y1][var$] : capture la zone */
            if (rt->sp >= 5) {
                int x1, y1, x2, y2, w, h;
                static char capbuf[640 * 400];
                v2 = gfa_value_pop(rt);   /* y2 */
                {
                    gfa_value *vx2 = gfa_value_pop(rt);  /* x2 */
                    gfa_value *vy1 = gfa_value_pop(rt);  /* y1 */
                    gfa_value *vx1 = gfa_value_pop(rt);  /* x1 */
                    gfa_value *var = gfa_value_pop(rt);  /* var$ (nom) */
                    x1 = (int)gfa_value_to_long(vx1);
                    y1 = (int)gfa_value_to_long(vy1);
                    x2 = (int)gfa_value_to_long(vx2);
                    y2 = (int)gfa_value_to_long(v2);
                    if (var && var->type == GFA_VAL_STRING && var->data.s) {
                        gfa_variable *gv =
                            gfa_var_lookup(rt->globals, var->data.s);
                        if (gv != NULL && gv->type == GFA_VAR_STRING) {
                            w = x2 - x1 + 1;
                            h = y2 - y1 + 1;
                            if (w < 1) w = 1;
                            if (h < 1) h = 1;
                            if (w * h > (int)sizeof(capbuf))
                                w = (int)sizeof(capbuf) / h;
                            gfx_capture(x1, y1, x1 + w - 1, y1 + h - 1,
                                        capbuf);
                            gfa_var_set_from_string(gv, capbuf);
                            /* La chaine contient des pixels bruts :
                               force longueur = nb de pixels */
                            gv->value.str.length = (os_int32)(w * h);
                            rt->capture_w = w;
                            rt->capture_h = h;
                        }
                    }
                    if (vx1) os_mem_free(vx1);
                    if (vy1) os_mem_free(vy1);
                    if (vx2) os_mem_free(vx2);
                    if (var) os_mem_free(var);
                }
                if (v2) os_mem_free(v2);
            }
            break;

        case OP_PUTBIT_GFX:
            /* Stack: [y][x][var$] : restitue la derniere zone capturee */
            if (rt->sp >= 3) {
                int w, h, bl, i2;
                static char capbuf[640 * 400];
                v2 = gfa_value_pop(rt);   /* x */
                {
                    gfa_value *vx = gfa_value_pop(rt);  /* y */
                    gfa_value *var = gfa_value_pop(rt); /* var$ */
                    w = rt->capture_w;
                    h = rt->capture_h;
                    if (var && var->type == GFA_VAL_STRING && var->data.s &&
                        w > 0 && h > 0) {
                        gfa_variable *gv =
                            gfa_var_lookup(rt->globals, var->data.s);
                        if (gv != NULL && gv->type == GFA_VAR_STRING &&
                            gv->value.str.data != NULL) {
                            os_mem_set(capbuf, 0, (size_t)w * (size_t)h);
                            bl = (int)gv->value.str.length;
                            if (bl > w * h) bl = w * h;
                            if (bl < 0) bl = 0;
                            if (bl > 0)
                                os_mem_copy(capbuf, gv->value.str.data,
                                             (size_t)bl);
                            gfx_restore((int)gfa_value_to_long(vx),
                                       (int)gfa_value_to_long(v2),
                                       capbuf, w, h);
                            gfx_update();
                        }
                    }
                    if (vx) os_mem_free(vx);
                    if (var) os_mem_free(var);
                }
                if (v2) os_mem_free(v2);
                (void)i2;
            }
            break;

        case OP_SETCOLOR:
            /* Stack: [val][n] : registre palette */
            if (rt->sp >= 2) {
                v2 = gfa_value_pop(rt);
                v1 = gfa_value_pop(rt);
                gfx_color_reg((int)gfa_value_to_long(v1),
                              (int)gfa_value_to_long(v2));
                if (v1) os_mem_free(v1);
                if (v2) os_mem_free(v2);
                gfx_update();
            }
            break;

        case OP_MODE_GFX:
            if (rt->sp >= 1) {
                v1 = gfa_value_pop(rt);
                gfx_mode((int)gfa_value_to_long(v1));
                if (v1) os_mem_free(v1);
            }
            break;

        case OP_CLIP_GFX:
            /* Stack: [x2][y2][x1][y1] ; tous nuls : reset */
            if (rt->sp >= 4) {
                v2 = gfa_value_pop(rt);
                {
                    gfa_value *vy2 = gfa_value_pop(rt);
                    gfa_value *vy1 = gfa_value_pop(rt);
                    gfa_value *vx1 = gfa_value_pop(rt);
                    int x1 = (int)gfa_value_to_long(vx1);
                    int y1 = (int)gfa_value_to_long(vy1);
                    int x2 = (int)gfa_value_to_long(vy2);
                    int y2 = (int)gfa_value_to_long(v2);
                    if (x1 == 0 && y1 == 0 && x2 == 0 && y2 == 0)
                        gfx_clip_reset();
                    else
                        gfx_clip(x1, y1, x2, y2);
                    if (vx1) os_mem_free(vx1);
                    if (vy1) os_mem_free(vy1);
                    if (vy2) os_mem_free(vy2);
                }
                if (v2) os_mem_free(v2);
            }
            break;

        /* ---------------------------------------------------------- */
        /* Fenetres GEM (emulation framebuffer)                       */
        /* ---------------------------------------------------------- */

        case OP_WINDOW_STMT:
            {
                int sub = inst->operand.int_val;
                if (sub == 0) {  /* CLEARW n */
                    if (rt->sp >= 1) gfa_value_pop(rt);
                    gfx_clear();
                    gfx_update();
                } else if (sub == 1) {  /* TITLEW n, "t" */
                    if (rt->sp >= 2) {
                        v2 = gfa_value_pop(rt);
                        v1 = gfa_value_pop(rt);
                        if (v2 && v2->type == GFA_VAL_STRING && v2->data.s)
                            gfx_text(2, 0, v2->data.s);
                        if (v1) os_mem_free(v1);
                        if (v2) os_mem_free(v2);
                        gfx_update();
                    } else {
                        while (rt->sp > 0) gfa_value_discard(rt, 1);
                    }
                } else if (sub == 2 || sub == 4) {
                    /* INFOW / MWOUT n, x, y, w, h : cadre */
                    if (rt->sp >= 5) {
                        gfa_value *a[5];
                        int i2;
                        for (i2 = 0; i2 < 5; i2++) a[i2] = gfa_value_pop(rt);
                        gfx_box((int)gfa_value_to_long(a[1]),
                                (int)gfa_value_to_long(a[2]),
                                (int)(gfa_value_to_long(a[1]) +
                                      gfa_value_to_long(a[3])),
                                (int)(gfa_value_to_long(a[2]) +
                                      gfa_value_to_long(a[4])));
                        for (i2 = 0; i2 < 5; i2++)
                            if (a[i2]) os_mem_free(a[i2]);
                        gfx_update();
                    } else {
                        while (rt->sp > 0) gfa_value_discard(rt, 1);
                    }
                } else if (sub == 5) {  /* GETSIZE n : pousse w,h,y,x */
                    if (rt->sp >= 1) gfa_value_pop(rt);
                    gfa_value_push_long(rt, 0);  /* w */
                    gfa_value_push_long(rt, 0);  /* h */
                    gfa_value_push_long(rt, 0);  /* y */
                    gfa_value_push_long(rt, 0);  /* x */
                } else {
                    /* TOPW / WINDTAB / SETDRAW / SHOWM / HIDEM */
                    while (rt->sp > 0) gfa_value_discard(rt, 1);
                }
            }
            break;

        /* ---------------------------------------------------------- */
        /* Turtle (DRAW)                                              */
        /* ---------------------------------------------------------- */

        case OP_DRAW_TURTLE:
            if (rt->sp >= 1) {
                v1 = gfa_value_pop(rt);
                if (v1 && v1->type == GFA_VAL_STRING && v1->data.s)
                    gfa_turtle_exec(rt, v1->data.s);
                if (v1) os_mem_free(v1);
                gfx_update();
            }
            break;

        case OP_DRAW_QUERY:
            if (rt->sp >= 1) {
                int q = (int)gfa_value_to_long(gfa_value_pop(rt));
                if (q == 0)      gfa_value_push_long(rt, (os_int32)rt->turtle_x);
                else if (q == 1) gfa_value_push_long(rt, (os_int32)rt->turtle_y);
                else             gfa_value_push_long(rt, (os_int32)rt->turtle_angle);
            } else {
                gfa_value_push_long(rt, 0);
            }
            break;

        /* ---------------------------------------------------------- */
        /* Matrices (MAT)                                             */
        /* ---------------------------------------------------------- */

        case OP_MAT_CLR: case OP_MAT_ONE: case OP_MAT_CPY:
        case OP_MAT_ADD: case OP_MAT_SUB: case OP_MAT_MUL:
        case OP_MAT_TRANS: case OP_MAT_INV: case OP_MAT_DET:
        case OP_MAT_RANG: case OP_MAT_NORM: case OP_MAT_SET:
        case OP_MAT_PRINT: case OP_MAT_READ: case OP_MAT_INPUT:
            {
                int sub;
                const char *target = "";
                const char *src1 = NULL;
                const char *src2 = NULL;
                double value = 0.0;
                int has_value = 0;
                int ret;

                if (inst->opcode == OP_MAT_CLR) sub = MAT_OP_CLR;
                else if (inst->opcode == OP_MAT_ONE) sub = MAT_OP_ONE;
                else if (inst->opcode == OP_MAT_CPY) sub = MAT_OP_CPY;
                else if (inst->opcode == OP_MAT_ADD) sub = MAT_OP_ADD;
                else if (inst->opcode == OP_MAT_SUB) sub = MAT_OP_SUB;
                else if (inst->opcode == OP_MAT_MUL) sub = MAT_OP_MUL;
                else if (inst->opcode == OP_MAT_TRANS) sub = MAT_OP_TRANS;
                else if (inst->opcode == OP_MAT_INV) sub = MAT_OP_INV;
                else if (inst->opcode == OP_MAT_DET) sub = MAT_OP_DET;
                else if (inst->opcode == OP_MAT_RANG) sub = MAT_OP_RANG;
                else if (inst->opcode == OP_MAT_NORM) sub = MAT_OP_NORM;
                else if (inst->opcode == OP_MAT_SET) sub = MAT_OP_SET;
                else if (inst->opcode == OP_MAT_PRINT) sub = MAT_OP_PRINT;
                else if (inst->opcode == OP_MAT_INPUT) sub = MAT_OP_INPUT;
                else sub = MAT_OP_READ;

                if (inst->operand.str_index >= 0 &&
                    inst->operand.str_index < rt->program->str_count) {
                    target = rt->program->strings[inst->operand.str_index];
                }
                if (inst->has_operand2 && inst->operand2.index2 >= 0 &&
                    inst->operand2.index2 < rt->program->str_count) {
                    src1 = rt->program->strings[inst->operand2.index2];
                }
                /* Forme sans cible : MAT DET(a) / MAT RANG(a) / … */
                if (target[0] == '\0' && src1 != NULL &&
                    (sub == MAT_OP_DET || sub == MAT_OP_RANG ||
                     sub == MAT_OP_NORM)) {
                    ret = gfa_matrix_scalar(rt, sub, src1);
                    if (ret != 0 && !runtime_error(rt, ret, "MAT error"))
                        return -1;
                    break;
                }
                switch (sub) {
                    case MAT_OP_ADD:
                    case MAT_OP_SUB:
                    case MAT_OP_MUL:
                        if (rt->sp >= 1) {
                            v1 = gfa_value_pop(rt);
                            if (v1 && v1->type == GFA_VAL_STRING &&
                                v1->data.s)
                                src2 = v1->data.s;
                            if (v1) os_mem_free(v1);
                        }
                        break;
                    case MAT_OP_SET:
                        if (rt->sp >= 1) {
                            v1 = gfa_value_pop(rt);
                            value = gfa_value_to_float(v1);
                            has_value = 1;
                            if (v1) os_mem_free(v1);
                        }
                        break;
                    default:
                        break;
                }
                /* Promouvoir les tableaux 2D en matrices si besoin
                   (un DIM classique cree un tableau, MAT en fait
                   une matrice). */
                {
                    gfa_variable *pm;
                    if (target[0] != '\0') {
                        pm = gfa_var_lookup(rt->globals, target);
                        if (pm != NULL && pm->type == GFA_VAR_ARRAY &&
                            pm->value.arr.num_dims == 2 &&
                            pm->value.arr.data != NULL)
                            pm->value.arr.is_matrix = 1;
                    }
                    if (src1 != NULL) {
                        pm = gfa_var_lookup(rt->globals, src1);
                        if (pm != NULL && pm->type == GFA_VAR_ARRAY &&
                            pm->value.arr.num_dims == 2 &&
                            pm->value.arr.data != NULL)
                            pm->value.arr.is_matrix = 1;
                    }
                    if (src2 != NULL) {
                        pm = gfa_var_lookup(rt->globals, src2);
                        if (pm != NULL && pm->type == GFA_VAR_ARRAY &&
                            pm->value.arr.num_dims == 2 &&
                            pm->value.arr.data != NULL)
                            pm->value.arr.is_matrix = 1;
                    }
                }
                ret = gfa_matrix_exec(rt, sub, target, src1, src2,
                                      has_value, value);
                if (ret != 0) {
                    if (!runtime_error(rt, ret, "MAT error"))
                        return -1;
                }
            }
            break;

        case OP_ELLIPSE_GFX:
            /* Stack: [x][y][rx][ry][fill] */
            if (rt->sp >= 5) {
                int ex, ey, erx, ery, efill;
                gfa_value *ef[5];
                int ei;
                for (ei = 0; ei < 5; ei++) ef[ei] = gfa_value_pop(rt);
                ex  = (int)gfa_value_to_long(ef[0]);
                ey  = (int)gfa_value_to_long(ef[1]);
                erx = (int)gfa_value_to_long(ef[2]);
                ery = (int)gfa_value_to_long(ef[3]);
                efill = (int)gfa_value_to_long(ef[4]);
                gfx_ellipse(ex, ey, erx, ery, efill);
                for (ei = 0; ei < 5; ei++)
                    if (ef[ei]) os_mem_free(ef[ei]);
                gfx_update();
            } else {
                while (rt->sp > 0) gfa_value_discard(rt, 1);
            }
            break;

        case OP_ACHAR_GFX:
            /* Stack: [x][y][code] */
            if (rt->sp >= 3) {
                v2 = gfa_value_pop(rt);   /* code */
                {
                    gfa_value *vy = gfa_value_pop(rt);
                    gfa_value *vx = gfa_value_pop(rt);
                    gfx_achar((int)gfa_value_to_long(vx),
                              (int)gfa_value_to_long(vy),
                              (int)gfa_value_to_long(v2));
                    if (vx) os_mem_free(vx);
                    if (vy) os_mem_free(vy);
                }
                if (v2) os_mem_free(v2);
                gfx_update();
            } else {
                while (rt->sp > 0) gfa_value_discard(rt, 1);
            }
            break;

        case OP_DIM:
            /* Pile: [d1][d2]...[dn] (d1 en premier). str_index = nom,
               operand2.index2 = ndim. Recree le tableau s'il a ete
               libere par ERASE (data == NULL). */
            {
                int ndim, i;
                os_int32 dims[7];
                const char *name = NULL;
                gfa_variable *dv;
                if (inst->operand.str_index >= 0 &&
                    inst->operand.str_index < rt->program->str_count)
                    name = rt->program->strings[inst->operand.str_index];
                ndim = inst->has_operand2 ? inst->operand2.index2 : 0;
                if (ndim < 1) ndim = 1;
                if (ndim > 7) ndim = 7;
                for (i = ndim - 1; i >= 0 && rt->sp > 0; i--) {
                    gfa_value *dv1 = gfa_value_pop(rt);
                    dims[i] = (os_int32)gfa_value_to_long(dv1);
                    if (dv1) os_mem_free(dv1);
                }
                if (name != NULL && rt->sp >= 0) {
                    dv = gfa_var_lookup(rt->globals, name);
                    if (dv == NULL || dv->type != GFA_VAR_ARRAY ||
                        dv->value.arr.data == NULL) {
                        gfa_var_type et = gfa_var_type_from_name(name);
                        if (et == GFA_VAR_STRING || et == GFA_VAR_BOOL)
                            et = GFA_VAR_FLOAT;  /* tableaux numeriques */
                        gfa_var_array_create(rt->globals, name,
                                             et, ndim, dims, 0);
                    }
                }
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
