/*
 * vm_builtin.c - VM : opcode OP_CALL_BUILTIN (fonctions integrees)
 * ==============================================================
 * Decoupe de runtime.c (refactor 2026-08-19).
 * Dispatch de toutes les fonctions integrees GFA : maths, chaines,
 * memoire (PEEK/POKE), TOS, touches, repertoire (FSFIRST/FSNEXT), ...
 *
 * Contrat avec execute_instruction() (voir vm_internal.h) :
 *   retourne VM_ADV (0)  : l'instruction est terminee,
 *                          execute_instruction avance rt->ip
 *   retourne VM_RET0 (1) : l'instruction est terminee, rt->ip est
 *                          deja gere (pas d'increment)
 *   retourne < 0         : erreur fatale, propagee a la boucle
 *   retourne  > 1        : code de sortie (valeur - 1), propage
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

/* Etat de l'iteration de repertoire (FSFIRST/FSNEXT/FNAME/...) */
static os_file_info g_fs_info;
static int          g_fs_state = 0;  /* 0=aucune 1=en cours 2=terminee */
static os_int32     g_fs_pos   = 0;

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

/*
 * vm_exec_builtin - Execute l'opcode OP_CALL_BUILTIN.
 * operand = type de token de la fonction ; les parametres sont
 * sur la pile (le dernier push = le dernier argument).
 */
int vm_exec_builtin(gfa_runtime *rt, gfa_instruction *inst, os_int32 operand)
{
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

                    /* DMACONTROL / DMASOUND - emules, resultat 0 */
                    case TOK_DMACONTROL:
                    case TOK_DMASOUND:
                        if (rt->sp >= 1) {
                            gfa_value *sv = gfa_value_pop(rt);
                            if (sv) {
                                if (sv->owns_string && sv->data.s) os_mem_free(sv->data.s);
                                os_mem_free(sv);
                            }
                        }
                        gfa_value_push_long(rt, 0);
                        break;

                    /* SYSTEM "cmd" / EXEC "cmd" : execute la
                       commande shell et continue le programme. */
                    case TOK_SYSTEM:
                    case TOK_EXEC:
                        {
                            int rc_sys = 0;
                            if (rt->sp >= 1) {
                                gfa_value *sv = gfa_value_pop(rt);
                                if (sv != NULL &&
                                    sv->type == GFA_VAL_STRING &&
                                    sv->data.s != NULL) {
                                    rc_sys = (int)os_system(sv->data.s);
                                }
                                if (sv) {
                                    if (sv->owns_string && sv->data.s) os_mem_free(sv->data.s);
                                    os_mem_free(sv);
                                }
                            }
                            gfa_value_push_long(rt, rc_sys);
                        }
                        break;

                    /* CHAIN "prog" : execute un autre programme, qui
                       remplace le courant. Le hook (branche par main)
                       lit le fichier et relance l'interpreteur. */
                    case TOK_CHAIN:
                        {
                            int rc_ch = 0;
                            if (rt->sp >= 1) {
                                gfa_value *sv = gfa_value_pop(rt);
                                if (sv != NULL &&
                                    sv->type == GFA_VAL_STRING &&
                                    sv->data.s != NULL) {
                                    if (rt->chain_fn != NULL) {
                                        rc_ch = rt->chain_fn(sv->data.s);
                                    } else {
                                        rc_ch = -1;
                                    }
                                }
                                if (sv) {
                                    if (sv->owns_string && sv->data.s) os_mem_free(sv->data.s);
                                    os_mem_free(sv);
                                }
                            }
                            rt->running = 0;
                            if (rc_ch != 0) {
                                /* 19 = fichier introuvable (code GFA) */
                                runtime_error(rt, 19, "CHAIN: fichier introuvable");
                            }
                            if (rc_ch < 0) return rc_ch;
                                if (rc_ch == 0) return VM_RET0;
                                return rc_ch + 1;
                        }
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
            return 0;  /* VM_ADV : execute_instruction fait rt->ip++ */
}
