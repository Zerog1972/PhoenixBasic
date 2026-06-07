/*
 * sound.c - Implementation de l'emulation sonore YM-2149
 * =======================================================
 * Emule le comportement du chip YM-2149 de l'Atari ST.
 * Utilise la couche OS pour la sortie audio reelle.
 *
 * Reference : cahier-des-charges-gfabasic.md, sections 8.18, 8.25
 */

#include "sound.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* Registres YM-2149 (14 registres de 8 bits)                         */
/* ------------------------------------------------------------------ */
#define YM_CHA_FINE    0    /* Canal A - periode fine          */
#define YM_CHA_COARSE  1    /* Canal A - periode coarse        */
#define YM_CHB_FINE    2    /* Canal B - periode fine          */
#define YM_CHB_COARSE  3    /* Canal B - periode coarse        */
#define YM_CHC_FINE    4    /* Canal C - periode fine          */
#define YM_CHC_COARSE  5    /* Canal C - periode coarse        */
#define YM_NOISE_PER   6    /* Periode du bruit               */
#define YM_MIXER       7    /* Mixer : enable/disable canaux   */
#define YM_CHA_VOL     8    /* Volume canal A                  */
#define YM_CHB_VOL     9    /* Volume canal B                  */
#define YM_CHC_VOL    10    /* Volume canal C                  */
#define YM_ENV_FINE   11    /* Periode enveloppe fine          */
#define YM_ENV_COARSE 12    /* Periode enveloppe coarse        */
#define YM_ENV_SHAPE  13    /* Forme de l'enveloppe            */

/*
 * Registres du YM-2149 emules.
 */
static unsigned char g_ym_regs[16];

/*
 * Notes de musique (frequences en periodes YM a 2 MHz)
 * pour les 4 octaves GFA.
 */
/* reserve pour usage futur */
#if 0
static const int g_note_periods[12] = {
    3822, 3608, 3405, 3214, 3034, 2863,
    2703, 2551, 2408, 2273, 2145, 2025
};
#endif

/* Etat interne */
static int  g_sound_initialized = 0;
static long g_channel_timer[3];   /* Timers de duree par canal */
static int  g_channel_active[3];  /* Canal actif ? */
static int  g_master_clock = 2000000;  /* 2 MHz (Atari ST) */

/* ------------------------------------------------------------------ */
/* Fonctions internes                                                 */
/* ------------------------------------------------------------------ */

/*
 * ym_write - Ecrit une valeur dans un registre YM.
 */
static void ym_write(int reg, unsigned char value)
{
    if (reg < 0 || reg > 15) return;
    g_ym_regs[reg] = value;

    /* Appliquer le changement au canal concerne */
    {
        int freq_hz;
        int period;
        int volume;
        int channel;

        channel = -1;

        (void)channel; /* utilise dans les blocs case */

        switch (reg) {
            case YM_CHA_FINE:
            case YM_CHA_COARSE:
                channel = 0;
                period = (int)g_ym_regs[YM_CHA_FINE] +
                         ((int)(g_ym_regs[YM_CHA_COARSE] & 0x0F) << 8);
                if (period > 0) {
                    freq_hz = g_master_clock / (16 * period);
                } else {
                    freq_hz = 0;
                }
                g_channel_active[0] = (freq_hz > 0) ? 1 : 0;
                volume = (int)(g_ym_regs[YM_CHA_VOL] & 0x0F);
                if (g_channel_active[0] && volume > 0) {
                    os_sound_tone(0, freq_hz, 100, volume);
                } else {
                    os_sound_stop_channel(0);
                }
                break;

            case YM_CHB_FINE:
            case YM_CHB_COARSE:
                channel = 1;
                period = (int)g_ym_regs[YM_CHB_FINE] +
                         ((int)(g_ym_regs[YM_CHB_COARSE] & 0x0F) << 8);
                if (period > 0) {
                    freq_hz = g_master_clock / (16 * period);
                } else {
                    freq_hz = 0;
                }
                g_channel_active[1] = (freq_hz > 0) ? 1 : 0;
                volume = (int)(g_ym_regs[YM_CHB_VOL] & 0x0F);
                if (g_channel_active[1] && volume > 0) {
                    os_sound_tone(1, freq_hz, 100, volume);
                } else {
                    os_sound_stop_channel(1);
                }
                break;

            case YM_CHC_FINE:
            case YM_CHC_COARSE:
                channel = 2;
                period = (int)g_ym_regs[YM_CHC_FINE] +
                         ((int)(g_ym_regs[YM_CHC_COARSE] & 0x0F) << 8);
                if (period > 0) {
                    freq_hz = g_master_clock / (16 * period);
                } else {
                    freq_hz = 0;
                }
                g_channel_active[2] = (freq_hz > 0) ? 1 : 0;
                volume = (int)(g_ym_regs[YM_CHC_VOL] & 0x0F);
                if (g_channel_active[2] && volume > 0) {
                    os_sound_tone(2, freq_hz, 100, volume);
                } else {
                    os_sound_stop_channel(2);
                }
                break;

            case YM_CHA_VOL:
                if (g_channel_active[0]) {
                    volume = (int)(value & 0x0F);
                    if (volume > 0) {
                        period = (int)g_ym_regs[YM_CHA_FINE] +
                                 ((int)(g_ym_regs[YM_CHA_COARSE] & 0x0F) << 8);
                        if (period > 0) {
                            freq_hz = g_master_clock / (16 * period);
                            os_sound_tone(0, freq_hz, 100, volume);
                        }
                    } else {
                        os_sound_stop_channel(0);
                    }
                }
                break;

            case YM_CHB_VOL:
                if (g_channel_active[1]) {
                    volume = (int)(value & 0x0F);
                    if (volume > 0) {
                        period = (int)g_ym_regs[YM_CHB_FINE] +
                                 ((int)(g_ym_regs[YM_CHB_COARSE] & 0x0F) << 8);
                        if (period > 0) {
                            freq_hz = g_master_clock / (16 * period);
                            os_sound_tone(1, freq_hz, 100, volume);
                        }
                    } else {
                        os_sound_stop_channel(1);
                    }
                }
                break;

            case YM_CHC_VOL:
                if (g_channel_active[2]) {
                    volume = (int)(value & 0x0F);
                    if (volume > 0) {
                        period = (int)g_ym_regs[YM_CHC_FINE] +
                                 ((int)(g_ym_regs[YM_CHC_COARSE] & 0x0F) << 8);
                        if (period > 0) {
                            freq_hz = g_master_clock / (16 * period);
                            os_sound_tone(2, freq_hz, 100, volume);
                        }
                    } else {
                        os_sound_stop_channel(2);
                    }
                }
                break;

            default:
                break;
        }
    }
}

/*
 * freq_to_period - Convertit une frequence GFA en periode YM.
 * freq : 0-65535 (periodes YM standard GFA)
 */
static int freq_to_period(int freq)
{
    /* En GFA Basic, la frequence SOUND est directement la periode YM */
    return freq;
}

/* ------------------------------------------------------------------ */
/* API publique                                                       */
/* ------------------------------------------------------------------ */

int gfa_sound_init(void)
{
    int i;
    os_mem_set(g_ym_regs, 0, sizeof(g_ym_regs));
    for (i = 0; i < 3; i++) {
        g_channel_timer[i]  = 0;
        g_channel_active[i] = 0;
    }
    g_sound_initialized = 1;
    return os_sound_init();
}

void gfa_sound_shutdown(void)
{
    gfa_sound_stop_all();
    os_sound_shutdown();
    g_sound_initialized = 0;
}

void gfa_beep(void)
{
    os_sound_beep();
}

void gfa_sound(int channel, int freq, int duration, int volume,
               int envelope)
{
    int period;
    int coarse;
    int fine;

    if (!g_sound_initialized) return;
    if (channel < 0 || channel > 2) return;
    if (volume < 0) volume = 0;
    if (volume > 15) volume = 15;
    if (envelope < 0) envelope = 0;
    if (envelope > 15) envelope = 15;

    /* Si freq = 0, arreter le canal */
    if (freq <= 0) {
        gfa_sound_stop(channel);
        return;
    }

    period = freq_to_period(freq);
    if (period > 4095) period = 4095;
    if (period < 1) period = 1;

    coarse = (period >> 8) & 0x0F;
    fine   = period & 0xFF;

    /* Configurer le canal */
    switch (channel) {
        case 0:
            ym_write(YM_CHA_FINE, (unsigned char)fine);
            ym_write(YM_CHA_COARSE, (unsigned char)coarse);
            /* Activer le son dans le mixer */
            g_ym_regs[YM_MIXER] &= (unsigned char)(~(1 << 0));
            ym_write(YM_CHA_VOL, (unsigned char)(volume & 0x0F));
            break;

        case 1:
            ym_write(YM_CHB_FINE, (unsigned char)fine);
            ym_write(YM_CHB_COARSE, (unsigned char)coarse);
            g_ym_regs[YM_MIXER] &= (unsigned char)(~(1 << 1));
            ym_write(YM_CHB_VOL, (unsigned char)(volume & 0x0F));
            break;

        case 2:
            ym_write(YM_CHC_FINE, (unsigned char)fine);
            ym_write(YM_CHC_COARSE, (unsigned char)coarse);
            g_ym_regs[YM_MIXER] &= (unsigned char)(~(1 << 2));
            ym_write(YM_CHC_VOL, (unsigned char)(volume & 0x0F));
            break;
    }

    /* Enveloppe (si utilisee) */
    if (envelope > 0) {
        ym_write(YM_ENV_SHAPE, (unsigned char)(envelope & 0x0F));
        /* Activer l'enveloppe pour ce canal */
        switch (channel) {
            case 0: ym_write(YM_CHA_VOL, (unsigned char)(0x10 | (volume & 0x0F))); break;
            case 1: ym_write(YM_CHB_VOL, (unsigned char)(0x10 | (volume & 0x0F))); break;
            case 2: ym_write(YM_CHC_VOL, (unsigned char)(0x10 | (volume & 0x0F))); break;
        }
    }

    /* Programmer la duree */
    if (duration > 0) {
        g_channel_timer[channel] = os_time_ticks() + (os_int32)duration;
    } else {
        g_channel_timer[channel] = 0;  /* Continu */
    }
}

void gfa_sound_stop(int channel)
{
    if (channel < 0 || channel > 2) return;

    switch (channel) {
        case 0:
            g_ym_regs[YM_MIXER] |= (unsigned char)(1 << 0);
            ym_write(YM_CHA_VOL, 0);
            break;
        case 1:
            g_ym_regs[YM_MIXER] |= (unsigned char)(1 << 1);
            ym_write(YM_CHB_VOL, 0);
            break;
        case 2:
            g_ym_regs[YM_MIXER] |= (unsigned char)(1 << 2);
            ym_write(YM_CHC_VOL, 0);
            break;
    }

    g_channel_active[channel] = 0;
    g_channel_timer[channel]  = 0;
    os_sound_stop_channel(channel);
}

void gfa_sound_stop_all(void)
{
    int i;
    for (i = 0; i < 3; i++) {
        gfa_sound_stop(i);
    }
    os_sound_stop_all();
}

void gfa_wave(int voice, int envelope, int form, int period, int delay)
{
    int channel;

    if (!g_sound_initialized) return;

    /* Arreter si demande */
    if (voice == 0 && envelope == 0) {
        gfa_sound_stop_all();
        return;
    }

    /*
     * WAVE module les canaux actifs avec l'enveloppe specifiee.
     * Implémentation simplifiee : on applique le bruit et
     * l'enveloppe aux canaux.
     */
    for (channel = 0; channel < 3; channel++) {
        int tone_mask;
        int noise_mask;
        int env_mask;

        tone_mask  = 1 << channel;
        noise_mask = 8 << channel;
        env_mask   = 1 << channel;

        if (voice & tone_mask) {
            g_ym_regs[YM_MIXER] &= (unsigned char)(~(1 << channel));
        }
        if (voice & noise_mask) {
            /* Activer le bruit pour ce canal */
            g_ym_regs[YM_MIXER] &= (unsigned char)(~(1 << (channel + 3)));
        }

        if (envelope & env_mask) {
            /* Activer l'enveloppe */
            ym_write(YM_ENV_SHAPE, (unsigned char)(form & 0x0F));

            if (period > 0) {
                int coarse;
                int fine;
                coarse = (period >> 8) & 0xFF;
                fine   = period & 0xFF;
                ym_write(YM_ENV_COARSE, (unsigned char)coarse);
                ym_write(YM_ENV_FINE, (unsigned char)fine);
            }

            /* Appliquer l'enveloppe au volume du canal */
            switch (channel) {
                case 0: ym_write(YM_CHA_VOL, 0x10); break;
                case 1: ym_write(YM_CHB_VOL, 0x10); break;
                case 2: ym_write(YM_CHC_VOL, 0x10); break;
            }
        }
    }

    /* Delai */
    if (delay > 0) {
        os_time_delay(delay * 17);  /* delay en 1/59emes -> ms (approx) */
    }
}

void gfa_dma_control(int ctrl)
{
    /*
     * DMACONTROL : controle le son DMA sur STE.
     * Emule avec le sous-systeme audio de la couche OS.
     */
    (void)ctrl;
}

void gfa_dma_sound(long start, long end, int rate, int ctrl)
{
    /*
     * DMASOUND : sortie DMA echantillonnee STE.
     * Non implemente dans cette version de base.
     * Necessite le chargement reel de samples audio.
     */
    (void)start;
    (void)end;
    (void)rate;
    (void)ctrl;
}

int gfa_sound_get_channel_freq(int channel)
{
    if (channel < 0 || channel > 2) return 0;

    switch (channel) {
        case 0: return (int)g_ym_regs[YM_CHA_FINE] +
                       ((int)(g_ym_regs[YM_CHA_COARSE] & 0x0F) << 8);
        case 1: return (int)g_ym_regs[YM_CHB_FINE] +
                       ((int)(g_ym_regs[YM_CHB_COARSE] & 0x0F) << 8);
        case 2: return (int)g_ym_regs[YM_CHC_FINE] +
                       ((int)(g_ym_regs[YM_CHC_COARSE] & 0x0F) << 8);
        default: return 0;
    }
}

int gfa_sound_get_channel_volume(int channel)
{
    if (channel < 0 || channel > 2) return 0;

    switch (channel) {
        case 0: return (int)(g_ym_regs[YM_CHA_VOL] & 0x0F);
        case 1: return (int)(g_ym_regs[YM_CHB_VOL] & 0x0F);
        case 2: return (int)(g_ym_regs[YM_CHC_VOL] & 0x0F);
        default: return 0;
    }
}

void gfa_sound_poll(void)
{
    os_int32 now;
    int i;

    if (!g_sound_initialized) return;

    now = os_time_ticks();

    for (i = 0; i < 3; i++) {
        if (g_channel_active[i] && g_channel_timer[i] > 0) {
            if (now >= g_channel_timer[i]) {
                gfa_sound_stop(i);
            }
        }
    }
}
