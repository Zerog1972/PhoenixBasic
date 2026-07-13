/*
 * events.c - Implementation de la gestion des evenements GFA
 * ==========================================================
 * Gerer les evenements asynchrones : EVERY, AFTER, ON BREAK,
 * ON ERROR et ON MENU.
 *
 * Reference : cahier-des-charges-gfabasic.md, sections 8.19, 11
 */

#include "events.h"
#include <string.h>

/* Nombre max de callbacks par type */
#define GFA_EVENT_MAX_CALLBACKS 8

/* Etat d'un timer (EVERY/AFTER) */
typedef struct {
    int      active;
    os_int32 interval;      /* Intervalle en ticks (200 Hz) */
    os_int32 next_trigger;  /* Prochain declenchement (ticks absolus) */
    int      label_index;   /* Etiquette GOSUB associee */
    int      repeat;        /* OS_TRUE pour EVERY, OS_FALSE pour AFTER */
    int      paused;        /* OS_TRUE si en pause */
} gfa_timer_event;

/* Etat global du module */
static struct {
    int initialized;

    /* Callbacks par type d'evenement */
    gfa_event_callback callbacks[GFA_EVENT_COUNT];
    void              *callback_data[GFA_EVENT_COUNT];

    /* Timers EVERY / AFTER */
    gfa_timer_event every_timer;
    gfa_timer_event after_timer;

    /* Gestion du break */
    int break_enabled;
    int break_pending;

    /* Gestion des erreurs */
    int error_code;
    int error_label;
    int error_active;

    /* Menu */
    int          menu_active;
    int          menu_labels[32];   /* Max 32 elements de menu */
    int          menu_button_labels[8];
    int          menu_key_labels[8];
    int          menu_message_label;
} g_events;

/* ------------------------------------------------------------------ */
/* Messages d'erreur GFA                                              */
/* ------------------------------------------------------------------ */

static const char *g_error_messages[101];

static void init_error_messages(void)
{
    int i;
    for (i = 0; i <= 100; i++) {
        g_error_messages[i] = "Undefined error";
    }

    g_error_messages[0]  = "Division by zero";
    g_error_messages[1]  = "Overflow";
    g_error_messages[2]  = "Not Integer -2147483648 .. 2147483647";
    g_error_messages[3]  = "Not Byte 0 .. 255";
    g_error_messages[4]  = "Not Word -32768 .. 32767";
    g_error_messages[5]  = "Square root only for positive numbers";
    g_error_messages[6]  = "Logarithm only for numbers greater than zero";
    g_error_messages[8]  = "Out of memory";
    g_error_messages[10] = "String too long max. 32767 characters";
    g_error_messages[14] = "Array dimensioned twice";
    g_error_messages[15] = "Array not dimensioned";
    g_error_messages[16] = "Array index too large";
    g_error_messages[19] = "Procedure not found";
    g_error_messages[20] = "Label not found";
    g_error_messages[22] = "File already open";
    g_error_messages[23] = "File # wrong";
    g_error_messages[24] = "File not open";
    g_error_messages[26] = "End of file reached";
    g_error_messages[30] = "Merge - Not an ASCII file";
    g_error_messages[32] = "Syntax error program aborted";
    g_error_messages[34] = "Out of data";
    g_error_messages[37] = "Disk full";
    g_error_messages[42] = "Parameter missing";
    g_error_messages[48] = "Open R Record length wrong";
    g_error_messages[49] = "Too many R-files (max 31)";
    g_error_messages[50] = "Not an R-File";
    g_error_messages[69] = "ENDFUNC without RETURN";
    g_error_messages[98] = "Command only available on STE";
    g_error_messages[100]= "GFA BASIC Version";
}

/* ------------------------------------------------------------------ */
/* Initialisation / Arret                                             */
/* ------------------------------------------------------------------ */

void gfa_events_init(void)
{
    int i;

    os_mem_set(&g_events, 0, sizeof(g_events));

    for (i = 0; i < GFA_EVENT_COUNT; i++) {
        g_events.callbacks[i]     = NULL;
        g_events.callback_data[i] = NULL;
    }

    for (i = 0; i < 32; i++) {
        g_events.menu_labels[i]       = -1;
        if (i < 8) {
            g_events.menu_button_labels[i] = -1;
            g_events.menu_key_labels[i]    = -1;
        }
    }
    g_events.menu_message_label = -1;

    g_events.break_enabled = 1;
    g_events.break_pending = 0;
    g_events.error_code    = 0;
    g_events.error_label   = -1;
    g_events.error_active  = 0;
    g_events.menu_active   = 0;

    g_events.every_timer.active = 0;
    g_events.after_timer.active = 0;

    g_events.initialized = 1;

    init_error_messages();
}

void gfa_events_shutdown(void)
{
    g_events.initialized = 0;
}

/* ------------------------------------------------------------------ */
/* Polling                                                            */
/* ------------------------------------------------------------------ */

int gfa_events_poll(void)
{
    int triggered;
    os_int32 now_ticks;

    if (!g_events.initialized) return 0;

    triggered = 0;
    now_ticks = os_time_ticks();

    /* Verifier le timer EVERY */
    if (g_events.every_timer.active && !g_events.every_timer.paused) {
        if (now_ticks >= g_events.every_timer.next_trigger) {
            if (g_events.callbacks[GFA_EVENT_EVERY] != NULL) {
                g_events.callbacks[GFA_EVENT_EVERY](
                    GFA_EVENT_EVERY,
                    g_events.every_timer.label_index,
                    g_events.callback_data[GFA_EVENT_EVERY]);
                triggered++;
            }
            /* Reprogrammer le prochain declenchement */
            g_events.every_timer.next_trigger =
                now_ticks + g_events.every_timer.interval;
        }
    }

    /* Verifier le timer AFTER */
    if (g_events.after_timer.active && !g_events.after_timer.paused) {
        if (now_ticks >= g_events.after_timer.next_trigger) {
            if (g_events.callbacks[GFA_EVENT_AFTER] != NULL) {
                g_events.callbacks[GFA_EVENT_AFTER](
                    GFA_EVENT_AFTER,
                    g_events.after_timer.label_index,
                    g_events.callback_data[GFA_EVENT_AFTER]);
                triggered++;
            }
            g_events.after_timer.active = 0;  /* AFTER ne se declenche qu'une fois */
        }
    }

    /* Verifier le break */
    if (g_events.break_enabled && gfa_break_check()) {
        g_events.break_pending = 1;
        if (g_events.callbacks[GFA_EVENT_ON_BREAK] != NULL) {
            g_events.callbacks[GFA_EVENT_ON_BREAK](
                GFA_EVENT_ON_BREAK, 0,
                g_events.callback_data[GFA_EVENT_ON_BREAK]);
            triggered++;
        }
        g_events.break_pending = 0;
    }

    return triggered;
}

/* ------------------------------------------------------------------ */
/* Callbacks                                                          */
/* ------------------------------------------------------------------ */

int gfa_events_register(gfa_event_type type, gfa_event_callback callback,
                         void *user_data)
{
    if (type < 0 || type >= GFA_EVENT_COUNT) return -1;

    g_events.callbacks[type]     = callback;
    g_events.callback_data[type] = user_data;
    return 0;
}

void gfa_events_unregister(gfa_event_type type)
{
    if (type >= 0 && type < GFA_EVENT_COUNT) {
        g_events.callbacks[type]     = NULL;
        g_events.callback_data[type] = NULL;
    }
}

/* ------------------------------------------------------------------ */
/* EVERY / AFTER                                                      */
/* ------------------------------------------------------------------ */

int gfa_every(os_int32 ticks, int label_index)
{
    if (ticks <= 0) {
        gfa_every_stop();
        return 0;
    }

    g_events.every_timer.active       = 1;
    g_events.every_timer.interval     = ticks;
    g_events.every_timer.next_trigger = os_time_ticks() + ticks;
    g_events.every_timer.label_index  = label_index;
    g_events.every_timer.repeat       = OS_TRUE;
    g_events.every_timer.paused       = 0;

    return 0;
}

void gfa_every_cont(void)
{
    g_events.every_timer.paused = 0;
}

void gfa_every_stop(void)
{
    g_events.every_timer.active = 0;
}

int gfa_after(os_int32 ticks, int label_index)
{
    if (ticks <= 0) {
        gfa_after_stop();
        return 0;
    }

    g_events.after_timer.active       = 1;
    g_events.after_timer.interval     = ticks;
    g_events.after_timer.next_trigger = os_time_ticks() + ticks;
    g_events.after_timer.label_index  = label_index;
    g_events.after_timer.repeat       = OS_FALSE;
    g_events.after_timer.paused       = 0;

    return 0;
}

void gfa_after_cont(void)
{
    g_events.after_timer.paused = 0;
}

void gfa_after_stop(void)
{
    g_events.after_timer.active = 0;
}

/* ------------------------------------------------------------------ */
/* ON BREAK / ON ERROR                                                */
/* ------------------------------------------------------------------ */

void gfa_on_break_gosub(int label_index)
{
    /* Store the break handler label index */
    g_events.break_pending = 0;
    /* The runtime will use this label when a break occurs */
    if (g_events.callbacks[GFA_EVENT_ON_BREAK] != NULL) {
        g_events.callbacks[GFA_EVENT_ON_BREAK](
            GFA_EVENT_ON_BREAK,
            (os_int32)label_index,
            g_events.callback_data[GFA_EVENT_ON_BREAK]);
    }
}

void gfa_on_break_cont(void)
{
    g_events.break_pending = 0;
}

void gfa_on_error_gosub(int label_index)
{
    g_events.error_label  = label_index;
    g_events.error_active = 1;
}

void gfa_on_error_disable(void)
{
    g_events.error_label  = -1;
    g_events.error_active = 0;
}

void gfa_error_raise(int error_code)
{
    g_events.error_code = error_code;

    if (g_events.error_active && g_events.error_label >= 0) {
        /* Le runtime appellera le GOSUB associe */
        if (g_events.callbacks[GFA_EVENT_ON_ERROR] != NULL) {
            g_events.callbacks[GFA_EVENT_ON_ERROR](
                GFA_EVENT_ON_ERROR,
                (os_int32)error_code,
                g_events.callback_data[GFA_EVENT_ON_ERROR]);
        }
    }
}

int gfa_error_get(void)
{
    return g_events.error_code;
}

const char *gfa_error_get_string(int error_code)
{
    if (error_code < 0 || error_code > 100) {
        return "Undefined error";
    }
    return g_error_messages[error_code];
}

void gfa_error_clear(void)
{
    g_events.error_code = 0;
}

/* ------------------------------------------------------------------ */
/* ON MENU                                                            */
/* ------------------------------------------------------------------ */

void gfa_on_menu(void)
{
    g_events.menu_active = 1;
}

void gfa_on_menu_gosub(int menu_index, int label_index)
{
    if (menu_index >= 0 && menu_index < 32) {
        g_events.menu_labels[menu_index] = label_index;
    }
}

void gfa_on_menu_button(int button_index, int label_index)
{
    if (button_index >= 0 && button_index < 8) {
        g_events.menu_button_labels[button_index] = label_index;
    }
}

void gfa_on_menu_key_gosub(int key_index, int label_index)
{
    if (key_index >= 0 && key_index < 8) {
        g_events.menu_key_labels[key_index] = label_index;
    }
}

void gfa_on_menu_message_gosub(int label_index)
{
    g_events.menu_message_label = label_index;
}

/* ------------------------------------------------------------------ */
/* Break                                                              */
/* ------------------------------------------------------------------ */

int gfa_break_check(void)
{
    /*
     * Sur Atari ST : Ctrl+Shift+Alt simultanes.
     * Sur systemes modernes : Ctrl+C.
     * Pour le moment, retourne toujours OS_FALSE (pas de break).
     * L'implementation reelle utilisera os_con_input_key() ou
     * un gestionnaire de signal (SIGINT).
     */
    return OS_FALSE;
}

void gfa_break_enable(void)
{
    g_events.break_enabled = 1;
}

void gfa_break_disable(void)
{
    g_events.break_enabled = 0;
}
