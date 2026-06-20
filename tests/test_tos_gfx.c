/*
 * test_tos_gfx.c - Tests TOS (GEMDOS/BIOS/XBIOS) et Graphisme ANSI
 * ==================================================================
 */

#include "runtime.h"
#include "token.h"
#include "tos.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(expr, msg) do { \
    g_tests_run++; \
    if (expr) { \
        g_tests_passed++; \
        printf("  [PASS] %s\n", msg); \
    } else { \
        g_tests_failed++; \
        printf("  [FAIL] %s (line %d)\n", msg, __LINE__); \
    } \
} while(0)

#define TOLERANCE 0.0001

/* ------------------------------------------------------------------ */
/* Tests GEMDOS                                                        */
/* ------------------------------------------------------------------ */

static void test_gemdos(void)
{
    os_int32 ret;

    printf("\n--- GEMDOS ---\n");

    /* GEMDOS Fversion - Version TOS */
    ret = gfa_gemdos(GEMDOS_FVERSION, 0, 0);
    TEST_ASSERT(ret == 0x0015, "GEMDOS FVERSION = 0x0015 (TOS 1.04)");

    /* GEMDOS Dgetdrv - Lecteur courant */
    ret = gfa_gemdos(GEMDOS_DGETDRV, 0, 0);
    TEST_ASSERT(ret >= 1 && ret <= 26, "GEMDOS DGETDRV returns valid drive (1-26)");

    /* GEMDOS Tgetdate - Date systeme */
    ret = gfa_gemdos(GEMDOS_TGETDATE, 0, 0);
    TEST_ASSERT(ret > 0, "GEMDOS TGETDATE returns packed date");

    /* GEMDOS Tgettime - Heure systeme */
    ret = gfa_gemdos(GEMDOS_TGETTIME, 0, 0);
    TEST_ASSERT(ret > 0, "GEMDOS TGETTIME returns packed time");

    /* GEMDOS Super - Mode superviseur */
    ret = gfa_gemdos(GEMDOS_SUPER, 0, 0);
    TEST_ASSERT(ret == 0, "GEMDOS SUPER returns 0 (emulated)");

    /* GEMDOS Fgetdta - Adresse DTA */
    ret = gfa_gemdos(GEMDOS_FGETDTA, 0, 0);
    TEST_ASSERT(ret == 0, "GEMDOS FGETDTA returns 0 (emulated)");

    /* GEMDOS Fsetdta - Fixer DTA */
    ret = gfa_gemdos(GEMDOS_FSETDTA, 0, 0);
    TEST_ASSERT(ret == 0, "GEMDOS FSETDTA returns 0");

    /* GEMDOS Dsetdrv - Changer lecteur */
    ret = gfa_gemdos(GEMDOS_DSETDRV, 1, 0);
    TEST_ASSERT(ret == 0, "GEMDOS DSETDRV succeeds");
    /* Restore */
    gfa_gemdos(GEMDOS_DSETDRV, 0, 0);

    /* GEMDOS Fonction invalide */
    ret = gfa_gemdos(0xFFFF, 0, 0);
    TEST_ASSERT(ret == -32, "GEMDOS unknown function returns -32");

    /* GEMDOS Fdelete - appel basique */
    ret = gfa_gemdos(GEMDOS_FDELETE, 0, 0);
    TEST_ASSERT(ret == 0, "GEMDOS FDELETE(NULL) returns 0 (no-op)");

    /* GEMDOS Fsfirst / Fsnext */
    ret = gfa_gemdos(GEMDOS_FSFIRST, 0, 0);
    TEST_ASSERT(ret == -49, "GEMDOS FSFIRST returns -49 (no more files)");

    ret = gfa_gemdos(GEMDOS_FSNEXT, 0, 0);
    TEST_ASSERT(ret == -49, "GEMDOS FSNEXT returns -49 (no more files)");

    /* GEMDOS Malloc / Mfree (via handle 32-bit sur 64-bit) */
    {
        os_int32 handle;
        handle = gfa_gemdos(GEMDOS_MALLOC, 128, 0);
        TEST_ASSERT(handle >= 1, "GEMDOS MALLOC(128) returns handle >= 1");
        if (handle >= 1) {
            ret = gfa_gemdos(GEMDOS_MFREE, handle, 0);
            TEST_ASSERT(ret == 0, "GEMDOS MFREE(handle) returns 0");
        }
        /* Allocation de taille 0 */
        handle = gfa_gemdos(GEMDOS_MALLOC, 0, 0);
        TEST_ASSERT(handle == 0, "GEMDOS MALLOC(0) returns 0 (no alloc)");
        /* Liberation de handle invalide (no crash) */
        ret = gfa_gemdos(GEMDOS_MFREE, 0, 0);
        TEST_ASSERT(ret == 0, "GEMDOS MFREE(0) returns 0 (no crash)");
        ret = gfa_gemdos(GEMDOS_MFREE, 999, 0);
        TEST_ASSERT(ret == 0, "GEMDOS MFREE(999) returns 0 (no crash)");
    }

}

/* ------------------------------------------------------------------ */
/* Tests BIOS                                                         */
/* ------------------------------------------------------------------ */

static void test_bios(void)
{
    os_int32 ret;

    printf("\n--- BIOS ---\n");

    /* BIOS Bconstat */
    ret = gfa_bios(BIOS_BCOSTAT, 0, 0);
    TEST_ASSERT(ret == 0, "BIOS BCOSTAT returns 0 (no key pending)");

    /* BIOS Bconout */
    ret = gfa_bios(BIOS_BCONOUT, 'T', 0);
    TEST_ASSERT(ret == 0, "BIOS BCONOUT('T') returns 0");

    /* BIOS Tickcal */
    ret = gfa_bios(BIOS_TICKCAL, 0, 0);
    TEST_ASSERT(ret == 200, "BIOS TICKCAL = 200 (Atari ST standard)");

    /* BIOS Kbshift (non implemente → -1) */
    ret = gfa_bios(0x0C, 0, 0);
    TEST_ASSERT(ret == -1, "BIOS KBSHIFT returns -1 (not implemented)");

    /* BIOS fonction inconnue */
    ret = gfa_bios(0xFF, 0, 0);
    TEST_ASSERT(ret == -1, "BIOS unknown function returns -1");
}

/* ------------------------------------------------------------------ */
/* Tests XBIOS                                                        */
/* ------------------------------------------------------------------ */

static void test_xbios(void)
{
    os_int32 ret;

    printf("\n--- XBIOS ---\n");

    /* XBIOS Getrez */
    ret = gfa_xbios(XBIOS_GETRES, 0, 0);
    TEST_ASSERT(ret >= -1 && ret <= 2, "XBIOS GETRES returns valid resolution (-1 to 2)");

    /* XBIOS Physbase */
    ret = gfa_xbios(XBIOS_PHYSBASE, 0, 0);
    TEST_ASSERT(ret != 0, "XBIOS PHYSBASE returns non-zero address");

    /* XBIOS Logbase */
    ret = gfa_xbios(XBIOS_LOGABASE, 0, 0);
    TEST_ASSERT(ret != 0, "XBIOS LOGABASE returns non-zero address");

    /* XBIOS Random */
    ret = gfa_xbios(XBIOS_RANDOM, 0, 0);
    TEST_ASSERT(ret >= 0 && ret <= 0x00FFFFFF, "XBIOS RANDOM returns 24-bit value");

    /* XBIOS Gettime */
    ret = gfa_xbios(XBIOS_GETTIME, 0, 0);
    TEST_ASSERT(ret > 0, "XBIOS GETTIME returns positive value");

    /* XBIOS Blitmode */
    ret = gfa_xbios(XBIOS_BLITMODE, -1, 0);
    TEST_ASSERT(ret == 0, "XBIOS BLITMODE(-1) returns 0 (no blitter)");
    ret = gfa_xbios(XBIOS_BLITMODE, 0, 0);
    TEST_ASSERT(ret == 0, "XBIOS BLITMODE(0) returns 0");

    /* XBIOS fonction inconnue */
    ret = gfa_xbios(0xFF, 0, 0);
    TEST_ASSERT(ret == -1, "XBIOS unknown function returns -1");
}

/* ------------------------------------------------------------------ */
/* Tests VDISYS / GEMSYS                                              */
/* ------------------------------------------------------------------ */

static void test_vdi_aes(void)
{
    os_int32 ret;

    printf("\n--- VDI / AES ---\n");

    /* VDISYS */
    ret = gfa_vdisys(0, 0, 0, 0);
    TEST_ASSERT(ret == 0, "VDISYS returns 0 (placeholder)");

    /* GEMSYS appl_init */
    ret = gfa_gemsys(10, 0, 0, 0, 0);
    TEST_ASSERT(ret == 1, "GEMSYS appl_init returns 1 (fake app ID)");

    /* GEMSYS appl_exit */
    ret = gfa_gemsys(19, 0, 0, 0, 0);
    TEST_ASSERT(ret == 0, "GEMSYS appl_exit returns 0");

    /* GEMSYS fonction inconnue */
    ret = gfa_gemsys(999, 0, 0, 0, 0);
    TEST_ASSERT(ret == 0, "GEMSYS unknown function returns 0");
}

/* ------------------------------------------------------------------ */
/* Tests Graphisme ANSI (via bytecode)                                */
/* ------------------------------------------------------------------ */

static void test_graphics_bytecode(void)
{
    gfa_runtime *rt;
    gfa_bytecode *bc;

    printf("\n--- Graphics (bytecode) ---\n");

    /* COLOR 2 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_bytecode_emit_int(bc, OP_PUSH_CONST, 2);
    gfa_bytecode_emit(bc, OP_COLOR);
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(1, "COLOR 2 executes without crash (ANSI placeholder)");
    gfa_runtime_shutdown(rt);

    /* COLOR 1, 2 (avec bg) */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_bytecode_emit_int(bc, OP_PUSH_CONST, 2);
    gfa_bytecode_emit_int(bc, OP_PUSH_CONST, 1);
    gfa_bytecode_emit(bc, OP_COLOR);
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(1, "COLOR 1,2 executes without crash");
    gfa_runtime_shutdown(rt);

    /* LINE x1,y1,x2,y2 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_bytecode_emit_int(bc, OP_PUSH_CONST, 10);
    gfa_bytecode_emit_int(bc, OP_PUSH_CONST, 20);
    gfa_bytecode_emit_int(bc, OP_PUSH_CONST, 100);
    gfa_bytecode_emit_int(bc, OP_PUSH_CONST, 200);
    gfa_bytecode_emit(bc, OP_LINE_GFX);
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(1, "LINE 10,20,100,200 executes without crash");
    gfa_runtime_shutdown(rt);

    /* BOX x1,y1,x2,y2 */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_bytecode_emit_int(bc, OP_PUSH_CONST, 5);
    gfa_bytecode_emit_int(bc, OP_PUSH_CONST, 5);
    gfa_bytecode_emit_int(bc, OP_PUSH_CONST, 50);
    gfa_bytecode_emit_int(bc, OP_PUSH_CONST, 50);
    gfa_bytecode_emit(bc, OP_BOX_GFX);
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(1, "BOX 5,5,50,50 executes without crash");
    gfa_runtime_shutdown(rt);

    /* PBOX (filled box) */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_bytecode_emit_int(bc, OP_PUSH_CONST, 10);
    gfa_bytecode_emit_int(bc, OP_PUSH_CONST, 10);
    gfa_bytecode_emit_int(bc, OP_PUSH_CONST, 30);
    gfa_bytecode_emit_int(bc, OP_PUSH_CONST, 30);
    gfa_bytecode_emit(bc, OP_PBOX_GFX);
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(1, "PBOX 10,10,30,30 executes without crash");
    gfa_runtime_shutdown(rt);

    /* CIRCLE x,y,r */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_bytecode_emit_int(bc, OP_PUSH_CONST, 50);
    gfa_bytecode_emit_int(bc, OP_PUSH_CONST, 50);
    gfa_bytecode_emit_int(bc, OP_PUSH_CONST, 25);
    gfa_bytecode_emit(bc, OP_CIRCLE_GFX);
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(1, "CIRCLE 50,50,25 executes without crash");
    gfa_runtime_shutdown(rt);

    /* PCIRCLE (filled circle) */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_bytecode_emit_int(bc, OP_PUSH_CONST, 100);
    gfa_bytecode_emit_int(bc, OP_PUSH_CONST, 100);
    gfa_bytecode_emit_int(bc, OP_PUSH_CONST, 30);
    gfa_bytecode_emit_int(bc, OP_PUSH_CONST, 1);  /* fill flag */
    gfa_bytecode_emit(bc, OP_CIRCLE_GFX);
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(1, "PCIRCLE 100,100,30 executes without crash");
    gfa_runtime_shutdown(rt);

    /* COLOR sans argument (par defaut) */
    rt = gfa_runtime_init();
    bc = gfa_bytecode_create();
    gfa_bytecode_emit(bc, OP_COLOR);
    gfa_bytecode_emit(bc, OP_END);
    gfa_runtime_load(rt, bc);
    gfa_runtime_execute(rt);
    TEST_ASSERT(1, "COLOR (default) executes without crash");
    gfa_runtime_shutdown(rt);
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("========================================\n");
    printf(" Tests TOS (GEMDOS, BIOS, XBIOS) + GFX\n");
    printf(" PhoenixBasic - GFA Basic 3.5\n");
    printf("========================================\n");

    os_init();

    test_gemdos();
    test_bios();
    test_xbios();
    test_vdi_aes();
    test_graphics_bytecode();

    printf("\n========================================\n");
    printf(" Resultat : %d/%d reussis, %d echoues\n",
           g_tests_passed, g_tests_run, g_tests_failed);
    printf("========================================\n");

    if (g_tests_failed > 0) {
        printf("\n*** %d TEST(S) ECHOUES ***\n", g_tests_failed);
        return 1;
    }

    printf("\n*** TOUS LES TESTS ONT REUSSI ***\n");
    return 0;
}
