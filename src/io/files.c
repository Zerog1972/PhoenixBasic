/*
 * files.c - Implementation de la gestion des fichiers GFA Basic 3.5
 * ==================================================================
 * Couche intermediaire entre le runtime GFA et la couche OS.
 * Toutes les operations de fichiers du GFA Basic transitent par ce module.
 *
 * Reference : cahier-des-charges-gfabasic.md, section 8.7
 */

#include "files.h"
#include <string.h>
#include <stdio.h>

/* Table globale des fichiers ouverts */
static gfa_file g_file_table[GFA_MAX_CHANNELS];
static int      g_files_initialized = 0;

/* Buffer pour FIELD (mode Random) */
#define GFA_FIELD_BUFFER_MAX 65536
/* static char g_field_data[GFA_FIELD_BUFFER_MAX]; -- reserve pour usage futur */

/* ------------------------------------------------------------------ */
/* Initialisation                                                     */
/* ------------------------------------------------------------------ */

void gfa_files_init(void)
{
    int i;
    for (i = 0; i < GFA_MAX_CHANNELS; i++) {
        g_file_table[i].channel        = i;
        g_file_table[i].mode           = GFA_FILE_CLOSED;
        g_file_table[i].handle         = NULL;
        g_file_table[i].filename[0]    = '\0';
        g_file_table[i].record_length  = 0;
        g_file_table[i].current_record = 0;
        g_file_table[i].field_buffer   = NULL;
        g_file_table[i].field_size     = 0;
        g_file_table[i].field_count    = 0;
    }
    g_files_initialized = 1;
}

void gfa_files_shutdown(void)
{
    if (!g_files_initialized) return;
    gfa_close(-1);  /* Fermer tous les fichiers */
    g_files_initialized = 0;
}

gfa_file *gfa_files_get(int channel)
{
    if (channel < 1 || channel >= GFA_MAX_CHANNELS) {
        return NULL;
    }
    if (g_file_table[channel].mode == GFA_FILE_CLOSED) {
        return NULL;
    }
    return &g_file_table[channel];
}

int gfa_files_get_count(void)
{
    int count;
    int i;
    count = 0;
    for (i = 1; i < GFA_MAX_CHANNELS; i++) {
        if (g_file_table[i].mode != GFA_FILE_CLOSED) {
            count++;
        }
    }
    return count;
}

/* ------------------------------------------------------------------ */
/* Conversion mode GFA -> OS                                          */
/* ------------------------------------------------------------------ */

static char mode_to_gfa_char(gfa_file_mode mode)
{
    switch (mode) {
        case GFA_FILE_INPUT:   return 'I';
        case GFA_FILE_OUTPUT:  return 'O';
        case GFA_FILE_APPEND:  return 'A';
        case GFA_FILE_RANDOM:  return 'R';
        case GFA_FILE_UPDATE:  return 'U';
        default:               return '?';
    }
}

static gfa_file_mode mode_from_string(const char *mode_str)
{
    if (mode_str == NULL || mode_str[0] == '\0') {
        return GFA_FILE_CLOSED;
    }

    switch (mode_str[0]) {
        case 'I': case 'i': return GFA_FILE_INPUT;
        case 'O': case 'o': return GFA_FILE_OUTPUT;
        case 'A': case 'a': return GFA_FILE_APPEND;
        case 'R': case 'r': return GFA_FILE_RANDOM;
        case 'U': case 'u': return GFA_FILE_UPDATE;
        default:            return GFA_FILE_CLOSED;
    }
}

/* ------------------------------------------------------------------ */
/* OPEN                                                               */
/* ------------------------------------------------------------------ */

int gfa_open(const char *mode_str, int channel, const char *filename,
             int record_len)
{
    gfa_file_mode mode;
    char gfa_mode_char;
    os_file_handle os_handle;
    gfa_file *gf;

    if (!g_files_initialized) {
        return 0; /* Non initialise, erreur silencieuse */
    }

    if (channel < 1 || channel >= GFA_MAX_CHANNELS) {
        return 23; /* File # wrong */
    }

    if (g_file_table[channel].mode != GFA_FILE_CLOSED) {
        return 22; /* File already open */
    }

    mode = mode_from_string(mode_str);
    if (mode == GFA_FILE_CLOSED) {
        return 21; /* Mode invalide */
    }

    /* Verifier le nombre de fichiers Random */
    if (mode == GFA_FILE_RANDOM) {
        int rc;
        int i;
        rc = 0;
        for (i = 1; i < GFA_MAX_CHANNELS; i++) {
            if (g_file_table[i].mode == GFA_FILE_RANDOM) {
                rc++;
            }
        }
        if (rc >= GFA_MAX_RANDOM_FILES) {
            return 49; /* Too many R-files */
        }
        if (record_len < 1 || record_len > 32767) {
            return 48; /* Record length wrong */
        }
    }

    gfa_mode_char = mode_to_gfa_char(mode);
    if (gfa_mode_char == '?') {
        return 21;
    }

    os_handle = os_file_open(filename, gfa_mode_char, channel);
    if (os_handle == NULL) {
        /* Verifier si le fichier existe deja (mode Input) */
        if (mode == GFA_FILE_INPUT && !os_fs_exist(filename)) {
            return -33; /* File not found (code GEMDOS) */
        }
        return 21; /* Erreur d'ouverture */
    }

    /* Initialiser l'entree de la table */
    gf = &g_file_table[channel];
    gf->channel        = channel;
    gf->mode           = mode;
    gf->handle         = os_handle;
    gf->record_length  = record_len;
    gf->current_record = 0;
    gf->field_buffer   = NULL;
    gf->field_size     = 0;
    gf->field_count    = 0;

    /* Copier le nom du fichier */
    strncpy(gf->filename, filename, sizeof(gf->filename) - 1);
    gf->filename[sizeof(gf->filename) - 1] = '\0';

    /* Allouer le buffer FIELD pour le mode Random */
    if (mode == GFA_FILE_RANDOM && record_len > 0) {
        gf->field_buffer = (char *)os_mem_alloc((size_t)record_len);
        if (gf->field_buffer != NULL) {
            os_mem_set(gf->field_buffer, 0, (size_t)record_len);
            gf->field_size = record_len;
        }
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/* CLOSE                                                              */
/* ------------------------------------------------------------------ */

void gfa_close(int channel)
{
    int i;

    if (!g_files_initialized) return;

    if (channel == -1) {
        /* Fermer tous les fichiers */
        for (i = 1; i < GFA_MAX_CHANNELS; i++) {
            if (g_file_table[i].mode != GFA_FILE_CLOSED) {
                if (g_file_table[i].handle != NULL) {
                    os_file_close(g_file_table[i].handle);
                }
                if (g_file_table[i].field_buffer != NULL) {
                    os_mem_free(g_file_table[i].field_buffer);
                }
                g_file_table[i].mode         = GFA_FILE_CLOSED;
                g_file_table[i].handle       = NULL;
                g_file_table[i].field_buffer = NULL;
                g_file_table[i].field_size   = 0;
                g_file_table[i].field_count  = 0;
                g_file_table[i].filename[0]  = '\0';
            }
        }
        return;
    }

    if (channel < 1 || channel >= GFA_MAX_CHANNELS) return;

    if (g_file_table[channel].mode == GFA_FILE_CLOSED) return;

    if (g_file_table[channel].handle != NULL) {
        os_file_close(g_file_table[channel].handle);
    }
    if (g_file_table[channel].field_buffer != NULL) {
        os_mem_free(g_file_table[channel].field_buffer);
    }

    g_file_table[channel].mode         = GFA_FILE_CLOSED;
    g_file_table[channel].handle       = NULL;
    g_file_table[channel].field_buffer = NULL;
    g_file_table[channel].field_size   = 0;
    g_file_table[channel].field_count  = 0;
    g_file_table[channel].filename[0]  = '\0';
}

/* ------------------------------------------------------------------ */
/* INPUT # / LINE INPUT #                                             */
/* ------------------------------------------------------------------ */

int gfa_input_channel(int channel, char *buffer, int bufsize)
{
    gfa_file *gf;
    int i;
    int c;

    if (buffer == NULL || bufsize < 1) return -1;

    gf = gfa_files_get(channel);
    if (gf == NULL || gf->handle == NULL) return -1;

    i = 0;
    while (i < bufsize - 1) {
        os_int32 nread;
        char ch;
        nread = os_file_read(gf->handle, &ch, 1);
        if (nread <= 0) {
            if (i == 0) return -1;  /* EOF */
            break;
        }
        c = (int)(unsigned char)ch;

        /* Arreter a la fin de ligne ou fin de fichier */
        if (c == '\n' || c == '\r') {
            /* Consommer le \n apres \r si present */
            if (c == '\r') {
                char next;
                os_int32 nr2;
                nr2 = os_file_read(gf->handle, &next, 1);
                if (nr2 > 0 && next != '\n') {
                    /* Remettre le caractere dans le flux (simplifie) */
                    os_file_seek(gf->handle, -1, OS_SEEK_CUR);
                }
            }
            break;
        }
        buffer[i++] = (char)c;
    }
    buffer[i] = '\0';
    return i;
}

char *gfa_line_input_channel(int channel)
{
    char buffer[4096];
    int len;

    len = gfa_input_channel(channel, buffer, (int)sizeof(buffer));
    if (len < 0) {
        return NULL;
    }

    /* Allouer une copie exacte */
    {
        char *result;
        result = (char *)os_mem_alloc((size_t)(len + 1));
        if (result == NULL) return NULL;
        strcpy(result, buffer);
        return result;
    }
}

/* ------------------------------------------------------------------ */
/* PRINT # / WRITE #                                                  */
/* ------------------------------------------------------------------ */

int gfa_print_channel(int channel, const char *data)
{
    gfa_file *gf;
    os_int32 written;

    gf = gfa_files_get(channel);
    if (gf == NULL || gf->handle == NULL) return -1;

    if (data == NULL) return 0;

    written = os_file_write(gf->handle, data, (os_int32)strlen(data));
    if (written != (os_int32)strlen(data)) {
        return -1;
    }

    return 0;
}

int gfa_write_channel(int channel, const char *data)
{
    gfa_file *gf;
    os_int32 written;
    int len;

    gf = gfa_files_get(channel);
    if (gf == NULL || gf->handle == NULL) return -1;

    if (data == NULL) return 0;

    len = (int)strlen(data);
    written = os_file_write(gf->handle, data, (os_int32)len);
    if (written != (os_int32)len) return -1;

    /* WRITE ajoute un retour a la ligne */
    {
        os_int32 wr;
        wr = os_file_write(gf->handle, "\n", 1);
        if (wr != 1) return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/* GET # / PUT # (mode Random)                                        */
/* ------------------------------------------------------------------ */

int gfa_get_channel(int channel, long record_num)
{
    gfa_file *gf;
    long pos;
    os_int32 nread;

    gf = gfa_files_get(channel);
    if (gf == NULL || gf->mode != GFA_FILE_RANDOM) return -1;

    if (gf->record_length <= 0) return -1;

    if (record_num >= 0) {
        gf->current_record = record_num;
    }

    pos = gf->current_record * (long)gf->record_length;
    os_file_seek(gf->handle, (os_int32)pos, OS_SEEK_SET);

    if (gf->field_buffer != NULL) {
        nread = os_file_read(gf->handle, gf->field_buffer,
                             (os_int32)gf->record_length);
        if (nread < (os_int32)gf->record_length) {
            return -1;
        }
    }

    gf->current_record++;
    return 0;
}

int gfa_put_channel(int channel, long record_num)
{
    gfa_file *gf;
    long pos;
    os_int32 written;

    gf = gfa_files_get(channel);
    if (gf == NULL || gf->mode != GFA_FILE_RANDOM) return -1;

    if (gf->record_length <= 0) return -1;

    if (record_num >= 0) {
        gf->current_record = record_num;
    }

    pos = gf->current_record * (long)gf->record_length;
    os_file_seek(gf->handle, (os_int32)pos, OS_SEEK_SET);

    if (gf->field_buffer != NULL) {
        written = os_file_write(gf->handle, gf->field_buffer,
                                (os_int32)gf->record_length);
        if (written != (os_int32)gf->record_length) {
            return -1;
        }
    }

    gf->current_record++;
    return 0;
}

/* ------------------------------------------------------------------ */
/* SEEK / RELSEEK / EOF / LOF / LOC                                   */
/* ------------------------------------------------------------------ */

long gfa_seek(int channel, long position)
{
    gfa_file *gf;
    os_int32 pos;

    gf = gfa_files_get(channel);
    if (gf == NULL) return -1L;

    pos = os_file_seek(gf->handle, (os_int32)position, OS_SEEK_SET);
    return (long)pos;
}

long gfa_relseek(int channel, long offset)
{
    gfa_file *gf;
    os_int32 pos;

    gf = gfa_files_get(channel);
    if (gf == NULL) return -1L;

    pos = os_file_seek(gf->handle, (os_int32)offset, OS_SEEK_CUR);
    return (long)pos;
}

int gfa_eof(int channel)
{
    gfa_file *gf;

    gf = gfa_files_get(channel);
    if (gf == NULL) return -1;  /* TRUE pour un fichier non ouvert */

    return os_file_eof(gf->handle) ? -1 : 0;
}

long gfa_lof(int channel)
{
    gfa_file *gf;
    os_int32 size;

    gf = gfa_files_get(channel);
    if (gf == NULL) return 0L;

    size = os_file_size(gf->handle);
    return (long)size;
}

long gfa_loc(int channel)
{
    gfa_file *gf;
    os_int32 pos;

    gf = gfa_files_get(channel);
    if (gf == NULL) return 0L;

    pos = os_file_tell(gf->handle);

    /* Pour les fichiers Random, retourner le numero d'enregistrement */
    if (gf->mode == GFA_FILE_RANDOM && gf->record_length > 0) {
        return (long)(pos / gf->record_length) + 1L;
    }

    /* Pour les fichiers sequentiels, retourner la position / 128 + 1 */
    return (long)(pos / 128L) + 1L;
}

/* ------------------------------------------------------------------ */
/* EXIST / KILL / NAME                                                */
/* ------------------------------------------------------------------ */

int gfa_exist(const char *filename)
{
    return os_fs_exist(filename) ? -1 : 0;
}

void gfa_kill(const char *filename)
{
    os_fs_delete(filename);
}

void gfa_name_file(const char *oldname, const char *newname)
{
    os_fs_rename(oldname, newname);
}

/* ------------------------------------------------------------------ */
/* FIELD / LSET / RSET                                                */
/* ------------------------------------------------------------------ */

int gfa_field(int channel, int field_size, const char *field_name)
{
    gfa_file *gf;

    (void)field_name; /* Le nom est gere par le runtime (variables) */

    gf = gfa_files_get(channel);
    if (gf == NULL || gf->mode != GFA_FILE_RANDOM) {
        return 50; /* Not an R-File */
    }

    if (gf->field_buffer == NULL) {
        gf->field_buffer = (char *)os_mem_alloc((size_t)field_size);
        if (gf->field_buffer == NULL) return 8; /* Out of memory */
        gf->field_size = field_size;
    }

    gf->field_count++;
    return 0;
}

void gfa_lset(char *field_var, const char *value, int field_len)
{
    int vlen;
    int i;

    if (field_var == NULL || field_len <= 0) return;

    os_mem_set(field_var, ' ', (size_t)field_len);

    if (value == NULL) return;

    vlen = (int)strlen(value);
    if (vlen > field_len) vlen = field_len;

    for (i = 0; i < vlen; i++) {
        field_var[i] = value[i];
    }
}

void gfa_rset(char *field_var, const char *value, int field_len)
{
    int vlen;
    int i;
    int start;

    if (field_var == NULL || field_len <= 0) return;

    os_mem_set(field_var, ' ', (size_t)field_len);

    if (value == NULL) return;

    vlen = (int)strlen(value);
    if (vlen > field_len) vlen = field_len;

    start = field_len - vlen;
    for (i = 0; i < vlen; i++) {
        field_var[start + i] = value[i];
    }
}

/* ------------------------------------------------------------------ */
/* BLOAD / BSAVE / BGET / BPUT / SGET / SPUT                         */
/* ------------------------------------------------------------------ */

long gfa_bload(const char *filename, long address)
{
    os_file_handle fh;
    os_int32 file_size;
    os_int32 nread;

    fh = os_file_open(filename, 'I', 0);
    if (fh == NULL) return 0L;

    file_size = os_file_size(fh);
    if (file_size <= 0) {
        os_file_close(fh);
        return 0L;
    }

    if (address == 0L) {
        /* Allouer de la memoire si pas d'adresse specifiee */
        address = (long)(size_t)os_mem_alloc((size_t)file_size);
        if (address == 0L) {
            os_file_close(fh);
            return 0L;
        }
    }

    nread = os_file_read(fh, (void *)(size_t)address, file_size);
    os_file_close(fh);

    if (nread != file_size) return 0L;
    return address;
}

int gfa_bsave(const char *filename, long start_addr, long end_addr)
{
    os_file_handle fh;
    os_int32 size;
    os_int32 written;

    if (end_addr <= start_addr) return -1;

    fh = os_file_open(filename, 'O', 0);
    if (fh == NULL) return -1;

    size = (os_int32)(end_addr - start_addr);
    written = os_file_write(fh, (const void *)(size_t)start_addr, size);
    os_file_close(fh);

    if (written != size) return -1;
    return 0;
}

int gfa_bget(int channel, void *buffer, int size)
{
    gfa_file *gf;
    os_int32 nread;

    gf = gfa_files_get(channel);
    if (gf == NULL || buffer == NULL || size <= 0) return -1;

    nread = os_file_read(gf->handle, buffer, (os_int32)size);
    return (int)nread;
}

int gfa_bput(int channel, const void *buffer, int size)
{
    gfa_file *gf;
    os_int32 written;

    gf = gfa_files_get(channel);
    if (gf == NULL || buffer == NULL || size <= 0) return -1;

    written = os_file_write(gf->handle, buffer, (os_int32)size);
    return (int)written;
}

int gfa_sget(int channel)
{
    /* SGET lit un octet depuis un peripherique serie */
    /* Simplifie : lit depuis le fichier */
    gfa_file *gf;
    char c;
    os_int32 nread;

    gf = gfa_files_get(channel);
    if (gf == NULL || gf->handle == NULL) return -1;

    nread = os_file_read(gf->handle, &c, 1);
    if (nread <= 0) return -1;

    return (int)(unsigned char)c;
}

int gfa_sput(int channel, int data)
{
    gfa_file *gf;
    char c;
    os_int32 written;

    gf = gfa_files_get(channel);
    if (gf == NULL || gf->handle == NULL) return -1;

    c = (char)(data & 0xFF);
    written = os_file_write(gf->handle, &c, 1);
    if (written != 1) return -1;

    return 0;
}
