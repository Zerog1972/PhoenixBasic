/*
 * os_layer.c - Implementation de la couche d'abstraction OS
 * ===========================================================
 * Fournit toutes les primitives systeme necessaires a l'emulateur
 * GFA Basic 3.5. Implementation portable en C89 strict.
 *
 * Plateformes supportees :
 *   - Linux/Unix (POSIX)
 *   - Windows (MinGW/Cygwin)
 *   - macOS (POSIX)
 *
 * Conventions :
 *   - C89 strict (pas de //, pas d'inline, pas de bool, declarations
 *     en tete de bloc)
 *
 * Reference : cahier-des-charges-gfabasic.md, sections 2.2, 7, 9, 15.2
 */

/*
 * Feature test macros pour C89/POSIX. Doivent etre avant tout include.
 */
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "os_layer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/* ------------------------------------------------------------------ */
/* Sélection de la plateforme                                         */
/* ------------------------------------------------------------------ */
#if defined(_WIN32) || defined(_WIN64) || defined(__MINGW32__)
    #define OS_PLATFORM_WINDOWS 1
    #include <windows.h>
    #include <direct.h>
    #include <io.h>
    #define OS_MKDIR(path)  _mkdir(path)
    #define OS_RMDIR(path)  _rmdir(path)
    #define OS_CHDIR(path)  _chdir(path)
    #define OS_GETCWD(buf,s) _getcwd(buf,s)
    #define OS_UNLINK(path) _unlink(path)
    #define OS_RENAME(o,n)  rename(o,n)
    #define OS_ACCESS(path,m) _access(path,m)
    #define OS_SLEEP_MS(ms) Sleep(ms)
    typedef DWORD os_thread_id;
    /* Pour les fonctions non supportées nativement */
    #ifndef OS_HAVE_GETTIMEOFDAY
        #define OS_HAVE_GETTIMEOFDAY 0
    #endif
#else
    /* POSIX (Linux, macOS, BSD, etc.) */
    #define OS_PLATFORM_POSIX 1
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/time.h>
    #include <sys/types.h>
    #include <dirent.h>
    #include <errno.h>
    #define OS_MKDIR(path)  mkdir(path, 0755)
    #define OS_RMDIR(path)  rmdir(path)
    #define OS_CHDIR(path)  chdir(path)
    #define OS_GETCWD(buf,s) getcwd(buf,s)
    #define OS_UNLINK(path) unlink(path)
    #define OS_RENAME(o,n)  rename(o,n)
    #define OS_ACCESS(path,m) access(path,m)
    #define OS_SLEEP_MS(ms) usleep((ms) * 1000)
    #ifdef __APPLE__
        #define OS_HAVE_GETTIMEOFDAY 1
    #else
        #define OS_HAVE_GETTIMEOFDAY 1
    #endif
#endif

/* ------------------------------------------------------------------ */
/* Constantes spécifiques Atari ST                                    */
/* ------------------------------------------------------------------ */

/* Moment de démarrage du programme (pour émuler TIMER) */
static os_int32 g_startup_ticks_ms = 0;

/* Palette Atari ST standard (16 couleurs, format 0x00RRGGBB) */
const unsigned long os_st_palette[16] = {
    0x00000000UL,  /*  0: Noir           */
    0x00000080UL,  /*  1: Bleu foncé     */
    0x00008000UL,  /*  2: Vert foncé     */
    0x00008080UL,  /*  3: Cyan foncé     */
    0x00800000UL,  /*  4: Rouge foncé    */
    0x00800080UL,  /*  5: Magenta foncé  */
    0x00808000UL,  /*  6: Marron         */
    0x00C0C0C0UL,  /*  7: Gris clair     */
    0x00808080UL,  /*  8: Gris foncé     */
    0x000000FFUL,  /*  9: Bleu clair     */
    0x0000FF00UL,  /* 10: Vert clair     */
    0x0000FFFFUL,  /* 11: Cyan clair     */
    0x00FF0000UL,  /* 12: Rouge clair    */
    0x00FF00FFUL,  /* 13: Magenta clair  */
    0x00FFFF00UL,  /* 14: Jaune          */
    0x00FFFFFFUL   /* 15: Blanc          */
};

/* Fichier périphérique nul */
#if defined(OS_PLATFORM_WINDOWS)
    #define OS_NULL_DEVICE "NUL"
#else
    #define OS_NULL_DEVICE "/dev/null"
#endif

/* ------------------------------------------------------------------ */
/* État global du module                                              */
/* ------------------------------------------------------------------ */

/* Erreur courante */
static os_error_code g_last_error = OS_ERR_NONE;

/* Driver d'affichage enregistré */
static const os_display_driver *g_display_driver = NULL;

/* Résolution d'affichage courante */
static int g_display_resolution = -1;

/* Écho console */
static int g_con_echo = 1;

/* Buffer pour les chaînes date/heure retournées */
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
    int             channel;       /* numéro de canal (0-99)        */
    char            gfa_mode;      /* mode GFA : 'I','O','A','R','U' */
    char            filename[256]; /* nom du fichier                */
    int             in_use;        /* drapeau d'utilisation          */
} os_channel_entry;

static os_channel_entry g_channels[OS_MAX_CHANNELS];

/* Buffer pour os_dir_first/next (émulation FSFIRST/FSNEXT sur POSIX) */
#if defined(OS_PLATFORM_POSIX)
static DIR            *g_dir_stream = NULL;
static char            g_dir_pattern[256];
static int             g_dir_attr_mask;
#endif

/* ------------------------------------------------------------------ */
/* Fonctions utilitaires internes                                     */
/* ------------------------------------------------------------------ */

/*
 * get_ms_now — Retourne le temps courant en millisecondes.
 * Utilise gettimeofday (POSIX) ou GetTickCount (Windows).
 */
static os_int32 get_ms_now(void)
{
#if defined(OS_PLATFORM_WINDOWS)
    return (os_int32)GetTickCount();
#elif OS_HAVE_GETTIMEOFDAY
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (os_int32)((tv.tv_sec * 1000L) + (tv.tv_usec / 1000L));
#else
    return (os_int32)((clock() * 1000L) / CLOCKS_PER_SEC);
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
 * en chaîne de mode fopen().
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
 * gfa_mode_valid — Vérifie si le mode d'ouverture est valide.
 */
static int gfa_mode_valid(char mode)
{
    return (mode == OS_GFAMODE_INPUT  || mode == OS_GFAMODE_OUTPUT ||
            mode == OS_GFAMODE_APPEND || mode == OS_GFAMODE_RANDOM ||
            mode == OS_GFAMODE_UPDATE);
}

/* ------------------------------------------------------------------ */
/* Initialisation / Arrêt                                             */
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
    /* Arrêter l'affichage si actif */
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

    /* Valider les paramètres */
    if (name == NULL || !gfa_mode_valid(gfa_mode)) {
        g_last_error = OS_ERR_INVALID_HANDLE;
        return NULL;
    }

    if (channel < 0 || channel >= OS_MAX_CHANNELS) {
        g_last_error = OS_ERR_INVALID_HANDLE;
        return NULL;
    }

    /* Vérifier si le canal est déjà ouvert */
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
#if defined(OS_PLATFORM_POSIX)
        if (errno == ENOENT) {
            g_last_error = OS_ERR_FILE_NOT_FOUND;
        } else if (errno == EACCES || errno == EROFS) {
            g_last_error = OS_ERR_ACCESS_DENIED;
        } else if (errno == ENOSPC || errno == EDQUOT) {
            g_last_error = OS_ERR_DISK_FULL;
        } else if (errno == EMFILE || errno == ENFILE) {
            g_last_error = OS_ERR_TOO_MANY_OPEN;
        } else {
            g_last_error = OS_ERR_INTERNAL;
        }
#else
        g_last_error = OS_ERR_FILE_NOT_FOUND;
#endif
        return NULL;
    }

    entry = &g_channels[channel];
    entry->stdio_handle = fp;
    entry->os_handle    = (os_file_handle)entry;
    entry->channel      = channel;
    entry->gfa_mode     = gfa_mode;
    entry->in_use       = 1;

    /* Copier le nom du fichier (sécurisé) */
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
/* Opérations sur le système de fichiers                              */
/* ------------------------------------------------------------------ */

int os_fs_exist(const char *name)
{
    if (name == NULL) {
        g_last_error = OS_ERR_FILE_NOT_FOUND;
        return OS_FALSE;
    }

#if defined(OS_PLATFORM_WINDOWS)
    if (_access(name, 0) == 0) {
#else
    if (access(name, F_OK) == 0) {
#endif
        g_last_error = OS_ERR_NONE;
        return OS_TRUE;
    }

    g_last_error = OS_ERR_FILE_NOT_FOUND;
    return OS_FALSE;
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
    (void)drive;  /* Simplifié : retourne une estimation */

    /*
     * Sur un vrai Atari ST, DFREE lit la table d'allocation FAT.
     * Ici on retourne une estimation basée sur la mémoire disponible
     * du système hôte. Les couches supérieures (GEMDOS) peuvent
     * spécialiser ce comportement.
     */

    g_last_error = OS_ERR_NONE;
    /*
     * Retourne une valeur arbitraire élevée pour simuler un disque
     * raisonnablement rempli (environ 10 Mo libres).
     * Les implémentations réelles de GEMDOS feront un DFREE précis.
     */
    return (os_int32)(10L * 1024L * 1024L);
}

os_int32 os_fs_total(int drive)
{
    (void)drive;
    g_last_error = OS_ERR_NONE;
    return (os_int32)(100L * 1024L * 1024L);
}

/* ------------------------------------------------------------------ */
/* Répertoires                                                        */
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

        if (OS_GETCWD(buf, size) != NULL) {
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

#if defined(OS_PLATFORM_WINDOWS)
    {
        WIN32_FIND_DATA find_data;
        HANDLE hFind;
        int found;

        hFind = FindFirstFile(pattern, &find_data);
        if (hFind == INVALID_HANDLE_VALUE) {
            g_last_error = OS_ERR_NO_MORE_FILES;
            return OS_ERR_NO_MORE_FILES;
        }

        /* Stocker le handle pour les appels suivants */
        /* Approche simplifiée : utilisation d'une statique */
        /* Note : une implémentation complète nécessiterait de gérer
           plusieurs recherches simultanées. */

        strncpy(g_dir_pattern, pattern, sizeof(g_dir_pattern) - 1);
        g_dir_pattern[sizeof(g_dir_pattern) - 1] = '\0';
        g_dir_attr_mask = attr;

        /* Remplir les infos */
        strncpy(info->name, find_data.cFileName, 13);
        info->name[13] = '\0';
        info->attr = (os_byte)find_data.dwFileAttributes;
        info->size = (os_int32)find_data.nFileSizeLow;

        /* Format GEMDOS packé pour la date/heure (simplifié) */
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
        found = 1; /* Marque trouvé */

        g_last_error = OS_ERR_NONE;
        return 0;
    }
#else
    {
        /* POSIX : utiliser opendir/readdir */
        char dir_path[256];
        const char *last_slash;
        DIR *dir;
        size_t pattern_len;

        (void)attr; /* Le filtrage par attribut sera fait au niveau GEMDOS */

        /* Extraire le chemin du répertoire à partir du pattern */
        last_slash = strrchr(pattern, '/');
#if defined(_WIN32)
        {
            const char *bslash = strrchr(pattern, '\\');
            if (bslash != NULL && (last_slash == NULL || bslash > last_slash)) {
                last_slash = bslash;
            }
        }
#endif
        if (last_slash != NULL) {
            size_t dir_len = (size_t)(last_slash - pattern);
            if (dir_len >= sizeof(dir_path)) dir_len = sizeof(dir_path) - 1;
            strncpy(dir_path, pattern, dir_len);
            dir_path[dir_len] = '\0';
        } else {
            strcpy(dir_path, ".");
        }

        dir = opendir(dir_path);
        if (dir == NULL) {
            g_last_error = OS_ERR_NO_MORE_FILES;
            return OS_ERR_NO_MORE_FILES;
        }

        /* Sauvegarder le stream pour os_dir_next */
        if (g_dir_stream != NULL) {
            closedir(g_dir_stream);
        }
        g_dir_stream = dir;

        /* Sauvegarder le pattern */
        pattern_len = strlen(pattern);
        if (pattern_len >= sizeof(g_dir_pattern)) {
            pattern_len = sizeof(g_dir_pattern) - 1;
        }
        {
            size_t copy_len;
            copy_len = (pattern_len < sizeof(g_dir_pattern) - 1)
                       ? pattern_len : (sizeof(g_dir_pattern) - 1);
            strncpy(g_dir_pattern, pattern, copy_len);
            g_dir_pattern[copy_len] = '\0';
        }
        g_dir_attr_mask = attr;

        /* Trouver le premier fichier correspondant */
        /*
         * Parcourir les entrées et appeler os_dir_next pour trouver
         * la première correspondance effective.
         */
        return os_dir_next(info);
    }
#endif
}

int os_dir_next(os_file_info *info)
{
    if (info == NULL) {
        g_last_error = OS_ERR_NO_MORE_FILES;
        return OS_ERR_NO_MORE_FILES;
    }

#if defined(OS_PLATFORM_WINDOWS)
    {
        WIN32_FIND_DATA find_data;
        HANDLE hFind;
        int found;

        hFind = FindFirstFile(g_dir_pattern, &find_data);
        if (hFind == INVALID_HANDLE_VALUE) {
            /* Pas de nouveau fichier */
            g_last_error = OS_ERR_NO_MORE_FILES;
            return OS_ERR_NO_MORE_FILES;
        }

        /* Il faudrait en réalité maintenir un index, mais l'approche
           simplifiée retourne toujours le même fichier. */
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
        struct dirent *de;
        struct stat st;
        char full_path[512];

        if (g_dir_stream == NULL) {
            g_last_error = OS_ERR_NO_MORE_FILES;
            return OS_ERR_NO_MORE_FILES;
        }

        /*
         * Parcourir les entrées jusqu'à trouver un fichier
         * correspondant au pattern (filtrage simplifié).
         */
        while ((de = readdir(g_dir_stream)) != NULL) {
            /* Ignorer . et .. */
            if (de->d_name[0] == '.' &&
                (de->d_name[1] == '\0' ||
                 (de->d_name[1] == '.' && de->d_name[2] == '\0'))) {
                continue;
            }

            /* Construire le chemin complet pour stat() */
            {
                const char *last_slash;
                size_t dir_len;
                last_slash = strrchr(g_dir_pattern, '/');
                if (last_slash != NULL) {
                    dir_len = (size_t)(last_slash - g_dir_pattern);
                    if (dir_len >= sizeof(full_path) - 1) {
                        dir_len = sizeof(full_path) - 1;
                    }
                    strncpy(full_path, g_dir_pattern, dir_len);
                    full_path[dir_len] = '\0';
                    strncat(full_path, "/", sizeof(full_path) - strlen(full_path) - 1);
                } else {
                    full_path[0] = '\0';
                }
            }
            strncat(full_path, de->d_name, sizeof(full_path) - strlen(full_path) - 1);

            if (stat(full_path, &st) != 0) {
                continue;
            }

            /* Remplir les infos */
            strncpy(info->name, de->d_name, 13);
            info->name[13] = '\0';

            /* Attributs (conversion POSIX → GEMDOS) */
            info->attr = 0;
            if (S_ISDIR(st.st_mode)) info->attr |= 0x10;  /* répertoire */
            if (!(st.st_mode & S_IWUSR)) info->attr |= 0x01; /* lecture seule */

            info->size = (os_int32)st.st_size;

            /* Date/heure GEMDOS packée */
            {
                struct tm *lt;
                lt = localtime(&st.st_mtime);
                if (lt != NULL) {
                    info->date = (os_word)(((lt->tm_year - 80) << 9) |
                                           ((lt->tm_mon + 1) << 5) |
                                           lt->tm_mday);
                    info->time = (os_word)((lt->tm_hour << 11) |
                                           (lt->tm_min << 5) |
                                           (lt->tm_sec / 2));
                } else {
                    info->date = 0;
                    info->time = 0;
                }
            }

            g_last_error = OS_ERR_NONE;
            return 0;
        }

        /* Plus de fichiers */
        closedir(g_dir_stream);
        g_dir_stream = NULL;

        g_last_error = OS_ERR_NO_MORE_FILES;
        return OS_ERR_NO_MORE_FILES;
    }
#endif
}

/* ------------------------------------------------------------------ */
/* Console                                                            */
/* ------------------------------------------------------------------ */

int os_con_input_char(void)
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

int os_con_input_key(void)
{
#if defined(OS_PLATFORM_WINDOWS)
    if (_kbhit()) {
        return _getch();
    }
    return -1;
#else
    /*
     * POSIX : l'entrée non bloquante nécessite de configurer le
     * terminal en mode raw. Ceci sera géré par le module console
     * (console.c) qui utilisera termios/ioctl.
     *
     * Pour le moment, retourne -1 (pas de touche).
     */
    (void)0;
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
     * Séquence d'échappement VT-52 pour positionnement curseur.
     * Format : ESC Y l c  (ligne, colonne en ASCII + 32)
     *
     * Également supportée : séquence ANSI CSI pour les terminaux
     * modernes.
     */
    if (col < 1) col = 1;
    if (line < 1) line = 1;

    /*
     * On émet les deux séquences : d'abord ANSI (moderne),
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
     * POSITION DU CURSEUR : Nécessite l'interrogation du terminal
     * via DSR (Device Status Report). Retourne 1 par défaut.
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
     * CLS en GFA Basic émet ESC E puis CR (VT-52).
     * On émet aussi la séquence ANSI pour compatibilité.
     */
    /* ANSI : ESC [ 2 J */
    fprintf(stdout, "\033[2J");
    /* VT-52 : ESC E */
    fprintf(stdout, "\033E");
    /* Retour en haut à gauche */
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
    /* L'implémentation réelle nécessite termios (POSIX) ou
       SetConsoleMode (Windows). Pour le moment, simple flag. */
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
    /* Conversion ms → ticks 200 Hz avec arrondi */
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
        /* La modification de la date système nécessite des
           privilèges root sur POSIX. On stocke simplement la
           valeur ; le module GEMDOS/TOS pourra l'utiliser. */
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
     * Format GEMDOS packé :
     *   Bits 0-4   : secondes / 2
     *   Bits 5-10  : minutes (0-59)
     *   Bits 11-15 : heures (0-23)
     *   Bits 16-20 : jour (1-31)
     *   Bits 21-24 : mois (1-12)
     *   Bits 25-31 : année - 1980
     */
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

void os_time_set_raw_gemdos(os_int32 packed_time)
{
    /*
     * La modification de l'heure système nécessite des privilèges.
     * On décode simplement la valeur pour le suivi logiciel.
     */
    (void)packed_time;
}

/* ------------------------------------------------------------------ */
/* Mémoire                                                            */
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

os_int32 os_mem_available(os_int32 *total)
{
    /*
     * FRE(0) retourne la mémoire libre.
     * Sur un système moderne, c'est une estimation.
     * Les couches supérieures pourront affiner via GEMDOS.
     */
    if (total != NULL) {
        *total = (os_int32)(256L * 1024L * 1024L);  /* 256 Mo */
    }

    g_last_error = OS_ERR_NONE;
    return (os_int32)(128L * 1024L * 1024L);  /* 128 Mo libres */
}

os_int32 os_mem_largest_block(void)
{
    /*
     * FRE(1) : taille du plus grand bloc contigu.
     * Estimation simplifiée.
     */
    g_last_error = OS_ERR_NONE;
    return (os_int32)(64L * 1024L * 1024L);
}

/* ------------------------------------------------------------------ */
/* Son                                                                */
/* ------------------------------------------------------------------ */

void os_sound_beep(void)
{
    /* BEEP : émet un bip système */
#if defined(OS_PLATFORM_WINDOWS)
    MessageBeep(MB_OK);
#else
    fprintf(stdout, "\a");
    fflush(stdout);
#endif
}

int os_sound_init(void)
{
    /* Le sous-système audio sera initialisé dans le module sound.c */
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
     * L'implémentation réelle du YM-2149 sera faite dans le module
     * sound.c. Pour le moment, émet un simple bip console.
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
/* Divers système                                                     */
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
     * Sur système hôte : retourne 1 pour simuler A:.
     */
#if defined(OS_PLATFORM_WINDOWS)
    {
        char cwd[256];
        if (_getcwd(cwd, sizeof(cwd)) != NULL && cwd[0] != '\0' && cwd[1] == ':') {
            return (toupper(cwd[0]) - 'A' + 1);
        }
    }
#endif
    return 1;  /* Simuler lecteur A: */
}

int os_sys_set_drive(int drive)
{
#if defined(OS_PLATFORM_WINDOWS)
    char path[4];
    if (drive < 1 || drive > 26) {
        g_last_error = OS_ERR_INVALID_DRIVE;
        return -1;
    }
    path[0] = (char)('A' + drive - 1);
    path[1] = ':';
    path[2] = '\\';
    path[3] = '\0';
    if (!SetCurrentDirectory(path)) {
        g_last_error = OS_ERR_INVALID_DRIVE;
        return -1;
    }
#else
    (void)drive;
#endif
    g_last_error = OS_ERR_NONE;
    return 0;
}

os_int32 os_sys_get_basepage(void)
{
    /*
     * BASEPAGE : adresse de la page de base du programme.
     * Sur l'Atari ST, la basepage est une structure de 256 octets
     * placée au début de l'espace mémoire du processus.
     *
     * Retourne une valeur factice pour l'émulation (les adresses
     * réelles sur le système hôte n'ont pas de sens).
     */
    g_last_error = OS_ERR_NONE;
    return (os_int32)0x10000L;  /* Adresse factice */
}

void os_sys_quit(int exit_code)
{
    /* Fermer tout proprement */
    os_shutdown();
    exit(exit_code);
}
