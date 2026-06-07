/*
 * events.h - Gestion des evenements GFA Basic 3.5
 * ================================================
 * Implémente EVERY, AFTER, ON BREAK, ON ERROR, ON MENU et
 * la boucle d'evenements AES (EVNT_*).
 *
 * Reference : cahier-des-charges-gfabasic.md, sections 8.19, 11
 */

#ifndef GFA_EVENTS_H
#define GFA_EVENTS_H

#include "os_layer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Types d'evenements                                                 */
/* ------------------------------------------------------------------ */

typedef enum {
    GFA_EVENT_NONE = 0,
    GFA_EVENT_EVERY,      /* Appel periodique (ticks)        */
    GFA_EVENT_AFTER,      /* Appel differe unique            */
    GFA_EVENT_ON_BREAK,   /* Interruption clavier (Ctrl+C)   */
    GFA_EVENT_ON_ERROR,   /* Erreur d'execution              */
    GFA_EVENT_ON_MENU,    /* Selection menu GEM              */
    GFA_EVENT_TIMER,      /* Minuteur AES (EVNT_TIMER)       */
    GFA_EVENT_KEYBD,      /* Evenement clavier AES           */
    GFA_EVENT_MOUSE,      /* Evenement souris AES            */
    GFA_EVENT_MESSAGE,    /* Message AES (EVNT_MESAG)        */
    GFA_EVENT_BUTTON,     /* Clics boutons AES               */
    GFA_EVENT_DCLICK,     /* Double-clic AES                 */
    GFA_EVENT_COUNT
} gfa_event_type;

/* ------------------------------------------------------------------ */
/* Callback d'evenement                                               */
/* ------------------------------------------------------------------ */

/*
 * gfa_event_callback - Type de fonction appelee lors d'un evenement.
 * Retourne 0 pour continuer, non-zero pour arreter.
 */
typedef int (*gfa_event_callback)(gfa_event_type type, os_int32 data,
                                   void *user_data);

/* ------------------------------------------------------------------ */
/* API de gestion des evenements                                      */
/* ------------------------------------------------------------------ */

/*
 * gfa_events_init - Initialise le sous-systeme d'evenements.
 */
void gfa_events_init(void);

/*
 * gfa_events_shutdown - Arrete tous les evenements en cours.
 */
void gfa_events_shutdown(void);

/*
 * gfa_events_poll - Verifie et declenche les evenements arrivant
 * a echeance. A appeler regulierement dans la boucle principale.
 *
 * Retourne le nombre d'evenements declenches.
 */
int gfa_events_poll(void);

/*
 * gfa_events_register - Enregistre un callback pour un type d'evenement.
 * Retourne 0 si succes, -1 si erreur.
 */
int gfa_events_register(gfa_event_type type, gfa_event_callback callback,
                         void *user_data);

/*
 * gfa_events_unregister - Desactive le callback pour un type d'evenement.
 */
void gfa_events_unregister(gfa_event_type type);

/* ------------------------------------------------------------------ */
/* EVERY / AFTER                                                      */
/* ------------------------------------------------------------------ */

/*
 * gfa_every - Programme un appel periodique.
 * ticks : intervalle en 1/200emes de seconde
 * label : etiquette a appeler (index dans la table des etiquettes)
 *
 * Equivalent GFA : EVERY ticks GOSUB label
 */
int gfa_every(os_int32 ticks, int label_index);

/*
 * gfa_every_cont - Reactive les appels EVERY.
 * Equivalent GFA : EVERY CONT
 */
void gfa_every_cont(void);

/*
 * gfa_every_stop - Desactive les appels EVERY.
 * Equivalent GFA : EVERY STOP
 */
void gfa_every_stop(void);

/*
 * gfa_after - Programme un appel differe unique.
 * ticks : delai en 1/200emes de seconde
 * label : etiquette a appeler
 *
 * Equivalent GFA : AFTER ticks GOSUB label
 */
int gfa_after(os_int32 ticks, int label_index);

/*
 * gfa_after_cont - Reactive les appels AFTER.
 * Equivalent GFA : AFTER CONT
 */
void gfa_after_cont(void);

/*
 * gfa_after_stop - Desactive les appels AFTER.
 * Equivalent GFA : AFTER STOP
 */
void gfa_after_stop(void);

/* ------------------------------------------------------------------ */
/* ON BREAK / ON ERROR                                                */
/* ------------------------------------------------------------------ */

/*
 * gfa_on_break_gosub - Definit le gestionnaire d'interruption.
 * Equivalent GFA : ON BREAK GOSUB label
 */
void gfa_on_break_gosub(int label_index);

/*
 * gfa_on_break_cont - Continue apres une interruption.
 * Equivalent GFA : ON BREAK CONT
 */
void gfa_on_break_cont(void);

/*
 * gfa_on_error_gosub - Definit le gestionnaire d'erreurs.
 * Equivalent GFA : ON ERROR GOSUB label
 */
void gfa_on_error_gosub(int label_index);

/*
 * gfa_on_error_disable - Desactive le gestionnaire d'erreurs.
 */
void gfa_on_error_disable(void);

/*
 * gfa_error_raise - Declenche une erreur GFA.
 * error_code : code d'erreur GFA (0-100)
 * Equivalent GFA : ERROR n
 */
void gfa_error_raise(int error_code);

/*
 * gfa_error_get - Retourne le dernier code d'erreur.
 * Equivalent GFA : ERR
 */
int gfa_error_get(void);

/*
 * gfa_error_get_string - Retourne le message d'erreur.
 * Equivalent GFA : ERR$
 */
const char *gfa_error_get_string(int error_code);

/*
 * gfa_error_clear - Reinitialise l'etat d'erreur.
 */
void gfa_error_clear(void);

/* ------------------------------------------------------------------ */
/* ON MENU                                                            */
/* ------------------------------------------------------------------ */

/*
 * gfa_on_menu - Active la gestion des menus GEM.
 * Equivalent GFA : ON MENU
 */
void gfa_on_menu(void);

/*
 * gfa_on_menu_gosub - Associe une routine a un element de menu.
 * Equivalent GFA : ON MENU GOSUB n
 */
void gfa_on_menu_gosub(int menu_index, int label_index);

/*
 * gfa_on_menu_button - Associe un numero de bouton au menu.
 * Equivalent GFA : ON MENU BUTTON n
 */
void gfa_on_menu_button(int button_index, int label_index);

/*
 * gfa_on_menu_key_gosub - Routine pour raccourci clavier menu.
 * Equivalent GFA : ON MENU KEY GOSUB n
 */
void gfa_on_menu_key_gosub(int key_index, int label_index);

/*
 * gfa_on_menu_message_gosub - Routine pour message AES.
 * Equivalent GFA : ON MENU MESSAGE GOSUB
 */
void gfa_on_menu_message_gosub(int label_index);

/* ------------------------------------------------------------------ */
/* Break (Ctrl+C)                                                     */
/* ------------------------------------------------------------------ */

/*
 * gfa_break_check - Verifie si l'utilisateur a demande l'arret
 * (Ctrl+Shift+Alt sur Atari ST, Ctrl+C sur systemes modernes).
 * Retourne OS_TRUE si break demande, OS_FALSE sinon.
 */
int gfa_break_check(void);

/*
 * gfa_break_enable - Active la detection du break.
 */
void gfa_break_enable(void);

/*
 * gfa_break_disable - Desactive la detection du break.
 */
void gfa_break_disable(void);

#ifdef __cplusplus
}
#endif

#endif /* GFA_EVENTS_H */
