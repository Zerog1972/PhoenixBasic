/*
 * tos.h - Emulation TOS (GEMDOS, BIOS, XBIOS) pour GFA Basic 3.5
 * ==============================================================
 * Fournit les fonctions d'appel systeme de l'Atari ST.
 *
 * Reference : cahier-des-charges-gfabasic.md, sections 8.17, 9.3-9.5
 */

#ifndef GFA_TOS_H
#define GFA_TOS_H

#include "os_layer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* GEMDOS (GEM Disk Operating System)                                 */
/* ------------------------------------------------------------------ */

/*
 * gfa_gemdos - Appel d'une fonction GEMDOS.
 * fn   : numero de fonction GEMDOS
 * arg1 : premier argument (entier 32 bits)
 * arg2 : deuxieme argument (entier 32 bits)
 * Retourne la valeur de retour de la fonction.
 *
 * Equivalent GFA : GEMDOS(fn[, arg1, arg2])
 */
os_int32 gfa_gemdos(os_int32 fn, os_int32 arg1, os_int32 arg2);

/*
 * Constantes pour les fonctions GEMDOS courantes.
 */
#define GEMDOS_PTERM        0x00
#define GEMDOS_CCONIN       0x01
#define GEMDOS_CCONOUT      0x02
#define GEMDOS_CAUXIN       0x03
#define GEMDOS_CAUXOUT      0x04
#define GEMDOS_CPRNOUT      0x05
#define GEMDOS_CRAWIO       0x06
#define GEMDOS_CRAWIN       0x07
#define GEMDOS_CNECIN       0x08
#define GEMDOS_CCONWS       0x09
#define GEMDOS_CCONRS       0x0A
#define GEMDOS_CCONIS       0x0B
#define GEMDOS_DGETDRV      0x19
#define GEMDOS_FSETDTA      0x1A
#define GEMDOS_DSETDRV      0x0E
#define GEMDOS_SUPER        0x20
#define GEMDOS_TGETDATE     0x2A
#define GEMDOS_TGETTIME     0x2C
#define GEMDOS_FGETDTA      0x2F
#define GEMDOS_FVERSION     0x30
#define GEMDOS_FOPEN        0x3D
#define GEMDOS_FCLOSE       0x3E
#define GEMDOS_FREAD        0x3F
#define GEMDOS_FWRITE       0x40
#define GEMDOS_FDELETE      0x41
#define GEMDOS_FSEEK        0x42
#define GEMDOS_FATTRIB      0x43
#define GEMDOS_MALLOC       0x48
#define GEMDOS_MFREE        0x49
#define GEMDOS_MSHRINK      0x4A
#define GEMDOS_PEXEC        0x4B
#define GEMDOS_PTERMRES     0x4C
#define GEMDOS_FSFIRST      0x4E
#define GEMDOS_FSNEXT       0x4F
#define GEMDOS_FRENAME      0x56
#define GEMDOS_FDATIME      0x57
#define GEMDOS_MKDIR        0x59
#define GEMDOS_RMDIR        0x5A
#define GEMDOS_SETBLOCK     0x5B
#define GEMDOS_PDOMAINS     0x71

/* ------------------------------------------------------------------ */
/* BIOS (Basic Input/Output System)                                   */
/* ------------------------------------------------------------------ */

/*
 * gfa_bios - Appel d'une fonction BIOS.
 * fn   : numero de fonction BIOS
 * arg1, arg2 : arguments
 *
 * Equivalent GFA : BIOS(fn[, arg1, arg2])
 */
os_int32 gfa_bios(os_int32 fn, os_int32 arg1, os_int32 arg2);

/*
 * Constantes pour les fonctions BIOS courantes.
 */
#define BIOS_BCOSTAT        0x01
#define BIOS_BCONIN         0x02
#define BIOS_BCONOUT        0x03
#define BIOS_RWABS          0x04
#define BIOS_SETEXC         0x05
#define BIOS_TICKCAL        0x06
#define BIOS_GETBPB         0x07
#define BIOS_BCONSTAT       0x08
#define BIOS_MEDIACH        0x09
#define BIOS_DRVMAP         0x0A
#define BIOS_KBSHIFT        0x0B

/* ------------------------------------------------------------------ */
/* XBIOS (Extended BIOS)                                              */
/* ------------------------------------------------------------------ */

/*
 * gfa_xbios - Appel d'une fonction XBIOS.
 * fn   : numero de fonction XBIOS
 * arg1, arg2 : arguments
 *
 * Equivalent GFA : XBIOS(fn[, arg1, arg2])
 */
os_int32 gfa_xbios(os_int32 fn, os_int32 arg1, os_int32 arg2);

/*
 * Constantes pour les fonctions XBIOS courantes.
 */
#define XBIOS_INITMOUSE      0
#define XBIOS_PHYSBASE       2
#define XBIOS_LOGABASE       3
#define XBIOS_GETRES         4
#define XBIOS_SETSCREEN      5
#define XBIOS_SETPALETTE     6
#define XBIOS_SETCOLOR       7
#define XBIOS_FLOPYRD        8
#define XBIOS_FLOPYWR        9
#define XBIOS_FLOPYFMT      10
#define XBIOS_MIDIWS        12
#define XBIOS_RS232_GET_IO  14
#define XBIOS_RS232_CONFIG  15
#define XBIOS_KEYTAB        16
#define XBIOS_RANDOM        17
#define XBIOS_PROTOBOOT     18
#define XBIOS_FLOPYVER      19
#define XBIOS_SCREENDUMP    20
#define XBIOS_CURSORCONF    21
#define XBIOS_SETTIME       22
#define XBIOS_GETTIME       23
#define XBIOS_BIOSKEYS      24
#define XBIOS_IKBDWS        25
#define XBIOS_JDISINT       26
#define XBIOS_JENABINT      27
#define XBIOS_SOUNDCMD      28
#define XBIOS_SETPRT        29
#define XBIOS_BLITMODE      64

/* ------------------------------------------------------------------ */
/* VDISYS (appel VDI generique)                                       */
/* ------------------------------------------------------------------ */

/*
 * gfa_vdisys - Appel VDI generique.
 * Equivalent GFA : VDISYS[opcode [,c_int,c_pts[,subopc]]]
 */
os_int32 gfa_vdisys(os_int32 opcode, os_int32 c_int, os_int32 c_pts,
                     os_int32 subopc);

/* ------------------------------------------------------------------ */
/* GEMSYS (appel AES generique)                                       */
/* ------------------------------------------------------------------ */

/*
 * gfa_gemsys - Appel AES generique.
 * Equivalent GFA : GEMSYS(fn, ...)
 */
os_int32 gfa_gemsys(os_int32 fn, os_int32 arg1, os_int32 arg2,
                     os_int32 arg3, os_int32 arg4);

#ifdef __cplusplus
}
#endif

#endif /* GFA_TOS_H */
