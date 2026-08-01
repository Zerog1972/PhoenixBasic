/*
 * os_layer.h - Couche d'abstraction du systeme d'exploitation
 * ============================================================
 * Implementation compatible C89 pour l'emulateur GFA Basic 3.5
 * (Atari ST).
 *
 * Ce module fournit toutes les interfaces d'abstraction OS necessaires
 * aux couches superieures : fichiers, console, temps, memoire, affichage,
 * son et informations systeme.
 *
 * Conventions C89 strictes :
 *   - Commentaires slash-etoile ... etoile-slash (standard C89)
 *   - Variables declarees en debut de bloc
 *   - Pas de types bool, inline, ni declarations for-loop
 *   - Prototypes complets pour toutes les fonctions
 *
 * Reference : cahier-des-charges-gfabasic.md, sections 2.2, 7, 9, 15.2
 */

#ifndef OS_LAYER_H
#define OS_LAYER_H

#include <stddef.h>  /* size_t */

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Types de base                                                       */
/* ------------------------------------------------------------------ */

/* Entiers de taille garantie (émulés pour C89) */
typedef unsigned char  os_byte;     /*  8 bits non signé */
typedef unsigned short os_word;     /* 16 bits non signé */
typedef unsigned long  os_dword;    /* 32 bits non signé */
typedef signed short   os_int16;    /* 16 bits signé */
typedef signed long    os_int32;    /* 32 bits signé */

/* Booléen C89 (0 = faux, non nul = vrai) */
typedef int os_bool;
#define OS_FALSE 0
#define OS_TRUE  1

/* Descripteur de fichier opaque */
typedef void* os_file_handle;

/* Attributs de fichier (pour FSFIRST/FSNEXT) */
typedef struct {
    char   name[14];       /* nom 8.3 + '\0' */
    os_byte attr;          /* attributs : bit 0=lecture seule, 1=caché,
                               2=système, 3=volume, 4=répertoire,
                               5=archive */
    os_int32 size;         /* taille en octets */
    os_word  date;         /* date (format GEMDOS packé) */
    os_word  time;         /* heure (format GEMDOS packé) */
} os_file_info;

/* Codes d'erreur du module OS */
typedef enum {
    OS_ERR_NONE = 0,
    OS_ERR_FILE_NOT_FOUND = -33,
    OS_ERR_PATH_NOT_FOUND = -34,
    OS_ERR_TOO_MANY_OPEN = -35,
    OS_ERR_ACCESS_DENIED = -36,
    OS_ERR_INVALID_HANDLE = -37,
    OS_ERR_OUT_OF_MEMORY = -39,
    OS_ERR_INVALID_DRIVE = -46,
    OS_ERR_NO_MORE_FILES = -49,
    OS_ERR_DISK_FULL    = -68,   /* code distinct (-37 deja utilise) */
    OS_ERR_WRITE_FAULT  = -10,
    OS_ERR_READ_FAULT   = -11,
    OS_ERR_INTERNAL     = -65,
    OS_ERR_NOT_IMPL     = -32    /* fonction non implémentée */
} os_error_code;

/* ------------------------------------------------------------------ */
/* Modes d'ouverture de fichier (compatibles GFA Basic)                */
/* ------------------------------------------------------------------ */
/* Modes binaires : l'Atari ST n'a pas de conversion CR/LF,
   les fichiers sont toujours accedes en binaire. */
#define OS_FMODE_INPUT   "rb"  /* "I" : lecture (fichier doit exister) */
#define OS_FMODE_OUTPUT  "wb"  /* "O" : écriture (écrase si existe) */
#define OS_FMODE_APPEND  "ab"  /* "A" : ajout en fin de fichier */
#define OS_FMODE_RANDOM  "r+b" /* "R" : lecture/écriture aléatoire */
#define OS_FMODE_UPDATE  "w+b" /* "U" : mise à jour (crée si inexistant) */

/* Modes d'ouverture GFA Basic (caractères) */
#define OS_GFAMODE_INPUT   'I'
#define OS_GFAMODE_OUTPUT  'O'
#define OS_GFAMODE_APPEND  'A'
#define OS_GFAMODE_RANDOM  'R'
#define OS_GFAMODE_UPDATE  'U'

/* ------------------------------------------------------------------ */
/* Modes de résolution d'écran (Atari ST)                             */
/* ------------------------------------------------------------------ */
#define OS_ST_MODE_LOW      0    /* 320×200, 16 couleurs */
#define OS_ST_MODE_MEDIUM   1    /* 640×200, 4 couleurs  */
#define OS_ST_MODE_HIGH     2    /* 640×400, monochrome  */

/* Dimensions maximales */
#define OS_SCREEN_MAX_WIDTH   640
#define OS_SCREEN_MAX_HEIGHT  400
#define OS_SCREEN_WIDTH_LO    320
#define OS_SCREEN_HEIGHT_LO   200

/* Palette Atari ST standard (16 couleurs RGB) */
extern const unsigned long os_st_palette[16];

/* ------------------------------------------------------------------ */
/* Interface d'affichage générique (DisplayDriver)                    */
/* ------------------------------------------------------------------ */
typedef struct os_display_driver {
    /*
     * Initialise l'affichage dans le mode donné.
     * mode : OS_ST_MODE_LOW, _MEDIUM ou _HIGH
     * retourne 0 si succès, code d'erreur sinon.
     */
    int  (*init)(int mode);

    /*
     * Arrête le sous-système d'affichage et libère les ressources.
     */
    void (*shutdown)(void);

    /*
     * Efface tout l'écran avec la couleur spécifiée (index palette).
     */
    void (*clear)(int color);

    /*
     * Allume un pixel aux coordonnées (x, y) avec la couleur donnée.
     */
    void (*set_pixel)(int x, int y, int color);

    /*
     * Retourne la couleur (index palette) du pixel en (x, y).
     * Retourne -1 si hors écran.
     */
    int  (*get_pixel)(int x, int y);

    /*
     * Trace une ligne de (x1, y1) à (x2, y2) avec la couleur courante.
     */
    void (*draw_line)(int x1, int y1, int x2, int y2, int color);

    /*
     * Affiche du texte aux coordonnées graphiques (x, y).
     */
    void (*draw_text)(int x, int y, const char *text, int color);

    /*
     * Remplit un rectangle de couleur donnée.
     */
    void (*fill_rect)(int x, int y, int w, int h, int color);

    /*
     * Rafraîchit l'affichage (swap buffers ou flush).
     */
    void (*update)(void);

    /*
     * Modifie une entrée de la palette de couleurs.
     * index : 0..15, rgb : couleur 24 bits (0xRRGGBB).
     */
    void (*set_palette)(int index, unsigned long rgb);

    /*
     * Obtient l'entrée de la palette.
     */
    unsigned long (*get_palette)(int index);

    /*
     * Attend et retourne le prochain événement utilisateur.
     * Les événements sont encodés dans un entier 32 bits :
     *   bits 31-24 : type (0=none, 1=key, 2=mouse_move, 3=mouse_btn,
     *                         4=window_redraw, 5=window_close)
     *   bits 23-16 : modificateurs clavier (shift, ctrl, alt)
     *   bits 15-8  : code touche / bouton souris
     *   bits 7-0   : réservé
     * Retourne 0 si pas d'événement (mode non-bloquant).
     */
    os_int32 (*poll_event)(void);

    /*
     * Attend un événement (bloquant).
     */
    os_int32 (*wait_event)(void);

    /*
     * Obtient la résolution courante de l'écran.
     */
    int (*get_resolution)(void);

} os_display_driver;

/* ------------------------------------------------------------------ */
/* API publique — Système (OS Layer)                                  */
/* ------------------------------------------------------------------ */

/*
 * os_init — Initialise la couche d'abstraction OS.
 * Retourne 0 si succès, code d'erreur sinon.
 */
int os_init(void);

/*
 * os_shutdown — Libère toutes les ressources de la couche OS.
 */
void os_shutdown(void);

/*
 * os_get_error — Retourne le dernier code d'erreur.
 */
os_error_code os_get_error(void);

/*
 * os_get_error_string — Retourne un message décrivant le code d'erreur.
 */
const char* os_get_error_string(os_error_code code);

/*
 * os_get_last_error_string — Retourne le message de la dernière erreur.
 */
const char* os_get_last_error_string(void);

/* ------------------------------------------------------------------ */
/* API publique — Affichage                                           */
/* ------------------------------------------------------------------ */

/*
 * os_display_register — Enregistre un driver d'affichage.
 * À appeler avant toute opération graphique.
 */
int os_display_register(const os_display_driver *driver);

/*
 * os_display_get — Retourne un pointeur vers le driver d'affichage
 * courant (ou NULL si aucun n'est enregistré).
 */
const os_display_driver* os_display_get(void);

/*
 * os_display_set_mode — Initialise l'affichage dans le mode donné.
 * Utilise le driver enregistré. Wrapper pratique.
 */
int os_display_set_mode(int mode);

/*
 * os_display_get_resolution — Retourne la résolution courante.
 * 0 = LOW, 1 = MEDIUM, 2 = HIGH, -1 si pas d'affichage.
 */
int os_display_get_resolution(void);

/* ------------------------------------------------------------------ */
/* API publique — Gestion de fichiers                                  */
/* ------------------------------------------------------------------ */

/*
 * os_file_open — Ouvre un fichier avec le mode GFA Basic ('I','O','A',
 * 'R','U') et le numéro de canal. Le numéro de canal (0-99) est
 * stocké pour référence ultérieure.
 *
 * Retourne un handle opaque, ou NULL si erreur.
 */
os_file_handle os_file_open(const char *name, char gfa_mode, int channel);

/*
 * os_file_close — Ferme un fichier ouvert.
 * Retourne 0 si succès, code d'erreur sinon.
 */
int os_file_close(os_file_handle handle);
int os_file_close_by_channel(int channel);

/*
 * os_file_read — Lit jusqu'à size octets depuis le fichier.
 * Retourne le nombre d'octets lus, ou -1 si erreur.
 */
os_int32 os_file_read(os_file_handle handle, void *buffer, os_int32 size);

/*
 * os_file_write — Écrit size octets dans le fichier.
 * Retourne le nombre d'octets écrits, ou -1 si erreur.
 */
os_int32 os_file_write(os_file_handle handle, const void *buffer, os_int32 size);

/*
 * os_file_seek — Positionne le pointeur de fichier.
 * Retourne la nouvelle position absolue, ou -1 si erreur.
 */
os_int32 os_file_seek(os_file_handle handle, os_int32 offset, int whence);
#define OS_SEEK_SET  0
#define OS_SEEK_CUR  1
#define OS_SEEK_END  2

/*
 * os_file_tell — Retourne la position courante (LOC).
 * Retourne -1 si erreur.
 */
os_int32 os_file_tell(os_file_handle handle);

/*
 * os_file_size — Retourne la taille du fichier en octets (LOF).
 * Retourne -1 si erreur.
 */
os_int32 os_file_size(os_file_handle handle);

/*
 * os_file_eof — Teste la fin de fichier (EOF).
 * Retourne OS_TRUE si EOF, OS_FALSE sinon.
 */
int os_file_eof(os_file_handle handle);

/*
 * os_file_get_handle_by_channel — Retourne le handle associé au canal
 * donné, ou NULL si le canal n'est pas ouvert.
 */
os_file_handle os_file_get_handle_by_channel(int channel);

/*
 * os_file_get_channel — Retourne le numéro de canal associé au handle.
 * Retourne -1 si invalide.
 */
int os_file_get_channel(os_file_handle handle);

/*
 * os_file_flush — Vide les buffers d'écriture du fichier.
 */
int os_file_flush(os_file_handle handle);

/* ------------------------------------------------------------------ */
/* API publique — Opérations sur le système de fichiers               */
/* ------------------------------------------------------------------ */

/*
 * os_fs_exist — Teste l'existence d'un fichier ou répertoire (EXIST).
 * Retourne OS_TRUE si existe, OS_FALSE sinon.
 */
int os_fs_exist(const char *name);

/*
 * os_fs_delete — Supprime un fichier (KILL).
 * Retourne 0 si succès.
 */
int os_fs_delete(const char *name);

/*
 * os_fs_rename — Renomme un fichier ou répertoire (NAME).
 * Retourne 0 si succès.
 */
int os_fs_rename(const char *old_name, const char *new_name);

/*
 * os_fs_free — Retourne l'espace libre sur le disque en octets (DFREE).
 * drive : 0=courant, 1=A:, 2=B:, ...
 * Retourne -1 si erreur.
 */
os_int32 os_fs_free(int drive);

/*
 * os_fs_total — Retourne la capacité totale du disque en octets.
 * drive : 0=courant, 1=A:, ...
 * Retourne -1 si erreur.
 */
os_int32 os_fs_total(int drive);

/* ------------------------------------------------------------------ */
/* API publique — Répertoires                                         */
/* ------------------------------------------------------------------ */

/*
 * os_dir_mkdir — Crée un répertoire (MKDIR).
 * Retourne 0 si succès.
 */
int os_dir_mkdir(const char *name);

/*
 * os_dir_rmdir — Supprime un répertoire vide (RMDIR).
 * Retourne 0 si succès.
 */
int os_dir_rmdir(const char *name);

/*
 * os_dir_chdir — Change le répertoire courant (CHDIR).
 * Retourne 0 si succès.
 */
int os_dir_chdir(const char *name);

/*
 * os_dir_getcwd — Retourne le répertoire courant.
 * Affecte *len avec la longueur de la chaîne retournée.
 * La chaîne est allouée dynamiquement (à libérer avec os_mem_free).
 * Retourne NULL si erreur.
 */
char* os_dir_getcwd(int *len);

/*
 * os_dir_first — Trouve le premier fichier correspondant au masque
 * (FSFIRST). Remplit info avec les attributs.
 * attr : masque d'attributs (0 = fichiers normaux uniquement).
 * Retourne 0 si trouvé, OS_ERR_NO_MORE_FILES si aucun.
 */
int os_dir_first(const char *pattern, int attr, os_file_info *info);

/*
 * os_dir_next — Trouve le fichier suivant (FSNEXT).
 * info doit être celui rempli par os_dir_first.
 * Retourne 0 si trouvé, OS_ERR_NO_MORE_FILES si plus aucun.
 */
int os_dir_next(os_file_info *info);

/* ------------------------------------------------------------------ */
/* API publique — Console (entrées/sorties texte)                     */
/* ------------------------------------------------------------------ */

/*
 * os_con_input_char — Lit un caractère du clavier (bloquant).
 * Retourne le code ASCII, ou -1 si erreur.
 */
int os_con_input_char(void);

/*
 * os_con_input_key — Lit un caractère sans attente (INKEY$).
 * Retourne le code ASCII, ou -1 si aucune touche pressée.
 */
int os_con_input_key(void);

/*
 * os_con_output_char — Émet un caractère vers la console (CCONOUT).
 */
void os_con_output_char(int c);

/*
 * os_con_output_string — Émet une chaîne vers la console.
 */
void os_con_output_string(const char *s);

/*
 * os_con_cursor_goto — Positionne le curseur en (col, ligne).
 * Coordonnées 1-indexées (compatibles LOCATE).
 */
void os_con_cursor_goto(int col, int line);

/*
 * os_con_cursor_get_x — Retourne la colonne courante du curseur (1-indexé).
 * Équivalent POS(0) / CRSCOL.
 */
int os_con_cursor_get_x(void);

/*
 * os_con_cursor_get_y — Retourne la ligne courante du curseur (1-indexé).
 * Équivalent CRSLIN.
 */
int os_con_cursor_get_y(void);

/*
 * os_con_clear — Efface l'écran (CLS). Émet la séquence VT-52 ESC E.
 */
void os_con_clear(void);

/*
 * os_con_clear_to_eol — Efface du curseur jusqu'à la fin de la ligne.
 */
void os_con_clear_to_eol(void);

/*
 * os_con_set_echo — Active/désactive l'écho clavier.
 */
void os_con_set_echo(int on);

/*
 * os_con_get_echo — Retourne OS_TRUE si l'écho est activé.
 */
int os_con_get_echo(void);

/* ------------------------------------------------------------------ */
/* API publique — Temps et minuteurs                                  */
/* ------------------------------------------------------------------ */

/*
 * os_time_ticks — Retourne le nombre de ticks (1/200 s) depuis le
 * démarrage du système. Équivalent TIMER de GFA / LPEEK(&H4BA).
 */
os_int32 os_time_ticks(void);

/*
 * os_time_millis — Retourne le temps écoulé en millisecondes depuis
 * le démarrage du système.
 */
os_int32 os_time_millis(void);

/*
 * os_time_delay — Suspend l'exécution pendant delay_ms millisecondes.
 * Équivalent DELAY.
 */
void os_time_delay(os_int32 delay_ms);

/*
 * os_time_get_date — Retourne la date système sous forme de chaîne
 * au format "JJ.MM.AAAA" (ou "MM/JJ/AAAA" si mode US).
 * Équivalent DATE$.
 * La chaîne retournée est dans un buffer statique interne.
 */
const char* os_time_get_date(int us_format);

/*
 * os_time_get_time — Retourne l'heure système au format "HH:MM:SS".
 * Équivalent TIME$.
 */
const char* os_time_get_time(void);

/*
 * os_time_set_date — Règle la date système.
 * format : chaîne au format "JJ.MM.AAAA" ou "MM/JJ/AAAA".
 * Retourne 0 si succès.
 */
int os_time_set_date(const char *date_str);

/*
 * os_time_set_time — Règle l'heure système.
 * format : chaîne au format "HH:MM:SS".
 * Retourne 0 si succès.
 */
int os_time_set_time(const char *time_str);

/*
 * os_time_set_datetime — Règle date et heure (SETTIME).
 * Retourne 0 si succès.
 */
int os_time_set_datetime(const char *time_str, const char *date_str);

/*
 * os_time_get_raw_gemdos — Retourne l'heure au format GEMDOS packé.
 * (utilisé pour XBIOS(23)/Tgettime)
 */
os_int32 os_time_get_raw_gemdos(void);

/*
 * os_time_set_raw_gemdos — Règle l'heure au format GEMDOS packé.
 * (utilisé pour XBIOS(22)/Tsettime)
 */
void os_time_set_raw_gemdos(os_int32 packed_time);

/* ------------------------------------------------------------------ */
/* API publique — Mémoire                                             */
/* ------------------------------------------------------------------ */

/*
 * os_mem_alloc — Alloue un bloc de mémoire de size octets.
 * Retourne un pointeur vers le bloc, ou NULL si échec.
 * Équivalent MALLOC de GFA.
 */
void* os_mem_alloc(size_t size);

/*
 * os_mem_free — Libère un bloc alloué avec os_mem_alloc.
 * Équivalent MFREE.
 */
void os_mem_free(void *ptr);

/*
 * os_mem_realloc — Redimensionne un bloc mémoire.
 * Retourne le nouveau pointeur, ou NULL si échec.
 */
void* os_mem_realloc(void *ptr, size_t new_size);

/*
 * os_mem_copy — Copie n octets de src vers dst.
 * Équivalent BMOVE.
 */
void os_mem_copy(void *dst, const void *src, size_t n);

/*
 * os_mem_set — Remplit n octets à l'adresse dst avec la valeur c.
 */
void os_mem_set(void *dst, int c, size_t n);

/*
 * os_strdup — Alloue une copie de la chaîne s (équivalent C89 de strdup).
 * À libérer avec os_mem_free(). Retourne NULL si s==NULL ou si
 * l'allocation échoue.
 */
char* os_strdup(const char *s);

/*
 * os_mem_available — Retourne la mémoire disponible en octets.
 * Équivalent FRE(0).
 * total : si non nul, retourne la mémoire totale.
 */
os_int32 os_mem_available(os_int32 *total);

/*
 * os_mem_largest_block — Retourne la taille du plus grand bloc
 * contigu disponible. Équivalent FRE(1).
 */
os_int32 os_mem_largest_block(void);

/* ------------------------------------------------------------------ */
/* API publique — Son                                                 */
/* ------------------------------------------------------------------ */

/*
 * os_sound_beep — Émet un bip système. Équivalent BEEP.
 */
void os_sound_beep(void);

/*
 * os_sound_init — Initialise le sous-système audio.
 * Retourne 0 si succès.
 */
int os_sound_init(void);

/*
 * os_sound_shutdown — Arrête et libère le sous-système audio.
 */
void os_sound_shutdown(void);

/*
 * os_sound_tone — Joue une tonalité sur un canal.
 * channel : 0-2 (canal A/B/C du YM-2149)
 * freq_hz : fréquence en Hertz
 * duration_ms : durée en millisecondes
 * volume : 0-15
 */
void os_sound_tone(int channel, int freq_hz, int duration_ms, int volume);

/*
 * os_sound_stop_channel — Arrête le son sur un canal.
 */
void os_sound_stop_channel(int channel);

/*
 * os_sound_stop_all — Arrête tous les canaux.
 */
void os_sound_stop_all(void);

/* ------------------------------------------------------------------ */
/* API publique — Divers système                                      */
/* ------------------------------------------------------------------ */

/*
 * os_sys_get_env — Lit une variable d'environnement.
 * Équivalent SHEL_ENVRN.
 * Retourne la valeur ou "" si non définie.
 * La chaîne est dans un buffer statique interne.
 */
const char* os_sys_get_env(const char *name);

/*
 * os_sys_get_drive — Retourne le numéro du lecteur courant (1=A:, ...).
 */
int os_sys_get_drive(void);

/*
 * os_sys_set_drive — Change le lecteur courant (CHDRIVE).
 * Retourne 0 si succès.
 */
int os_sys_set_drive(int drive);

/*
 * os_sys_get_basepage — Retourne une adresse émulant la page de base
 * du programme. Équivalent BASEPAGE.
 */
os_int32 os_sys_get_basepage(void);

/*
 * os_sys_quit — Termine le programme avec le code de retour donné.
 * Équivalent QUIT / Pterm().
 */
void os_sys_quit(int exit_code);

#ifdef __cplusplus
}
#endif

#endif /* OS_LAYER_H */
