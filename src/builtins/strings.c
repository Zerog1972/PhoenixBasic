/*
 * strings.c - Implementation des fonctions chaines GFA Basic 3.5
 * ==============================================================
 * Toutes les fonctions retournant char* utilisent un buffer interne
 * alloue dynamiquement que l'appelant doit liberer avec os_mem_free().
 *
 * Conventions C89 strictes.
 * Reference : cahier-des-charges-gfabasic.md, section 8.9
 */

#include "strings.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <float.h>

/* Taille maximale des buffers internes */
#define GFA_STR_BUFSIZE 512

/* ------------------------------------------------------------------ */
/* Fonctions utilitaires internes                                     */
/* ------------------------------------------------------------------ */

/*
 * alloc_buffer - Alloue un buffer de taille donnee et le met a zero.
 */
static char *alloc_buffer(size_t size)
{
    char *buf;
    buf = (char *)os_mem_alloc(size);
    if (buf != NULL) {
        os_mem_set(buf, 0, size);
    }
    return buf;
}

/*
 * is_valid_index - Verifie qu'un index (1-indexe) est valide pour s.
 */
/* reservee pour usage futur */
#if 0
static int is_valid_index(const char *s, int idx)
{
    if (s == NULL || idx < 1) return 0;
    return (idx <= (int)strlen(s));
}
#endif

/* ------------------------------------------------------------------ */
/* Fonctions de base                                                  */
/* ------------------------------------------------------------------ */

int gfa_asc(const char *s)
{
    if (s == NULL || s[0] == '\0') {
        return 0;
    }
    return (int)((unsigned char)s[0]);
}

const char *gfa_chr(int code)
{
    static char buf[2];
    code = code & 0xFF;
    buf[0] = (char)code;
    buf[1] = '\0';
    return buf;
}

int gfa_len(const char *s)
{
    if (s == NULL) {
        return 0;
    }
    return (int)strlen(s);
}

char *gfa_mid(const char *s, int pos, int len)
{
    int slen;
    int actual_len;
    char *result;

    if (s == NULL || pos < 1 || len < 1) {
        return gfa_str_new("");
    }

    slen = (int)strlen(s);
    if (pos > slen) {
        return gfa_str_new("");
    }

    /* Ajuster la longueur si elle depasse la chaine */
    actual_len = len;
    if (pos + actual_len - 1 > slen) {
        actual_len = slen - pos + 1;
    }

    result = alloc_buffer((size_t)(actual_len + 1));
    if (result == NULL) {
        return gfa_str_new("");
    }

    strncpy(result, s + pos - 1, (size_t)actual_len);
    result[actual_len] = '\0';
    return result;
}

int gfa_mid_assign(char *s, int pos, int len, const char *value)
{
    int slen, vlen;
    int i;

    if (s == NULL || pos < 1 || len < 1) {
        return -1;
    }

    slen = (int)strlen(s);
    if (pos > slen) {
        return -1;
    }

    /* Ajuster si depasse */
    if (pos + len - 1 > slen) {
        len = slen - pos + 1;
    }

    vlen = (value != NULL) ? (int)strlen(value) : 0;

    for (i = 0; i < len; i++) {
        if (i < vlen) {
            s[pos - 1 + i] = value[i];
        } else {
            s[pos - 1 + i] = ' ';
        }
    }

    return 0;
}

char *gfa_left(const char *s, int n)
{
    return gfa_mid(s, 1, n);
}

char *gfa_right(const char *s, int n)
{
    int slen;
    if (s == NULL || n < 1) {
        return gfa_str_new("");
    }
    slen = (int)strlen(s);
    if (n > slen) {
        n = slen;
    }
    return gfa_mid(s, slen - n + 1, n);
}

int gfa_instr(int pos, const char *haystack, const char *needle)
{
    const char *found;
    int haystack_len;
    int result;

    if (haystack == NULL || needle == NULL || needle[0] == '\0') {
        return 0;
    }

    haystack_len = (int)strlen(haystack);

    if (pos < 1) {
        pos = 1;
    }
    if (pos > haystack_len) {
        return 0;
    }

    found = strstr(haystack + pos - 1, needle);
    if (found == NULL) {
        return 0;
    }

    result = (int)(found - haystack) + 1;
    return result;
}

int gfa_rinstr(int pos, const char *haystack, const char *needle)
{
    int haystack_len, needle_len;
    int i;

    if (haystack == NULL || needle == NULL || needle[0] == '\0') {
        return 0;
    }

    haystack_len = (int)strlen(haystack);
    needle_len   = (int)strlen(needle);

    if (needle_len > haystack_len) {
        return 0;
    }

    if (pos < 1 || pos > haystack_len) {
        pos = haystack_len;
    }

    /* Chercher de droite a gauche */
    for (i = pos - needle_len; i >= 0; i--) {
        if (strncmp(haystack + i, needle, (size_t)needle_len) == 0) {
            return i + 1;
        }
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/* Conversion                                                         */
/* ------------------------------------------------------------------ */

char *gfa_str_float(double value)
{
    char *buf;
    buf = alloc_buffer(GFA_STR_BUFSIZE);
    if (buf == NULL) {
        return gfa_str_new("");
    }

    /*
     * Format GFA Basic : jusqu'a 14 chiffres significatifs.
     * Si la valeur est entiere, pas de point decimal.
     */
    if (floor(value) == value && fabs(value) < 1.0e14) {
        sprintf(buf, "%.0f", value);
    } else {
        char tmp[128];
        int i, len, trail;

        sprintf(tmp, "%.14g", value);

        /* Nettoyer le format GFA : supprimer les zeros inutiles */
        len = (int)strlen(tmp);
        /* Verifier si la notation scientifique est utilisee */
        {
            int has_e;
            has_e = 0;
            for (i = 0; i < len; i++) {
                if (tmp[i] == 'e' || tmp[i] == 'E') {
                    has_e = 1;
                    break;
                }
            }
            if (!has_e) {
                /* Supprimer les zeros de fin apres la virgule */
                {
                    int dot_pos;
                    dot_pos = -1;
                    for (i = len - 1; i >= 0; i--) {
                        if (tmp[i] == '.') {
                            dot_pos = i;
                            break;
                        }
                    }
                    if (dot_pos >= 0) {
                        trail = len - 1;
                        while (trail > dot_pos && tmp[trail] == '0') {
                            trail--;
                        }
                        if (trail == dot_pos) {
                            tmp[dot_pos] = '\0';  /* supprimer le point aussi */
                        } else {
                            tmp[trail + 1] = '\0';
                        }
                    }
                }
            }
        }

        strcpy(buf, tmp);
    }

    return buf;
}

char *gfa_str_float_fmt(double value, int total_digits, int decimals)
{
    char fmt[32];
    char *buf;

    buf = alloc_buffer(GFA_STR_BUFSIZE);
    if (buf == NULL) {
        return gfa_str_new("");
    }

    if (total_digits < 1) total_digits = 8;
    if (decimals < 0) decimals = 2;
    if (decimals >= total_digits) decimals = total_digits - 1;

    sprintf(fmt, "%%%d.%df", total_digits, decimals);
    sprintf(buf, fmt, value);

    return buf;
}

char *gfa_str_long(os_int32 value)
{
    char *buf;
    buf = alloc_buffer(GFA_STR_BUFSIZE);
    if (buf == NULL) {
        return gfa_str_new("");
    }
    sprintf(buf, "%ld", (long)value);
    return buf;
}

double gfa_val(const char *s)
{
    char *endptr;
    double result;

    if (s == NULL || s[0] == '\0') {
        return 0.0;
    }

    result = strtod(s, &endptr);

    /* Si aucun caractere n'a ete converti, retourner 0 */
    if (endptr == s) {
        return 0.0;
    }

    return result;
}

int gfa_val_count(const char *s)
{
    char *endptr;

    if (s == NULL || s[0] == '\0') {
        return 0;
    }

    strtod(s, &endptr);
    return (int)(endptr - s);
}

char *gfa_bin(os_int32 value, int digits)
{
    char *buf;
    int i;
    unsigned long uval;

    if (digits < 1) digits = 1;
    if (digits > 32) digits = 32;

    buf = alloc_buffer((size_t)(digits + 1));
    if (buf == NULL) {
        return gfa_str_new("");
    }

    uval = (unsigned long)value;

    for (i = digits - 1; i >= 0; i--) {
        buf[i] = (uval & 1UL) ? '1' : '0';
        uval >>= 1;
    }
    buf[digits] = '\0';

    return buf;
}

char *gfa_hex(os_int32 value, int digits)
{
    char *buf;
    if (digits < 1) digits = 1;
    if (digits > 8) digits = 8;

    buf = alloc_buffer((size_t)(digits + 1));
    if (buf == NULL) {
        return gfa_str_new("");
    }

    sprintf(buf, "%0*lX", digits, (unsigned long)(value & 0xFFFFFFFFUL));

    return buf;
}

char *gfa_oct(os_int32 value, int digits)
{
    char *buf;
    if (digits < 1) digits = 1;
    if (digits > 11) digits = 11;

    buf = alloc_buffer((size_t)(digits + 1));
    if (buf == NULL) {
        return gfa_str_new("");
    }

    sprintf(buf, "%0*lo", digits, (unsigned long)(value & 0xFFFFFFFFUL));

    return buf;
}

char *gfa_upper(const char *s)
{
    char *result;
    int i, len;

    if (s == NULL) {
        return gfa_str_new("");
    }

    len = (int)strlen(s);
    result = alloc_buffer((size_t)(len + 1));
    if (result == NULL) {
        return gfa_str_new("");
    }

    for (i = 0; i < len; i++) {
        result[i] = (char)toupper((unsigned char)s[i]);
    }
    result[len] = '\0';

    return result;
}

char *gfa_lower(const char *s)
{
    char *result;
    int i, len;

    if (s == NULL) {
        return gfa_str_new("");
    }

    len = (int)strlen(s);
    result = alloc_buffer((size_t)(len + 1));
    if (result == NULL) {
        return gfa_str_new("");
    }

    for (i = 0; i < len; i++) {
        result[i] = (char)tolower((unsigned char)s[i]);
    }
    result[len] = '\0';

    return result;
}

/* ------------------------------------------------------------------ */
/* Generation de chaines                                              */
/* ------------------------------------------------------------------ */

char *gfa_space(int n)
{
    char *result;
    int i;

    if (n < 0) n = 0;
    if (n > 32767) n = 32767;

    result = alloc_buffer((size_t)(n + 1));
    if (result == NULL) {
        return gfa_str_new("");
    }

    for (i = 0; i < n; i++) {
        result[i] = ' ';
    }
    result[n] = '\0';

    return result;
}

char *gfa_string(int n, const char *s)
{
    char *result;
    int slen;
    int total;
    int i;

    if (n <= 0 || s == NULL) {
        return gfa_str_new("");
    }

    slen = (int)strlen(s);
    if (slen == 0) {
        return gfa_str_new("");
    }

    total = n * slen;
    if (total > 32767) total = 32767;

    result = alloc_buffer((size_t)(total + 1));
    if (result == NULL) {
        return gfa_str_new("");
    }

    for (i = 0; i < n && (i * slen) < total; i++) {
        strncpy(result + (i * slen), s, (size_t)slen);
    }
    result[total] = '\0';

    return result;
}

char *gfa_string_char(int n, int code)
{
    char ch[2];
    ch[0] = (char)(code & 0xFF);
    ch[1] = '\0';
    return gfa_string(n, ch);
}

/* ------------------------------------------------------------------ */
/* Nettoyage                                                          */
/* ------------------------------------------------------------------ */

char *gfa_trim(const char *s)
{
    char *temp;
    char *result;

    if (s == NULL) {
        return gfa_str_new("");
    }

    temp = gfa_ltrim(s);
    result = gfa_rtrim(temp);
    os_mem_free(temp);

    return result;
}

char *gfa_ltrim(const char *s)
{
    const char *start;

    if (s == NULL) {
        return gfa_str_new("");
    }

    start = s;
    while (*start != '\0' && (*start == ' ' || *start == '\t')) {
        start++;
    }

    return gfa_str_new(start);
}

char *gfa_rtrim(const char *s)
{
    char *result;
    int len;
    int end;

    if (s == NULL) {
        return gfa_str_new("");
    }

    len = (int)strlen(s);
    result = alloc_buffer((size_t)(len + 1));
    if (result == NULL) {
        return gfa_str_new("");
    }

    strcpy(result, s);

    end = len - 1;
    while (end >= 0 && (result[end] == ' ' || result[end] == '\t')) {
        end--;
    }
    result[end + 1] = '\0';

    return result;
}

/* ------------------------------------------------------------------ */
/* Conversion binaire <-> chaine                                      */
/* ------------------------------------------------------------------ */

char *gfa_mki(os_int32 value)
{
    char *buf;
    unsigned short w;

    buf = alloc_buffer(3);
    if (buf == NULL) return gfa_str_new("");

    w = (unsigned short)(value & 0xFFFF);
    buf[0] = (char)((w >> 8) & 0xFF);
    buf[1] = (char)(w & 0xFF);
    buf[2] = '\0';

    return buf;
}

char *gfa_mkl(os_int32 value)
{
    char *buf;
    unsigned long l;

    buf = alloc_buffer(5);
    if (buf == NULL) return gfa_str_new("");

    l = (unsigned long)value;
    buf[0] = (char)((l >> 24) & 0xFF);
    buf[1] = (char)((l >> 16) & 0xFF);
    buf[2] = (char)((l >> 8)  & 0xFF);
    buf[3] = (char)(l & 0xFF);
    buf[4] = '\0';

    return buf;
}

char *gfa_mks(double value)
{
    /* En GFA Basic, MKS encode un flottant en 4 octets.
       Ici, on utilise le format IEEE-754 simple precision comme approximation. */
    char *buf;
    float f;
    unsigned char *p;

    buf = alloc_buffer(5);
    if (buf == NULL) return gfa_str_new("");

    f = (float)value;
    p = (unsigned char *)&f;

    /* Big-endian (Motorola 68k) */
    buf[0] = (char)p[3];
    buf[1] = (char)p[2];
    buf[2] = (char)p[1];
    buf[3] = (char)p[0];
    buf[4] = '\0';

    return buf;
}

char *gfa_mkf(double value)
{
    /* 6 octets - approximation */
    char *buf;
    buf = alloc_buffer(7);
    if (buf == NULL) return gfa_str_new("");
    os_mem_set(buf, 0, 7);
    /* Format simplifie (GFA utilise un format proprietaire 6 octets) */
    {
        float f;
        unsigned char *p;
        f = (float)value;
        p = (unsigned char *)&f;
        buf[0] = (char)p[3];
        buf[1] = (char)p[2];
        buf[2] = (char)p[1];
        buf[3] = (char)p[0];
    }
    buf[6] = '\0';
    return buf;
}

char *gfa_mkd(double value)
{
    /* 8 octets - approximation IEEE double */
    char *buf;
    unsigned char *p;
    int i;

    buf = alloc_buffer(9);
    if (buf == NULL) return gfa_str_new("");

    p = (unsigned char *)&value;

    /* Big-endian (Motorola 68k) */
    for (i = 0; i < 8; i++) {
        buf[i] = (char)p[7 - i];
    }
    buf[8] = '\0';

    return buf;
}

int gfa_cvi(const char *s)
{
    unsigned short w;
    if (s == NULL) return 0;
    w  = ((unsigned short)(unsigned char)s[0]) << 8;
    w |= ((unsigned short)(unsigned char)s[1]);
    return (int)(short)w;
}

os_int32 gfa_cvl(const char *s)
{
    unsigned long l;
    if (s == NULL) return 0;
    l  = ((unsigned long)(unsigned char)s[0]) << 24;
    l |= ((unsigned long)(unsigned char)s[1]) << 16;
    l |= ((unsigned long)(unsigned char)s[2]) << 8;
    l |= ((unsigned long)(unsigned char)s[3]);
    return (os_int32)l;
}

double gfa_cvs(const char *s)
{
    float f;
    unsigned char *p;
    if (s == NULL) return 0.0;
    p = (unsigned char *)&f;
    p[3] = (unsigned char)s[0];
    p[2] = (unsigned char)s[1];
    p[1] = (unsigned char)s[2];
    p[0] = (unsigned char)s[3];
    return (double)f;
}

double gfa_cvf(const char *s)
{
    return gfa_cvs(s);  /* Approximation */
}

double gfa_cvd(const char *s)
{
    double d;
    unsigned char *p;
    int i;
    if (s == NULL) return 0.0;
    p = (unsigned char *)&d;
    for (i = 0; i < 8; i++) {
        p[7 - i] = (unsigned char)s[i];
    }
    return d;
}

char *gfa_insert(const char *target, const char *source)
{
    char *result;
    int tlen, slen;

    if (target == NULL && source == NULL) {
        return gfa_str_new("");
    }
    if (target == NULL) return gfa_str_new(source);
    if (source == NULL) return gfa_str_new(target);

    tlen = (int)strlen(target);
    slen = (int)strlen(source);

    result = alloc_buffer((size_t)(tlen + slen + 1));
    if (result == NULL) return gfa_str_new("");

    strcpy(result, target);
    strcat(result, source);

    return result;
}

/* ------------------------------------------------------------------ */
/* Utilitaires                                                        */
/* ------------------------------------------------------------------ */

char *gfa_str_new(const char *s)
{
    char *result;
    size_t len;

    if (s == NULL) {
        result = alloc_buffer(1);
        if (result != NULL) result[0] = '\0';
        return result;
    }

    len = strlen(s);
    result = alloc_buffer(len + 1);
    if (result == NULL) {
        return NULL;
    }

    strcpy(result, s);
    return result;
}

char *gfa_str_dup_n(const char *s, int n)
{
    char *result;
    int i;

    if (s == NULL || n <= 0) {
        return gfa_str_new("");
    }

    result = alloc_buffer((size_t)(n + 1));
    if (result == NULL) {
        return gfa_str_new("");
    }

    for (i = 0; i < n && s[i] != '\0'; i++) {
        result[i] = s[i];
    }
    result[i] = '\0';

    return result;
}

char *gfa_str_concat(const char *a, const char *b)
{
    return gfa_str_concat3(a, b, "");
}

char *gfa_str_concat3(const char *a, const char *b, const char *c)
{
    char *result;
    size_t alen, blen, clen, total;

    alen = (a != NULL) ? strlen(a) : 0;
    blen = (b != NULL) ? strlen(b) : 0;
    clen = (c != NULL) ? strlen(c) : 0;

    total = alen + blen + clen;
    result = alloc_buffer(total + 1);
    if (result == NULL) {
        return gfa_str_new("");
    }

    result[0] = '\0';
    if (a != NULL && alen > 0) strcpy(result, a);
    if (b != NULL && blen > 0) strcat(result, b);
    if (c != NULL && clen > 0) strcat(result, c);

    return result;
}
