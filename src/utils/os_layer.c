/*
 * os_layer.c - Implementation de la couche d'abstraction OS
 * ===========================================================
 * Fournit toutes les primitives systeme necessaires a l'emulateur
 * GFA Basic 3.5. Implementation portable en C89 strict.
 *
 * Plateformes supportees :
 *   - Windows (MinGW/Cygwin)   : branche _WIN32
 *   - Atari ST (Pure C 1.1)    : branche !_WIN32 (GEMDOS/TOS)
 *
 * Conventions :
 *   - C89 strict (pas de //, pas d'inline, pas de bool, declarations
 *     en tete de bloc)
 *   - Branches systeme isolees par #ifdef _WIN32 / #else
 *
 * Reference : cahier-des-charges-gfabasic.md, sections 2.2, 7, 9, 15.2
 */

#include "os_layer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#include <conio.h>

/* ------------------------------------------------------------------ */
/* Macros plateforme Windows                                          */
/* ------------------------------------------------------------------ */
#define OS_MKDIR(path)    _mkdir(path)
#define OS_RMDIR(path)    _rmdir(path)
#define OS_CHDIR(path)    _chdir(path)
#define OS_GETCWD(buf,s)  _getcwd(buf,s)
#define OS_UNLINK(path)   _unlink(path)
#define OS_RENAME(o,n)    rename(o,n)
#define OS_SLEEP_MS(ms)   Sleep(ms)
#else
/* ------------------------------------------------------------------ */
/* Atari ST : GEMDOS / BIOS / XBIOS                                   */
/*  - Pure C 1.1 : <tos.h>                                            */
/*  - gcc-mint (m68k-atari-mintelf) : <osbind.h> + <mint/ostruct.h>   */
/* ------------------------------------------------------------------ */
#ifdef GFA_TARGET_MINT
#include <osbind.h>
#include <mint/ostruct.h>
#else
#include <tos.h>
#endif

#define OS_MKDIR(path)    Dcreate(path)
#define OS_RMDIR(path)    Ddelete(path)
#define OS_CHDIR(path)    Dsetpath(path)
#define OS_GETCWD(buf,s)  Dgetpath(buf, 0)
#define OS_UNLINK(path)   Ddelete(path)
#define OS_RENAME(o,n)    Frename(0, n, o)
#define OS_SLEEP_MS(ms)   os_tos_delay(ms)

/*
 * os_tos_get_hz200 — Lit le compteur 200 Hz du TOS (adresse $4BA).
 * Equivalent LPEEK(&H4BA) de GFA Basic.
 */
static os_int32 os_tos_get_hz200(void)
{
    unsigned long *p;
    p = (unsigned long *)(size_t)0x4BAUL;
    return (os_int32)*p;
}

/*
 * os_tos_delay — Attend delay_ms millisecondes (boucle sur 200 Hz).
 */
static void os_tos_delay(unsigned long delay_ms)
{
    unsigned long start;
    unsigned long elapsed;
    unsigned long now;

    start = (unsigned long)os_tos_get_hz200();
    elapsed = (delay_ms * 200UL) / 1000UL;

    for (;;) {
        now = (unsigned long)os_tos_get_hz200();
        if ((now - start) >= elapsed) break;
    }
}
#endif

/* ------------------------------------------------------------------ */
/* Constantes specifiques Atari ST                                    */
/* ------------------------------------------------------------------ */

/* Moment de demarrage du programme (pour emuler TIMER) */
static os_int32 g_startup_ticks_ms = 0;

/* Palette Atari ST standard (16 couleurs, format 0x00RRGGBB) */
const unsigned long os_st_palette[16] = {
    0x00000000UL,  /*  0: Noir           */
    0x00000080UL,  /*  1: Bleu fonce     */
    0x00008000UL,  /*  2: Vert fonce     */
    0x00008080UL,  /*  3: Cyan fonce     */
    0x00800000UL,  /*  4: Rouge fonce    */
    0x00800080UL,  /*  5: Magenta fonce  */
    0x00808000UL,  /*  6: Marron         */
    0x00C0C0C0UL,  /*  7: Gris clair     */
    0x00808080UL,  /*  8: Gris fonce     */
    0x000000FFUL,  /*  9: Bleu clair     */
    0x0000FF00UL,  /* 10: Vert clair     */
    0x0000FFFFUL,  /* 11: Cyan clair     */
    0x00FF0000UL,  /* 12: Rouge clair    */
    0x00FF00FFUL,  /* 13: Magenta clair  */
    0x00FFFF00UL,  /* 14: Jaune          */
    0x00FFFFFFUL   /* 15: Blanc          */
};

/* ------------------------------------------------------------------ */
/* Etat global du module                                              */
/* ------------------------------------------------------------------ */

/* Erreur courante */
static os_error_code g_last_error = OS_ERR_NONE;

/* Driver d'affichage enregistre */
static const os_display_driver *g_display_driver = NULL;

/* Resolution d'affichage courante */
static int g_display_resolution = -1;

/* Echo console */
static int g_con_echo = 1;

/* Buffer pour les chaines date/heure retournees */
static char g_date_buffer[16];
static char g_time_buffer[16];
static char g_env_buffer[256];

/* Virtual clock - stores SETTIME values so DATE$ / TIME$ return them */
static int g_virtual_clock_active = 0;
static int g_virtual_hour   = 0;
static int g_virtual_min    = 0;
static int g_virtual_sec    = 0;
static int g_virtual_day    = 1;
static int g_virtual_month  = 1;
static int g_virtual_year   = 1980;

/* Table des descripteurs de fichiers ouverts (canaux 0-99) */
#define OS_MAX_CHANNELS 100
typedef struct {
    FILE           *stdio_handle;  /* handle C standard            */
    os_file_handle  os_handle;     /* pointeur vers cette structure */
    int             channel;       /* numero de canal (0-99)        */
    char            gfa_mode;      /* mode GFA : 'I','O','A','R','U' */
    char            filename[256]; /* nom du fichier                */
    int             in_use;        /* drapeau d'utilisation          */
} os_channel_entry;

static os_channel_entry g_channels[OS_MAX_CHANNELS];

/* Buffer pour os_dir_first/next (emulation FSFIRST/FSNEXT) */
static char            g_dir_pattern[256];
static int             g_dir_attr_mask;

/* ------------------------------------------------------------------ */
/* Fonctions utilitaires internes                                     */
/* ------------------------------------------------------------------ */

/*
 * get_ms_now — Retourne le temps courant en millisecondes.
 */
static os_int32 get_ms_now(void)
{
#ifdef _WIN32
    return (os_int32)GetTickCount();
#else
    unsigned long hz200;
    unsigned long secs;
    unsigned long frac;

    /* Conversion du compteur 200 Hz TOS en millisecondes */
    hz200 = (unsigned long)os_tos_get_hz200();
    secs  = hz200 / 200UL;
    frac  = hz200 % 200UL;
    {
        os_int32 ms;
        ms = (os_int32)(secs * 1000UL + (frac * 1000UL) / 200UL);
        return ms;
    }
#endif
}

/*
 * channel_init — Initialise la table des canaux.
 */
static void channels_init(void)
{
    int i;
    for (i = 0; i < OS_MAX_CHANNELS; i++) {
        g_channels[i].stdio_handle = NULL;
        g_channels[i].os_handle    = NULL;
        g_channels[i].channel      = i;
        g_channels[i].gfa_mode     = 0;
        g_channels[i].filename[0]  = '\0';
        g_channels[i].in_use       = 0;
    }
}

/*
 * channels_close_all — Ferme tous les canaux ouverts.
 */
static void channels_close_all(void)
{
    int i;
    for (i = 0; i < OS_MAX_CHANNELS; i++) {
        if (g_channels[i].in_use && g_channels[i].stdio_handle != NULL) {
            fclose(g_channels[i].stdio_handle);
            g_channels[i].in_use = 0;
            g_channels[i].stdio_handle = NULL;
        }
    }
}

/*
 * gfa_mode_to_c_mode — Convertit un mode GFA ('I','O','A','R','U')
 * en chaine de mode fopen().
 */
static const char* gfa_mode_to_c_mode(char gfa_mode)
{
    switch (gfa_mode) {
        case OS_GFAMODE_INPUT:   return OS_FMODE_INPUT;
        case OS_GFAMODE_OUTPUT:  return OS_FMODE_OUTPUT;
        case OS_GFAMODE_APPEND:  return OS_FMODE_APPEND;
        case OS_GFAMODE_RANDOM:  return OS_FMODE_RANDOM;
        case OS_GFAMODE_UPDATE:  return OS_FMODE_UPDATE;
        default: return NULL;
    }
}

/*
 * gfa_mode_valid — Verifie si le mode d'ouverture est valide.
 */
static int gfa_mode_valid(char mode)
{
    return (mode == OS_GFAMODE_INPUT  || mode == OS_GFAMODE_OUTPUT ||
            mode == OS_GFAMODE_APPEND || mode == OS_GFAMODE_RANDOM ||
            mode == OS_GFAMODE_UPDATE);
}

/* ------------------------------------------------------------------ */
/* Initialisation / Arret                                             */
/* ------------------------------------------------------------------ */

int os_init(void)
{
    g_last_error = OS_ERR_NONE;
    g_startup_ticks_ms = get_ms_now();
    g_display_driver = NULL;
    g_display_resolution = -1;
    g_con_echo = 1;

    g_date_buffer[0] = '\0';
    g_time_buffer[0] = '\0';
    g_env_buffer[0] = '\0';

    channels_init();

    return 0;
}

void os_shutdown(void)
{
    /* Arreter l'affichage si actif */
    if (g_display_driver != NULL && g_display_driver->shutdown != NULL) {
        g_display_driver->shutdown();
    }
    g_display_driver = NULL;
    g_display_resolution = -1;

    /* Fermer tous les fichiers */
    channels_close_all();

    g_last_error = OS_ERR_NONE;
}

/* ------------------------------------------------------------------ */
/* Gestion des erreurs                                                */
/* ------------------------------------------------------------------ */

os_error_code os_get_error(void)
{
    return g_last_error;
}

const char* os_get_error_string(os_error_code code)
{
    switch (code) {
        case OS_ERR_NONE:             return "No error";
        case OS_ERR_FILE_NOT_FOUND:   return "File not found";
        case OS_ERR_PATH_NOT_FOUND:   return "Path not found";
        case OS_ERR_TOO_MANY_OPEN:    return "Too many open files";
        case OS_ERR_ACCESS_DENIED:    return "Access denied";
        case OS_ERR_INVALID_HANDLE:   return "Invalid handle";
        case OS_ERR_OUT_OF_MEMORY:    return "Out of memory";
        case OS_ERR_INVALID_DRIVE:    return "Invalid drive specification";
        case OS_ERR_NO_MORE_FILES:    return "No more files";
        case OS_ERR_DISK_FULL:        return "Disk full";
        case OS_ERR_WRITE_FAULT:      return "Write fault";
        case OS_ERR_READ_FAULT:       return "Read fault";
        case OS_ERR_INTERNAL:         return "Internal error";
        case OS_ERR_NOT_IMPL:         return "Function not implemented";
        default:                      return "Unknown error";
    }
}

const char* os_get_last_error_string(void)
{
    return os_get_error_string(g_last_error);
}

/* ------------------------------------------------------------------ */
/* Affichage                                                          */
/* ------------------------------------------------------------------ */

int os_display_register(const os_display_driver *driver)
{
    if (driver == NULL) {
        g_last_error = OS_ERR_INVALID_HANDLE;
        return -1;
    }
    g_display_driver = driver;
    g_last_error = OS_ERR_NONE;
    return 0;
}

const os_display_driver* os_display_get(void)
{
    return g_display_driver;
}

int os_display_set_mode(int mode)
{
    if (g_display_driver == NULL || g_display_driver->init == NULL) {
        g_last_error = OS_ERR_NOT_IMPL;
        return -1;
    }

    if (mode < OS_ST_MODE_LOW || mode > OS_ST_MODE_HIGH) {
        g_last_error = OS_ERR_INTERNAL;
        return -1;
    }

    if (g_display_driver->init(mode) != 0) {
        g_last_error = OS_ERR_INTERNAL;
        return -1;
    }

    g_display_resolution = mode;
    g_last_error = OS_ERR_NONE;
    return 0;
}

int os_display_get_resolution(void)
{
    if (g_display_driver != NULL && g_display_driver->get_resolution != NULL) {
        return g_display_driver->get_resolution();
    }
    return g_display_resolution;
}

/* ------------------------------------------------------------------ */
/* Gestion de fichiers                                                */
/* ------------------------------------------------------------------ */

os_file_handle os_file_open(const char *name, char gfa_mode, int channel)
{
    const char *c_mode;
    FILE *fp;
    os_channel_entry *entry;

    /* Valider les parametres */
    if (name == NULL || !gfa_mode_valid(gfa_mode)) {
        g_last_error = OS_ERR_INVALID_HANDLE;
        return NULL;
    }

    if (channel < 0 || channel >= OS_MAX_CHANNELS) {
        g_last_error = OS_ERR_INVALID_HANDLE;
        return NULL;
    }

    /* Verifier si le canal est deja ouvert */
    if (g_channels[channel].in_use) {
        g_last_error = OS_ERR_ACCESS_DENIED;
        return NULL;
    }

    c_mode = gfa_mode_to_c_mode(gfa_mode);
    if (c_mode == NULL) {
        g_last_error = OS_ERR_INVALID_HANDLE;
        return NULL;
    }

    fp = fopen(name, c_mode);
    if (fp == NULL) {
        g_last_error = OS_ERR_FILE_NOT_FOUND;
        return NULL;
    }

    entry = &g_channels[channel];
    entry->stdio_handle = fp;
    entry->os_handle    = (os_file_handle)entry;
    entry->channel      = channel;
    entry->gfa_mode     = gfa_mode;
    entry->in_use       = 1;

    /* Copier le nom du fichier (securise) */
    strncpy(entry->filename, name, sizeof(entry->filename) - 1);
    entry->filename[sizeof(entry->filename) - 1] = '\0';

    g_last_error = OS_ERR_NONE;
    return (os_file_handle)entry;
}

int os_file_close(os_file_handle handle)
{
    os_channel_entry *entry;

    if (handle == NULL) {
        g_last_error = OS_ERR_INVALID_HANDLE;
        return -1;
    }

    entry = (os_channel_entry *)handle;

    if (!entry->in_use || entry->stdio_handle == NULL) {
        g_last_error = OS_ERR_INVALID_HANDLE;
        return -1;
    }

    fclose(entry->stdio_handle);
    entry->stdio_handle = NULL;
    entry->in_use = 0;
    entry->filename[0] = '\0';
    entry->gfa_mode = 0;

    g_last_error = OS_ERR_NONE;
    return 0;
}

int os_file_close_by_channel(int channel)
{
    if (channel < 0 || channel >= OS_MAX_CHANNELS) {
        g_last_error = OS_ERR_INVALID_HANDLE;
        return -1;
    }

    if (!g_channels[channel].in_use) {
        g_last_error = OS_ERR_INVALID_HANDLE;
        return -1;
    }

    return os_file_close((os_file_handle)&g_channels[channel]);
}

os_int32 os_file_read(os_file_handle handle, void *buffer, os_int32 size)
{
    os_channel_entry *entry;
    size_t result;

    if (handle == NULL || buffer == NULL || size < 0) {
        g_last_error = OS_ERR_INVALID_HANDLE;
        return -1;
    }

    entry = (os_channel_entry *)handle;
    if (!entry->in_use || entry->stdio_handle == NULL) {
        g_last_error = OS_ERR_INVALID_HANDLE;
        return -1;
    }

    result = fread(buffer, 1, (size_t)size, entry->stdio_handle);
    if (result == 0 && ferror(entry->stdio_handle)) {
        g_last_error = OS_ERR_READ_FAULT;
        return -1;
    }

    g_last_error = OS_ERR_NONE;
    return (os_int32)result;
}

os_int32 os_file_write(os_file_handle handle, const void *buffer, os_int32 size)
{
    os_channel_entry *entry;
    size_t result;

    if (handle == NULL || buffer == NULL || size < 0) {
        g_last_error = OS_ERR_INVALID_HANDLE;
        return -1;
    }

    entry = (os_channel_entry *)handle;
    if (!entry->in_use || entry->stdio_handle == NULL) {
        g_last_error = OS_ERR_INVALID_HANDLE;
        return -1;
    }

    result = fwrite(buffer, 1, (size_t)size, entry->stdio_handle);
    if (result < (size_t)size) {
        if (ferror(entry->stdio_handle)) {
            g_last_error = OS_ERR_WRITE_FAULT;
        } else {
            g_last_error = OS_ERR_DISK_FULL;
        }
        return -1;
    }

    g_last_error = OS_ERR_NONE;
    return (os_int32)result;
}

os_int32 os_file_seek(os_file_handle handle, os_int32 offset, int whence)
{
    os_channel_entry *entry;
    int c_whence;
    long result;

    if (handle == NULL) {
        g_last_error = OS_ERR_INVALID_HANDLE;
        return -1;
    }

    entry = (os_channel_entry *)handle;
    if (!entry->in_use || entry->stdio_handle == NULL) {
        g_last_error = OS_ERR_INVALID_HANDLE;
        return -1;
    }

    switch (whence) {
        case OS_SEEK_SET: c_whence = SEEK_SET; break;
        case OS_SEEK_CUR: c_whence = SEEK_CUR; break;
        case OS_SEEK_END: c_whence = SEEK_END; break;
        default:
            g_last_error = OS_ERR_INVALID_HANDLE;
            return -1;
    }

    if (fseek(entry->stdio_handle, (long)offset, c_whence) != 0) {
        g_last_error = OS_ERR_INTERNAL;
        return -1;
    }

    result = ftell(entry->stdio_handle);
    if (result < 0) {
        g_last_error = OS_ERR_INTERNAL;
        return -1;
    }

    g_last_error = OS_ERR_NONE;
    return (os_int32)result;
}

os_int32 os_file_tell(os_file_handle handle)
{
    os_channel_entry *entry;
    long result;

    if (handle == NULL) {
        g_last_error = OS_ERR_INVALID_HANDLE;
        return -1;
    }

    entry = (os_channel_entry *)handle;
    if (!entry->in_use || entry->stdio_handle == NULL) {
        g_last_error = OS_ERR_INVALID_HANDLE;
        return -1;
    }

    result = ftell(entry->stdio_handle);
    if (result < 0) {
        g_last_error = OS_ERR_INTERNAL;
        return -1;
    }

    g_last_error = OS_ERR_NONE;
    return (os_int32)result;
}

os_int32 os_file_size(os_file_handle handle)
{
    os_channel_entry *entry;
    long current_pos;
    long file_size;

    if (handle == NULL) {
        g_last_error = OS_ERR_INVALID_HANDLE;
        return -1;
    }

    entry = (os_channel_entry *)handle;
    if (!entry->in_use || entry->stdio_handle == NULL) {
        g_last_error = OS_ERR_INVALID_HANDLE;
        return -1;
    }

    current_pos = ftell(entry->stdio_handle);
    if (current_pos < 0) {
        g_last_error = OS_ERR_INTERNAL;
        return -1;
    }

    if (fseek(entry->stdio_handle, 0, SEEK_END) != 0) {
        g_last_error = OS_ERR_INTERNAL;
        return -1;
    }

    file_size = ftell(entry->stdio_handle);

    /* Restaurer la position */
    fseek(entry->stdio_handle, current_pos, SEEK_SET);

    if (file_size < 0) {
        g_last_error = OS_ERR_INTERNAL;
        return -1;
    }

    g_last_error = OS_ERR_NONE;
    return (os_int32)file_size;
}

int os_file_eof(os_file_handle handle)
{
    os_channel_entry *entry;
    int result;

    if (handle == NULL) {
        g_last_error = OS_ERR_INVALID_HANDLE;
        return OS_FALSE;
    }

    entry = (os_channel_entry *)handle;
    if (!entry->in_use || entry->stdio_handle == NULL) {
        g_last_error = OS_ERR_INVALID_HANDLE;
        return OS_FALSE;
    }

    result = feof(entry->stdio_handle);
    g_last_error = OS_ERR_NONE;
    return result ? OS_TRUE : OS_FALSE;
}

os_file_handle os_file_get_handle_by_channel(int channel)
{
    if (channel < 0 || channel >= OS_MAX_CHANNELS) {
        g_last_error = OS_ERR_INVALID_HANDLE;
        return NULL;
    }

    if (!g_channels[channel].in_use) {
        g_last_error = OS_ERR_INVALID_HANDLE;
        return NULL;
    }

    g_last_error = OS_ERR_NONE;
    return (os_file_handle)&g_channels[channel];
}

int os_file_get_channel(os_file_handle handle)
{
    os_channel_entry *entry;

    if (handle == NULL) {
        return -1;
    }

    entry = (os_channel_entry *)handle;
    if (!entry->in_use) {
        return -1;
    }

    return entry->channel;
}

int os_file_flush(os_file_handle handle)
{
    os_channel_entry *entry;

    if (handle == NULL) {
        g_last_error = OS_ERR_INVALID_HANDLE;
        return -1;
    }

    entry = (os_channel_entry *)handle;
    if (!entry->in_use || entry->stdio_handle == NULL) {
        g_last_error = OS_ERR_INVALID_HANDLE;
        return -1;
    }

    fflush(entry->stdio_handle);
    g_last_error = OS_ERR_NONE;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Operations sur le systeme de fichiers                              */
/* ------------------------------------------------------------------ */

int os_fs_exist(const char *name)
{
    if (name == NULL) {
        g_last_error = OS_ERR_FILE_NOT_FOUND;
        return OS_FALSE;
    }

#ifdef _WIN32
    if (_access(name, 0) == 0) {
        g_last_error = OS_ERR_NONE;
        return OS_TRUE;
    }

    g_last_error = OS_ERR_FILE_NOT_FOUND;
    return OS_FALSE;
#else
    {
        int result;
        char pattern[256];
        size_t len;

        len = strlen(name);
        if (len < sizeof(pattern) - 2) {
            strcpy(pattern, name);
            pattern[len++] = '\\';
            pattern[len++] = '*';
            pattern[len] = '\0';
        } else {
            strncpy(pattern, name, sizeof(pattern) - 1);
            pattern[sizeof(pattern) - 1] = '\0';
        }

        result = Fsfirst(pattern, 0);
        if (result == 0) {
            g_last_error = OS_ERR_NONE;
            return OS_TRUE;
        }

        g_last_error = OS_ERR_FILE_NOT_FOUND;
        return OS_FALSE;
    }
#endif
}

int os_fs_delete(const char *name)
{
    if (name == NULL) {
        g_last_error = OS_ERR_FILE_NOT_FOUND;
        return -1;
    }

    if (OS_UNLINK(name) != 0) {
        g_last_error = OS_ERR_FILE_NOT_FOUND;
        return -1;
    }

    g_last_error = OS_ERR_NONE;
    return 0;
}

int os_fs_rename(const char *old_name, const char *new_name)
{
    if (old_name == NULL || new_name == NULL) {
        g_last_error = OS_ERR_FILE_NOT_FOUND;
        return -1;
    }

    if (OS_RENAME(old_name, new_name) != 0) {
        g_last_error = OS_ERR_FILE_NOT_FOUND;
        return -1;
    }

    g_last_error = OS_ERR_NONE;
    return 0;
}

os_int32 os_fs_free(int drive)
{
#ifdef _WIN32
    (void)drive;  /* Simplifie : retourne une estimation */

    g_last_error = OS_ERR_NONE;
    return (os_int32)(10L * 1024L * 1024L);
#else
    /*
     * GEMDOS Dfree : interroge la table d'allocation du disque.
     * info[0] = secteurs totaux, info[1] = secteurs libres,
     * info[2] = octets/secteur, info[3] = secteurs/cluster.
     * drive : 0 = courant, 1 = A:, 2 = B:, ...
     */
    {
        long info[4];
        Dfree(info, drive);
        g_last_error = OS_ERR_NONE;
        return (os_int32)(info[1] * info[2]);
    }
#endif
}

os_int32 os_fs_total(int drive)
{
#ifdef _WIN32
    (void)drive;
    g_last_error = OS_ERR_NONE;
    return (os_int32)(100L * 1024L * 1024L);
#else
    {
        long info[4];
        Dfree(info, drive);
        g_last_error = OS_ERR_NONE;
        return (os_int32)(info[0] * info[2]);
    }
#endif
}

/* ------------------------------------------------------------------ */
/* Repertoires                                                        */
/* ------------------------------------------------------------------ */

int os_dir_mkdir(const char *name)
{
    if (name == NULL) {
        g_last_error = OS_ERR_PATH_NOT_FOUND;
        return -1;
    }

    if (OS_MKDIR(name) != 0) {
        g_last_error = OS_ERR_PATH_NOT_FOUND;
        return -1;
    }

    g_last_error = OS_ERR_NONE;
    return 0;
}

int os_dir_rmdir(const char *name)
{
    if (name == NULL) {
        g_last_error = OS_ERR_PATH_NOT_FOUND;
        return -1;
    }

    if (OS_RMDIR(name) != 0) {
        g_last_error = OS_ERR_PATH_NOT_FOUND;
        return -1;
    }

    g_last_error = OS_ERR_NONE;
    return 0;
}

int os_dir_chdir(const char *name)
{
    if (name == NULL) {
        g_last_error = OS_ERR_PATH_NOT_FOUND;
        return -1;
    }

    if (OS_CHDIR(name) != 0) {
        g_last_error = OS_ERR_PATH_NOT_FOUND;
        return -1;
    }

    g_last_error = OS_ERR_NONE;
    return 0;
}

char* os_dir_getcwd(int *len)
{
    char *buf;
    size_t size;
    int length;

    size = 256;

    for (;;) {
        buf = (char *)os_mem_alloc(size);
        if (buf == NULL) {
            g_last_error = OS_ERR_OUT_OF_MEMORY;
            return NULL;
        }

        if (OS_GETCWD(buf, size) >= 0) {
            length = (int)strlen(buf);
            if (len != NULL) {
                *len = length;
            }
            g_last_error = OS_ERR_NONE;
            return buf;
        }

        os_mem_free(buf);

        /* Si le buffer est trop petit, augmenter la taille */
        if (size > 65536) {
            g_last_error = OS_ERR_INTERNAL;
            return NULL;
        }
        size *= 2;
    }
}

int os_dir_first(const char *pattern, int attr, os_file_info *info)
{
    if (pattern == NULL || info == NULL) {
        g_last_error = OS_ERR_FILE_NOT_FOUND;
        return OS_ERR_FILE_NOT_FOUND;
    }

#ifdef _WIN32
    {
        WIN32_FIND_DATA find_data;
        HANDLE hFind;

        hFind = FindFirstFile(pattern, &find_data);
        if (hFind == INVALID_HANDLE_VALUE) {
            g_last_error = OS_ERR_NO_MORE_FILES;
            return OS_ERR_NO_MORE_FILES;
        }

        /* Stocker le handle pour les appels suivants */
        /* Approche simplifiee : utilisation d'une statique */

        strncpy(g_dir_pattern, pattern, sizeof(g_dir_pattern) - 1);
        g_dir_pattern[sizeof(g_dir_pattern) - 1] = '\0';
        g_dir_attr_mask = attr;

        strncpy(info->name, find_data.cFileName, 13);
        info->name[13] = '\0';
        info->attr = (os_byte)find_data.dwFileAttributes;
        info->size = (os_int32)find_data.nFileSizeLow;

        /* Format GEMDOS packe pour la date/heure (simplifie) */
        {
            SYSTEMTIME st;
            FileTimeToSystemTime(&find_data.ftLastWriteTime, &st);
            info->date = (os_word)(((st.wYear - 1980) << 9) |
                                   (st.wMonth << 5) |
                                   st.wDay);
            info->time = (os_word)((st.wHour << 11) |
                                   (st.wMinute << 5) |
                                   (st.wSecond / 2));
        }

        FindClose(hFind);

        g_last_error = OS_ERR_NONE;
        return 0;
    }
#else
    /*
     * GEMDOS Fsfirst/Fsnext : la DTA globale contient le resultat
     * (attributs, date/heure packees, taille, nom 8.3).
     */
    {
        int result;
#ifdef GFA_TARGET_MINT
        _DTA *dta;
#else
        DTA *dta;
#endif

        strncpy(g_dir_pattern, pattern, sizeof(g_dir_pattern) - 1);
        g_dir_pattern[sizeof(g_dir_pattern) - 1] = '\0';
        g_dir_attr_mask = attr;

        result = Fsfirst(pattern, attr);
        if (result < 0) {
            g_last_error = OS_ERR_NO_MORE_FILES;
            return OS_ERR_NO_MORE_FILES;
        }

        dta = Fgetdta();
        strncpy(info->name, dta->dta_name, 13);
        info->name[13] = '\0';
#ifdef GFA_TARGET_MINT
        info->attr = (os_byte)dta->dta_attribute;
#else
        info->attr = (os_byte)dta->dta_attr;
#endif
        info->size = (os_int32)dta->dta_size;
        info->time = dta->dta_time;
        info->date = dta->dta_date;

        g_last_error = OS_ERR_NONE;
        return 0;
    }
#endif
}

int os_dir_next(os_file_info *info)
{
    if (info == NULL) {
        g_last_error = OS_ERR_NO_MORE_FILES;
        return OS_ERR_NO_MORE_FILES;
    }

#ifdef _WIN32
    {
        WIN32_FIND_DATA find_data;
        HANDLE hFind;

        hFind = FindFirstFile(g_dir_pattern, &find_data);
        if (hFind == INVALID_HANDLE_VALUE) {
            /* Pas de nouveau fichier */
            g_last_error = OS_ERR_NO_MORE_FILES;
            return OS_ERR_NO_MORE_FILES;
        }

        /* Approche simplifiee : retourne toujours le meme fichier. */
        strncpy(info->name, find_data.cFileName, 13);
        info->name[13] = '\0';
        info->attr = (os_byte)find_data.dwFileAttributes;
        info->size = (os_int32)find_data.nFileSizeLow;
        {
            SYSTEMTIME st;
            FileTimeToSystemTime(&find_data.ftLastWriteTime, &st);
            info->date = (os_word)(((st.wYear - 1980) << 9) |
                                   (st.wMonth << 5) |
                                   st.wDay);
            info->time = (os_word)((st.wHour << 11) |
                                   (st.wMinute << 5) |
                                   (st.wSecond / 2));
        }
        FindClose(hFind);
        g_last_error = OS_ERR_NONE;
        return 0;
    }
#else
    {
        int result;
#ifdef GFA_TARGET_MINT
        _DTA *dta;
#else
        DTA *dta;
#endif

        result = Fsnext();
        if (result < 0) {
            g_last_error = OS_ERR_NO_MORE_FILES;
            return OS_ERR_NO_MORE_FILES;
        }

        dta = Fgetdta();
        strncpy(info->name, dta->dta_name, 13);
        info->name[13] = '\0';
#ifdef GFA_TARGET_MINT
        info->attr = (os_byte)dta->dta_attribute;
#else
        info->attr = (os_byte)dta->dta_attr;
#endif
        info->size = (os_int32)dta->dta_size;
        info->time = dta->dta_time;
        info->date = dta->dta_date;

        g_last_error = OS_ERR_NONE;
        return 0;
    }
#endif
}

/* ------------------------------------------------------------------ */
/* Console                                                            */
/* ------------------------------------------------------------------ */

int os_con_input_char(void)
{
#ifdef _WIN32
    {
        int c;
        c = getchar();
        if (c == EOF) {
            g_last_error = OS_ERR_READ_FAULT;
            return -1;
        }
        g_last_error = OS_ERR_NONE;
        return c;
    }
#else
    {
        int c;
        c = Cconin() & 0xFF;
        g_last_error = OS_ERR_NONE;
        return c;
    }
#endif
}

int os_con_input_key(void)
{
#ifdef _WIN32
    if (_kbhit()) {
        return _getch();
    }
    return -1;
#else
    if (Cconis()) {
        return Cconin() & 0xFF;
    }
    return -1;
#endif
}

void os_con_output_char(int c)
{
    putchar(c);
    fflush(stdout);
}

void os_con_output_string(const char *s)
{
    if (s != NULL) {
        fputs(s, stdout);
        fflush(stdout);
    }
}

void os_con_cursor_goto(int col, int line)
{
    /*
     * Sequence d'echappement VT-52 pour positionnement curseur.
     * Format : ESC Y l c  (ligne, colonne en ASCII + 32)
     *
     * Egalement supportee : sequence ANSI CSI pour les terminaux
     * modernes.
     */
    if (col < 1) col = 1;
    if (line < 1) line = 1;

    /*
     * On emet les deux sequences : d'abord ANSI (moderne),
     * puis VT-52 (fallback).
     */
    /* ANSI : ESC [ line ; col H */
    fprintf(stdout, "\033[%d;%dH", line, col);

    /* VT-52 : ESC Y line+31 col+31 */
    if (line <= 24 && col <= 80) {
        fprintf(stdout, "\033Y%c%c", (char)(line + 31), (char)(col + 31));
    }

    fflush(stdout);
}

int os_con_cursor_get_x(void)
{
    /*
     * POSITION DU CURSEUR : Necessite l'interrogation du terminal
     * via DSR (Device Status Report). Retourne 1 par defaut.
     * Le module console (console.c) pourra maintenir cette position
     * via un suivi logiciel.
     */
    return 1;
}

int os_con_cursor_get_y(void)
{
    return 1;
}

void os_con_clear(void)
{
    /*
     * CLS en GFA Basic emet ESC E puis CR (VT-52).
     * On emet aussi la sequence ANSI pour compatibilite.
     */
    /* ANSI : ESC [ 2 J */
    fprintf(stdout, "\033[2J");
    /* VT-52 : ESC E */
    fprintf(stdout, "\033E");
    /* Retour en haut a gauche */
    fprintf(stdout, "\r");
    fflush(stdout);
}

void os_con_clear_to_eol(void)
{
    /* ANSI : ESC [ K */
    fprintf(stdout, "\033[K");
    fflush(stdout);
}

void os_con_set_echo(int on)
{
    g_con_echo = on ? 1 : 0;
    /* L'implementation reelle necessite SetConsoleMode (Windows).
       Pour le moment, simple flag. */
}

int os_con_get_echo(void)
{
    return g_con_echo;
}

/* ------------------------------------------------------------------ */
/* Temps et minuteurs                                                 */
/* ------------------------------------------------------------------ */

os_int32 os_time_ticks(void)
{
    /*
     * TIMER GFA : retourne le nombre de ticks 1/200s depuis le boot.
     * Sur Atari ST : LPEEK(&H4BA).
     * Ici : conversion depuis millisecondes.
     */
    os_int32 delta_ms;
    os_int32 ticks;

    delta_ms = get_ms_now() - g_startup_ticks_ms;
    /* Conversion ms -> ticks 200 Hz avec arrondi */
    ticks = (os_int32)(((os_int32)(delta_ms) * 200L) / 1000L);

    return ticks;
}

os_int32 os_time_millis(void)
{
    os_int32 delta_ms;
    delta_ms = get_ms_now() - g_startup_ticks_ms;
    return delta_ms;
}

void os_time_delay(os_int32 delay_ms)
{
    if (delay_ms > 0) {
        OS_SLEEP_MS((unsigned long)delay_ms);
    }
}

const char* os_time_get_date(int us_format)
{
    time_t now;
    struct tm *lt;

    if (g_virtual_clock_active) {
        if (us_format) {
            sprintf(g_date_buffer, "%02d/%02d/%04d",
                    g_virtual_month, g_virtual_day, g_virtual_year);
        } else {
            sprintf(g_date_buffer, "%02d.%02d.%04d",
                    g_virtual_day, g_virtual_month, g_virtual_year);
        }
        return g_date_buffer;
    }

    now = time(NULL);
    lt = localtime(&now);

    if (lt == NULL) {
        g_date_buffer[0] = '\0';
        return g_date_buffer;
    }

    if (us_format) {
        /* Format US : MM/DD/YYYY */
        sprintf(g_date_buffer, "%02d/%02d/%04d",
                (lt->tm_mon + 1) % 100, (lt->tm_mday) % 100,
                (lt->tm_year + 1900) % 10000);
    } else {
        /* Format europeen : DD.MM.YYYY */
        sprintf(g_date_buffer, "%02d.%02d.%04d",
                (lt->tm_mday) % 100, (lt->tm_mon + 1) % 100,
                (lt->tm_year + 1900) % 10000);
    }

    return g_date_buffer;
}

const char* os_time_get_time(void)
{
    time_t now;
    struct tm *lt;

    if (g_virtual_clock_active) {
        sprintf(g_time_buffer, "%02d:%02d:%02d",
                g_virtual_hour, g_virtual_min, g_virtual_sec);
        return g_time_buffer;
    }

    now = time(NULL);
    lt = localtime(&now);

    if (lt == NULL) {
        g_time_buffer[0] = '\0';
        return g_time_buffer;
    }

    sprintf(g_time_buffer, "%02d:%02d:%02d",
            lt->tm_hour, lt->tm_min, lt->tm_sec);

    return g_time_buffer;
}

int os_time_set_date(const char *date_str)
{
    int day, month, year;

    if (date_str == NULL) {
        g_last_error = OS_ERR_INTERNAL;
        return -1;
    }

    /* Essayer les deux formats */
    if (sscanf(date_str, "%d.%d.%d", &day, &month, &year) == 3 ||
        sscanf(date_str, "%d/%d/%d", &month, &day, &year) == 3) {
        /* La modification de la date systeme necessite des
           privileges. On stocke simplement la valeur ; le module
           GEMDOS/TOS pourra l'utiliser. */
        g_virtual_clock_active = 1;
        g_virtual_day   = day;
        g_virtual_month = month;
        g_virtual_year  = year;
        g_last_error = OS_ERR_NONE;
        return 0;
    }

    g_last_error = OS_ERR_INTERNAL;
    return -1;
}

int os_time_set_time(const char *time_str)
{
    int hour, min, sec;

    if (time_str == NULL) {
        g_last_error = OS_ERR_INTERNAL;
        return -1;
    }

    if (sscanf(time_str, "%d:%d:%d", &hour, &min, &sec) == 3) {
        g_virtual_clock_active = 1;
        g_virtual_hour = hour;
        g_virtual_min  = min;
        g_virtual_sec  = sec;
        g_last_error = OS_ERR_NONE;
        return 0;
    }

    g_last_error = OS_ERR_INTERNAL;
    return -1;
}

int os_time_set_datetime(const char *time_str, const char *date_str)
{
    int result;

    result = os_time_set_time(time_str);
    if (result != 0) return result;

    result = os_time_set_date(date_str);
    return result;
}

os_int32 os_time_get_raw_gemdos(void)
{
    /*
     * Format GEMDOS packe :
     *   Bits 0-4   : secondes / 2
     *   Bits 5-10  : minutes (0-59)
     *   Bits 11-15 : heures (0-23)
     *   Bits 16-20 : jour (1-31)
     *   Bits 21-24 : mois (1-12)
     *   Bits 25-31 : annee - 1980
     */
#ifdef _WIN32
    {
        time_t now;
        struct tm *lt;
        os_int32 packed;

        now = time(NULL);
        lt = localtime(&now);

        if (lt == NULL) return 0;

        packed  = (lt->tm_sec / 2) & 0x1F;
        packed |= (lt->tm_min & 0x3F) << 5;
        packed |= (lt->tm_hour & 0x1F) << 11;
        packed |= (lt->tm_mday & 0x1F) << 16;
        packed |= ((lt->tm_mon + 1) & 0x0F) << 21;
        packed |= ((lt->tm_year + 1900 - 1980) & 0x7F) << 25;

        return packed;
    }
#else
    {
        os_int32 packed;

        packed  = (os_int32)Tgettime() & 0xFFFF;         /* heure 16 bits */
        packed |= ((os_int32)Tgetdate() & 0xFFFF) << 16; /* date 16 bits  */

        return packed;
    }
#endif
}

void os_time_set_raw_gemdos(os_int32 packed_time)
{
    /*
     * La modification de l'heure systeme necessite des privileges.
     * On decode simplement la valeur pour le suivi logiciel.
     */
#ifdef _WIN32
    (void)packed_time;
#else
    if (g_virtual_clock_active) {
        os_int32 date;
        os_int32 time;

        date = (packed_time >> 16) & 0xFFFF;
        time = packed_time & 0xFFFF;

        g_virtual_year  = ((date >> 9) & 0x7F) + 1980;
        g_virtual_month = (date >> 5) & 0x0F;
        g_virtual_day   = date & 0x1F;
        g_virtual_hour  = (time >> 11) & 0x1F;
        g_virtual_min   = (time >> 5) & 0x3F;
        g_virtual_sec   = (time & 0x1F) * 2;
    }
#endif
}

/* ------------------------------------------------------------------ */
/* Memoire                                                            */
/* ------------------------------------------------------------------ */

void* os_mem_alloc(size_t size)
{
    void *ptr;
    if (size == 0) {
        g_last_error = OS_ERR_NONE;
        return NULL;
    }
    ptr = malloc(size);
    if (ptr == NULL) {
        g_last_error = OS_ERR_OUT_OF_MEMORY;
    } else {
        g_last_error = OS_ERR_NONE;
    }
    return ptr;
}

void os_mem_free(void *ptr)
{
    if (ptr != NULL) {
        free(ptr);
    }
    g_last_error = OS_ERR_NONE;
}

void* os_mem_realloc(void *ptr, size_t new_size)
{
    void *new_ptr;
    if (new_size == 0) {
        os_mem_free(ptr);
        g_last_error = OS_ERR_NONE;
        return NULL;
    }
    if (ptr == NULL) {
        return os_mem_alloc(new_size);
    }
    new_ptr = realloc(ptr, new_size);
    if (new_ptr == NULL) {
        g_last_error = OS_ERR_OUT_OF_MEMORY;
    } else {
        g_last_error = OS_ERR_NONE;
    }
    return new_ptr;
}

void os_mem_copy(void *dst, const void *src, size_t n)
{
    if (dst != NULL && src != NULL && n > 0) {
        memmove(dst, src, n);
    }
}

void os_mem_set(void *dst, int c, size_t n)
{
    if (dst != NULL && n > 0) {
        memset(dst, c, n);
    }
}

char* os_strdup(const char *s)
{
    size_t len;
    char  *dup;

    if (s == NULL) {
        g_last_error = OS_ERR_NONE;
        return NULL;
    }
    len = strlen(s);
    dup = (char *)os_mem_alloc(len + 1);
    if (dup == NULL) {
        return NULL;  /* g_last_error deja positionne par os_mem_alloc */
    }
    os_mem_copy(dup, s, len + 1);
    g_last_error = OS_ERR_NONE;
    return dup;
}

os_int32 os_mem_available(os_int32 *total)
{
    /*
     * FRE(0) retourne la memoire libre.
     */
#ifdef _WIN32
    if (total != NULL) {
        *total = (os_int32)(256L * 1024L * 1024L);  /* 256 Mo */
    }

    g_last_error = OS_ERR_NONE;
    return (os_int32)(128L * 1024L * 1024L);  /* 128 Mo libres */
#else
    {
        long mem;
        long total_bytes;
        long free_bytes;

        mem = (long)Malloc(-1L);
        total_bytes = mem;
        mem = (long)Malloc(0L);
        free_bytes = mem;

        if (total != NULL) {
            *total = (os_int32)total_bytes;
        }

        g_last_error = OS_ERR_NONE;
        return (os_int32)free_bytes;
    }
#endif
}

os_int32 os_mem_largest_block(void)
{
    /*
     * FRE(1) : taille du plus grand bloc contigu.
     */
#ifdef _WIN32
    g_last_error = OS_ERR_NONE;
    return (os_int32)(64L * 1024L * 1024L);
#else
    {
        long largest;
        largest = (long)Malloc(-2L);
        g_last_error = OS_ERR_NONE;
        return (os_int32)largest;
    }
#endif
}

/* ------------------------------------------------------------------ */
/* Son                                                                */
/* ------------------------------------------------------------------ */

void os_sound_beep(void)
{
    /* BEEP : emet un bip systeme */
#ifdef _WIN32
    MessageBeep(MB_OK);
#else
    Bconout(0, 7);  /* BIOS : caractere BEL (0x07) sur la console */
#endif
}

int os_sound_init(void)
{
    /* Le sous-systeme audio sera initialise dans le module sound.c */
    g_last_error = OS_ERR_NONE;
    return 0;
}

void os_sound_shutdown(void)
{
    g_last_error = OS_ERR_NONE;
}

void os_sound_tone(int channel, int freq_hz, int duration_ms, int volume)
{
    /*
     * L'implementation reelle du YM-2149 sera faite dans le module
     * sound.c. Pour le moment, emet un simple bip console.
     */
    (void)channel;
    (void)freq_hz;
    (void)duration_ms;
    (void)volume;

    if (volume > 0) {
        os_sound_beep();
    }
}

void os_sound_stop_channel(int channel)
{
    (void)channel;
    g_last_error = OS_ERR_NONE;
}

void os_sound_stop_all(void)
{
    g_last_error = OS_ERR_NONE;
}

/* ------------------------------------------------------------------ */
/* Divers systeme                                                     */
/* ------------------------------------------------------------------ */

const char* os_sys_get_env(const char *name)
{
    const char *value;

    if (name == NULL) {
        g_env_buffer[0] = '\0';
        return g_env_buffer;
    }

    value = getenv(name);
    if (value == NULL) {
        g_env_buffer[0] = '\0';
    } else {
        strncpy(g_env_buffer, value, sizeof(g_env_buffer) - 1);
        g_env_buffer[sizeof(g_env_buffer) - 1] = '\0';
    }

    g_last_error = OS_ERR_NONE;
    return g_env_buffer;
}

int os_sys_get_drive(void)
{
    /*
     * Sur Atari ST : 0 = courant, 1 = A:, 2 = B: ...
     */
#ifdef _WIN32
    {
        char cwd[256];
        if (_getcwd(cwd, sizeof(cwd)) != NULL && cwd[0] != '\0' && cwd[1] == ':') {
            return (toupper(cwd[0]) - 'A' + 1);
        }
    }
    return 1;  /* Simuler lecteur A: */
#else
    {
        char path[256];

        if (Dgetpath(path, 0) >= 0) {
            if (path[0] >= 'A' && path[0] <= 'Z') {
                return (path[0] - 'A' + 1);
            }
            if (path[0] >= 'a' && path[0] <= 'z') {
                return (path[0] - 'a' + 1);
            }
        }
        return 1;  /* Lecteur par defaut A: */
    }
#endif
}

int os_sys_set_drive(int drive)
{
    if (drive < 1 || drive > 26) {
        g_last_error = OS_ERR_INVALID_DRIVE;
        return -1;
    }

#ifdef _WIN32
    {
        char path[4];
        path[0] = (char)('A' + drive - 1);
        path[1] = ':';
        path[2] = '\\';
        path[3] = '\0';
        if (!SetCurrentDirectory(path)) {
            g_last_error = OS_ERR_INVALID_DRIVE;
            return -1;
        }
    }
#else
    Dsetdrv(drive - 1);
#endif
    g_last_error = OS_ERR_NONE;
    return 0;
}

os_int32 os_sys_get_basepage(void)
{
    /*
     * BASEPAGE : adresse de la page de base du programme.
     * Sur l'Atari ST, la basepage est une structure de 256 octets
     * placee au debut de l'espace memoire du processus.
     */
#ifdef _WIN32
    g_last_error = OS_ERR_NONE;
    return (os_int32)0x10000L;  /* Adresse factice */
#else
#ifdef GFA_TARGET_MINT
    g_last_error = OS_ERR_NONE;
    return (os_int32)(size_t)_base;
#else
    g_last_error = OS_ERR_NONE;
    return (os_int32)(size_t)_BasPag;
#endif
#endif
}

void os_sys_quit(int exit_code)
{
    /* Fermer tout proprement */
    os_shutdown();
    exit(exit_code);
}