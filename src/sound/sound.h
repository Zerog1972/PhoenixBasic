/*
 * sound.h - Generation sonore GFA Basic 3.5 (YM-2149)
 * =====================================================
 * Emule le chip sonore YM-2149 (3 canaux + bruit).
 * Implémente SOUND, BEEP, WAVE, DMASOUND, DMACONTROL.
 *
 * Reference : cahier-des-charges-gfabasic.md, sections 8.18, 8.25
 */

#ifndef GFA_SOUND_H
#define GFA_SOUND_H

#include "os_layer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Canaux sonores (YM-2149) */
#define GFA_SOUND_CHANNEL_A  0
#define GFA_SOUND_CHANNEL_B  1
#define GFA_SOUND_CHANNEL_C  2

/* Masques de canaux pour WAVE */
#define GFA_WAVE_CH1_TONE    1
#define GFA_WAVE_CH2_TONE    2
#define GFA_WAVE_CH3_TONE    4
#define GFA_WAVE_CH1_NOISE   8
#define GFA_WAVE_CH2_NOISE  16
#define GFA_WAVE_CH3_NOISE  32

/* ------------------------------------------------------------------ */
/* API Son                                                            */
/* ------------------------------------------------------------------ */

/*
 * gfa_sound_init - Initialise le sous-systeme audio.
 * Retourne 0 si succes.
 */
int gfa_sound_init(void);

/*
 * gfa_sound_shutdown - Arrete tout son et libere les ressources.
 */
void gfa_sound_shutdown(void);

/*
 * gfa_beep - Emet un bip systeme.
 * Equivalent GFA : BEEP
 */
void gfa_beep(void);

/*
 * gfa_sound - Joue une note sur un canal.
 * channel  : 0-2 (canal A/B/C du YM-2149)
 * freq     : frequence (0-65535, 0 = silence)
 * duration : duree en 1/50emes de seconde (0 = continu)
 * volume   : volume 0-15 (0 = silence, 15 = max)
 * envelope : numero d'enveloppe (0-15)
 *
 * Equivalent GFA : SOUND channel, freq, duration, volume, envelope
 */
void gfa_sound(int channel, int freq, int duration, int volume,
               int envelope);

/*
 * gfa_sound_stop - Arrete le son sur un canal specifique.
 */
void gfa_sound_stop(int channel);

/*
 * gfa_sound_stop_all - Arrete tous les canaux.
 */
void gfa_sound_stop_all(void);

/*
 * gfa_wave - Modulation sonore complexe.
 * voice    : masque des canaux et bruit (voir GFA_WAVE_*)
 * envelope : canaux affectes par l'enveloppe
 * form     : forme d'enveloppe (0-15)
 * period   : periode de l'enveloppe * 125000
 * delay    : delai en 1/59emes de seconde
 *
 * Equivalent GFA : WAVE voice, envelope, form, period, delay
 */
void gfa_wave(int voice, int envelope, int form, int period, int delay);

/*
 * gfa_dma_control - Controle du son DMA (STE uniquement).
 * ctrl : 0=stop, 1=jouer une fois, 2=boucle
 * Equivalent GFA : DMACONTROL ctrl
 */
void gfa_dma_control(int ctrl);

/*
 * gfa_dma_sound - Sortie son DMA echantillonne (STE uniquement).
 * start : adresse debut echantillon
 * end   : adresse fin echantillon
 * rate  : frequence (0=6.25, 1=12.5, 2=25, 3=50 kHz)
 * ctrl  : mode de controle DMA
 * Equivalent GFA : DMASOUND debut, fin, freq[, ctrl]
 */
void gfa_dma_sound(long start, long end, int rate, int ctrl);

/*
 * gfa_sound_get_channel_freq - Retourne la frequence courante d'un canal.
 */
int gfa_sound_get_channel_freq(int channel);

/*
 * gfa_sound_get_channel_volume - Retourne le volume courant d'un canal.
 */
int gfa_sound_get_channel_volume(int channel);

/*
 * gfa_sound_poll - Met a jour l'etat du son (appeler periodiquement).
 * Necessaire pour gerer les durees de notes.
 */
void gfa_sound_poll(void);

#ifdef __cplusplus
}
#endif

#endif /* GFA_SOUND_H */
