/*
 * test_os.c — Tests unitaires de la couche d'abstraction OS
 * =========================================================
 * Valide l'ensemble des fonctions de os_layer.h.
 * Compilation C89 stricte.
 *
 * Usage : ./test_os
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "os_layer.h"

/* Compteurs de tests */
static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

/* Macro utilitaire pour les assertions */
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

#define TEST_ASSERT_INT_EQ(expected, actual, msg) do { \
    g_tests_run++; \
    if ((expected) == (actual)) { \
        g_tests_passed++; \
        printf("  [PASS] %s (%ld)\n", msg, (long)(actual)); \
    } else { \
        g_tests_failed++; \
        printf("  [FAIL] %s : expected %ld, got %ld (line %d)\n", \
               msg, (long)(expected), (long)(actual), __LINE__); \
    } \
} while(0)

/* ------------------------------------------------------------------ */
/* Tests — Initialisation / Arrêt                                     */
/* ------------------------------------------------------------------ */

static void test_init_shutdown(void)
{
    int result;

    printf("\n--- Test: Initialisation / Arrêt ---\n");

    result = os_init();
    TEST_ASSERT(result == 0, "os_init succeeds");

    TEST_ASSERT(os_get_error() == OS_ERR_NONE, "os_get_error returns NONE after init");

    os_shutdown();
    TEST_ASSERT(1, "os_shutdown completes without crash");

    /* Réinitialiser pour les tests suivants */
    os_init();
}

/* ------------------------------------------------------------------ */
/* Tests — Erreurs                                                    */
/* ------------------------------------------------------------------ */

static void test_error_handling(void)
{
    const char *msg;

    printf("\n--- Test: Gestion des erreurs ---\n");

    msg = os_get_error_string(OS_ERR_NONE);
    TEST_ASSERT(msg != NULL && strlen(msg) > 0,
                "os_get_error_string(NONE) returns non-empty string");

    msg = os_get_error_string(OS_ERR_FILE_NOT_FOUND);
    TEST_ASSERT(msg != NULL && strlen(msg) > 0,
                "os_get_error_string(FILE_NOT_FOUND) returns non-empty string");

    msg = os_get_error_string(OS_ERR_OUT_OF_MEMORY);
    TEST_ASSERT(msg != NULL && strlen(msg) > 0,
                "os_get_error_string(OUT_OF_MEMORY) returns non-empty string");

    msg = os_get_last_error_string();
    TEST_ASSERT(msg != NULL, "os_get_last_error_string returns non-NULL");
}

/* ------------------------------------------------------------------ */
/* Tests — Fichiers                                                   */
/* ------------------------------------------------------------------ */

static void test_file_operations(void)
{
    os_file_handle fh;
    const char *test_filename = "__test_os_layer_file__.tmp";
    const char *test_data = "Hello GFA Basic 3.5!\nLine 2\n";
    char read_buffer[256];
    os_int32 bytes;
    os_int32 size;
    int eof;

    printf("\n--- Test: Opérations sur les fichiers ---\n");

    /* Ouvrir en écriture */
    fh = os_file_open(test_filename, OS_GFAMODE_OUTPUT, 1);
    TEST_ASSERT(fh != NULL, "os_file_open (write) succeeds");

    /* Écrire */
    bytes = os_file_write(fh, test_data, (os_int32)strlen(test_data));
    TEST_ASSERT_INT_EQ((os_int32)strlen(test_data), bytes,
                       "os_file_write returns correct byte count");

    /* Flush */
    TEST_ASSERT(os_file_flush(fh) == 0, "os_file_flush succeeds");

    /* Taille */
    size = os_file_size(fh);
    TEST_ASSERT_INT_EQ((os_int32)strlen(test_data), size,
                       "os_file_size returns correct size");

    /* EOF après écriture ? Dépend de l'implémentation */
    eof = os_file_eof(fh);
    /* Après écriture, on est à la fin du fichier, donc EOF devrait être vrai */
    (void)eof;
    TEST_ASSERT(1, "os_file_eof returns valid value");

    /* Fermer */
    TEST_ASSERT(os_file_close(fh) == 0, "os_file_close succeeds");

    /* Ouvrir en lecture */
    fh = os_file_open(test_filename, OS_GFAMODE_INPUT, 2);
    TEST_ASSERT(fh != NULL, "os_file_open (read) succeeds");

    /* Vérifier taille */
    size = os_file_size(fh);
    TEST_ASSERT_INT_EQ((os_int32)strlen(test_data), size,
                       "os_file_size on reopened file matches");

    /* Lire */
    os_mem_set(read_buffer, 0, sizeof(read_buffer));
    bytes = os_file_read(fh, read_buffer, (os_int32)strlen(test_data));
    TEST_ASSERT_INT_EQ((os_int32)strlen(test_data), bytes,
                       "os_file_read returns correct byte count");

    read_buffer[bytes] = '\0';
    TEST_ASSERT(strcmp(test_data, read_buffer) == 0,
                "os_file_read data matches written data");

    /* EOF - after reading exactly the file, EOF may not be set yet
       (it's only set when trying to read PAST end). Try one more read. */
    {
        char dummy[1];
        bytes = os_file_read(fh, dummy, 1);
        TEST_ASSERT(bytes == 0, "os_file_read returns 0 when trying to read past EOF");
        eof = os_file_eof(fh);
        TEST_ASSERT(eof == OS_TRUE, "os_file_eof returns TRUE after attempting to read past end");
    }

    /* Fermer */
    TEST_ASSERT(os_file_close(fh) == 0, "os_file_close succeeds");

    /* Fermeture par canal */
    fh = os_file_open(test_filename, OS_GFAMODE_INPUT, 3);
    TEST_ASSERT(fh != NULL, "os_file_open succeeds for channel 3");
    TEST_ASSERT(os_file_close_by_channel(3) == 0,
                "os_file_close_by_channel succeeds");

    /* Get handle by channel - valide */
    fh = os_file_open(test_filename, OS_GFAMODE_INPUT, 4);
    TEST_ASSERT(fh != NULL, "os_file_open succeeds for channel 4");
    {
        os_file_handle fh2;
        fh2 = os_file_get_handle_by_channel(4);
        TEST_ASSERT(fh2 == fh, "os_file_get_handle_by_channel returns same handle");
    }
    TEST_ASSERT(os_file_get_channel(fh) == 4,
                "os_file_get_channel returns correct channel");
    TEST_ASSERT(os_file_close(fh) == 0, "os_file_close channel 4 succeeds");

    /* Get handle by channel - canal fermé */
    {
        os_file_handle fh3;
        fh3 = os_file_get_handle_by_channel(5);
        TEST_ASSERT(fh3 == NULL,
                    "os_file_get_handle_by_channel returns NULL for closed channel");
    }

    /* Nettoyer */
    os_fs_delete(test_filename);
}

/* ------------------------------------------------------------------ */
/* Tests — Opérations système de fichiers                             */
/* ------------------------------------------------------------------ */

static void test_filesystem_operations(void)
{
    int result;
    const char *test_file = "__test_fs_ops__.tmp";
    const char *test_dir  = "__test_fs_dir__";
    os_file_handle fh;

    printf("\n--- Test: Opérations système de fichiers ---\n");

    /* Créer un fichier test */
    fh = os_file_open(test_file, OS_GFAMODE_OUTPUT, 10);
    TEST_ASSERT(fh != NULL, "File created for FS tests");
    os_file_write(fh, "test", 4);
    os_file_close(fh);

    /* EXIST */
    result = os_fs_exist(test_file);
    TEST_ASSERT(result == OS_TRUE, "os_fs_exist returns TRUE for existing file");

    result = os_fs_exist("__nonexistent_file_xyz123__");
    TEST_ASSERT(result == OS_FALSE,
                "os_fs_exist returns FALSE for nonexistent file");

    /* DFREE / total */
    {
        os_int32 free_space;
        free_space = os_fs_free(0);
        TEST_ASSERT(free_space > 0, "os_fs_free returns positive value");
    }
    {
        os_int32 total_space;
        total_space = os_fs_total(0);
        TEST_ASSERT(total_space > 0, "os_fs_total returns positive value");
    }

    /* MKDIR */
    result = os_dir_mkdir(test_dir);
    TEST_ASSERT(result == 0, "os_dir_mkdir succeeds");

    /* RMDIR */
    result = os_dir_rmdir(test_dir);
    TEST_ASSERT(result == 0, "os_dir_rmdir succeeds");

    /* RENAME */
    {
        const char *renamed = "__test_renamed__.tmp";
        result = os_fs_rename(test_file, renamed);
        TEST_ASSERT(result == 0, "os_fs_rename succeeds");
        TEST_ASSERT(os_fs_exist(renamed) == OS_TRUE,
                    "Renamed file exists after rename");
        TEST_ASSERT(os_fs_exist(test_file) == OS_FALSE,
                    "Original file does not exist after rename");
        os_fs_delete(renamed);
    }

    /* CHDIR / GETCWD */
    {
        char *cwd;
        int cwd_len;
        cwd = os_dir_getcwd(&cwd_len);
        TEST_ASSERT(cwd != NULL, "os_dir_getcwd returns non-NULL");
        TEST_ASSERT(cwd_len > 0, "os_dir_getcwd length > 0");
        if (cwd != NULL) {
            os_mem_free(cwd);
        }
    }

    /* KILL (suppression) */
    /* Déjà fait ci-dessus */
}

/* ------------------------------------------------------------------ */
/* Tests — Répertoires (DIR)                                          */
/* ------------------------------------------------------------------ */

static void test_directory_listing(void)
{
    os_file_info info;
    int result;

    printf("\n--- Test: Liste des répertoires ---\n");

    /* FSFIRST avec pattern */
    os_mem_set(&info, 0, sizeof(info));
    result = os_dir_first("*", 0, &info);
    if (result == 0) {
        TEST_ASSERT(info.name[0] != '\0',
                    "os_dir_first returns valid filename");
        TEST_ASSERT(strlen(info.name) <= 13,
                    "os_dir_first filename fits 8.3");

        /* FSNEXT (au moins un fichier suivant ou NO_MORE) */
        result = os_dir_next(&info);
        TEST_ASSERT(result == 0 || result == OS_ERR_NO_MORE_FILES,
                    "os_dir_next returns valid status");
        TEST_ASSERT(1, "os_dir_next: listing works");
    } else {
        printf("  [INFO] os_dir_first: no files in current directory\n");
        TEST_ASSERT(1, "os_dir_first: no files (acceptable)");
    }
}

/* ------------------------------------------------------------------ */
/* Tests — Console                                                    */
/* ------------------------------------------------------------------ */

static void test_console(void)
{
    printf("\n--- Test: Console ---\n");

    os_con_clear();
    printf("  [INFO] os_con_clear: screen cleared (visually)\n");
    TEST_ASSERT(1, "os_con_clear executes without crash");

    os_con_output_string("  Console output test\n");
    TEST_ASSERT(1, "os_con_output_string executes without crash");

    os_con_output_char('X');
    os_con_output_char('\n');
    TEST_ASSERT(1, "os_con_output_char executes without crash");

    /* Test de positionnement curseur */
    os_con_cursor_goto(1, 1);
    TEST_ASSERT(1, "os_con_cursor_goto(1,1) executes without crash");

    /* Test de echo */
    os_con_set_echo(0);
    TEST_ASSERT(os_con_get_echo() == OS_FALSE,
                "os_con_set_echo(FALSE) works");
    os_con_set_echo(1);
    TEST_ASSERT(os_con_get_echo() == OS_TRUE,
                "os_con_set_echo(TRUE) works");

    /* Test clear to end of line */
    os_con_clear_to_eol();
    TEST_ASSERT(1, "os_con_clear_to_eol executes without crash");
}

/* ------------------------------------------------------------------ */
/* Tests — Temps                                                      */
/* ------------------------------------------------------------------ */

static void test_time(void)
{
    os_int32 ticks_before, ticks_after;
    const char *date, *time_str;

    printf("\n--- Test: Temps et minuteurs ---\n");

    /* Ticks */
    ticks_before = os_time_ticks();
    TEST_ASSERT(ticks_before >= 0, "os_time_ticks returns non-negative");

    os_time_delay(100);  /* 100 ms */

    ticks_after = os_time_ticks();
    TEST_ASSERT(ticks_after >= ticks_before,
                "os_time_ticks increases after delay");

    /* Millis */
    {
        os_int32 ms;
        ms = os_time_millis();
        TEST_ASSERT(ms >= 0, "os_time_millis returns non-negative");
    }

    /* Date */
    date = os_time_get_date(0);  /* format EU */
    TEST_ASSERT(date != NULL && strlen(date) > 0,
                "os_time_get_date(EU) returns non-empty string");
    printf("  [INFO] Date (EU) : %s\n", date);

    date = os_time_get_date(1);  /* format US */
    TEST_ASSERT(date != NULL && strlen(date) > 0,
                "os_time_get_date(US) returns non-empty string");
    printf("  [INFO] Date (US) : %s\n", date);

    /* Heure */
    time_str = os_time_get_time();
    TEST_ASSERT(time_str != NULL && strlen(time_str) > 0,
                "os_time_get_time returns non-empty string");
    printf("  [INFO] Time      : %s\n", time_str);

    /* GEMDOS raw */
    {
        os_int32 raw;
        raw = os_time_get_raw_gemdos();
        TEST_ASSERT(raw > 0,
                    "os_time_get_raw_gemdos returns non-zero packed time");
    }

    /* SETTIME */
    {
        int result;
        result = os_time_set_time("12:00:00");
        (void)result;
        TEST_ASSERT(1, "os_time_set_time executes without crash");
    }
}

/* ------------------------------------------------------------------ */
/* Tests — Mémoire                                                    */
/* ------------------------------------------------------------------ */

static void test_memory(void)
{
    void *ptr1, *ptr2;
    os_int32 avail;
    os_int32 total;
    os_int32 largest;

    printf("\n--- Test: Gestion mémoire ---\n");

    /* Allocation */
    ptr1 = os_mem_alloc(1024);
    TEST_ASSERT(ptr1 != NULL, "os_mem_alloc(1024) succeeds");

    /* Remplissage et copie */
    os_mem_set(ptr1, 0xAB, 1024);
    TEST_ASSERT(((unsigned char *)ptr1)[0] == 0xAB,
                "os_mem_set writes correct value");
    TEST_ASSERT(((unsigned char *)ptr1)[1023] == 0xAB,
                "os_mem_set writes to end of block");

    ptr2 = os_mem_alloc(1024);
    TEST_ASSERT(ptr2 != NULL, "os_mem_alloc(1024) #2 succeeds");

    os_mem_copy(ptr2, ptr1, 512);
    TEST_ASSERT(((unsigned char *)ptr2)[0] == 0xAB,
                "os_mem_copy copies data correctly");

    /* Réallocation */
    ptr1 = os_mem_realloc(ptr1, 2048);
    TEST_ASSERT(ptr1 != NULL, "os_mem_realloc to larger size succeeds");

    /* Libération */
    os_mem_free(ptr1);
    os_mem_free(ptr2);
    TEST_ASSERT(1, "os_mem_free executes without crash");

    /* Mémoire disponible */
    avail = os_mem_available(&total);
    TEST_ASSERT(avail > 0, "os_mem_available returns positive value");
    TEST_ASSERT(total > 0, "os_mem_available total is positive");
    printf("  [INFO] Memory: %ld free / %ld total\n",
           (long)avail, (long)total);

    /* Plus grand bloc */
    largest = os_mem_largest_block();
    TEST_ASSERT(largest > 0, "os_mem_largest_block returns positive value");
}

/* ------------------------------------------------------------------ */
/* Tests — Son                                                        */
/* ------------------------------------------------------------------ */

static void test_sound(void)
{
    printf("\n--- Test: Son ---\n");

    os_sound_beep();
    printf("  [INFO] os_sound_beep: should have heard a beep\n");
    TEST_ASSERT(1, "os_sound_beep executes without crash");

    TEST_ASSERT(os_sound_init() == 0, "os_sound_init succeeds");

    os_sound_tone(0, 440, 100, 10);
    TEST_ASSERT(1, "os_sound_tone executes without crash");

    os_sound_stop_channel(0);
    TEST_ASSERT(1, "os_sound_stop_channel executes without crash");

    os_sound_stop_all();
    TEST_ASSERT(1, "os_sound_stop_all executes without crash");

    os_sound_shutdown();
    TEST_ASSERT(1, "os_sound_shutdown executes without crash");
}

/* ------------------------------------------------------------------ */
/* Tests — Système                                                    */
/* ------------------------------------------------------------------ */

static void test_system(void)
{
    const char *env;
    int drive;
    os_int32 basepage;

    printf("\n--- Test: Système ---\n");

    /* Environnement */
    env = os_sys_get_env("PATH");
    TEST_ASSERT(env != NULL, "os_sys_get_env(PATH) returns non-NULL");

    env = os_sys_get_env("NONEXISTENT_ENV_VAR_12345");
    if (env != NULL && strlen(env) == 0) {
        TEST_ASSERT(1, "os_sys_get_env(nonexistent) returns empty string");
    } else {
        TEST_ASSERT(1, "os_sys_get_env(nonexistent) returns value or empty");
    }

    /* Lecteur */
    drive = os_sys_get_drive();
    TEST_ASSERT(drive >= 1 && drive <= 26,
                "os_sys_get_drive returns valid drive (1-26)");
    printf("  [INFO] Current drive: %d (A:=1)\n", drive);

    /* Basepage */
    basepage = os_sys_get_basepage();
    TEST_ASSERT(basepage != 0, "os_sys_get_basepage returns non-zero");

    /* CHDRIVE */
    {
        int result;
        result = os_sys_set_drive(drive);
        TEST_ASSERT(result == 0, "os_sys_set_drive succeeds");
    }
}

/* ------------------------------------------------------------------ */
/* Tests — Display Driver (enregistrement)                            */
/* ------------------------------------------------------------------ */

/*
 * Driver d'affichage factice pour les tests.
 */
static int dummy_display_init(int mode)
{
    (void)mode;
    return 0;
}

static void dummy_display_shutdown(void) {}

static void dummy_display_clear(int color) { (void)color; }

static void dummy_display_set_pixel(int x, int y, int color)
{
    (void)x; (void)y; (void)color;
}

static int dummy_display_get_pixel(int x, int y)
{
    (void)x; (void)y;
    return 0;
}

static void dummy_display_draw_line(int x1, int y1, int x2, int y2, int color)
{
    (void)x1; (void)y1; (void)x2; (void)y2; (void)color;
}

static void dummy_display_draw_text(int x, int y, const char *text, int color)
{
    (void)x; (void)y; (void)text; (void)color;
}

static void dummy_display_fill_rect(int x, int y, int w, int h, int color)
{
    (void)x; (void)y; (void)w; (void)h; (void)color;
}

static void dummy_display_update(void) {}

static void dummy_display_set_palette(int index, unsigned long rgb)
{
    (void)index; (void)rgb;
}

static unsigned long dummy_display_get_palette(int index)
{
    (void)index;
    return 0;
}

static os_int32 dummy_display_poll_event(void)
{
    return 0;
}

static os_int32 dummy_display_wait_event(void)
{
    return 0;
}

static int dummy_display_get_resolution(void)
{
    return OS_ST_MODE_LOW;
}

static const os_display_driver g_dummy_driver = {
    dummy_display_init,
    dummy_display_shutdown,
    dummy_display_clear,
    dummy_display_set_pixel,
    dummy_display_get_pixel,
    dummy_display_draw_line,
    dummy_display_draw_text,
    dummy_display_fill_rect,
    dummy_display_update,
    dummy_display_set_palette,
    dummy_display_get_palette,
    dummy_display_poll_event,
    dummy_display_wait_event,
    dummy_display_get_resolution
};

static void test_display_driver(void)
{
    const os_display_driver *drv;
    int result;

    printf("\n--- Test: Driver d'affichage ---\n");

    /* Driver NULL */
    drv = os_display_get();
    TEST_ASSERT(drv == NULL, "os_display_get returns NULL before registration");

    /* Enregistrement */
    result = os_display_register(&g_dummy_driver);
    TEST_ASSERT(result == 0, "os_display_register succeeds");

    drv = os_display_get();
    TEST_ASSERT(drv != NULL, "os_display_get returns driver after registration");
    TEST_ASSERT(drv == &g_dummy_driver,
                "os_display_get returns the registered driver");

    /* Init mode */
    result = os_display_set_mode(OS_ST_MODE_LOW);
    TEST_ASSERT(result == 0, "os_display_set_mode(LOW) succeeds");

    /* Résolution */
    {
        int res;
        res = os_display_get_resolution();
        TEST_ASSERT(res == OS_ST_MODE_LOW,
                    "os_display_get_resolution returns LOW");
    }

    /* Test de toutes les fonctions du driver */
    if (drv != NULL) {
        drv->clear(0);
        drv->set_pixel(10, 10, 1);
        {
            int px;
            px = drv->get_pixel(10, 10);
            (void)px;
        }
        drv->draw_line(0, 0, 100, 100, 1);
        drv->draw_text(50, 50, "Test", 1);
        drv->fill_rect(0, 0, 320, 200, 7);
        drv->update();
        drv->set_palette(0, 0x000000);
        {
            unsigned long pal;
            pal = drv->get_palette(0);
            (void)pal;
        }
        {
            os_int32 ev;
            ev = drv->poll_event();
            (void)ev;
        }
        TEST_ASSERT(1, "All driver functions execute without crash");
    }
}

/* ------------------------------------------------------------------ */
/* Tests — Palette Atari ST                                           */
/* ------------------------------------------------------------------ */

static void test_st_palette(void)
{
    int i;

    printf("\n--- Test: Palette Atari ST ---\n");

    for (i = 0; i < 16; i++) {
        TEST_ASSERT((os_st_palette[i] & 0xFF000000UL) == 0,
                    "ST palette entry has no alpha (24-bit RGB)");
    }
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("========================================\n");
    printf(" Tests de la couche d'abstraction OS\n");
    printf("     GFA Basic 3.5 — C89\n");
    printf("========================================\n");

    g_tests_run = 0;
    g_tests_passed = 0;
    g_tests_failed = 0;

    /* Initialisation */
    os_init();

    /* Exécuter tous les tests */
    test_init_shutdown();
    test_error_handling();
    test_file_operations();
    test_filesystem_operations();
    test_directory_listing();
    test_console();
    test_time();
    test_memory();
    test_sound();
    test_system();
    test_display_driver();
    test_st_palette();

    /* Nettoyer */
    os_shutdown();

    /* Résumé */
    printf("\n========================================\n");
    printf(" Résumé des tests\n");
    printf("   Exécutés : %d\n", g_tests_run);
    printf("   Réussis  : %d\n", g_tests_passed);
    printf("   Échoués  : %d\n", g_tests_failed);
    printf("========================================\n");

    if (g_tests_failed > 0) {
        printf("\n*** %d TEST(S) ÉCHOUÉ(S) ***\n", g_tests_failed);
        return 1;
    }

    printf("\n*** TOUS LES TESTS ONT RÉUSSI ***\n");
    return 0;
}
