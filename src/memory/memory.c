/*
 * memory.c - Gestion des variables et de la memoire GFA Basic 3.5
 * ===============================================================
 * Implementation de la table de symboles, creation/recherche/suppression
 * de variables, gestion des tableaux multidimensionnels.
 *
 * Reference : cahier-des-charges-gfabasic.md, sections 6.1-6.3, 8.5
 */

#include "runtime.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Fonction de hachage pour les noms de variables                     */
/* ------------------------------------------------------------------ */

static unsigned int hash_string(const char *name, int num_buckets)
{
    unsigned int h;
    const char *p;

    h = 5381;
    if (name != NULL) {
        for (p = name; *p != '\0'; p++) {
            h = ((h << 5) + h) + (unsigned int)(unsigned char)(*p);
        }
    }

    return h % (unsigned int)num_buckets;
}

/* ------------------------------------------------------------------ */
/* Table de symboles                                                  */
/* ------------------------------------------------------------------ */

gfa_symbol_table *gfa_symbol_table_init(int num_buckets)
{
    gfa_symbol_table *table;
    int i;

    if (num_buckets < 1) num_buckets = 64;

    table = (gfa_symbol_table *)os_mem_alloc(sizeof(gfa_symbol_table));
    if (table == NULL) return NULL;

    table->buckets = (gfa_variable **)os_mem_alloc(
        (size_t)num_buckets * sizeof(gfa_variable *));
    if (table->buckets == NULL) {
        os_mem_free(table);
        return NULL;
    }

    for (i = 0; i < num_buckets; i++) {
        table->buckets[i] = NULL;
    }

    table->num_buckets = num_buckets;
    table->count       = 0;
    table->option_base = 0;
    table->def_type    = '#';  /* Float par defaut */

    return table;
}

void gfa_symbol_table_free(gfa_symbol_table *table)
{
    int i;

    if (table == NULL) return;

    /* Liberer toutes les variables */
    for (i = 0; i < table->num_buckets; i++) {
        gfa_variable *var;
        gfa_variable *next;

        var = table->buckets[i];
        while (var != NULL) {
            next = var->next;
            gfa_var_delete(table, var);
            var = next;
        }
    }

    os_mem_free(table->buckets);
    os_mem_free(table);
}

void gfa_symbol_table_clear_vars(gfa_symbol_table *table)
{
    int i;

    if (table == NULL) return;

    for (i = 0; i < table->num_buckets; i++) {
        gfa_variable *var;
        gfa_variable *next;

        var = table->buckets[i];
        while (var != NULL) {
            next = var->next;
            /* Ne pas supprimer les variables reservees */
            if (!var->is_reserved) {
                gfa_var_delete(table, var);
            }
            var = next;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Creation / Recherche / Suppression de variables                    */
/* ------------------------------------------------------------------ */

gfa_variable *gfa_var_create(gfa_symbol_table *table, const char *name,
                              gfa_var_type type)
{
    gfa_variable *var;
    unsigned int bucket;
    int name_len;

    if (table == NULL || name == NULL) return NULL;

    /* Verifier si la variable existe deja */
    var = gfa_var_lookup(table, name);
    if (var != NULL) {
        /* Mettre a jour le type si necessaire */
        var->type = type;
        return var;
    }

    /* Creer la nouvelle variable */
    var = (gfa_variable *)os_mem_alloc(sizeof(gfa_variable));
    if (var == NULL) return NULL;

    os_mem_set(var, 0, sizeof(gfa_variable));

    name_len = (int)strlen(name);
    var->name = (char *)os_mem_alloc((size_t)(name_len + 1));
    if (var->name == NULL) {
        os_mem_free(var);
        return NULL;
    }
    strcpy(var->name, name);
    var->name_len    = name_len;
    var->type        = type;
    var->is_global   = 1;
    var->is_reserved = 0;

    /* Initialiser selon le type */
    switch (type) {
        case GFA_VAR_BOOL:
            var->value.bool_val = 0;
            break;
        case GFA_VAR_BYTE:
            var->value.byte_val = 0;
            break;
        case GFA_VAR_WORD:
            var->value.word_val = 0;
            break;
        case GFA_VAR_LONG:
            var->value.long_val = 0;
            break;
        case GFA_VAR_FLOAT:
            var->value.float_val = 0.0;
            break;
        case GFA_VAR_STRING:
            var->value.str.data     = NULL;
            var->value.str.length   = 0;
            var->value.str.capacity = 0;
            break;
        case GFA_VAR_ARRAY:
            var->value.arr.elem_type = GFA_VAR_FLOAT;
            var->value.arr.num_dims = 0;
            var->value.arr.dim_sizes = NULL;
            var->value.arr.data = NULL;
            var->value.arr.total_elements = 0;
            var->value.arr.element_size = 8; /* float par defaut */
            break;
        default:
            break;
    }

    /* Inserer dans la table de hachage */
    bucket = hash_string(name, table->num_buckets);
    var->next = table->buckets[bucket];
    table->buckets[bucket] = var;
    table->count++;

    return var;
}

gfa_variable *gfa_var_lookup(gfa_symbol_table *table, const char *name)
{
    unsigned int bucket;
    gfa_variable *var;

    if (table == NULL || name == NULL) return NULL;

    bucket = hash_string(name, table->num_buckets);
    var    = table->buckets[bucket];

    while (var != NULL) {
        if (strcmp(var->name, name) == 0) {
            return var;
        }
        var = var->next;
    }

    return NULL;
}

void gfa_var_delete(gfa_symbol_table *table, gfa_variable *var)
{
    unsigned int bucket;
    gfa_variable *prev, *curr;

    if (table == NULL || var == NULL) return;

    bucket = hash_string(var->name, table->num_buckets);

    prev = NULL;
    curr = table->buckets[bucket];

    while (curr != NULL) {
        if (curr == var) {
            if (prev == NULL) {
                table->buckets[bucket] = curr->next;
            } else {
                prev->next = curr->next;
            }
            table->count--;
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    /* Liberer les ressources de la variable */
    if (var->name != NULL) {
        os_mem_free(var->name);
        var->name = NULL;
    }

    if (var->type == GFA_VAR_STRING) {
        if (var->value.str.data != NULL) {
            os_mem_free(var->value.str.data);
            var->value.str.data = NULL;
        }
    } else if (var->type == GFA_VAR_ARRAY) {
        if (var->value.arr.dim_sizes != NULL) {
            os_mem_free(var->value.arr.dim_sizes);
            var->value.arr.dim_sizes = NULL;
        }
        if (var->value.arr.data != NULL) {
            os_mem_free(var->value.arr.data);
            var->value.arr.data = NULL;
        }
    }

    os_mem_free(var);
}

/* ------------------------------------------------------------------ */
/* Lecture / Ecriture de variables                                    */
/* ------------------------------------------------------------------ */

double gfa_var_get_as_float(gfa_variable *var)
{
    if (var == NULL) return 0.0;

    switch (var->type) {
        case GFA_VAR_BOOL:
            return (var->value.bool_val != 0) ? -1.0 : 0.0;
        case GFA_VAR_BYTE:
            return (double)var->value.byte_val;
        case GFA_VAR_WORD:
            return (double)var->value.word_val;
        case GFA_VAR_LONG:
            return (double)var->value.long_val;
        case GFA_VAR_FLOAT:
            return var->value.float_val;
        case GFA_VAR_STRING:
            return gfa_val(var->value.str.data != NULL
                           ? var->value.str.data : "");
        default:
            return 0.0;
    }
}

os_int32 gfa_var_get_as_long(gfa_variable *var)
{
    if (var == NULL) return 0;

    switch (var->type) {
        case GFA_VAR_BOOL:
            return (var->value.bool_val != 0) ? -1 : 0;
        case GFA_VAR_BYTE:
            return (os_int32)var->value.byte_val;
        case GFA_VAR_WORD:
            return (os_int32)var->value.word_val;
        case GFA_VAR_LONG:
            return var->value.long_val;
        case GFA_VAR_FLOAT:
            return (os_int32)gfa_fix(var->value.float_val);
        case GFA_VAR_STRING:
            return (os_int32)gfa_val(var->value.str.data != NULL
                                     ? var->value.str.data : "");
        default:
            return 0;
    }
}

const char *gfa_var_get_as_string(gfa_variable *var)
{
    static char buf[256];

    if (var == NULL) return "";

    if (var->type == GFA_VAR_STRING) {
        return (var->value.str.data != NULL) ? var->value.str.data : "";
    }

    /* Convertir les types numeriques en chaine */
    {
        char *s;
        s = gfa_str_float(gfa_var_get_as_float(var));
        strncpy(buf, s, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        os_mem_free(s);
    }

    return buf;
}

void gfa_var_set_from_float(gfa_variable *var, double value)
{
    if (var == NULL) return;

    switch (var->type) {
        case GFA_VAR_BOOL:
            var->value.bool_val = (os_byte)((value != 0.0) ? 255 : 0);
            break;
        case GFA_VAR_BYTE:
            var->value.byte_val = (os_byte)((unsigned long)value & 0xFF);
            break;
        case GFA_VAR_WORD:
            var->value.word_val = (os_int16)((long)value & 0xFFFF);
            break;
        case GFA_VAR_LONG:
            var->value.long_val = (os_int32)(long)value;
            break;
        case GFA_VAR_FLOAT:
            var->value.float_val = value;
            break;
        case GFA_VAR_STRING:
            {
                char *s;
                s = gfa_str_float(value);
                gfa_var_set_from_string(var, s);
                os_mem_free(s);
            }
            break;
        default:
            break;
    }
}

void gfa_var_set_from_long(gfa_variable *var, os_int32 value)
{
    gfa_var_set_from_float(var, (double)value);
}

static void gfa_var_set_str_data(gfa_variable *var, const char *value, int len)
{
    /* Allouer ou reallouer le buffer */
    if (var->value.str.data == NULL || var->value.str.capacity < len + 1) {
        if (var->value.str.data != NULL) {
            os_mem_free(var->value.str.data);
        }
        var->value.str.capacity = len + 1;
        var->value.str.data = (char *)os_mem_alloc(
            (size_t)(var->value.str.capacity));
        if (var->value.str.data == NULL) {
            var->value.str.length = 0;
            var->value.str.capacity = 0;
            return;
        }
    }

    memcpy(var->value.str.data, value, (size_t)len);
    var->value.str.data[len] = '\0';
    var->value.str.length = len;
}

void gfa_var_set_from_string(gfa_variable *var, const char *value)
{
    int len;

    if (var == NULL || value == NULL) return;

    if (var->type != GFA_VAR_STRING) {
        /* Conversion implicite : essayer de parser en nombre */
        gfa_var_set_from_float(var, gfa_val(value));
        return;
    }

    len = (int)strlen(value);

    /* Limite GFA Basic : 32767 caracteres max */
    if (len > 32767) len = 32767;

    gfa_var_set_str_data(var, value, len);
}

/*
 * gfa_var_set_from_string_len - Affectation avec longueur explicite
 * (chaines binaires, ex : resultat de MKI$).
 */
void gfa_var_set_from_string_len(gfa_variable *var, const char *value,
                                 os_int32 len)
{
    if (var == NULL || value == NULL) return;
    if (var->type != GFA_VAR_STRING) return;
    if (len < 0) len = 0;
    if (len > 32767) len = 32767;
    gfa_var_set_str_data(var, value, (int)len);
}

void *gfa_var_get_address(gfa_variable *var)
{
    if (var == NULL) return NULL;

    switch (var->type) {
        case GFA_VAR_BOOL:   return (void *)&var->value.bool_val;
        case GFA_VAR_BYTE:   return (void *)&var->value.byte_val;
        case GFA_VAR_WORD:   return (void *)&var->value.word_val;
        case GFA_VAR_LONG:   return (void *)&var->value.long_val;
        case GFA_VAR_FLOAT:  return (void *)&var->value.float_val;
        case GFA_VAR_STRING: return (void *)var->value.str.data;
        case GFA_VAR_ARRAY:  return (void *)var->value.arr.data;
        default:             return NULL;
    }
}

/* ------------------------------------------------------------------ */
/* Gestion des tableaux                                               */
/* ------------------------------------------------------------------ */

gfa_variable *gfa_var_array_create(gfa_symbol_table *table,
                                    const char *name, gfa_var_type elem_type,
                                    int num_dims, os_int32 *dim_sizes,
                                    os_int32 base)
{
    gfa_variable *var;
    os_int32 total;
    os_int32 elem_size;
    int i;

    if (num_dims < 1 || num_dims > 7) return NULL;
    if (dim_sizes == NULL) return NULL;

    /* Calculer le nombre total d'elements et la taille */
    total = 1;
    for (i = 0; i < num_dims; i++) {
        total *= dim_sizes[i];
    }

    /* Taille d'un element selon le type */
    switch (elem_type) {
        case GFA_VAR_BOOL:   elem_size = 1; break;
        case GFA_VAR_BYTE:   elem_size = 1; break;
        case GFA_VAR_WORD:   elem_size = 2; break;
        case GFA_VAR_LONG:   elem_size = 4; break;
        case GFA_VAR_FLOAT:  elem_size = 8; break;
        case GFA_VAR_STRING: elem_size = (os_int32)sizeof(char *); break;
        default:             elem_size = 8; break;
    }

    /* Creer la variable */
    var = gfa_var_create(table, name, GFA_VAR_ARRAY);
    if (var == NULL) return NULL;

    var->value.arr.elem_type      = elem_type;
    var->value.arr.num_dims       = num_dims;
    var->value.arr.total_elements = total;
    var->value.arr.element_size   = (int)elem_size;
    var->value.arr.base           = base;

    /* Copier les tailles de dimensions */
    var->value.arr.dim_sizes = (os_int32 *)os_mem_alloc(
        (size_t)num_dims * sizeof(os_int32));
    if (var->value.arr.dim_sizes == NULL) {
        gfa_var_delete(table, var);
        return NULL;
    }
    for (i = 0; i < num_dims; i++) {
        var->value.arr.dim_sizes[i] = dim_sizes[i];
    }

    /* Allouer les donnees du tableau */
    var->value.arr.data = (char *)os_mem_alloc(
        (size_t)(total * elem_size));
    if (var->value.arr.data == NULL) {
        gfa_var_delete(table, var);
        return NULL;
    }
    os_mem_set(var->value.arr.data, 0, (size_t)(total * elem_size));

    return var;
}

void *gfa_var_array_get_element(gfa_variable *var, int *indices)
{
    os_int32 offset;
    int i;

    if (var == NULL || var->type != GFA_VAR_ARRAY) return NULL;
    if (var->value.arr.data == NULL) return NULL;
    if (indices == NULL) return NULL;

    /* Calculer l'offset lineaire */
    offset = 0;
    for (i = 0; i < var->value.arr.num_dims; i++) {
        os_int32 stride;
        int j;

        stride = 1;
        for (j = i + 1; j < var->value.arr.num_dims; j++) {
            stride *= var->value.arr.dim_sizes[j];
        }

        offset += (os_int32)indices[i] * stride;
    }

    if (offset < 0 || offset >= var->value.arr.total_elements) {
        return NULL;  /* Index hors limites */
    }

    return (void *)(var->value.arr.data +
                    (size_t)(offset * var->value.arr.element_size));
}

void gfa_var_array_fill(gfa_variable *var, double value)
{
    os_int32 i;
    char *data;

    if (var == NULL || var->type != GFA_VAR_ARRAY) return;

    data = var->value.arr.data;
    if (data == NULL) return;

    for (i = 0; i < var->value.arr.total_elements; i++) {
        void *elem;
        elem = (void *)(data + (size_t)(i * var->value.arr.element_size));

        switch (var->value.arr.elem_type) {
            case GFA_VAR_BOOL:
                *(os_byte *)elem = (os_byte)((value != 0.0) ? 255 : 0);
                break;
            case GFA_VAR_BYTE:
                *(os_byte *)elem = (os_byte)((unsigned long)value & 0xFF);
                break;
            case GFA_VAR_WORD:
                *(os_int16 *)elem = (os_int16)((long)value & 0xFFFF);
                break;
            case GFA_VAR_LONG:
                *(os_int32 *)elem = (os_int32)(long)value;
                break;
            case GFA_VAR_FLOAT:
                *(double *)elem = value;
                break;
            default:
                break;
        }
    }
}

os_int32 gfa_var_array_count(gfa_variable *var)
{
    if (var == NULL || var->type != GFA_VAR_ARRAY) return 0;
    return var->value.arr.total_elements;
}

/* ------------------------------------------------------------------ */
/* Bytecode                                                           */
/* ------------------------------------------------------------------ */

gfa_bytecode *gfa_bytecode_create(void)
{
    gfa_bytecode *bc;

    bc = (gfa_bytecode *)os_mem_alloc(sizeof(gfa_bytecode));
    if (bc == NULL) return NULL;

    bc->code = (gfa_instruction *)os_mem_alloc(
        (size_t)GFA_BYTECODE_INIT_SIZE * sizeof(gfa_instruction));
    if (bc->code == NULL) {
        os_mem_free(bc);
        return NULL;
    }

    bc->length   = 0;
    bc->capacity = GFA_BYTECODE_INIT_SIZE;
    bc->strings  = NULL;
    bc->str_count = 0;
    bc->data_values = NULL;
    bc->data_count  = 0;
    bc->data_ptr    = 0;

    return bc;
}

void gfa_bytecode_free(gfa_bytecode *bc)
{
    int i;

    if (bc == NULL) return;

    if (bc->strings != NULL) {
        for (i = 0; i < bc->str_count; i++) {
            if (bc->strings[i] != NULL) {
                os_mem_free(bc->strings[i]);
            }
        }
        os_mem_free(bc->strings);
    }

    if (bc->data_values != NULL) {
        os_mem_free(bc->data_values);
        bc->data_values = NULL;
    }

    if (bc->code != NULL) {
        os_mem_free(bc->code);
    }

    os_mem_free(bc);
}

static int bytecode_expand(gfa_bytecode *bc)
{
    int new_capacity;
    gfa_instruction *new_code;

    new_capacity = bc->capacity * 2;
    new_code = (gfa_instruction *)os_mem_realloc(bc->code,
        (size_t)new_capacity * sizeof(gfa_instruction));
    if (new_code == NULL) return -1;

    bc->code = new_code;
    bc->capacity = new_capacity;
    return 0;
}

int gfa_bytecode_emit(gfa_bytecode *bc, gfa_opcode opcode)
{
    gfa_instruction *inst;

    if (bc == NULL) return -1;

    if (bc->length >= bc->capacity) {
        if (bytecode_expand(bc) != 0) return -1;
    }

    inst = &bc->code[bc->length];
    os_mem_set(inst, 0, sizeof(gfa_instruction));
    inst->opcode = opcode;
    inst->operand.int_val = 0;
    inst->has_operand2 = 0;

    bc->length++;
    return bc->length - 1;
}

int gfa_bytecode_emit_int(gfa_bytecode *bc, gfa_opcode opcode,
                           os_int32 operand)
{
    int idx;

    idx = gfa_bytecode_emit(bc, opcode);
    if (idx >= 0) {
        bc->code[idx].operand.int_val = operand;
    }
    return idx;
}

int gfa_bytecode_emit_float(gfa_bytecode *bc, gfa_opcode opcode,
                             double operand)
{
    int idx;

    idx = gfa_bytecode_emit(bc, opcode);
    if (idx >= 0) {
        bc->code[idx].operand.float_val = operand;
    }
    return idx;
}

int gfa_bytecode_emit_str(gfa_bytecode *bc, gfa_opcode opcode,
                           const char *str)
{
    int str_idx;
    int idx;

    str_idx = gfa_bytecode_add_string(bc, str);
    idx = gfa_bytecode_emit_int(bc, opcode, (os_int32)str_idx);
    return idx;
}

int gfa_bytecode_add_string(gfa_bytecode *bc, const char *str)
{
    char **new_strings;
    int idx;

    if (bc == NULL) return -1;

    /* Check for duplicate */
    for (idx = 0; idx < bc->str_count; idx++) {
        if (bc->strings[idx] && str && strcmp(bc->strings[idx], str) == 0)
            return idx;
    }

    new_strings = (char **)os_mem_realloc(bc->strings,
        (size_t)(bc->str_count + 1) * sizeof(char *));
    if (new_strings == NULL) return -1;

    bc->strings = new_strings;
    idx = bc->str_count;
    bc->strings[idx] = gfa_str_new(str);
    bc->str_count++;

    return idx;
}

void gfa_bytecode_patch(gfa_bytecode *bc, int index, os_int32 operand)
{
    if (bc == NULL || index < 0 || index >= bc->length) return;
    bc->code[index].operand.int_val = operand;
}

int gfa_bytecode_current_ip(gfa_bytecode *bc)
{
    if (bc == NULL) return -1;
    return bc->length;
}

/* ------------------------------------------------------------------ */
/* Pile de valeurs                                                    */
/* ------------------------------------------------------------------ */

void gfa_value_push_bool(gfa_runtime *rt, int value)
{
    if (rt == NULL || rt->sp >= GFA_VALUE_STACK_SIZE) return;
    rt->value_stack[rt->sp].type = GFA_VAL_BOOL;
    rt->value_stack[rt->sp].data.b = (os_byte)(value ? 255 : 0);
    rt->value_stack[rt->sp].owns_string = 0;
    rt->sp++;
}

void gfa_value_push_byte(gfa_runtime *rt, os_byte value)
{
    if (rt == NULL || rt->sp >= GFA_VALUE_STACK_SIZE) return;
    rt->value_stack[rt->sp].type = GFA_VAL_BYTE;
    rt->value_stack[rt->sp].data.b = value;
    rt->value_stack[rt->sp].owns_string = 0;
    rt->sp++;
}

void gfa_value_push_word(gfa_runtime *rt, os_int16 value)
{
    if (rt == NULL || rt->sp >= GFA_VALUE_STACK_SIZE) return;
    rt->value_stack[rt->sp].type = GFA_VAL_WORD;
    rt->value_stack[rt->sp].data.w = value;
    rt->value_stack[rt->sp].owns_string = 0;
    rt->sp++;
}

void gfa_value_push_long(gfa_runtime *rt, os_int32 value)
{
    if (rt == NULL || rt->sp >= GFA_VALUE_STACK_SIZE) return;
    rt->value_stack[rt->sp].type = GFA_VAL_LONG;
    rt->value_stack[rt->sp].data.l = value;
    rt->value_stack[rt->sp].owns_string = 0;
    rt->sp++;
}

void gfa_value_push_float(gfa_runtime *rt, double value)
{
    if (rt == NULL || rt->sp >= GFA_VALUE_STACK_SIZE) return;
    rt->value_stack[rt->sp].type = GFA_VAL_FLOAT;
    rt->value_stack[rt->sp].data.f = value;
    rt->value_stack[rt->sp].owns_string = 0;
    rt->sp++;
}

void gfa_value_push_string(gfa_runtime *rt, char *str, int owns)
{
    gfa_value_push_string_len(rt, str, 0, owns);
}

/*
 * gfa_value_push_string_len - Pousse une chaine avec longueur explicite
 * (0 = strlen). Permet les chaines binaires (octets nuls) des
 * fonctions MKI$/MKL$/MKF$/MKD$.
 */
void gfa_value_push_string_len(gfa_runtime *rt, char *str,
                               os_int32 len, int owns)
{
    if (rt == NULL || rt->sp >= GFA_VALUE_STACK_SIZE) return;
    rt->value_stack[rt->sp].type = GFA_VAL_STRING;
    rt->value_stack[rt->sp].data.s = str;
    rt->value_stack[rt->sp].owns_string = owns;
    rt->value_stack[rt->sp].str_len = len;
    rt->sp++;
}

void gfa_value_push_addr(gfa_runtime *rt, void *addr)
{
    if (rt == NULL || rt->sp >= GFA_VALUE_STACK_SIZE) return;
    rt->value_stack[rt->sp].type = GFA_VAL_ADDRESS;
    rt->value_stack[rt->sp].data.addr = addr;
    rt->value_stack[rt->sp].owns_string = 0;
    rt->sp++;
}

gfa_value *gfa_value_pop(gfa_runtime *rt)
{
    gfa_value *val;

    if (rt == NULL || rt->sp <= 0) return NULL;

    val = (gfa_value *)os_mem_alloc(sizeof(gfa_value));
    if (val == NULL) return NULL;

    *val = rt->value_stack[--rt->sp];
    return val;
}

gfa_value *gfa_value_peek(gfa_runtime *rt, int depth)
{
    if (rt == NULL || rt->sp <= depth) return NULL;
    return &rt->value_stack[rt->sp - 1 - depth];
}

void gfa_value_discard(gfa_runtime *rt, int count)
{
    int i;

    if (rt == NULL || count <= 0) return;

    for (i = 0; i < count && rt->sp > 0; i++) {
        if (rt->value_stack[rt->sp - 1].type == GFA_VAL_STRING &&
            rt->value_stack[rt->sp - 1].owns_string &&
            rt->value_stack[rt->sp - 1].data.s != NULL) {
            os_mem_free(rt->value_stack[rt->sp - 1].data.s);
        }
        rt->sp--;
    }
}

double gfa_value_to_float(gfa_value *val)
{
    if (val == NULL) return 0.0;

    switch (val->type) {
        case GFA_VAL_BOOL:   return (val->data.b != 0) ? -1.0 : 0.0;
        case GFA_VAL_BYTE:   return (double)val->data.b;
        case GFA_VAL_WORD:   return (double)val->data.w;
        case GFA_VAL_LONG:   return (double)val->data.l;
        case GFA_VAL_FLOAT:  return val->data.f;
        case GFA_VAL_STRING: return gfa_val(val->data.s ? val->data.s : "");
        case GFA_VAL_ADDRESS:return (double)(long)(size_t)val->data.addr;
        default:             return 0.0;
    }
}

os_int32 gfa_value_to_long(gfa_value *val)
{
    if (val == NULL) return 0;

    switch (val->type) {
        case GFA_VAL_BOOL:   return (val->data.b != 0) ? -1 : 0;
        case GFA_VAL_BYTE:   return (os_int32)val->data.b;
        case GFA_VAL_WORD:   return (os_int32)val->data.w;
        case GFA_VAL_LONG:   return val->data.l;
        case GFA_VAL_FLOAT:  return (os_int32)(long)val->data.f;
        case GFA_VAL_STRING: return (os_int32)gfa_val(val->data.s ? val->data.s : "");
        case GFA_VAL_ADDRESS:return (os_int32)(long)(size_t)val->data.addr;
        default:             return 0;
    }
}

int gfa_value_to_bool(gfa_value *val)
{
    if (val == NULL) return 0;

    switch (val->type) {
        case GFA_VAL_BOOL:   return (val->data.b != 0) ? -1 : 0;
        case GFA_VAL_BYTE:   return (val->data.b != 0) ? -1 : 0;
        case GFA_VAL_WORD:   return (val->data.w != 0) ? -1 : 0;
        case GFA_VAL_LONG:   return (val->data.l != 0) ? -1 : 0;
        case GFA_VAL_FLOAT:  return (val->data.f != 0.0) ? -1 : 0;
        case GFA_VAL_STRING: {
            double d;
            d = gfa_val(val->data.s ? val->data.s : "");
            return (d != 0.0) ? -1 : 0;
        }
        case GFA_VAL_ADDRESS: return (val->data.addr != NULL) ? -1 : 0;
        default:             return 0;
    }
}
