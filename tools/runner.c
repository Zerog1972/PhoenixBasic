/*
 * runner.c - Mini-launcher TOS pour tests automatises (C89)
 * =========================================================
 * Lance GFABASIC.PRG avec un fichier .bas en argument via
 * Pexec() (GEMDOS), pour un test non interactif sous Hatari.
 *
 * Usage Hatari :
 *   hatari --disk-a DISK.ST --auto A:\RUNNER.PRG --conout 2 ...
 *
 * Compilation (toolchain m68k-atari-mintelf) :
 *   m68k-atari-mintelf-gcc -std=c89 -Os -mnoshort -m68000 \
 *       -o build/atari/RUNNER.PRG tools/runner.c
 */

#include <osbind.h>
#include <string.h>

/* Nom du programme GFA et du fichier source a lancer (8.3) */
#define GFA_PROG   "A:\\GFABASIC.PRG"
#define TEST_FILE  "A:\\TEST.BAS"

static int build_cmdline(char *buf, int bufsize)
{
    int len;

    /*
     * Convention GEMDOS : la cmdline doit commencer par le nom du
     * programme. Avec l'ABI mintlib (-mnoshort), le crt0 ne retire
     * PAS le nom du programme : argv[1] = 1er token, argv[2] = 2e...
     * On met donc le nom en premier puis le fichier source.
     * main.c utilise argv[argc-1] pour charger le fichier .bas.
     */
    strcpy(buf + 1, GFA_PROG);
    strcat(buf + 1, " ");
    strcat(buf + 1, TEST_FILE);

    len = (int)strlen(buf + 1);
    if (len >= bufsize - 3) {
        len = bufsize - 3;
    }

    buf[0] = (char)len;        /* octet longueur (format TOS) */
    buf[len + 1] = '\r';       /* CR final obligatoire         */
    buf[len + 2] = '\0';
    return len;
}

int main(void)
{
    char cmdline[128];
    long ret;

    build_cmdline(cmdline, (int)sizeof(cmdline));

    /*
     * Pexec mode 0 : charge et execute GFABASIC.PRG en lui
     * passant "A:\TEST.BAS" comme argument (argv[1]).
     */
    ret = Pexec(0, GFA_PROG, cmdline, NULL);
    if (ret < 0) {
        return (int)ret;
    }

    return 0;
}