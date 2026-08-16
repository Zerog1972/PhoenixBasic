/*
 * codegen.c - Generateur de bytecode GFA Basic 3.5
 * =================================================
 * Transforme un AST en bytecode executable par le runtime.
 * Version regeneree proprement le 7 juin 2026.
 *
 * Reference : cahier-des-charges-gfabasic.md, section 7.2
 */

#include "codegen.h"
#include "token.h"
#include "files.h"
#include "matrix.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* strcasecmp portable pour C89 */
static int strieq(const char *a, const char *b)
{
    if (a == NULL || b == NULL) return (a == b);
    while (*a && *b) {
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b))
            return 0;
        a++; b++;
    }
    return (*a == *b);
}

#define MAX_PATCHES 1024

typedef struct {
    gfa_bytecode *bc;
    gfa_symbol_table *sym;
    int error;
    int patch_stack[MAX_PATCHES];
    int patch_depth;
    int option_base;               /* OPTION BASE n                    */
    int exit_stack[MAX_PATCHES];   /* marqueurs de boucles imbriquees  */
    int exit_patch_base[MAX_PATCHES];
    int exit_patches[MAX_PATCHES]; /* sauts a patcher a la sortie      */
    int exit_patch_count;
    int exit_depth;
    gfa_label_info *labels;      /* labels connus (FN/PROC/GOTO)       */
    int label_count;
} codegen_ctx;

/* Forward declarations */
static void cg_program(codegen_ctx *ctx, ast_node *node);
static void cg_statement(codegen_ctx *ctx, ast_node *node);
static void cg_expression(codegen_ctx *ctx, ast_node *node);
static void cg_assign(codegen_ctx *ctx, ast_node *node);
static void cg_if(codegen_ctx *ctx, ast_node *node);
static void cg_for(codegen_ctx *ctx, ast_node *node);
static void cg_while(codegen_ctx *ctx, ast_node *node);
static void cg_repeat(codegen_ctx *ctx, ast_node *node);
static void cg_do_loop(codegen_ctx *ctx, ast_node *node);
static void cg_exit_if(codegen_ctx *ctx, ast_node *node);
static void cg_loop_enter(codegen_ctx *ctx);
static void cg_loop_leave(codegen_ctx *ctx);
static void cg_select(codegen_ctx *ctx, ast_node *node);
static void cg_print(codegen_ctx *ctx, ast_node *node);
static void cg_call(codegen_ctx *ctx, ast_node *node);
static int cg_name_is_label(codegen_ctx *ctx, const char *name);
static void cg_collect_data(ast_node *node, double **data_out, int *count_out);

/* Helpers */
static int cg_emit(codegen_ctx *ctx, gfa_opcode op) {
    return gfa_bytecode_emit(ctx->bc, op);
}
static int cg_emit_int(codegen_ctx *ctx, gfa_opcode op, os_int32 val) {
    return gfa_bytecode_emit_int(ctx->bc, op, val);
}
static int cg_emit_str(codegen_ctx *ctx, gfa_opcode op, const char *s) {
    return gfa_bytecode_emit_str(ctx->bc, op, s);
}
static int cg_emit_float_const(codegen_ctx *ctx, gfa_opcode op, double val) {
    int idx = gfa_bytecode_emit(ctx->bc, op);
    if (idx >= 0) ctx->bc->code[idx].operand.float_val = val;
    return idx;
}
static int cg_emit_ptr(codegen_ctx *ctx, gfa_opcode op, void *ptr) {
    int idx = gfa_bytecode_emit(ctx->bc, op);
    if (idx >= 0) ctx->bc->code[idx].operand.ptr_val = ptr;
    return idx;
}
static int cg_current(codegen_ctx *ctx) {
    return gfa_bytecode_current_ip(ctx->bc);
}
static void cg_patch(codegen_ctx *ctx, int index, os_int32 val) {
    gfa_bytecode_patch(ctx->bc, index, val);
}
static void cg_push_patch(codegen_ctx *ctx, int instr_index) {
    if (ctx->patch_depth < MAX_PATCHES)
        ctx->patch_stack[ctx->patch_depth++] = instr_index;
}
static void cg_pop_patch(codegen_ctx *ctx) {
    if (ctx->patch_depth > 0) {
        int idx = ctx->patch_stack[--ctx->patch_depth];
        cg_patch(ctx, idx, (os_int32)cg_current(ctx));
    }
}

static gfa_variable *cg_resolve_var(codegen_ctx *ctx, const char *name)
{
    gfa_variable *var;
    gfa_var_type vtype;

    if (name == NULL) return NULL;
    var = gfa_var_lookup(ctx->sym, name);
    if (var != NULL) return var;

    vtype = gfa_var_type_from_name(name);
    return gfa_var_create(ctx->sym, name, vtype);
}

/* ================================================================== */
/* DATA collection                                                    */
/* ================================================================== */

static void cg_collect_data(ast_node *node, double **data_out, int *count_out)
{
    if (!node) return;
    if (node->type == AST_DATA) {
        ast_node *val = node->left;
        while (val) {
            double dv = val->has_str && val->value.str_val
                        ? gfa_val(val->value.str_val)
                        : val->value.float_val;
            (*count_out)++;
            *data_out = (double *)os_mem_realloc(*data_out, (size_t)(*count_out) * sizeof(double));
            if (*data_out) (*data_out)[(*count_out)-1] = dv;
            val = val->right;
        }
    }
    cg_collect_data(node->left, data_out, count_out);
    cg_collect_data(node->right, data_out, count_out);
    cg_collect_data(node->cond, data_out, count_out);
    cg_collect_data(node->body, data_out, count_out);
    cg_collect_data(node->else_body, data_out, count_out);
}

/* ================================================================== */
/* API principale                                                     */
/* ================================================================== */

int gfa_codegen_compile(gfa_symbol_table *symbol_table,
                        ast_node *ast, gfa_bytecode **out_bc,
                        gfa_label_info *labels, int label_count)
{
    codegen_ctx ctx;
    int i;

    if (symbol_table == NULL || ast == NULL || out_bc == NULL) return -1;

    ctx.bc    = gfa_bytecode_create();
    ctx.sym   = symbol_table;
    ctx.error = 0;
    ctx.patch_depth = 0;
    ctx.option_base = 0;
    ctx.exit_depth = 0;
    ctx.exit_patch_count = 0;
    ctx.labels = labels;
    ctx.label_count = label_count;
    if (ctx.bc == NULL) return -1;

    /* Collect DATA values */
    {
        double *data_vals = NULL;
        int data_count = 0;
        cg_collect_data(ast, &data_vals, &data_count);
        ctx.bc->data_values = data_vals;
        ctx.bc->data_count = data_count;
    }

    cg_program(&ctx, ast);
    /* Label resolution pass */
    if (labels != NULL && label_count > 0) {
        /* Register label positions */
        for (i = 0; i < ctx.bc->length; i++) {
            gfa_instruction *inst = &ctx.bc->code[i];
            if (inst->opcode == OP_LABEL) {
                const char *lbl_name;
                int str_idx, j;
                str_idx = inst->operand.str_index;
                if (str_idx >= 0 && str_idx < ctx.bc->str_count) {
                    lbl_name = ctx.bc->strings[str_idx];
                    for (j = 0; j < label_count; j++) {
                        if (labels[j].name && lbl_name && strieq(labels[j].name, lbl_name)) {
                            labels[j].bytecode_ip = i;
                            break;
                        }
                    }
                }
            }
        }
        /* Patch JMP/CALL placeholders */
        for (i = 0; i < ctx.bc->length; i++) {
            gfa_instruction *inst = &ctx.bc->code[i];
            if ((inst->opcode == OP_JMP ||
                 inst->opcode == OP_JMP_IF_FALSE ||
                 inst->opcode == OP_JMP_IF_TRUE ||
                 inst->opcode == OP_CALL ||
                 inst->opcode == OP_ON_ERROR) &&
                inst->operand.int_val == -1 && inst->has_operand2) {
                const char *target_name;
                int str_idx, j;
                str_idx = inst->operand2.int_val2;
                if (str_idx >= 0 && str_idx < ctx.bc->str_count) {
                    target_name = ctx.bc->strings[str_idx];
                    for (j = 0; j < label_count; j++) {
                        if (labels[j].name && target_name && strieq(labels[j].name, target_name)) {
                            inst->operand.int_val = (os_int32)labels[j].bytecode_ip;
                            break;
                        }
                    }
                }
            }
        }
    }

    *out_bc = ctx.bc;
    return ctx.error;
}

/* ================================================================== */
/* Compilation                                                        */
/* ================================================================== */

static void cg_program(codegen_ctx *ctx, ast_node *node)
{
    ast_node *child;
    if (node == NULL) return;
    child = node->left;
    while (child != NULL) {
        cg_statement(ctx, child);
        child = child->right;
    }
    cg_emit(ctx, OP_END);
}

static void cg_statement(codegen_ctx *ctx, ast_node *node)
{
    ast_node *child;
    if (node == NULL) return;

    switch (node->type) {

    case AST_PROGRAM:
    case AST_STATEMENT_LIST:
        child = node->left;
        while (child != NULL) {
            if (child->type == AST_STATEMENT_LIST) {
                ast_node *inner = child->left;
                while (inner != NULL) {
                    cg_statement(ctx, inner);
                    inner = inner->right;
                }
            } else {
                cg_statement(ctx, child);
            }
            child = child->right;
        }
        break;

    case AST_ASSIGN:      cg_assign(ctx, node); break;
    case AST_IF:          cg_if(ctx, node); break;
    case AST_FOR:         cg_for(ctx, node); break;
    case AST_WHILE:       cg_while(ctx, node); break;
    case AST_REPEAT:      cg_repeat(ctx, node); break;
    case AST_SELECT:      cg_select(ctx, node); break;
    case AST_PRINT:       cg_print(ctx, node); break;
    case AST_GOTO:        /* fall through to GOSUB-like label handling */
    case AST_GOSUB:
        if (node->value.ident) {
            int str_idx = gfa_bytecode_add_string(ctx->bc, node->value.ident);
            int instr_idx = cg_emit_int(ctx,
                (node->type == AST_GOTO) ? OP_JMP : OP_CALL, -1);
            ctx->bc->code[instr_idx].has_operand2 = 1;
            ctx->bc->code[instr_idx].operand2.int_val2 = str_idx;
        }
        break;
    case AST_RETURN:
        /* RETURN [expression] : pour FUNCTION, push valeur avant OP_RET */
        if (node->left) {
            cg_expression(ctx, node->left);
        }
        cg_emit(ctx, OP_RET);
        break;
    case AST_CLS:         cg_emit(ctx, OP_CLS); break;
    case AST_END:         cg_emit(ctx, OP_END); break;
    case AST_STOP:        cg_emit(ctx, OP_STOP); break;
    case AST_BEEP:        cg_emit(ctx, OP_BEEP); break;
    case AST_OPENW:
        if (node->left) cg_expression(ctx, node->left);
        cg_emit(ctx, OP_OPENW);
        break;
    case AST_CLOSEW:
        cg_emit(ctx, OP_CLOSEW);
        break;
    case AST_ERROR:
        if (node->left) cg_expression(ctx, node->left);
        cg_emit(ctx, OP_ERROR);
        break;
    case AST_FATAL:
        /* FATAL n : push error code, then OP_FATAL (sets fatal flag and raises error) */
        if (node->left) cg_expression(ctx, node->left);
        cg_emit(ctx, OP_FATAL);
        break;
    case AST_RESUME:
        /* RESUME [NEXT] : operand 0 = normal, 1 = NEXT */
        cg_emit_int(ctx, OP_RESUME, (node->left != NULL) ? 1 : 0);
        break;
    case AST_SETTIME:
        /* SETTIME time$ [, date$] */
        if (node->left) {
            cg_expression(ctx, node->left);
            if (node->left->right) cg_expression(ctx, node->left->right);
        }
        cg_emit_int(ctx, OP_CALL_BUILTIN, (os_int32)TOK_SETTIME);
        break;
    case AST_SWAP:
        /* SWAP var1, var2 : push both, swap, pop_store back */
        if (node->left && node->left->right) {
            gfa_variable *v1 = NULL;
            gfa_variable *v2 = NULL;
            if (node->left->has_ident && node->left->value.ident)
                v1 = cg_resolve_var(ctx, node->left->value.ident);
            if (node->left->right->has_ident && node->left->right->value.ident)
                v2 = cg_resolve_var(ctx, node->left->right->value.ident);
            if (v1 && v2) {
                /* Push var1, var2, swap, then pop_store in reverse:
                 * Stack: [var1_val, var2_val] -> swap -> [var2_val, var1_val]
                 * Pop top (var1_val) into var2, pop next (var2_val) into var1 */
                cg_emit_ptr(ctx, OP_PUSH_VAR, (void *)v1);
                cg_emit_ptr(ctx, OP_PUSH_VAR, (void *)v2);
                cg_emit(ctx, OP_SWAP);
                cg_emit_ptr(ctx, OP_POP_STORE, (void *)v2);
                cg_emit_ptr(ctx, OP_POP_STORE, (void *)v1);
            }
        }
        break;
    case AST_ON_GOTO_GOSUB:
        {
            /* First child = index expression, rest = labels */
            ast_node *child;
            int label_count;
            child = node->left;
            if (child) {
                cg_expression(ctx, child);  /* index */
                child = child->right;
            }
            label_count = 0;
            while (child) {
                if (child->has_ident && child->value.ident) {
                    int str_idx;
                    str_idx = gfa_bytecode_add_string(ctx->bc, child->value.ident);
                    cg_emit_float_const(ctx, OP_PUSH_CONST, (double)str_idx);
                    label_count++;
                }
                child = child->right;
            }
            cg_emit_float_const(ctx, OP_PUSH_CONST, (double)label_count);
            cg_emit(ctx, (node->value.int_val == 0) ? OP_ON_GOTO : OP_ON_GOSUB);
        }
        break;
    case AST_TRON:        cg_emit(ctx, OP_TRON); break;
    case AST_TROFF:       cg_emit(ctx, OP_TROFF); break;
    case AST_BLOAD:
    case AST_BSAVE:
    case AST_BGET:
    case AST_BPUT:
        {
            int op = (node->type == AST_BLOAD) ? OP_BLOAD :
                     (node->type == AST_BSAVE) ? OP_BSAVE :
                     (node->type == AST_BGET)  ? OP_BGET  : OP_BPUT;
            ast_node *arg;
            arg = node->left;
            while (arg) { cg_expression(ctx, arg); arg = arg->right; }
            cg_emit(ctx, op);
            /* Forme instruction : le runtime pousse le nombre d'octets,
               depiler pour garder la pile propre. */
            cg_emit(ctx, OP_POP);
        }
        break;

    case AST_LOCATE:
        if (node->left) {
            cg_expression(ctx, node->left);
            if (node->left->right) {
                cg_expression(ctx, node->left->right);
                cg_emit(ctx, OP_LOCATE);
            }
        }
        break;

    case AST_SOUND:
        { ast_node *arg = node->left;
          while (arg) { cg_expression(ctx, arg); arg = arg->right; }
          cg_emit(ctx, OP_SOUND); }
        break;

    case AST_COLOR:
        if (node->left) {
            cg_expression(ctx, node->left);
            if (node->left->right) cg_expression(ctx, node->left->right);
            else cg_emit_float_const(ctx, OP_PUSH_CONST, (double)0);
            cg_emit(ctx, OP_COLOR);
        }
        break;

    case AST_LINE:   case AST_BOX:   case AST_PBOX:
    case AST_CIRCLE: case AST_PCIRCLE:
        { ast_node *arg = node->left; gfa_opcode gfx = OP_LINE_GFX;
          switch (node->type) {
              case AST_LINE: gfx=OP_LINE_GFX; break;
              case AST_BOX: case AST_PBOX: gfx=OP_BOX_GFX; break;
              case AST_CIRCLE: case AST_PCIRCLE: gfx=OP_CIRCLE_GFX; break;
              default: break;
          }
          while (arg) { cg_expression(ctx, arg); arg = arg->right; }
          cg_emit(ctx, gfx); }
        break;

    /* --- PROCEDURE / FUNCTION (C13, C14) --- */
    case AST_PROCEDURE:
    case AST_FUNCTION_DEF:
        { int jmp_skip = cg_emit_int(ctx, OP_JMP, 0);
          if (node->has_ident && node->value.ident) {
              int str_idx = gfa_bytecode_add_string(ctx->bc, node->value.ident);
              cg_emit_int(ctx, OP_LABEL, (os_int32)str_idx);
          }
          /* Pop args and save old values using OP_SAVE_LOCAL.
             VAR params use OP_BIND_REF (no save, no restore). */
          {
              ast_node *arg = node->left;
              int arg_count = 0;
              int *arg_is_ref;  /* 1 = VAR (by-ref) */
              { ast_node *a = arg; while (a) { arg_count++; a = a->right; } }
              if (arg_count > 0) {
                  gfa_variable **arg_vars;
                  int ai;
                  arg_vars = (gfa_variable **)malloc((size_t)arg_count * sizeof(gfa_variable *));
                  arg_is_ref = (int *)malloc((size_t)arg_count * sizeof(int));
                  if (arg_vars && arg_is_ref) {
                      ai = 0;
                      while (arg) {
                          if (arg->has_ident && arg->value.ident) {
                              arg_vars[ai] = cg_resolve_var(ctx, arg->value.ident);
                              arg_is_ref[ai] = (arg->line != 0) ? 1 : 0;
                          } else {
                              arg_vars[ai] = NULL;
                              arg_is_ref[ai] = 0;
                          }
                          ai++;
                          arg = arg->right;
                      }
                      /* Reverse order: last arg is top of stack */
                      for (ai = arg_count - 1; ai >= 0; ai--) {
                          if (arg_vars[ai]) {
                              if (arg_is_ref[ai])
                                  cg_emit_ptr(ctx, OP_BIND_REF, (void *)arg_vars[ai]);
                              else
                                  cg_emit_ptr(ctx, OP_SAVE_LOCAL, (void *)arg_vars[ai]);
                          }
                      }
                  }
                  free(arg_vars);
                  free(arg_is_ref);
              }
          }
          if (node->body) cg_statement(ctx, node->body);
          /* FUNCTION_DEF toujours OP_RET en fin (fallback) : pousse la
             valeur de la variable nom (resultat implicite) */
          if (node->type == AST_FUNCTION_DEF) {
              if (node->has_ident && node->value.ident) {
                  gfa_variable *rv = cg_resolve_var(ctx, node->value.ident);
                  if (rv) cg_emit_ptr(ctx, OP_PUSH_VAR, (void *)rv);
              }
              cg_emit(ctx, OP_RET);
          }
          cg_patch(ctx, jmp_skip, (os_int32)cg_current(ctx)); }
        break;

    case AST_ENDFUNC:  /* Fallback si pas deja fait par RETURN */
        cg_emit(ctx, OP_RET);
        break;

    case AST_DEFFN_RET:  /* RETURN d'un FN multi-lignes */
        if (node->has_ident && node->value.ident) {
            gfa_variable *rv2 = cg_resolve_var(ctx, node->value.ident);
            if (rv2) cg_emit_ptr(ctx, OP_PUSH_VAR, (void *)rv2);
        }
        cg_emit(ctx, OP_RET);
        break;

    /* --- LOCAL (C13) --- */
    case AST_LOCAL:
        { ast_node *var = node->left;
          while (var) {
              if (var->has_ident && var->value.ident)
                  cg_resolve_var(ctx, var->value.ident);
              var = var->right;
          } }
        break;

    case AST_LABEL:
        if (node->value.ident) {
            int str_idx = gfa_bytecode_add_string(ctx->bc, node->value.ident);
            cg_emit_int(ctx, OP_LABEL, (os_int32)str_idx);
        }
        break;

    /* --- DEFFN / FN (C14) --- */
    case AST_DEFFN:
        { int jmp_skip = cg_emit_int(ctx, OP_JMP, 0);
          if (node->has_ident && node->value.ident) {
              int str_idx = gfa_bytecode_add_string(ctx->bc, node->value.ident);
              cg_emit_int(ctx, OP_LABEL, (os_int32)str_idx);
          }
          /* Pop args (same as PROCEDURE) */
          {
              ast_node *arg = node->left;
              int arg_count = 0;
              { ast_node *a = arg; while (a) { arg_count++; a = a->right; } }
              if (arg_count > 0) {
                  gfa_variable **arg_vars;
                  int ai;
                  arg_vars = (gfa_variable **)malloc((size_t)arg_count * sizeof(gfa_variable *));
                  if (arg_vars) {
                      ai = 0;
                      while (arg) {
                          if (arg->has_ident && arg->value.ident)
                              arg_vars[ai++] = cg_resolve_var(ctx, arg->value.ident);
                          else
                              arg_vars[ai++] = NULL;
                          arg = arg->right;
                      }
                      for (ai = arg_count - 1; ai >= 0; ai--) {
                          if (arg_vars[ai])
                              cg_emit_ptr(ctx, OP_SAVE_LOCAL, (void *)arg_vars[ai]);
                      }
                      free(arg_vars);
                  }
              }
          }
          /* Body = single expression, then return ; ou corps
             multi-lignes (line=1) : resultat = variable nom */
          if (node->line == 1 && node->body) {
              cg_statement(ctx, node->body);
              if (node->has_ident && node->value.ident) {
                  gfa_variable *rv = cg_resolve_var(ctx, node->value.ident);
                  if (rv) cg_emit_ptr(ctx, OP_PUSH_VAR, (void *)rv);
              }
          } else if (node->body) {
              cg_expression(ctx, node->body);
          }
          cg_emit(ctx, OP_RET);
          cg_patch(ctx, jmp_skip, (os_int32)cg_current(ctx)); }
        break;

    /* --- INPUT (C5) --- */
    case AST_INPUT:
        { ast_node *arg = node->left;
          int has_channel = (node->value.int_val != 0) ? 1 : 0;
          if (has_channel && arg) {
              /* Skip channel child, emit OP_INPUT_FILE for each var */
              arg = arg->right;
              while (arg) {
                  if (arg->has_ident && arg->value.ident) {
                      gfa_variable *var = cg_resolve_var(ctx, arg->value.ident);
                      cg_expression(ctx, node->left);  /* push channel */
                      cg_emit_ptr(ctx, OP_INPUT_FILE, (void *)var);
                  }
                  arg = arg->right;
              }
          } else {
              while (arg) {
                  if (arg->has_ident && arg->value.ident) {
                      gfa_variable *var = cg_resolve_var(ctx, arg->value.ident);
                      cg_emit_ptr(ctx, OP_INPUT, (void *)var);
                  }
                  arg = arg->right;
              }
          } }
        break;

    case AST_LINE_INPUT:
        if (node->left && node->left->has_ident && node->left->value.ident) {
            gfa_variable *var = cg_resolve_var(ctx, node->left->value.ident);
            if (node->cond != NULL) {
                /* LINE INPUT #n, var$ : canal sur la pile */
                cg_expression(ctx, node->cond);
                cg_emit_ptr(ctx, OP_LINE_INPUT_FILE, (void *)var);
            } else {
                cg_emit_ptr(ctx, OP_LINE_INPUT, (void *)var);
            }
        }
        break;

    /* --- DATA/READ/RESTORE (C4) --- */
    case AST_DATA: break; /* handled by cg_collect_data */
    case AST_READ:
        { ast_node *arg = node->left;
          while (arg) {
              if (arg->has_ident && arg->value.ident) {
                  gfa_variable *var = cg_resolve_var(ctx, arg->value.ident);
                  cg_emit_int(ctx, OP_CALL_BUILTIN, (os_int32)TOK__DATA);
                  cg_emit_ptr(ctx, OP_POP_STORE, (void *)var);
              }
              arg = arg->right; } }
        break;
    case AST_RESTORE:
        cg_emit_int(ctx, OP_CALL_BUILTIN, (os_int32)TOK_RESTORE);
        break;

    /* --- DIM (C3) --- */
    case AST_DIM:
        { ast_node *name_node = node->left;
          if (name_node && name_node->has_ident && name_node->value.ident) {
              os_int32 dims[7]; int ndim = 0;
              ast_node *dim_node = name_node->right;
              while (dim_node && ndim < 7) {
                  dims[ndim++] = (os_int32)dim_node->value.float_val;
                  dim_node = dim_node->right;
              }
              if (ndim > 0) {
                  int i;
                  gfa_var_type et = gfa_var_type_from_name(name_node->value.ident);
                  if (et == GFA_VAR_STRING || et == GFA_VAR_BOOL)
                      et = GFA_VAR_FLOAT;  /* tableaux numeriques uniquement */
                  gfa_var_array_create(ctx->sym, name_node->value.ident,
                                       et, ndim, dims,
                                       (os_int32)ctx->option_base);
                  /* OP_DIM runtime : recree le tableau apres ERASE */
                  for (i = 0; i < ndim; i++)
                      cg_emit_float_const(ctx, OP_PUSH_CONST,
                                          (double)dims[i]);
                  {
                      int idx = cg_emit_str(ctx, OP_DIM,
                                            name_node->value.ident);
                      if (idx >= 0) {
                          ctx->bc->code[idx].has_operand2 = 1;
                          ctx->bc->code[idx].operand2.index2 = ndim;
                      }
                  }
              }
          } }
        break;

    /* --- OPEN/CLOSE (C9) --- */
    case AST_OPEN:
        { ast_node *arg = node->left;
          while (arg) { cg_expression(ctx, arg); arg = arg->right; }
          cg_emit(ctx, OP_OPEN_FILE); }
        break;
    case AST_CLOSE:
        if (node->left) { cg_expression(ctx, node->left); cg_emit(ctx, OP_CLOSE_FILE); }
        break;

    /* --- PEEK/POKE (C17) --- */
    case AST_PEEK:
        if (node->left) { cg_expression(ctx, node->left); cg_emit(ctx, OP_PEEK); }
        break;
    case AST_POKE:
        if (node->left) { cg_expression(ctx, node->left);
            if (node->left->right) { cg_expression(ctx, node->left->right); cg_emit(ctx, OP_POKE); } }
        break;
    case AST_DPEEK:
        if (node->left) { cg_expression(ctx, node->left); cg_emit(ctx, OP_DPEEK); }
        break;
    case AST_DPOKE:
        if (node->left) { cg_expression(ctx, node->left);
            if (node->left->right) { cg_expression(ctx, node->left->right); cg_emit(ctx, OP_DPOKE); } }
        break;
    case AST_LPEEK:
        if (node->left) { cg_expression(ctx, node->left); cg_emit(ctx, OP_LPEEK); }
        break;
    case AST_LPOKE:
        if (node->left) { cg_expression(ctx, node->left);
            if (node->left->right) { cg_expression(ctx, node->left->right); cg_emit(ctx, OP_LPOKE); } }
        break;
    case AST_SPOKE:
        if (node->left) { cg_expression(ctx, node->left);
            if (node->left->right) { cg_expression(ctx, node->left->right); cg_emit(ctx, OP_SPOKE); } }
        break;
    case AST_SDPOKE:
        if (node->left) { cg_expression(ctx, node->left);
            if (node->left->right) { cg_expression(ctx, node->left->right); cg_emit(ctx, OP_SDPOKE); } }
        break;
    case AST_SLPOKE:
        if (node->left) { cg_expression(ctx, node->left);
            if (node->left->right) { cg_expression(ctx, node->left->right); cg_emit(ctx, OP_SLPOKE); } }
        break;

    /* --- ON ERROR / EVERY / AFTER (C15, C16) --- */
    case AST_ON_ERROR:
        if (node->left && node->left->has_ident && node->left->value.ident) {
            int str_idx = gfa_bytecode_add_string(ctx->bc, node->left->value.ident);
            int instr_idx = cg_emit_int(ctx, OP_ON_ERROR, -1);
            ctx->bc->code[instr_idx].has_operand2 = 1;
            ctx->bc->code[instr_idx].operand2.int_val2 = str_idx;
        }
        break;
    case AST_EVERY:
        if (node->left) { cg_expression(ctx, node->left);
            if (node->left->right && node->left->right->has_ident) {
                int str_idx = gfa_bytecode_add_string(ctx->bc, node->left->right->value.ident);
                cg_emit_int(ctx, OP_EVERY, (os_int32)str_idx);
            } }
        break;
    case AST_AFTER:
        if (node->left) { cg_expression(ctx, node->left);
            if (node->left->right && node->left->right->has_ident) {
                int str_idx = gfa_bytecode_add_string(ctx->bc, node->left->right->value.ident);
                cg_emit_int(ctx, OP_AFTER, (os_int32)str_idx);
            } }
        break;

    /* --- Graphic attrs (C11) --- */
    case AST_DEFFILL: case AST_DEFLINE: case AST_DEFTEXT:
    case AST_DEFMOUSE: case AST_DEFMARK:
        break; /* no-op: state is stored elsewhere */

    case AST_PRINT_AT:
        if (node->left) {
            ast_node *x, *y, *exp;
            x = node->left;
            y = x->right;
            exp = (y != NULL) ? y->right : NULL;
            if (x != NULL && y != NULL && exp != NULL) {
                cg_expression(ctx, x);
                cg_expression(ctx, y);
                cg_expression(ctx, exp);
                cg_emit(ctx, OP_PRINT_AT);
            }
        }
        break;

    case AST_PRINT_USING:
        if (node->left) {
            ast_node *fmt, *exp;
            fmt = node->left;
            exp = fmt->right;
            if (fmt != NULL && exp != NULL) {
                cg_expression(ctx, fmt);
                cg_expression(ctx, exp);
                cg_emit(ctx, OP_PRINT_USING);
            }
        }
        break;

    /* --- Not yet implemented / no bytecode needed --- */
    /* ------------------------------------------------------------ */
    /* Nouvelles instructions (2026-08)                              */
    /* ------------------------------------------------------------ */
    case AST_ERASE:
        {
            ast_node *ch;
            for (ch = node->left; ch != NULL; ch = ch->right) {
                if (ch->has_ident && ch->value.ident) {
                    gfa_variable *v = cg_resolve_var(ctx,
                        ch->value.ident);
                    if (v != NULL)
                        cg_emit_ptr(ctx, OP_ERASE_VAR, (void *)v);
                }
            }
        }
        break;
    case AST_CLEAR:
        cg_emit(ctx, OP_CLEAR_ALL);
        break;
    case AST_QUIT:
        if (node->left != NULL)
            cg_expression(ctx, node->left);  /* code de sortie */
        cg_emit(ctx, OP_QUIT);
        break;
    case AST_QSORT_STMT:
        {
            gfa_variable *v = NULL;
            if (node->left && node->left->has_ident &&
                node->left->value.ident)
                v = cg_resolve_var(ctx, node->left->value.ident);
            if (v != NULL) {
                if (node->body) cg_expression(ctx, node->body);   /* lo */
                if (node->cond) cg_expression(ctx, node->cond);   /* hi */
                cg_emit_ptr(ctx,
                    (node->value.int_val != 0) ? OP_SSORT : OP_QSORT,
                    (void *)v);
            }
        }
        break;
    case AST_INSERT_ELEM:
        {
            gfa_variable *v = NULL;
            if (node->left && node->left->has_ident &&
                node->left->value.ident)
                v = cg_resolve_var(ctx, node->left->value.ident);
            if (v != NULL) {
                if (node->body) cg_expression(ctx, node->body);   /* idx */
                if (node->cond) cg_expression(ctx, node->cond);   /* val */
                cg_emit_ptr(ctx, OP_INSERT_ELEM, (void *)v);
            }
        }
        break;
    case AST_DELETE_ELEM:
        {
            gfa_variable *v = NULL;
            if (node->left && node->left->has_ident &&
                node->left->value.ident)
                v = cg_resolve_var(ctx, node->left->value.ident);
            if (v != NULL) {
                if (node->body) cg_expression(ctx, node->body);   /* idx */
                cg_emit_ptr(ctx, OP_DELETE_ELEM, (void *)v);
            }
        }
        break;
    case AST_DRAW:
        if (node->value.int_val != 0) {
            /* DRAW(n) : interrogation */
            if (node->body) cg_expression(ctx, node->body);
            cg_emit(ctx, OP_DRAW_QUERY);
            cg_emit(ctx, OP_POP);  /* statement : resultat jete */
        } else {
            /* DRAW "prog" : turtle */
            if (node->body) cg_expression(ctx, node->body);
            cg_emit(ctx, OP_DRAW_TURTLE);
        }
        break;
    case AST_WINDOW:
        {
            int sub = (int)node->value.int_val;
            int i;
            if (sub == 5) {
                /* GETSIZE n,x,y,w,h : les args sont des identifiants */
                ast_node *idn = NULL, *idy = NULL, *idw = NULL,
                         *idh = NULL;
                if (node->args && node->arg_count >= 5) {
                    idn = node->args[0]; idy = node->args[1];
                    idw = node->args[2]; idh = node->args[3];
                }
                if (idn != NULL)
                    cg_expression(ctx, idn);  /* n */
                cg_emit_int(ctx, OP_WINDOW_STMT, (os_int32)sub);
                /* Pile : [w][h][y][x] (x au sommet) */
                if (idh && idh->has_ident && idh->value.ident) {
                    gfa_variable *vh = cg_resolve_var(ctx,
                        idh->value.ident);
                    cg_emit_ptr(ctx, OP_POP_STORE, (void *)vh);
                } else cg_emit(ctx, OP_POP);
                if (idw && idw->has_ident && idw->value.ident) {
                    gfa_variable *vw = cg_resolve_var(ctx,
                        idw->value.ident);
                    cg_emit_ptr(ctx, OP_POP_STORE, (void *)vw);
                } else cg_emit(ctx, OP_POP);
                if (idy && idy->has_ident && idy->value.ident) {
                    gfa_variable *vy = cg_resolve_var(ctx,
                        idy->value.ident);
                    cg_emit_ptr(ctx, OP_POP_STORE, (void *)vy);
                } else cg_emit(ctx, OP_POP);
                (void)idn;
            } else {
                for (i = 0; i < node->arg_count; i++)
                    cg_expression(ctx, node->args[i]);
                cg_emit_int(ctx, OP_WINDOW_STMT, (os_int32)sub);
            }
        }
        break;
    case AST_GFX_STMT:
        {
            int mode = (int)node->value.int_val;
            ast_node *a0 = NULL, *a1 = NULL, *a2 = NULL, *a3 = NULL,
                     *a4 = NULL;
            int idx;
            if (node->args) {
                if (node->arg_count > 0) a0 = node->args[0];
                if (node->arg_count > 1) a1 = node->args[1];
                if (node->arg_count > 2) a2 = node->args[2];
                if (node->arg_count > 3) a3 = node->args[3];
                if (node->arg_count > 4) a4 = node->args[4];
            }
            switch (mode) {
                case 10:  /* ALINE x1,y1,x2,y2 */
                    cg_expression(ctx, a0);
                    cg_expression(ctx, a1);
                    cg_expression(ctx, a3);
                    cg_expression(ctx, a2);
                    cg_emit(ctx, OP_LINE_GFX);
                    break;
                case 11:  /* HLINE y,x1,x2 : polyligne (x1,y)-(x2,y) */
                    cg_expression(ctx, a1);
                    cg_expression(ctx, a0);
                    cg_expression(ctx, a2);
                    cg_expression(ctx, a0);
                    cg_emit_float_const(ctx, OP_PUSH_CONST, (double)2);
                    idx = cg_emit(ctx, OP_POLY_GFX);
                    if (idx >= 0) ctx->bc->code[idx].operand.int_val = 0;
                    break;
                case 12:  /* RBOX x1,y1,dx,dy : rectangle relatif */
                    cg_expression(ctx, a0);
                    cg_expression(ctx, a1);
                    cg_expression(ctx, a0);
                    cg_expression(ctx, a2);
                    cg_emit(ctx, OP_ADD);
                    cg_expression(ctx, a1);
                    cg_expression(ctx, a3);
                    cg_emit(ctx, OP_ADD);
                    cg_emit(ctx, OP_BOX_GFX);
                    break;
                case 13:  /* PELLIPSE x,y,rx,ry */
                    cg_expression(ctx, a0);
                    cg_expression(ctx, a1);
                    cg_expression(ctx, a2);
                    cg_expression(ctx, a3);
                    cg_emit_float_const(ctx, OP_PUSH_CONST, (double)1);
                    cg_emit(ctx, OP_ELLIPSE_GFX);
                    break;
                case 14:  /* PLOT x,y */
                    cg_expression(ctx, a0);
                    cg_expression(ctx, a1);
                    cg_emit(ctx, OP_PLOT_GFX);
                    break;
                case 15:  /* FILL x,y[,lim] */
                    cg_expression(ctx, a0);
                    cg_expression(ctx, a1);
                    if (a2 != NULL)
                        cg_expression(ctx, a2);
                    else
                        cg_emit_float_const(ctx, OP_PUSH_CONST, (double)-1);
                    cg_emit(ctx, OP_FILL_GFX);
                    break;
                case 16:  /* ATEXT/TEXT x,y,"texte" */
                    cg_expression(ctx, a0);
                    cg_expression(ctx, a1);
                    cg_expression(ctx, a2);
                    cg_emit(ctx, OP_TEXT_GFX);
                    break;
                case 17:  /* ACHAR x,y,code */
                    cg_expression(ctx, a0);
                    cg_expression(ctx, a1);
                    cg_expression(ctx, a2);
                    cg_emit(ctx, OP_ACHAR_GFX);
                    break;
                case 18:  /* SETCOLOR n,val */
                    cg_expression(ctx, a0);
                    cg_expression(ctx, a1);
                    cg_emit(ctx, OP_SETCOLOR);
                    break;
                case 19:  /* MODE n */
                    cg_expression(ctx, a0);
                    cg_emit(ctx, OP_MODE_GFX);
                    break;
                case 20: case 21: case 22: case 23:
                    /* POLYLINE/POLYFILL/CURVE/POLYMARK n, pts&() */
                    cg_expression(ctx, a0);  /* n */
                    if (a1 != NULL && a1->has_ident && a1->value.ident) {
                        gfa_variable *av = cg_resolve_var(ctx,
                            a1->value.ident);
                        idx = cg_emit_ptr(ctx, OP_POLY_GFX,
                            (void *)av);
                        if (idx >= 0)
                            ctx->bc->code[idx].operand.int_val =
                                (os_int32)(mode - 20);
                    } else {
                        idx = cg_emit(ctx, OP_POLY_GFX);
                        if (idx >= 0)
                            ctx->bc->code[idx].operand.int_val =
                                (os_int32)(mode - 20);
                    }
                    break;
                case 24:  /* CLIP/ACLIP x1,y1,x2,y2 */
                    cg_expression(ctx, a0);
                    cg_expression(ctx, a1);
                    cg_expression(ctx, a2);
                    cg_expression(ctx, a3);
                    cg_emit(ctx, OP_CLIP_GFX);
                    break;
                case 25:  /* GET x1,y1,x2,y2,var$ */
                    if (a4 != NULL && a4->has_ident && a4->value.ident)
                        cg_emit_str(ctx, OP_PUSH_STRING,
                                    a4->value.ident);
                    cg_expression(ctx, a0);
                    cg_expression(ctx, a1);
                    cg_expression(ctx, a3);
                    cg_expression(ctx, a2);
                    cg_emit(ctx, OP_GETBIT_GFX);
                    break;
                case 26:  /* PUT x,y,var$[,mode] */
                    if (a2 != NULL && a2->has_ident && a2->value.ident)
                        cg_emit_str(ctx, OP_PUSH_STRING,
                                    a2->value.ident);
                    cg_expression(ctx, a0);
                    cg_expression(ctx, a1);
                    cg_emit(ctx, OP_PUTBIT_GFX);
                    break;
                case 27:  /* WINDOW (x0,y0),(x1,y1) */
                    cg_expression(ctx, a0);
                    cg_expression(ctx, a1);
                    cg_expression(ctx, a2);
                    cg_expression(ctx, a3);
                    cg_emit(ctx, OP_WINDOW_GFX);
                    break;
                default:
                    break;
            }
        }
        break;
    case AST_MAT:
        {
            long sub = node->value.int_val;
            const char *target = NULL;
            const char *src1 = NULL;
            const char *src2 = NULL;
            int ti = -1, si = -1;
            int instr_idx;
            gfa_opcode mop;

            if (node->left && node->left->has_ident &&
                node->left->value.ident)
                target = node->left->value.ident;
            if (node->body && node->body->has_ident &&
                node->body->value.ident)
                src1 = node->body->value.ident;
            if (node->cond && node->cond->has_ident &&
                node->cond->value.ident)
                src2 = node->cond->value.ident;
            if (target != NULL)
                ti = gfa_bytecode_add_string(ctx->bc, target);
            if (src1 != NULL)
                si = gfa_bytecode_add_string(ctx->bc, src1);
            /* Arguments de pile */
            if (sub == MAT_OP_SET && node->step != NULL)
                cg_expression(ctx, node->step);
            if (src2 != NULL)
                cg_emit_str(ctx, OP_PUSH_STRING, src2);
            /* Opcode selon la sous-operation */
            if (sub == MAT_OP_READ)      mop = OP_MAT_READ;
            else if (sub == MAT_OP_PRINT) mop = OP_MAT_PRINT;
            else if (sub == MAT_OP_CLR)   mop = OP_MAT_CLR;
            else if (sub == MAT_OP_ONE)   mop = OP_MAT_ONE;
            else if (sub == MAT_OP_CPY)   mop = OP_MAT_CPY;
            else if (sub == MAT_OP_ADD)   mop = OP_MAT_ADD;
            else if (sub == MAT_OP_SUB)   mop = OP_MAT_SUB;
            else if (sub == MAT_OP_MUL)   mop = OP_MAT_MUL;
            else if (sub == MAT_OP_TRANS) mop = OP_MAT_TRANS;
            else if (sub == MAT_OP_INV)   mop = OP_MAT_INV;
            else if (sub == MAT_OP_DET)   mop = OP_MAT_DET;
            else if (sub == MAT_OP_RANG)  mop = OP_MAT_RANG;
            else if (sub == MAT_OP_NORM)  mop = OP_MAT_NORM;
            else if (sub == MAT_OP_SET)   mop = OP_MAT_SET;
            else                          mop = OP_MAT_CLR;
            /* MAT INPUT : comme READ mais depuis la console */
            if (sub == MAT_OP_INPUT) mop = OP_MAT_INPUT;
            instr_idx = cg_emit(ctx, mop);
            if (instr_idx >= 0) {
                ctx->bc->code[instr_idx].operand.str_index = ti;
                ctx->bc->code[instr_idx].has_operand2 = 1;
                ctx->bc->code[instr_idx].operand2.index2 = si;
            }
        }
        break;

    case AST_VOID: case AST_TILDE: case AST_LET:
    case AST_CALL:
        cg_call(ctx, node);
        /* Builtin en position statement : depiler le resultat pour
           garder la pile propre (residus = faux nb d'args ensuite). */
        if (!(node->has_ident && node->value.ident)) {
            cg_emit(ctx, OP_POP);
        }
        break;
    case AST_FN_CALL:      cg_call(ctx, node); break;
    case AST_DO_LOOP:   cg_do_loop(ctx, node); break;
    case AST_EXIT_IF:   cg_exit_if(ctx, node); break;
    case AST_OPTION_BASE:
        /* OPTION BASE n : base des tableaux DIM subsequent (constant) */
        {
            ast_node *e;
            ctx->option_base = 0;
            e = node->left;
            if (e && e->type == AST_ASSIGN &&
                !e->has_ident && !e->has_str)
                ctx->option_base = (int)e->value.float_val;
        }
        break;
    case AST_REM: case AST_LINE_NUMBER:
    case AST_ON_BREAK:
    case AST_DEFBIT: case AST_DEFBYT: case AST_DEFWRD:
    case AST_DEFNUM: case AST_DEFFLT: case AST_DEFSTR: case AST_DEFDBL:
    case AST_VTAB:
        break;

    default: break;
    }
}

/* ================================================================== */
/* Expressions                                                        */
/* ================================================================== */

static void cg_expression(codegen_ctx *ctx, ast_node *node)
{
    if (node == NULL) return;

    switch (node->type) {
    case AST_ASSIGN:
        if (node->left && node->left->right) {
            /* Binary operation */
            long op = node->value.int_val; gfa_opcode bc_op;
            cg_expression(ctx, node->left);
            cg_expression(ctx, node->left->right);
            switch ((int)op) {
                case TOK_PLUS: bc_op=OP_ADD; break;
                case TOK_MINUS:bc_op=OP_SUB; break;
                case TOK_STAR: bc_op=OP_MUL; break;
                case TOK_SLASH:bc_op=OP_DIV; break;
                case TOK_DIV_OP:bc_op=OP_INT_DIV;break;
                case TOK_CARET:bc_op=OP_POW; break;
                case TOK_MOD_OP:bc_op=OP_MOD;break;
                case TOK_EQ:  bc_op=OP_EQ;  break;
                case TOK_NE:  bc_op=OP_NE;  break;
                case TOK_LT:  bc_op=OP_LT;  break;
                case TOK_GT:  bc_op=OP_GT;  break;
                case TOK_LE:  bc_op=OP_LE;  break;
                case TOK_GE:  bc_op=OP_GE;  break;
                case TOK_APPROX_EQ:bc_op=OP_APPROX_EQ;break;
                case TOK_AND_OP:bc_op=OP_AND;break;
                case TOK_OR_OP:bc_op=OP_OR; break;
                case TOK_XOR_OP:bc_op=OP_XOR;break;
                case TOK_EQV_OP:bc_op=OP_EQV;break;
                case TOK_IMP_OP:bc_op=OP_IMP;break;
                default: bc_op=OP_ADD; break;
            }
            cg_emit(ctx, bc_op);
        } else if (node->left && !node->left->right) {
            /* Unary */
            long op = node->value.int_val;
            cg_expression(ctx, node->left);
            if (op == (long)TOK_MINUS) cg_emit(ctx, OP_NEG);
            else if (op == (long)TOK_NOT_OP || op == (long)TOK_TILDE) cg_emit(ctx, OP_NOT);
        } else {
            /* Leaf */
            if (node->has_str && node->value.str_val)
                cg_emit_str(ctx, OP_PUSH_STRING, node->value.str_val);
            else if (node->has_ident && node->value.ident) {
                gfa_variable *var = cg_resolve_var(ctx, node->value.ident);
                cg_emit_ptr(ctx, OP_PUSH_VAR, (void *)var);
            } else {
                cg_emit_float_const(ctx, OP_PUSH_CONST, node->value.float_val);
            }
        }
        break;

    case AST_CALL:
    case AST_FN_CALL:
        cg_call(ctx, node);
        break;

    default:
        cg_emit_float_const(ctx, OP_PUSH_CONST, (double)0);
        break;
    }
}

/* ================================================================== */
/* Statements                                                         */
/* ================================================================== */

static void cg_assign(codegen_ctx *ctx, ast_node *node)
{
    ast_node *target, *value;
    if (!node) return;
    target = node->left;
    value = (target) ? target->right : NULL;
    if (!target) return;

    /* Array assignment: a(indices) = expr */
    if (target->type == AST_CALL && target->has_ident && target->value.ident) {
        gfa_variable *var = cg_resolve_var(ctx, target->value.ident);
        int idx;
        /* Tableau connu OU nom inconnu (cree a l'execution par MAT) :
           le runtime depile proprement et neecrit que si la variable
           est bien un tableau au moment de l'execution. */
        if ((var != NULL && var->type == GFA_VAR_ARRAY) ||
            !cg_name_is_label(ctx, target->value.ident)) {
            int i;
            for (i = 0; i < target->arg_count; i++)
                cg_expression(ctx, target->args[i]);
            cg_expression(ctx, value);
            idx = cg_emit_ptr(ctx, OP_ARRAY_STORE, (void *)var);
            if (idx >= 0) {
                ctx->bc->code[idx].has_operand2 = 1;
                ctx->bc->code[idx].operand2.index2 = target->arg_count;
            }
            return;
        }
    }

    cg_expression(ctx, value);
    if (target->has_ident && target->value.ident) {
        gfa_variable *var = cg_resolve_var(ctx, target->value.ident);
        cg_emit_ptr(ctx, OP_POP_STORE, (void *)var);
    }
}

static void cg_if(codegen_ctx *ctx, ast_node *node)
{
    int jmp_else, jmp_end;
    if (!node) return;
    cg_expression(ctx, node->cond);
    jmp_else = cg_emit_int(ctx, OP_JMP_IF_FALSE, 0);
    cg_push_patch(ctx, jmp_else);
    cg_statement(ctx, node->body);
    jmp_end = cg_emit_int(ctx, OP_JMP, 0);
    cg_pop_patch(ctx);
    if (node->else_body) cg_statement(ctx, node->else_body);
    cg_patch(ctx, jmp_end, (os_int32)cg_current(ctx));
}

static void cg_for(codegen_ctx *ctx, ast_node *node)
{
    ast_node *vn, *sn, *en;
    int loop_start, jmp_end;
    if (!node) return;
    vn = node->left;
    sn = vn ? vn->right : NULL;
    en = sn ? sn->right : NULL;
    if (!vn || !sn || !en) return;

    cg_loop_enter(ctx);
    cg_expression(ctx, sn);
    if (vn->has_ident && vn->value.ident) {
        gfa_variable *v = cg_resolve_var(ctx, vn->value.ident);
        cg_emit_ptr(ctx, OP_POP_STORE, (void *)v);
    }
    loop_start = cg_current(ctx);
    if (vn->has_ident && vn->value.ident) {
        gfa_variable *v = cg_resolve_var(ctx, vn->value.ident);
        cg_emit_ptr(ctx, OP_PUSH_VAR, (void *)v);
    }
    cg_expression(ctx, en);
    cg_emit(ctx, (node->value.int_val != 0) ? OP_LT : OP_GT);
    jmp_end = cg_emit_int(ctx, OP_JMP_IF_TRUE, 0);
    cg_push_patch(ctx, jmp_end);
    cg_statement(ctx, node->body);
    if (vn->has_ident && vn->value.ident) {
        gfa_variable *v = cg_resolve_var(ctx, vn->value.ident);
        cg_emit_ptr(ctx, OP_PUSH_VAR, (void *)v);
        if (node->step) cg_expression(ctx, node->step);
        else cg_emit_float_const(ctx, OP_PUSH_CONST,
             (node->value.int_val != 0) ? (double)-1 : (double)1);
        cg_emit(ctx, OP_ADD);
        cg_emit_ptr(ctx, OP_POP_STORE, (void *)v);
    }
    cg_emit_int(ctx, OP_JMP, (os_int32)loop_start);
    cg_pop_patch(ctx);
    cg_loop_leave(ctx);
}

static void cg_while(codegen_ctx *ctx, ast_node *node)
{
    int loop_start, jmp_end;
    if (!node) return;
    cg_loop_enter(ctx);
    loop_start = cg_current(ctx);
    cg_expression(ctx, node->cond);
    jmp_end = cg_emit_int(ctx, OP_JMP_IF_FALSE, 0);
    cg_statement(ctx, node->body);
    cg_emit_int(ctx, OP_JMP, (os_int32)loop_start);
    cg_patch(ctx, jmp_end, (os_int32)cg_current(ctx));
    cg_loop_leave(ctx);
}

static void cg_repeat(codegen_ctx *ctx, ast_node *node)
{
    int loop_start;
    if (!node) return;
    cg_loop_enter(ctx);
    loop_start = cg_current(ctx);
    cg_statement(ctx, node->body);
    cg_expression(ctx, node->cond);
    cg_emit_int(ctx, OP_JMP_IF_FALSE, (os_int32)loop_start);
    cg_loop_leave(ctx);
}

/* ------------------------------------------------------------------ */
/* EXIT IF / DO...LOOP : points de sortie des boucles imbriquees      */
/* ------------------------------------------------------------------ */

static void cg_loop_enter(codegen_ctx *ctx)
{
    if (ctx->exit_depth < MAX_PATCHES) {
        ctx->exit_stack[ctx->exit_depth] = 0; /* marqueur (valeur non usee) */
        ctx->exit_patch_base[ctx->exit_depth] = ctx->exit_patch_count;
        ctx->exit_depth++;
    }
}

static void cg_loop_leave(codegen_ctx *ctx)
{
    int i;
    int pos;
    int base_patches;
    if (ctx->exit_depth == 0) return;
    ctx->exit_depth--;
    base_patches = ctx->exit_patch_base[ctx->exit_depth];
    pos = cg_current(ctx);
    for (i = base_patches; i < ctx->exit_patch_count; i++)
        cg_patch(ctx, ctx->exit_patches[i], (os_int32)pos);
    ctx->exit_patch_count = base_patches;
}

static void cg_exit_if(codegen_ctx *ctx, ast_node *node)
{
    int idx;
    if (ctx->exit_depth == 0) return;  /* hors boucle : ignore */
    if (node && node->cond)
        cg_expression(ctx, node->cond);
    idx = cg_emit_int(ctx,
        (node && node->cond) ? OP_JMP_IF_TRUE : OP_JMP, 0);
    if (idx >= 0 && ctx->exit_patch_count < MAX_PATCHES)
        ctx->exit_patches[ctx->exit_patch_count++] = idx;
}

static void cg_do_loop(codegen_ctx *ctx, ast_node *node)
{
    int loop_start;
    int cond_idx;
    int is_until;
    if (!node) return;
    /* node->value.int_val : 0 = LOOP WHILE, 1 = LOOP UNTIL */
    is_until = (node->value.int_val != 0) ? 1 : 0;
    cg_loop_enter(ctx);
    loop_start = cg_current(ctx);
    cg_statement(ctx, node->body);
    if (node->cond) {
        /* LOOP WHILE c : continuer si c vrai  (sortie si c faux)
           LOOP UNTIL c : continuer si c faux (sortie si c vrai) */
        cg_expression(ctx, node->cond);
        cond_idx = cg_emit_int(ctx,
            is_until ? OP_JMP_IF_TRUE : OP_JMP_IF_FALSE, 0);
        if (cond_idx >= 0 && ctx->exit_patch_count < MAX_PATCHES)
            ctx->exit_patches[ctx->exit_patch_count++] = cond_idx;
    }
    cg_emit_int(ctx, OP_JMP, (os_int32)loop_start);
    cg_loop_leave(ctx);
}

static void cg_select(codegen_ctx *ctx, ast_node *node)
{
    ast_node *case_node;
    int end_patches[32]; int end_count = 0; int i;
    if (!node) return;
    case_node = (node->body) ? node->body->left : NULL;
    while (case_node) {
        if (case_node->type == AST_CASE) {
            int jmp_next;
            cg_expression(ctx, node->cond);
            cg_expression(ctx, case_node->cond);
            /* SELECT/CASE uses OP_EQ (strict equality) to match
               GFA Basic 3.5 semantics; approximate == is only for
               IF/WHILE/UNTIL comparisons */
            cg_emit(ctx, OP_EQ);
            jmp_next = cg_emit_int(ctx, OP_JMP_IF_FALSE, 0);
            cg_statement(ctx, case_node->body);
            if (end_count < 32) end_patches[end_count++] = cg_emit_int(ctx, OP_JMP, 0);
            cg_patch(ctx, jmp_next, (os_int32)cg_current(ctx));
        } else if (case_node->type == AST_DEFAULT_CASE) {
            cg_statement(ctx, case_node->body);
            if (end_count < 32) end_patches[end_count++] = cg_emit_int(ctx, OP_JMP, 0);
        }
        case_node = case_node->right;
    }
    for (i = 0; i < end_count; i++)
        cg_patch(ctx, end_patches[i], (os_int32)cg_current(ctx));
}

static int cg_is_print_separator(ast_node *n)
{
    if (n == NULL) return 0;
    /* Separateur PRINT : noeud dedie cree par le parser (0 = ';', 1 = ',') */
    return (n->type == AST_PRINT_SEP) ? 1 : 0;
}

static void cg_print(codegen_ctx *ctx, ast_node *node)
{
    ast_node *arg;
    int has_channel;
    if (!node) return;
    has_channel = (node->value.int_val != 0) ? 1 : 0;
    arg = node->left;
    if (has_channel) {
        /* PRINT #n : first child = channel, rest = expressions */
        if (arg) {
            ast_node *expr = arg->right;
            while (expr) {
                if (!cg_is_print_separator(expr)) {
                    cg_expression(ctx, arg);    /* push channel */
                    cg_expression(ctx, expr);   /* push value */
                    cg_emit(ctx, OP_PRINT_CHAN);
                }
                expr = expr->right;
            }
        }
    } else {
        while (arg) {
            if (!cg_is_print_separator(arg)) {
                int has_semicolon;
                ast_node *next;
                cg_expression(ctx, arg);
                cg_emit(ctx, OP_PRINT);
                next = arg->right;
                /* Check if next sibling is a ; separator (suppress newline) */
                has_semicolon = (next != NULL && cg_is_print_separator(next)
                                 && next->value.float_val == 0.0);
                if (!has_semicolon) {
                    cg_emit(ctx, OP_PRINT_NL);
                }
            }
            arg = arg->right;
        }
        if (!node->left) { cg_emit_str(ctx, OP_PUSH_STRING, ""); cg_emit(ctx, OP_PRINT); cg_emit(ctx, OP_PRINT_NL); }
    }
}

static int cg_name_is_label(codegen_ctx *ctx, const char *name)
{
    int i;
    if (ctx->labels == NULL || name == NULL) return 0;
    for (i = 0; i < ctx->label_count; i++) {
        if (ctx->labels[i].name != NULL &&
            strieq(ctx->labels[i].name, name))
            return 1;
    }
    return 0;
}

static void cg_call(codegen_ctx *ctx, ast_node *node)
{
    int i;
    if (!node) return;
    if (node->args) {
        for (i = 0; i < node->arg_count; i++)
            cg_expression(ctx, node->args[i]);
    }
    if (node->has_ident && node->value.ident) {
        /* Fonction utilisateur (DEF FN / PROCEDURE) ou acces tableau */
        gfa_variable *var = cg_resolve_var(ctx, node->value.ident);
        int idx;
        if (var != NULL && var->type == GFA_VAR_ARRAY) {
            idx = cg_emit_ptr(ctx, OP_ARRAY_LOAD, (void *)var);
            if (idx >= 0) {
                ctx->bc->code[idx].has_operand2 = 1;
                ctx->bc->code[idx].operand2.index2 = node->arg_count;
            }
        } else if (cg_name_is_label(ctx, node->value.ident)) {
            /* Fonction utilisateur (DEF FN / PROCEDURE) */
            int str_idx = gfa_bytecode_add_string(ctx->bc, node->value.ident);
            idx = cg_emit_int(ctx, OP_CALL, -1);
            if (idx >= 0) {
                ctx->bc->code[idx].has_operand2 = 1;
                ctx->bc->code[idx].operand2.int_val2 = str_idx;
            }
        } else {
            /* Nom inconnu : tableau cree a l'execution (ex : MAT b = a
               avant DIM b). Le pointeur est stable (le runtime convertit
               la variable sur place) ; si ce n'est pas un tableau,
               OP_ARRAY_LOAD retourne 0. */
            idx = cg_emit_ptr(ctx, OP_ARRAY_LOAD, (void *)var);
            if (idx >= 0) {
                ctx->bc->code[idx].has_operand2 = 1;
                ctx->bc->code[idx].operand2.index2 = node->arg_count;
            }
        }
    } else {
        /* Built-in function */
        int tok;
        int op;
        int instr_idx;
        tok = (int)node->value.int_val;
        /* GEMDOS()/BIOS()/XBIOS() ont des opcodes dedies : le runtime
           attend [fn] [arg1] [arg2] sur la pile. */
        if (tok == TOK_GEMDOS)      op = OP_GEMDOS;
        else if (tok == TOK_BIOS)   op = OP_BIOS;
        else if (tok == TOK_XBIOS)  op = OP_XBIOS;
        else                        op = OP_CALL_BUILTIN;
        instr_idx = cg_emit_int(ctx, op, (os_int32)tok);
        if (op == OP_CALL_BUILTIN && instr_idx >= 0) {
            /* operand2 = nombre d'arguments (le runtime consomme
               exactement ces arguments et garantit un resultat unique
               sur la pile). */
            ctx->bc->code[instr_idx].has_operand2 = 1;
            ctx->bc->code[instr_idx].operand2.int_val2 = node->arg_count;
        }
    }
}
