/*
 * gfx.c - Implementation graphique C89 pur (sans SDL2)
 * =====================================================
 * Primitives Atari ST emulees dans un framebuffer memoire.
 * Le rendu est effectue dans le terminal via des sequences
 * ANSI (blocs 8x16 pixels -> cellules 80x24).
 *
 * Aucun composant externe : uniquement libc C89.
 */

#include "gfx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Constantes internes                                                */
/* ------------------------------------------------------------------ */

#define GFX_WIDTH   640
#define GFX_HEIGHT  400

/* Cellules terminal : 640/8 = 80 colonnes, 384/16 = 24 lignes */
#define GFX_TERM_COLS  80
#define GFX_TERM_ROWS  24
#define GFX_CELL_W     8
#define GFX_CELL_H     16

/* ------------------------------------------------------------------ */
/* Etat global                                                        */
/* ------------------------------------------------------------------ */

static unsigned char *g_fb = NULL;   /* Framebuffer : index palette 0-15 */
static int g_width  = 0;
static int g_height = 0;
static int g_fg_color = 1;   /* 1 = blanc Atari ST */
static int g_bg_color = 0;   /* 0 = noir Atari ST   */

/* Palette Atari ST 16 couleurs */
static unsigned long st_palette[16] = {
    0xFFFFFF,  /*  0 White       */
    0x000000,  /*  1 Black       */
    0xFF0000,  /*  2 Red         */
    0x00FF00,  /*  3 Green       */
    0x0000FF,  /*  4 Blue        */
    0x00FFFF,  /*  5 Cyan        */
    0xFFFF00,  /*  6 Yellow      */
    0xFF00FF,  /*  7 Magenta     */
    0x808080,  /*  8 Grey        */
    0xC0C0C0,  /*  9 Light Grey  */
    0x800000,  /* 10 Dark Red    */
    0x008000,  /* 11 Dark Green  */
    0x000080,  /* 12 Dark Blue   */
    0x008080,  /* 13 Dark Cyan   */
    0x808000,  /* 14 Brown       */
    0x800080   /* 15 Purple      */
};

/* ------------------------------------------------------------------ */
/* Helpers prives                                                    */
/* ------------------------------------------------------------------ */

static void put_pixel(int x, int y, int color)
{
    if (x < 0 || x >= g_width || y < 0 || y >= g_height) return;
    if (color < 0) color = 0;
    if (color > 15) color = 15;
    g_fb[(size_t)y * (size_t)g_width + (size_t)x] = (unsigned char)color;
}

static void draw_hline(int x1, int x2, int y, int color)
{
    int x;
    if (y < 0 || y >= g_height) return;
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (x1 < 0) x1 = 0;
    if (x2 >= g_width) x2 = g_width - 1;
    for (x = x1; x <= x2; x++) {
        g_fb[(size_t)y * (size_t)g_width + (size_t)x] = (unsigned char)color;
    }
}

static void draw_vline(int y1, int y2, int x, int color)
{
    int y;
    if (x < 0 || x >= g_width) return;
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
    if (y1 < 0) y1 = 0;
    if (y2 >= g_height) y2 = g_height - 1;
    for (y = y1; y <= y2; y++) {
        g_fb[(size_t)y * (size_t)g_width + (size_t)x] = (unsigned char)color;
    }
}

/* Convertit un index palette Atari ST en code ANSI fond */
static int ansi_bg_code(int idx)
{
    if (idx < 0) idx = 0;
    if (idx > 15) idx = 15;
    if (idx < 8)  return 40 + idx;      /* couleurs normales */
    return 100 + (idx - 8);             /* couleurs brillantes */
}

/* ------------------------------------------------------------------ */
/* API publique                                                       */
/* ------------------------------------------------------------------ */

unsigned long gfx_st_color(int index)
{
    if (index < 0 || index > 15) return 0xFFFFFF;
    return st_palette[index];
}

int gfx_init(int width, int height)
{
    size_t px;
    size_t fb_size;

    if (g_fb != NULL) gfx_shutdown();

    if (width  <= 0) width  = GFX_WIDTH;
    if (height <= 0) height = GFX_HEIGHT;

    g_width  = width;
    g_height = height;
    fb_size  = (size_t)width * (size_t)height;
    g_fb = (unsigned char *)malloc(fb_size);
    if (g_fb == NULL) {
        g_width = 0; g_height = 0;
        return -1;
    }

    for (px = 0; px < fb_size; px++) g_fb[px] = (unsigned char)g_bg_color;

    /* Rendu du terminal : curseur en haut a gauche, pas de scroll */
    printf("\033[2J\033[H");
    fflush(stdout);

    return 0;
}

void gfx_shutdown(void)
{
    if (g_fb != NULL) {
        free(g_fb);
        g_fb = NULL;
    }
    g_width  = 0;
    g_height = 0;
}

void gfx_update(void)
{
    int row, col, x, y;
    int term_h;

    if (g_fb == NULL) return;

    /* Hauteur visible : limitee pour eviter le scroll du terminal */
    term_h = g_height / GFX_CELL_H;
    if (term_h > GFX_TERM_ROWS) term_h = GFX_TERM_ROWS;
    if (term_h <= 0) term_h = 1;

    /* Curseur en haut a gauche */
    printf("\033[H");
    fflush(stdout);

    for (row = 0; row < term_h; row++) {
        int prev_bg = -1;
        int prev_fg = -1;
        for (col = 0; col < GFX_TERM_COLS; col++) {
            /* Couleurs dominantes dans la cellule 8x16 */
            int counts[16];
            int best = 0, best_count = 0;
            int c;
            int x0 = col * GFX_CELL_W;
            int y0 = row * GFX_CELL_H;
            int x1 = x0 + GFX_CELL_W;
            int y1 = y0 + GFX_CELL_H;

            for (c = 0; c < 16; c++) counts[c] = 0;

            for (y = y0; y < y1 && y < g_height; y++) {
                for (x = x0; x < x1 && x < g_width; x++) {
                    int idx = g_fb[(size_t)y * (size_t)g_width + (size_t)x];
                    counts[idx]++;
                }
            }

            for (c = 0; c < 16; c++) {
                if (counts[c] > best_count) {
                    best_count = counts[c];
                    best = c;
                }
            }

            /* Couleurs terminal : fg = contour de la cellule, bg = dominante.
               On dessine un espace sur fond de la couleur dominante. */
            {
                int bg = ansi_bg_code(best);
                int fg = ansi_bg_code(best);
                (void)fg;

                if (bg != prev_bg) {
                    printf("\033[0;%dm", bg);
                    prev_bg = bg;
                }
                putchar(' ');
            }
            (void)prev_fg;
        }
        printf("\033[0m");
        if (row < term_h - 1) putchar('\n');
    }
    fflush(stdout);
}

void gfx_clear(void)
{
    size_t px;
    size_t fb_size;
    if (g_fb == NULL) return;
    fb_size = (size_t)g_width * (size_t)g_height;
    for (px = 0; px < fb_size; px++)
        g_fb[px] = (unsigned char)g_bg_color;
}

void gfx_color(int fg, int bg)
{
    g_fg_color = fg;
    g_bg_color = bg;
}

void gfx_line(int x1, int y1, int x2, int y2)
{
    /* Algorithme de Bresenham */
    int dx, dy, sx, sy, err, e2;

    if (g_fb == NULL) return;

    dx = x2 - x1;
    if (dx < 0) dx = -dx;
    dy = y2 - y1;
    if (dy < 0) dy = -dy;
    sx = (x1 < x2) ? 1 : -1;
    sy = (y1 < y2) ? 1 : -1;
    err = dx - dy;

    for (;;) {
        put_pixel(x1, y1, g_fg_color);
        if (x1 == x2 && y1 == y2) break;
        e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 <  dx) { err += dx; y1 += sy; }
    }
}

void gfx_box(int x1, int y1, int x2, int y2)
{
    if (g_fb == NULL) return;
    draw_hline(x1, x2, y1, g_fg_color);
    draw_hline(x1, x2, y2, g_fg_color);
    draw_vline(y1, y2, x1, g_fg_color);
    draw_vline(y1, y2, x2, g_fg_color);
}

void gfx_fill_box(int x1, int y1, int x2, int y2)
{
    int y;
    if (g_fb == NULL) return;
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
    for (y = y1; y <= y2; y++)
        draw_hline(x1, x2, y, g_fg_color);
}

void gfx_circle(int x, int y, int r)
{
    int px, py;
    if (g_fb == NULL || r <= 0) return;

    px = r - 1; py = 0;
    while (px >= py) {
        put_pixel(x + px, y + py, g_fg_color);
        put_pixel(x + py, y + px, g_fg_color);
        put_pixel(x - py, y + px, g_fg_color);
        put_pixel(x - px, y + py, g_fg_color);
        put_pixel(x - px, y - py, g_fg_color);
        put_pixel(x - py, y - px, g_fg_color);
        put_pixel(x + py, y - px, g_fg_color);
        put_pixel(x + px, y - py, g_fg_color);
        py++;
        if ((px * px + py * py - r * r) > 0) px--;
    }
}

void gfx_fill_circle(int x, int y, int r)
{
    int px, py, i;
    if (g_fb == NULL || r <= 0) return;

    px = r; py = 0;
    while (px >= py) {
        for (i = x - px; i <= x + px; i++) {
            put_pixel(i, y + py, g_fg_color);
            put_pixel(i, y - py, g_fg_color);
        }
        for (i = x - py; i <= x + py; i++) {
            put_pixel(i, y + px, g_fg_color);
            put_pixel(i, y - px, g_fg_color);
        }
        py++;
        if ((px * px + py * py - r * r) > 0) px--;
    }
}