/*
 * files.h - Gestion des fichiers GFA Basic 3.5
 * =============================================
 * Implemente OPEN, CLOSE, INPUT#, PRINT#, GET#, PUT#, BLOAD, BSAVE,
 * SEEK, RELSEEK, EOF, LOF, LOC, FIELD, LSET, RSET, etc.
 *
 * Reference : cahier-des-charges-gfabasic.md, section 8.7
 */

#ifndef GFA_FILES_H
#define GFA_FILES_H

#include "os_layer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Nombre maximum de canaux */
#define GFA_MAX_CHANNELS  100

/* Nombre maximum de fichiers Random (mode "R") */
#define GFA_MAX_RANDOM_FILES 31

/* ------------------------------------------------------------------ */
/* Types et structures                                                */
/* ------------------------------------------------------------------ */

/*
 * gfa_file_mode - Modes d'ouverture GFA.
 */
typedef enum {
    GFA_FILE_CLOSED = 0,
    GFA_FILE_INPUT,     /* "I" : lecture sequentielle  */
    GFA_FILE_OUTPUT,    /* "O" : ecriture sequentielle  */
    GFA_FILE_APPEND,    /* "A" : ajout en fin           */
    GFA_FILE_RANDOM,    /* "R" : acces aleatoire (FIELD)*/
    GFA_FILE_UPDATE,    /* "U" : mise a jour            */
    GFA_FILE_BINARY_R,  /* BLOAD / BGET                 */
    GFA_FILE_BINARY_W   /* BSAVE / BPUT                 */
} gfa_file_mode;

/*
 * gfa_file - Descripteur de fichier GFA.
 */
typedef struct {
    int           channel;         /* Numero de canal (1-99, 0=console) */
    gfa_file_mode mode;            /* Mode d'ouverture                  */
    os_file_handle handle;         /* Handle OS sous-jacent             */
    char          filename[256];   /* Nom du fichier                    */
    int           record_length;   /* Longueur d'enregistrement (mode R)*/
    long          current_record;  /* Enregistrement courant (mode R)   */

    /* Buffer FIELD pour le mode Random */
    char         *field_buffer;    /* Buffer FIELD                      */
    int           field_size;      /* Taille du buffer FIELD            */
    int           field_count;     /* Nombre de champs FIELD definis    */

} gfa_file;

/* ------------------------------------------------------------------ */
/* Etat global des fichiers                                           */
/* ------------------------------------------------------------------ */

/*
 * gfa_files_init - Initialise le systeme de fichiers GFA.
 * A appeler au demarrage du runtime.
 */
void gfa_files_init(void);

/*
 * gfa_files_shutdown - Ferme tous les fichiers et libere la memoire.
 */
void gfa_files_shutdown(void);

/*
 * gfa_files_get - Retourne le descripteur du canal donne.
 * Retourne NULL si le canal est ferme ou inexistant.
 */
gfa_file *gfa_files_get(int channel);

/*
 * gfa_files_get_count - Retourne le nombre de fichiers ouverts.
 */
int gfa_files_get_count(void);

/* ------------------------------------------------------------------ */
/* Operations sur les fichiers                                        */
/* ------------------------------------------------------------------ */

/*
 * gfa_open - Ouvre un fichier.
 * mode_str : "I", "O", "A", "R", "U"
 * channel : 1-99
 * filename : chemin du fichier
 * record_len : longueur d'enregistrement (mode R uniquement)
 *
 * Retourne 0 si succes, code d'erreur GFA sinon.
 * Equivalent GFA : OPEN mode$, #n, "fichier"[, len]
 */
int gfa_open(const char *mode_str, int channel, const char *filename,
             int record_len);

/*
 * gfa_close - Ferme un fichier.
 * channel : numero de canal, ou -1 pour fermer TOUS les fichiers.
 * Equivalent GFA : CLOSE [#n]
 */
void gfa_close(int channel);

/*
 * gfa_input_channel - Lit des donnees depuis un fichier.
 * Variables separees par des virgules.
 * Equivalent GFA : INPUT #n, var...
 *
 * Retourne le nombre de variables lues, ou -1 si erreur/EOF.
 */
int gfa_input_channel(int channel, char *buffer, int bufsize);

/*
 * gfa_line_input_channel - Lit une ligne complete depuis un fichier.
 * Equivalent GFA : LINE INPUT #n, var$
 * Retourne la ligne lue (a liberer avec os_mem_free), ou NULL si EOF.
 */
char *gfa_line_input_channel(int channel);

/*
 * gfa_print_channel - Ecrit des donnees dans un fichier.
 * Equivalent GFA : PRINT #n, expr...
 * Retourne 0 si succes.
 */
int gfa_print_channel(int channel, const char *data);

/*
 * gfa_write_channel - Ecrit des donnees avec separateurs (WRITE #).
 * Equivalent GFA : WRITE #n, expr...
 * Retourne 0 si succes.
 */
int gfa_write_channel(int channel, const char *data);

/*
 * gfa_get_channel - Lecture binaire positionnee.
 * Equivalent GFA : GET #n[, pos]
 */
int gfa_get_channel(int channel, long record_num);

/*
 * gfa_put_channel - Ecriture binaire positionnee.
 * Equivalent GFA : PUT #n[, pos]
 */
int gfa_put_channel(int channel, long record_num);

/*
 * gfa_seek - Positionnement absolu dans un fichier.
 * Equivalent GFA : SEEK #n, pos
 */
long gfa_seek(int channel, long position);

/*
 * gfa_relseek - Positionnement relatif.
 * Equivalent GFA : RELSEEK #n, offset
 */
long gfa_relseek(int channel, long offset);

/*
 * gfa_eof - Teste la fin de fichier.
 * Equivalent GFA : EOF(#n)
 * Retourne -1 (TRUE) si EOF, 0 (FALSE) sinon.
 */
int gfa_eof(int channel);

/*
 * gfa_lof - Retourne la taille du fichier.
 * Equivalent GFA : LOF(#n)
 */
long gfa_lof(int channel);

/*
 * gfa_loc - Retourne la position courante dans le fichier.
 * Equivalent GFA : LOC(#n)
 */
long gfa_loc(int channel);

/*
 * gfa_exist - Teste l'existence d'un fichier.
 * Equivalent GFA : EXIST("fichier")
 * Retourne -1 (TRUE) si existe, 0 (FALSE) sinon.
 */
int gfa_exist(const char *filename);

/*
 * gfa_kill - Supprime un fichier.
 * Equivalent GFA : KILL "fichier"
 */
void gfa_kill(const char *filename);

/*
 * gfa_name_file - Renomme un fichier.
 * Equivalent GFA : NAME "ancien" AS "nouveau"
 */
void gfa_name_file(const char *oldname, const char *newname);

/* ------------------------------------------------------------------ */
/* Operations FIELD (mode Random)                                     */
/* ------------------------------------------------------------------ */

/*
 * gfa_field - Definit les champs d'un buffer d'acces aleatoire.
 * Equivalent GFA : FIELD #n, len AS var$...
 * Retourne 0 si succes, code d'erreur sinon.
 */
int gfa_field(int channel, int field_size, const char *field_name);

/*
 * gfa_field_buffer - Retourne le buffer FIELD du canal donne.
 * size receit la taille du buffer (si non NULL).
 */
char *gfa_field_buffer(int channel, int *size);

/*
 * gfa_lset - Affecte une valeur a un champ FIELD (aligne a gauche).
 * Equivalent GFA : LSET var$ = expr
 */
void gfa_lset(char *field_var, const char *value, int field_len);

/*
 * gfa_rset - Affecte une valeur a un champ FIELD (aligne a droite).
 * Equivalent GFA : RSET var$ = expr
 */
void gfa_rset(char *field_var, const char *value, int field_len);

/* ------------------------------------------------------------------ */
/* Fichiers binaires (BLOAD/BSAVE/BGET/BPUT/SGET/SPUT)               */
/* ------------------------------------------------------------------ */

/*
 * gfa_bload - Charge un fichier binaire en memoire.
 * Equivalent GFA : BLOAD "fichier"[, adresse]
 * Retourne l'adresse de chargement, ou 0 si erreur.
 */
long gfa_bload(const char *filename, long address);

/*
 * gfa_bsave - Sauvegarde un bloc memoire en fichier binaire.
 * Equivalent GFA : BSAVE "fichier", debut, fin
 * Retourne 0 si succes, -1 si erreur.
 */
int gfa_bsave(const char *filename, long start_addr, long end_addr);

/*
 * gfa_bget - Lecture binaire depuis un peripherique.
 * Equivalent GFA : BGET #n
 */
int gfa_bget(int channel, void *buffer, int size);

/*
 * gfa_bput - Ecriture binaire vers un peripherique.
 * Equivalent GFA : BPUT #n
 */
int gfa_bput(int channel, const void *buffer, int size);

/*
 * gfa_sget - Lecture depuis un peripherique serie.
 * Equivalent GFA : SGET #n
 */
int gfa_sget(int channel);

/*
 * gfa_sput - Ecriture vers un peripherique serie.
 * Equivalent GFA : SPUT #n, data
 */
int gfa_sput(int channel, int data);

#ifdef __cplusplus
}
#endif

#endif /* GFA_FILES_H */
