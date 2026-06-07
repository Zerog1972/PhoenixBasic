/*
 * tos.c - Implementation des appels TOS (GEMDOS, BIOS, XBIOS)
 * ===========================================================
 * Emule les appels systeme de l'Atari ST via la couche OS.
 *
 * Reference : cahier-des-charges-gfabasic.md, sections 8.17, 9.3-9.5
 */

#include "tos.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* GEMDOS                                                             */
/* ------------------------------------------------------------------ */

os_int32 gfa_gemdos(os_int32 fn, os_int32 arg1, os_int32 arg2)
{
    (void)arg2;  /* De nombreuses fonctions n'utilisent pas arg2 */

    switch (fn) {

        /* Pterm - Terminer le processus */
        case GEMDOS_PTERM:
            os_sys_quit((int)arg1);
            return 0;

        /* Cconin - Entree console avec echo */
        case GEMDOS_CCONIN:
            return (os_int32)os_con_input_char();

        /* Cconout - Sortie console */
        case GEMDOS_CCONOUT:
            os_con_output_char((int)(arg1 & 0xFF));
            return 0;

        /* Cnecin - Entree console sans echo */
        case GEMDOS_CNECIN:
            return (os_int32)os_con_input_char();

        /* Cconws - Sortie chaine console */
        case GEMDOS_CCONWS:
            {
                const char *str;
                str = (arg2 == 0)
                      ? (const char *)(size_t)arg1
                      : (const char *)(size_t)((arg2 << 16) | (arg1 & 0xFFFF));
                (void)str;
                /* En pratique, arg1 contient l'adresse */
                if (arg1 != 0) {
                    os_con_output_string((const char *)(size_t)arg1);
                }
            }
            return 0;

        /* Dgetdrv - Lecteur courant */
        case GEMDOS_DGETDRV:
            return (os_int32)os_sys_get_drive();

        /* Fsetdta - Fixer DTA */
        case GEMDOS_FSETDTA:
            return 0;  /* Le DTA est gere par les couches superieures */

        /* Dsetdrv - Changer lecteur */
        case GEMDOS_DSETDRV:
            os_sys_set_drive((int)arg1);
            return 0;

        /* Super - Mode superviseur (retourne l'adresse fictive) */
        case GEMDOS_SUPER:
            return 0;  /* Mode superviseur non applicable en emulation */

        /* Tgetdate - Obtenir date systeme */
        case GEMDOS_TGETDATE:
            return os_time_get_raw_gemdos();

        /* Tgettime - Obtenir heure systeme */
        case GEMDOS_TGETTIME:
            return os_time_get_raw_gemdos();

        /* Fgetdta - Adresse DTA */
        case GEMDOS_FGETDTA:
            return 0;  /* DTA fictif */

        /* Fversion - Version GEMDOS */
        case GEMDOS_FVERSION:
            return 0x0015;  /* Version TOS 1.04 (Rainbow TOS) */

        /* Fopen - Ouvrir fichier */
        case GEMDOS_FOPEN:
            {
                const char *fname;
                int mode;
                os_file_handle fh;
                char gfa_mode;

                fname = (const char *)(size_t)arg1;
                mode  = (int)arg2;

                switch (mode) {
                    case 0: gfa_mode = 'I'; break;  /* Lecture seule */
                    case 1: gfa_mode = 'O'; break;  /* Ecriture     */
                    case 2: gfa_mode = 'U'; break;  /* Lecture/ecriture */
                    default: return -36;  /* Access denied */
                }

                fh = os_file_open(fname, gfa_mode, 0);
                if (fh == NULL) {
                    os_error_code err;
                    err = os_get_error();
                    return (os_int32)err;  /* Retourne code GEMDOS negatif */
                }
                /* Retourner un handle fictif (pointeur comme int) */
                return (os_int32)((size_t)fh & 0xFFFFFFFFL);
            }
            break;

        /* Fclose - Fermer fichier */
        case GEMDOS_FCLOSE:
            {
                os_file_handle fh;
                fh = (os_file_handle)(size_t)arg1;
                os_file_close(fh);
            }
            return 0;

        /* Fread - Lire fichier */
        case GEMDOS_FREAD:
            {
                os_file_handle fh;
                os_int32 count;
                void *buf;
                os_int32 nread;

                fh    = (os_file_handle)(size_t)arg1;
                count = arg2;
                buf   = (void *)(size_t)0;  /* arg2 seul; l'adresse buffer
                                               est passe par registre */
                /* Simplifie : count vient de arg2 mais addr est separe */
                if (count > 0 && buf != NULL) {
                    nread = os_file_read(fh, buf, count);
                    return nread;
                }
            }
            return 0;

        /* Fwrite - Ecrire fichier */
        case GEMDOS_FWRITE:
            {
                os_file_handle fh;
                os_int32 count;
                const void *buf;
                os_int32 written;

                fh    = (os_file_handle)(size_t)arg1;
                count = arg2;
                buf   = (const void *)(size_t)0;
                if (count > 0 && buf != NULL) {
                    written = os_file_write(fh, buf, count);
                    return written;
                }
            }
            return 0;

        /* Fdelete - Supprimer fichier */
        case GEMDOS_FDELETE:
            os_fs_delete((const char *)(size_t)arg1);
            return 0;

        /* Fseek - Positionner dans fichier (retourne nouvelle position) */
        case GEMDOS_FSEEK:
            {
                os_file_handle fh;
                os_int32 offset;
                int whence;
                os_int32 pos;

                fh     = (os_file_handle)(size_t)arg1;
                offset = arg2;
                whence = 0;  /* 0=debut, 1=courant, 2=fin */
                pos = os_file_seek(fh, offset, whence);
                return pos;
            }
            break;

        /* Malloc - Allocation memoire */
        case GEMDOS_MALLOC:
            {
                void *ptr;
                ptr = os_mem_alloc((size_t)arg1);
                return (os_int32)((size_t)ptr & 0xFFFFFFFFL);
            }
            break;

        /* Mfree - Liberation memoire */
        case GEMDOS_MFREE:
            os_mem_free((void *)(size_t)arg1);
            return 0;

        /* Fsfirst - Premier fichier */
        case GEMDOS_FSFIRST:
            /*
             * FSFIRST retourne 0 si trouve, negatif sinon.
             * L'implementation complete necessite la gestion du DTA.
             */
            return -49;  /* No more files (simplifie) */

        /* Fsnext - Fichier suivant */
        case GEMDOS_FSNEXT:
            return -49;

        /* Frename - Renommer fichier */
        case GEMDOS_FRENAME:
            os_fs_rename((const char *)(size_t)arg1,
                         (const char *)(size_t)arg2);
            return 0;

        /* Mkdir - Creer repertoire */
        case GEMDOS_MKDIR:
            os_dir_mkdir((const char *)(size_t)arg1);
            return 0;

        /* Rmdir - Supprimer repertoire */
        case GEMDOS_RMDIR:
            os_dir_rmdir((const char *)(size_t)arg1);
            return 0;

        /* Fonctions non implementees : retourner -32 (Invalid function) */
        default:
            return -32;
    }
}

/* ------------------------------------------------------------------ */
/* BIOS                                                               */
/* ------------------------------------------------------------------ */

os_int32 gfa_bios(os_int32 fn, os_int32 arg1, os_int32 arg2)
{
    (void)arg2;

    switch (fn) {

        /* Bconstat - Statut entree console */
        case BIOS_BCOSTAT:
            return (os_int32)0;  /* 0 = pas de caractere en attente */

        /* Bconin - Entree console */
        case BIOS_BCONIN:
            return (os_int32)os_con_input_char();

        /* Bconout - Sortie console */
        case BIOS_BCONOUT:
            os_con_output_char((int)(arg1 & 0xFF));
            return 0;

        /* Tickcal - Calibration timer */
        case BIOS_TICKCAL:
            return 200;  /* 200 Hz (standard Atari ST) */

        /* Kbshift - Etat touches modificatrices */
        case BIOS_KBSHIFT:
            return 0;  /* Pas de modificateur enfonce */

        default:
            return -1;  /* General error BIOS */
    }
}

/* ------------------------------------------------------------------ */
/* XBIOS                                                              */
/* ------------------------------------------------------------------ */

os_int32 gfa_xbios(os_int32 fn, os_int32 arg1, os_int32 arg2)
{
    (void)arg2;

    switch (fn) {

        /* Getrez - Resolution ecran */
        case XBIOS_GETRES:
            return (os_int32)os_display_get_resolution();

        /* Physbase - Adresse memoire ecran physique */
        case XBIOS_PHYSBASE:
            return os_sys_get_basepage();

        /* Logbase - Adresse memoire ecran logique */
        case XBIOS_LOGABASE:
            return os_sys_get_basepage();

        /* Random - Nombre aleatoire 24 bits */
        case XBIOS_RANDOM:
            return (os_int32)((unsigned long)(os_time_ticks()) & 0x00FFFFFFUL);

        /* Gettime - Heure systeme */
        case XBIOS_GETTIME:
            return os_time_get_raw_gemdos();

        /* Settime - Regler heure */
        case XBIOS_SETTIME:
            os_time_set_raw_gemdos(arg1);
            return 0;

        /* Screendump - Hardcopy ecran */
        case XBIOS_SCREENDUMP:
            /* Non implemente */
            return 0;

        /* Blitmode - Interrogation/controle blitter */
        case XBIOS_BLITMODE:
            if (arg1 == -1) {
                return 0;  /* Bit 1 a 0 = pas de blitter emule */
            }
            return 0;

        default:
            return -1;
    }
}

/* ------------------------------------------------------------------ */
/* VDISYS                                                             */
/* ------------------------------------------------------------------ */

os_int32 gfa_vdisys(os_int32 opcode, os_int32 c_int, os_int32 c_pts,
                     os_int32 subopc)
{
    (void)opcode;
    (void)c_int;
    (void)c_pts;
    (void)subopc;

    /*
     * VDISYS est un appel VDI generique.
     * L'implementation sera faite dans le module graphics/vdi.c
     * lorsque les primitives VDI seront disponibles.
     */
    return 0;
}

/* ------------------------------------------------------------------ */
/* GEMSYS                                                             */
/* ------------------------------------------------------------------ */

os_int32 gfa_gemsys(os_int32 fn, os_int32 arg1, os_int32 arg2,
                     os_int32 arg3, os_int32 arg4)
{
    (void)arg3;
    (void)arg4;

    /*
     * GEMSYS est un appel AES generique.
     * L'implementation sera faite dans le module gem/aes.c
     * lorsque les fonctions AES seront disponibles.
     *
     * Pour le moment, on essaye de mapper les fonctions AES
     * les plus courantes.
     */
    switch (fn) {
        case 10:  /* appl_init */
            return (os_int32)1;  /* ID application fictif */

        case 19:  /* appl_exit */
            return 0;

        case 20:  /* evnt_keybd */
            return (os_int32)os_con_input_key();

        default:
            break;
    }

    (void)arg1;
    (void)arg2;

    return 0;
}
