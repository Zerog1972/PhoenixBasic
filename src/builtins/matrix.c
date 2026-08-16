/*
 * matrix.c - Operations matricielles GFA Basic 3.5 (MAT)
 * ========================================================
 * Les matrices sont des gfa_variable de type GFA_VAR_ARRAY
 * (2D, double precision, is_matrix = 1).
 *
 * C89 strict.
 */

#include "matrix.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../utils/os_layer.h"

/* ------------------------------------------------------------------ */
/* Recherche / creation                                                */
/* ------------------------------------------------------------------ */

gfa_variable *gfa_matrix_get(gfa_symbol_table *tab, const char *name)
{
    gfa_variable *v;

    if (tab == NULL || name == NULL) return NULL;
    v = gfa_var_lookup(tab, name);
    if (v == NULL) return NULL;
    if (v->type != GFA_VAR_ARRAY) return NULL;
    if (!v->value.arr.is_matrix) return NULL;
    if (v->value.arr.num_dims != 2 || v->value.arr.data == NULL) return NULL;
    return v;
}

gfa_variable *gfa_matrix_ensure(gfa_symbol_table *tab, const char *name,
                                int rows, int cols)
{
    gfa_variable *v;
    double *data;
    os_int32 *sizes;

    if (rows < 1) rows = 1;
    if (cols < 1) cols = 1;

    v = gfa_var_lookup(tab, name);
    if (v != NULL && v->type != GFA_VAR_ARRAY) {
        /* Variable scalaire : la convertir en matrice SUR PLACE
           (le codegen conserve des pointeurs sur les variables,
           une suppression/recreeation les rendrait orphelins). */
        if (v->type == GFA_VAR_STRING && v->value.str.data != NULL)
            os_mem_free(v->value.str.data);
        memset(&v->value, 0, sizeof(v->value));
        v->type = GFA_VAR_ARRAY;
    }
    if (v != NULL) {
        /* Redimensionnement (donnees reinitialisees) */
        if (v->value.arr.dim_sizes != NULL) {
            os_mem_free(v->value.arr.dim_sizes);
            v->value.arr.dim_sizes = NULL;
        }
        if (v->value.arr.data != NULL) {
            os_mem_free(v->value.arr.data);
            v->value.arr.data = NULL;
        }
    } else {
        v = gfa_var_create(tab, name, GFA_VAR_ARRAY);
        if (v == NULL) return NULL;
    }

    data = (double *)os_mem_alloc((size_t)rows * (size_t)cols * sizeof(double));
    if (data == NULL) return v;
    memset(data, 0, (size_t)rows * (size_t)cols * sizeof(double));

    /* dim_sizes doit etre alloue sur le tas : le pointeur est conserve
       dans la variable et lu plus tard (mat_rows/mat_cols). */
    sizes = (os_int32 *)os_mem_alloc(2 * sizeof(os_int32));
    if (sizes == NULL) {
        os_mem_free(data);
        return v;
    }
    sizes[0] = (os_int32)rows;
    sizes[1] = (os_int32)cols;
    v->value.arr.elem_type      = GFA_VAR_FLOAT;
    v->value.arr.num_dims       = 2;
    v->value.arr.dim_sizes      = sizes;
    v->value.arr.data           = (char *)data;
    v->value.arr.total_elements = (os_int32)rows * (os_int32)cols;
    v->value.arr.element_size   = (os_int32)sizeof(double);
    v->value.arr.base           = 0;
    v->value.arr.is_matrix      = 1;
    return v;
}

/* ------------------------------------------------------------------ */
/* Helpers internes                                                    */
/* ------------------------------------------------------------------ */

static double *mat_data(gfa_variable *m)
{
    return (double *)m->value.arr.data;
}

static int mat_rows(gfa_variable *m)
{
    return (int)m->value.arr.dim_sizes[0];
}

static int mat_cols(gfa_variable *m)
{
    return (int)m->value.arr.dim_sizes[1];
}

static double mat_get(gfa_variable *m, int r, int c)
{
    return mat_data(m)[(size_t)r * (size_t)mat_cols(m) + (size_t)c];
}

static void mat_set(gfa_variable *m, int r, int c, double v)
{
    mat_data(m)[(size_t)r * (size_t)mat_cols(m) + (size_t)c] = v;
}

/* ------------------------------------------------------------------ */
/* Operations elementaires                                             */
/* ------------------------------------------------------------------ */

static int mat_scale(gfa_variable *dst, double v)
{
    int r, c;
    for (r = 0; r < mat_rows(dst); r++)
        for (c = 0; c < mat_cols(dst); c++)
            mat_set(dst, r, c, v);
    return 0;
}

/* Addition : dst = a + b (memes dimensions) */
static gfa_variable *mat_add(gfa_symbol_table *tab, const char *name,
                             gfa_variable *a, gfa_variable *b)
{
    int rows, cols, r, c;
    gfa_variable *dst;

    rows = mat_rows(a);
    cols = mat_cols(a);
    if (mat_rows(b) != rows || mat_cols(b) != cols) return NULL;
    dst = gfa_matrix_ensure(tab, name, rows, cols);
    if (dst == NULL) return NULL;
    for (r = 0; r < rows; r++)
        for (c = 0; c < cols; c++)
            mat_set(dst, r, c, mat_get(a, r, c) + mat_get(b, r, c));
    return dst;
}

static gfa_variable *mat_sub(gfa_symbol_table *tab, const char *name,
                             gfa_variable *a, gfa_variable *b)
{
    int rows, cols, r, c;
    gfa_variable *dst;

    rows = mat_rows(a);
    cols = mat_cols(a);
    if (mat_rows(b) != rows || mat_cols(b) != cols) return NULL;
    dst = gfa_matrix_ensure(tab, name, rows, cols);
    if (dst == NULL) return NULL;
    for (r = 0; r < rows; r++)
        for (c = 0; c < cols; c++)
            mat_set(dst, r, c, mat_get(a, r, c) - mat_get(b, r, c));
    return dst;
}

/* Multiplication : dst = a * b (cols(a) = rows(b)) */
static gfa_variable *mat_mul(gfa_symbol_table *tab, const char *name,
                             gfa_variable *a, gfa_variable *b)
{
    int ra, ca, rb, cb, r, c, k;
    gfa_variable *dst;

    ra = mat_rows(a); ca = mat_cols(a);
    rb = mat_rows(b); cb = mat_cols(b);
    if (ca != rb) return NULL;
    dst = gfa_matrix_ensure(tab, name, ra, cb);
    if (dst == NULL) return NULL;
    for (r = 0; r < ra; r++) {
        for (c = 0; c < cb; c++) {
            double s = 0.0;
            for (k = 0; k < ca; k++)
                s += mat_get(a, r, k) * mat_get(b, k, c);
            mat_set(dst, r, c, s);
        }
    }
    return dst;
}

static gfa_variable *mat_trans(gfa_symbol_table *tab, const char *name,
                               gfa_variable *a)
{
    int r, c;
    gfa_variable *dst;
    dst = gfa_matrix_ensure(tab, name, mat_cols(a), mat_rows(a));
    if (dst == NULL) return NULL;
    for (r = 0; r < mat_rows(a); r++)
        for (c = 0; c < mat_cols(a); c++)
            mat_set(dst, c, r, mat_get(a, r, c));
    return dst;
}

/* Determinant (Laplace / elimination) */
static double mat_det_val(gfa_variable *a)
{
    int n = mat_rows(a);
    double m[32][32];
    int i, j, k, piv;
    double sign = 1.0, det;

    if (n != mat_cols(a)) return 0.0;
    if (n > 32) return 0.0;
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++)
            m[i][j] = mat_get(a, i, j);

    det = 1.0;
    for (k = 0; k < n - 1; k++) {
        piv = k;
        while (piv < n && fabs(m[piv][k]) < 1e-12) piv++;
        if (piv >= n) return 0.0;
        if (piv != k) {
            for (j = 0; j < n; j++) {
                double t = m[k][j];
                m[k][j] = m[piv][j];
                m[piv][j] = t;
            }
            sign = -sign;
        }
        det *= m[k][k];
        for (i = k + 1; i < n; i++) {
            double f = m[i][k] / m[k][k];
            for (j = k; j < n; j++)
                m[i][j] -= f * m[k][j];
        }
    }
    det *= m[n - 1][n - 1];
    det *= sign;
    return det;
}

/* Inversee (Gauss-Jordan) : NULL si singuliere */
static gfa_variable *mat_inv(gfa_symbol_table *tab, const char *name,
                             gfa_variable *a)
{
    int n = mat_rows(a);
    double m[32][64];
    int i, j, k, piv;
    gfa_variable *dst;

    if (n != mat_cols(a)) return NULL;
    if (n > 32) return NULL;
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            m[i][j] = mat_get(a, i, j);
            m[i][n + j] = (i == j) ? 1.0 : 0.0;
        }
    }
    for (k = 0; k < n; k++) {
        piv = k;
        while (piv < n && fabs(m[piv][k]) < 1e-12) piv++;
        if (piv >= n) return NULL; /* singuliere */
        if (piv != k) {
            for (j = 0; j < 2 * n; j++) {
                double t = m[k][j];
                m[k][j] = m[piv][j];
                m[piv][j] = t;
            }
        }
        {
            double d = m[k][k];
            for (j = 0; j < 2 * n; j++) m[k][j] /= d;
        }
        for (i = 0; i < n; i++) {
            if (i == k) continue;
            {
                double f = m[i][k];
                if (f != 0.0) {
                    for (j = 0; j < 2 * n; j++)
                        m[i][j] -= f * m[k][j];
                }
            }
        }
    }
    dst = gfa_matrix_ensure(tab, name, n, n);
    if (dst == NULL) return NULL;
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++)
            mat_set(dst, i, j, m[i][n + j]);
    return dst;
}

/* Rang (elimination gaussienne) */
static int mat_rang_val(gfa_variable *a)
{
    int n = mat_rows(a);
    int p = mat_cols(a);
    double m[32][32];
    int i, j, k, rank, piv;

    if (n > 32 || p > 32) return 0;
    for (i = 0; i < n; i++)
        for (j = 0; j < p; j++)
            m[i][j] = mat_get(a, i, j);
    rank = 0;
    for (k = 0; k < p && rank < n; k++) {
        piv = rank;
        while (piv < n && fabs(m[piv][k]) < 1e-12) piv++;
        if (piv >= n) continue;
        {
            int jj;
            for (jj = 0; jj < p; jj++) {
                double t = m[rank][jj];
                m[rank][jj] = m[piv][jj];
                m[piv][jj] = t;
            }
        }
        for (i = rank + 1; i < n; i++) {
            double f = m[i][k] / m[rank][k];
            for (j = k; j < p; j++)
                m[i][j] -= f * m[rank][j];
        }
        rank++;
    }
    return rank;
}

/* Norme euclidienne */
static double mat_norm_val(gfa_variable *a)
{
    int r, c;
    double s = 0.0;
    for (r = 0; r < mat_rows(a); r++)
        for (c = 0; c < mat_cols(a); c++)
            s += mat_get(a, r, c) * mat_get(a, r, c);
    return sqrt(s);
}

/* ------------------------------------------------------------------ */
/* Affichage / lecture                                                 */
/* ------------------------------------------------------------------ */

static void mat_fprint(double v)
{
    char buf[32];
    if (v == (double)(long)v && fabs(v) < 1e15) {
        snprintf(buf, sizeof(buf), "%ld", (long)v);
    } else {
        snprintf(buf, sizeof(buf), "%g", v);
    }
    os_con_output_string(buf);
}

int gfa_matrix_print(gfa_runtime *rt, gfa_variable *m)
{
    int r, c;
    (void)rt;
    if (m == NULL) return 17;
    for (r = 0; r < mat_rows(m); r++) {
        os_con_output_string("  ");
        for (c = 0; c < mat_cols(m); c++) {
            mat_fprint(mat_get(m, r, c));
            os_con_output_string("   ");
        }
        os_con_output_string("\n");
    }
    return 0;
}

int gfa_matrix_scalar(gfa_runtime *rt, int sub_op, const char *src)
{
    gfa_symbol_table *tab;
    gfa_variable *a;
    double v;
    char buf[64];

    tab = rt ? rt->globals : NULL;
    if (tab == NULL) return 17;
    a = gfa_matrix_get(tab, src);
    if (a == NULL) return 17;
    if (sub_op == MAT_OP_DET) {
        v = mat_det_val(a);
    } else if (sub_op == MAT_OP_RANG) {
        v = (double)mat_rang_val(a);
    } else if (sub_op == MAT_OP_NORM) {
        v = mat_norm_val(a);
    } else {
        return 17;
    }
    if (v == (double)(long)v)
        snprintf(buf, sizeof(buf), "%ld", (long)v);
    else
        snprintf(buf, sizeof(buf), "%g", v);
    os_con_output_string(buf);
    os_con_output_string("\n");
    return 0;
}

int gfa_matrix_read_data(gfa_runtime *rt, gfa_variable *m)
{
    int r, c;
    if (m == NULL) return 17;
    for (r = 0; r < mat_rows(m); r++) {
        for (c = 0; c < mat_cols(m); c++) {
            if (rt->data_ptr >= rt->data_count) return 17;
            mat_set(m, r, c, rt->data_values[rt->data_ptr]);
            rt->data_ptr++;
        }
    }
    return 0;
}

/* Lit une ligne de la console et la convertit en double.
   Retourne 1 si reussi, 0 si echec. */
static int mat_read_line_double(const char *prompt, double *out)
{
    char buf[256];
    int i = 0;
    int ch;

    os_con_output_string(prompt);
    while ((ch = os_con_input_char()) != '\n' && ch != '\r' &&
           ch != -1 && i < (int)sizeof(buf) - 1) {
        buf[i++] = (char)ch;
    }
    buf[i] = '\0';
    {
        char *end = NULL;
        double v = strtod(buf, &end);
        if (end == buf) return 0;
        *out = v;
    }
    os_con_output_string("\n");
    return 1;
}

int gfa_matrix_input(gfa_runtime *rt, gfa_variable *m)
{
    int r, c;
    double d;
    (void)rt;
    if (m == NULL) return 17;
    for (r = 0; r < mat_rows(m); r++) {
        for (c = 0; c < mat_cols(m); c++) {
            char prompt[64];
            snprintf(prompt, sizeof(prompt), "  (%d,%d) ", r + 1, c + 1);
            if (!mat_read_line_double(prompt, &d)) return 17;
            mat_set(m, r, c, d);
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Dispatch                                                            */
/* ------------------------------------------------------------------ */

int gfa_matrix_exec(gfa_runtime *rt, int sub_op, const char *target,
                    const char *src1, const char *src2,
                    int has_value, double value)
{
    gfa_symbol_table *tab;
    gfa_variable *a, *b;

    if (rt == NULL) return 17;
    tab = rt->globals;

    switch (sub_op) {

    case MAT_OP_CLR:
        a = gfa_matrix_get(tab, target);
        if (a == NULL) return 17;
        return mat_scale(a, 0.0);

    case MAT_OP_ONE:
        {
            int n, i, j;
            a = gfa_matrix_get(tab, target);
            if (a == NULL) return 17;
            n = mat_rows(a);
            if (n != mat_cols(a)) n = mat_cols(a);
            for (i = 0; i < mat_rows(a); i++)
                for (j = 0; j < mat_cols(a); j++)
                    mat_set(a, i, j, (i == j && j < n) ? 1.0 : 0.0);
            return 0;
        }

    case MAT_OP_SET:
        a = gfa_matrix_get(tab, target);
        if (a == NULL) return 17;
        if (!has_value) value = 0.0;
        return mat_scale(a, value);

    case MAT_OP_READ:
        a = gfa_matrix_get(tab, target);
        if (a == NULL) return 17;
        return gfa_matrix_read_data(rt, a);

    case MAT_OP_INPUT:
        a = gfa_matrix_get(tab, target);
        if (a == NULL) return 17;
        return gfa_matrix_input(rt, a);

    case MAT_OP_PRINT:
        a = gfa_matrix_get(tab, target);
        return gfa_matrix_print(rt, a);

    case MAT_OP_BASE:
        /* MAT BASE n : base des indices (conservee dans la table) */
        tab->option_base = (value >= 1.0) ? 1 : 0;
        return 0;

    case MAT_OP_CPY:
        a = gfa_matrix_get(tab, src1);
        if (a == NULL) return 17;
        {
            int r, cc;
            gfa_variable *dst;
            dst = gfa_matrix_ensure(tab, target, mat_rows(a), mat_cols(a));
            if (dst == NULL) return 17;
            for (r = 0; r < mat_rows(a); r++)
                for (cc = 0; cc < mat_cols(a); cc++)
                    mat_set(dst, r, cc, mat_get(a, r, cc));
            return 0;
        }

    case MAT_OP_ADD:
        a = gfa_matrix_get(tab, src1);
        b = gfa_matrix_get(tab, src2);
        if (a == NULL || b == NULL) return 17;
        if (mat_add(tab, target, a, b) == NULL) return 13;
        return 0;

    case MAT_OP_SUB:
        a = gfa_matrix_get(tab, src1);
        b = gfa_matrix_get(tab, src2);
        if (a == NULL || b == NULL) return 17;
        if (mat_sub(tab, target, a, b) == NULL) return 13;
        return 0;

    case MAT_OP_MUL:
        a = gfa_matrix_get(tab, src1);
        b = gfa_matrix_get(tab, src2);
        if (a == NULL || b == NULL) return 17;
        if (mat_mul(tab, target, a, b) == NULL) return 13;
        return 0;

    case MAT_OP_TRANS:
        a = gfa_matrix_get(tab, src1);
        if (a == NULL) return 17;
        if (mat_trans(tab, target, a) == NULL) return 17;
        return 0;

    case MAT_OP_INV:
        a = gfa_matrix_get(tab, src1);
        if (a == NULL) return 17;
        if (mat_inv(tab, target, a) == NULL) return 11; /* singuliere */
        return 0;

    case MAT_OP_DET:
        a = gfa_matrix_get(tab, src1);
        if (a == NULL) return 17;
        b = gfa_matrix_ensure(tab, target, 1, 1);
        if (b == NULL) return 17;
        mat_set(b, 0, 0, mat_det_val(a));
        return 0;

    case MAT_OP_RANG:
        a = gfa_matrix_get(tab, src1);
        if (a == NULL) return 17;
        b = gfa_matrix_ensure(tab, target, 1, 1);
        if (b == NULL) return 17;
        mat_set(b, 0, 0, (double)mat_rang_val(a));
        return 0;

    case MAT_OP_NORM:
        a = gfa_matrix_get(tab, src1);
        if (a == NULL) return 17;
        b = gfa_matrix_ensure(tab, target, 1, 1);
        if (b == NULL) return 17;
        mat_set(b, 0, 0, mat_norm_val(a));
        return 0;

    default:
        return 17;
    }
}
