/*
 * vm_statement.c - VM : opcodes instructions (statements)
 * ==============================================================
 * Decoupe de runtime.c (refactor 2026-08-19).
 * Opcodes "instructions" GFA : PRINT/INPUT, fichiers, graphismes,
 * son, memoire (PEEK/POKE), erreur (ON ERROR/RESUME/FATAL), MAT, ...
 *
 * Contrat avec execute_instruction() (voir vm_internal.h) :
 *   retourne VM_ADV (0)  : l'instruction est terminee,
 *                          execute_instruction avance rt->ip
 *   retourne VM_RET0 (1) : l'instruction est terminee, rt->ip est
 *                          deja gere (pas d'increment)
 *   retourne < 0         : erreur fatale, propagee a la boucle
 *
 * Reference : cahier-des-charges-gfabasic.md, section 7
 */

#include "runtime.h"
#include "vm_internal.h"
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
 * vm_exec_statement - Execute un opcode "instruction" GFA.
 * Chaque opcode est dispatche par execute_instruction() (runtime.c)
 * vers cette fonction ; le default est inatteignable.
 */
int vm_exec_statement(gfa_runtime *rt, gfa_instruction *inst, os_int32 operand)
{
    gfa_value *v1, *v2;
    gfa_variable *var;
    gfa_opcode op = inst->opcode;

    switch (op) {

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
            return VM_RET0;

        case OP_STOP:
            rt->stopped = 1;
            rt->running = 0;
            return VM_RET0;

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
            return VM_RET0;

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
                    return VM_RET0;
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
                    return VM_RET0;
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
                return VM_RET0;
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

        /* MAT DET/RANG/NORM en position expression : push scalaire */
        case OP_MAT_DET_EXPR:
        case OP_MAT_RANG_EXPR:
        case OP_MAT_NORM_EXPR:
            {
                int sub_ex;
                const char *mname = "";
                double mv = 0.0;
                gfa_variable *pm;

                if (inst->operand.str_index >= 0 &&
                    inst->operand.str_index < rt->program->str_count)
                    mname = rt->program->strings[inst->operand.str_index];
                if (inst->opcode == OP_MAT_RANG_EXPR)
                    sub_ex = MAT_OP_RANG;
                else if (inst->opcode == OP_MAT_NORM_EXPR)
                    sub_ex = MAT_OP_NORM;
                else
                    sub_ex = MAT_OP_DET;
                /* Promouvoir un tableau 2D en matrice si besoin */
                if (mname[0] != '\0') {
                    pm = gfa_var_lookup(rt->globals, mname);
                    if (pm != NULL && pm->type == GFA_VAR_ARRAY &&
                        pm->value.arr.num_dims == 2 &&
                        pm->value.arr.data != NULL)
                        pm->value.arr.is_matrix = 1;
                }
                if (gfa_matrix_scalar_value(rt, sub_ex, mname, &mv) != 0) {
                    if (!runtime_error(rt, 17, "MAT error"))
                        return -1;
                }
                gfa_value_push_float(rt, mv);
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
            /* Inatteignable : execute_instruction ne dispatche que
               des opcodes declares dans sa liste de cas. */
            return 0;
    }

    /* Inatteignable : tout cas se termine par break (retour ici)
       ou par return. */
    return 0;
}
