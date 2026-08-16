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
#include <math.h>

/* ------------------------------------------------------------------ */
/* Constantes internes                                                */
/* ------------------------------------------------------------------ */

#ifdef GFA_TARGET_MINT
/* Atari ST : RAM limitee (1-4 Mo). Framebuffer reduit 320x200 (64 Ko),
   conforme au mode basse resolution ST. Le malloc de 640x400 (256 Ko)
   depassait la memoire heap mintlib disponible apres le BSS de 400 Ko. */
#define GFX_WIDTH   320
#define GFX_HEIGHT  200
#else
#define GFX_WIDTH   640
#define GFX_HEIGHT  400
#endif

/* Cellules terminal : 640/8 = 80 colonnes, 384/16 = 24 lignes */
#define GFX_TERM_COLS  80
#define GFX_TERM_ROWS  24
#define GFX_CELL_W     8
#define GFX_CELL_H     16

/* ------------------------------------------------------------------ */
/* Etat global                                                        */
/* ------------------------------------------------------------------ */

static unsigned char *g_fb = NULL;     /* Framebuffer : index palette 0-15 */
static unsigned char g_fb_static[GFX_WIDTH * GFX_HEIGHT];
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

/* ================================================================ */
/* WINDOW (x0,y0), (x1,y1) : mapping coordonnees logiques ->       */
/* physiquement (framebuffer). window off = identite.               */
/* ================================================================ */
static int g_win_on = 0;
static double g_win_x0, g_win_y0, g_win_x1, g_win_y1;

void gfx_window(int x0, int y0, int x1, int y1)
{
    g_win_x0 = (double)x0;
    g_win_y0 = (double)y0;
    g_win_x1 = (double)x1;
    g_win_y1 = (double)y1;
    g_win_on = 1;
}

void gfx_window_reset(void)
{
    g_win_on = 0;
}

static int win_map_x(int x)
{
    if (!g_win_on || g_win_x1 == g_win_x0) return x;
    return (int)((double)x - g_win_x0) * (double)g_width /
           (g_win_x1 - g_win_x0);
}

static int win_map_y(int y)
{
    if (!g_win_on || g_win_y1 == g_win_y0) return y;
    return (int)((double)y - g_win_y0) * (double)g_height /
           (g_win_y1 - g_win_y0);
}

static int win_scale(int v)
{
    double sx, sy;
    if (!g_win_on) return v;
    if (g_win_x1 == g_win_x0 || g_win_y1 == g_win_y0) return v;
    sx = (double)g_width / (g_win_x1 - g_win_x0);
    sy = (double)g_height / (g_win_y1 - g_win_y0);
    return (int)((double)v * (sx + sy) / 2.0);
}


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

    /*
     * Framebuffer STATIQUE dans le BSS : le malloc() de la mintlib
     * bouclait sur Atari ST (heap fragmentee/fatiguee par le gros
     * BSS du programme). Un tableau fixe evite tout appel heap.
     */
    if (fb_size > sizeof(g_fb_static)) {
        g_width = 0; g_height = 0;
        return -1;
    }
    g_fb = g_fb_static;

    for (px = 0; px < fb_size; px++) g_fb[px] = (unsigned char)g_bg_color;

#ifndef GFA_TARGET_MINT
    /*
     * Rendu du terminal hote : curseur en haut a gauche, pas de scroll.
     * Sur Atari ST, ces sequences ANSI ne sont pas gerees par la console
     * VT52 et bloquaient le programme (verifie sous Hatari/EmuTOS).
     */
    printf("\033[2J\033[H");
    fflush(stdout);
#endif

    return 0;
}

void gfx_shutdown(void)
{
    /* Le framebuffer est statique (BSS) : rien a liberer. */
    g_fb = NULL;
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
    x = win_map_x(x); y = win_map_y(y);
    r = win_scale(r);
    if (r <= 0) return;

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

/* ------------------------------------------------------------------ */
/* Couleurs et modes                                                   */
/* ------------------------------------------------------------------ */

int gfx_fg_color(void)
{
    return g_fg_color;
}

void gfx_set_fg(int color)
{
    if (color < 0) color = 0;
    if (color > 15) color = 15;
    g_fg_color = color;
}

void gfx_color_reg(int n, int val)
{
    /* SETCOLOR n, val : si val est un index palette 0-15, re-affecte
       l'entree n de la palette a la couleur val (emulation simple). */
    if (n < 0 || n > 15) return;
    if (val < 0 || val > 15) return;
    st_palette[n] = st_palette[val];
}

static int g_write_mode = 0;

void gfx_mode(int mode)
{
    if (mode < 0) mode = 0;
    if (mode > 15) mode = 15;
    g_write_mode = mode;
}

int gfx_mode_get(void)
{
    return g_write_mode;
}

/* ------------------------------------------------------------------ */
/* Clipping (ACLIP)                                                    */
/* ------------------------------------------------------------------ */

static int g_clip_on = 0;
static int g_clip_x1, g_clip_y1, g_clip_x2, g_clip_y2;

void gfx_clip(int x1, int y1, int x2, int y2)
{
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
    g_clip_x1 = x1; g_clip_y1 = y1;
    g_clip_x2 = x2; g_clip_y2 = y2;
    g_clip_on = 1;
}

void gfx_clip_reset(void)
{
    g_clip_on = 0;
}

static int in_clip(int x, int y)

{
    if (!g_clip_on) return 1;
    return (x >= g_clip_x1 && x <= g_clip_x2 &&
            y >= g_clip_y1 && y <= g_clip_y2);
}

/* ------------------------------------------------------------------ */
/* Primitives de base                                                  */
/* ------------------------------------------------------------------ */

void gfx_plot(int x, int y)
{
    if (g_fb == NULL) return;
    if (!in_clip(x, y)) return;
    if (g_write_mode == 3) {
        /* XOR : inverse le pixel */
        unsigned char c;
        if (x < 0 || x >= g_width || y < 0 || y >= g_height) return;
        c = g_fb[(size_t)y * (size_t)g_width + (size_t)x];
        g_fb[(size_t)y * (size_t)g_width + (size_t)x] =
            (unsigned char)(c ^ (unsigned char)g_fg_color);
    } else {
        put_pixel(x, y, g_fg_color);
    }
}

int gfx_get_pixel(int x, int y)
{
    if (g_fb == NULL) return 0;
    if (x < 0 || x >= g_width || y < 0 || y >= g_height) return 0;
    return g_fb[(size_t)y * (size_t)g_width + (size_t)x];
}

void gfx_hline(int y, int x1, int x2)
{
    if (g_fb == NULL) return;
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (y < 0 || y >= g_height) return;
    if (x1 < 0) x1 = 0;
    if (x2 >= g_width) x2 = g_width - 1;
    for (; x1 <= x2; x1++) {
        if (in_clip(x1, y))
            g_fb[(size_t)y * (size_t)g_width + (size_t)x1] =
                (unsigned char)g_fg_color;
    }
}

void gfx_ellipse(int x, int y, int rx, int ry, int fill)
{
    int px, py;
    double t;
    if (g_fb == NULL || rx <= 0 || ry <= 0) return;
    for (t = 0.0; t < 6.283185307; t += 0.02) {
        px = x + (int)(rx * cos(t));
        py = y + (int)(ry * sin(t));
        if (fill) {
            if (in_clip(px, py))
                g_fb[(size_t)py * (size_t)g_width + (size_t)px] =
                    (unsigned char)g_fg_color;
        } else {
            gfx_plot(px, py);
        }
    }
}

void gfx_polyline(int n, const int *xy)
{
    int i;
    if (g_fb == NULL || n < 2 || xy == NULL) return;
    for (i = 1; i < n; i++) {
        gfx_line(xy[(i - 1) * 2], xy[(i - 1) * 2 + 1],
                 xy[i * 2], xy[i * 2 + 1]);
    }
}

void gfx_polygon(int n, const int *xy, int fill)
{
    int i, y, y0, y1;
    int x_min, x_max;
    int xs[64];
    int cnt;
    int mx[128];
    const int *xy_orig;
    if (g_fb == NULL || n < 3 || xy == NULL) return;
    xy_orig = xy;
    if (n > 64) n = 64;
    for (i = 0; i < n; i++) {
        mx[i * 2]     = win_map_x(xy[i * 2]);
        mx[i * 2 + 1] = win_map_y(xy[i * 2 + 1]);
    }
    xy = mx;
    x_min = xy[0]; x_max = xy[0];
    y0 = xy[1]; y1 = xy[1];
    for (i = 1; i < n; i++) {
        if (xy[i * 2] < x_min) x_min = xy[i * 2];
        if (xy[i * 2] > x_max) x_max = xy[i * 2];
        if (xy[i * 2 + 1] < y0) y0 = xy[i * 2 + 1];
        if (xy[i * 2 + 1] > y1) y1 = xy[i * 2 + 1];
    }
    if (fill) {
        /* Scanline : intersections avec chaque ligne horizontale */
        if (y0 < 0) y0 = 0;
        if (y1 >= g_height) y1 = g_height - 1;
        if (x_min < 0) x_min = 0;
        if (x_max >= g_width) x_max = g_width - 1;
        for (y = y0; y <= y1; y++) {
            cnt = 0;
            for (i = 0; i < n; i++) {
                int j = (i + 1) % n;
                int ya = xy[i * 2 + 1], yb = xy[j * 2 + 1];
                int xa = xy[i * 2],     xb = xy[j * 2];
                if ((ya <= y && yb > y) || (yb <= y && ya > y)) {
                    double t = (double)(y - ya) / (double)(yb - ya);
                    xs[cnt++] = (int)((double)xa + t * ((double)xb - (double)xa));
                    if (cnt >= 64) break;
                }
            }
            for (i = 0; i < cnt - 1; i += 2) {
                int xa = xs[i], xb = xs[i + 1];
                int xx;
                if (xa > xb) { int t = xa; xa = xb; xb = t; }
                if (xa < x_min) xa = x_min;
                if (xb > x_max) xb = x_max;
                for (xx = xa; xx <= xb; xx++) {
                    if (in_clip(xx, y))
                        g_fb[(size_t)y * (size_t)g_width + (size_t)xx] =
                            (unsigned char)g_fg_color;
                }
            }
        }
    }
    gfx_polyline(n, xy_orig);
}

void gfx_flood_fill(int x, int y, int border)
{
    static int stack_x[4096];
    static int stack_y[4096];
    int top = 0;
    int target;
    int new_color;

    if (g_fb == NULL) return;
    if (x < 0 || x >= g_width || y < 0 || y >= g_height) return;
    target = g_fb[(size_t)y * (size_t)g_width + (size_t)x];
    if (border >= 0 && target == border) return;
    new_color = g_fg_color;
    if (new_color == target) return;

    stack_x[0] = x;
    stack_y[0] = y;
    top = 1;
    while (top > 0) {
        int cx, cy, nx, ny;
        top--;
        cx = stack_x[top];
        cy = stack_y[top];
        if (cx < 0 || cx >= g_width || cy < 0 || cy >= g_height) continue;
        if (g_fb[(size_t)cy * (size_t)g_width + (size_t)cx] != target) continue;
        g_fb[(size_t)cy * (size_t)g_width + (size_t)cx] =
            (unsigned char)new_color;
        nx = cx + 1; ny = cy;
        if (top < 4095 && nx < g_width &&
            g_fb[(size_t)ny * (size_t)g_width + (size_t)nx] == target) {
            stack_x[top] = nx; stack_y[top] = ny; top++;
        }
        nx = cx - 1; ny = cy;
        if (top < 4095 && nx >= 0 &&
            g_fb[(size_t)ny * (size_t)g_width + (size_t)nx] == target) {
            stack_x[top] = nx; stack_y[top] = ny; top++;
        }
        nx = cx; ny = cy + 1;
        if (top < 4095 && ny < g_height &&
            g_fb[(size_t)ny * (size_t)g_width + (size_t)nx] == target) {
            stack_x[top] = nx; stack_y[top] = ny; top++;
        }
        nx = cx; ny = cy - 1;
        if (top < 4095 && ny >= 0 &&
            g_fb[(size_t)ny * (size_t)g_width + (size_t)nx] == target) {
            stack_x[top] = nx; stack_y[top] = ny; top++;
        }
    }
}

void gfx_bezier(int n, const int *xy)
{
    int i, s;
    if (g_fb == NULL || n < 3 || xy == NULL) return;
    /* Interpolation de De Casteljau, n points de controle */
    for (s = 0; s <= 200; s++) {
        double t = (double)s / 200.0;
        double px = 0.0, py = 0.0;
        int k;
        double pt[8][2];
        int m = n;
        for (i = 0; i < n; i++) {
            pt[i][0] = (double)win_map_x(xy[i * 2]);
            pt[i][1] = (double)win_map_y(xy[i * 2 + 1]);
        }
        for (k = 1; k < m; k++) {
            for (i = 0; i < m - k; i++) {
                pt[i][0] = (1.0 - t) * pt[i][0] + t * pt[i + 1][0];
                pt[i][1] = (1.0 - t) * pt[i][1] + t * pt[i + 1][1];
            }
        }
        px = pt[0][0];
        py = pt[0][1];
        gfx_plot((int)px, (int)py);
    }
}

/* ------------------------------------------------------------------ */
/* Police 5x7 (ASCII 32-126)                                           */
/* ------------------------------------------------------------------ */

static const unsigned char font5x7[95][7] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /*   */
    {0x00,0x00,0x24,0x00,0x00,0x24,0x00}, /* ! */
    {0x00,0x36,0x36,0x00,0x36,0x36,0x00}, /* " */
    {0x00,0x24,0x7E,0x24,0x7E,0x24,0x00}, /* # */
    {0x00,0x18,0x3C,0x7E,0x3C,0x18,0x00}, /* $ */
    {0x00,0x66,0x66,0x3C,0x18,0x18,0x00}, /* % */
    {0x00,0x36,0x66,0x3C,0x06,0x6C,0x00}, /* & */
    {0x00,0x06,0x0C,0x00,0x00,0x00,0x00}, /* ' */
    {0x00,0x0C,0x18,0x30,0x30,0x18,0x0C}, /* ( */
    {0x00,0x30,0x18,0x0C,0x0C,0x18,0x30}, /* ) */
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00}, /* * */
    {0x00,0x00,0x0C,0x0C,0x7E,0x0C,0x0C}, /* + */
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x18}, /* , */
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00}, /* - */
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C}, /* . */
    {0x00,0x00,0x06,0x0C,0x18,0x30,0x60}, /* / */
    {0x00,0x3C,0x66,0x66,0x66,0x3C,0x00}, /* 0 */
    {0x00,0x18,0x38,0x18,0x18,0x7E,0x00}, /* 1 */
    {0x00,0x3C,0x66,0x0C,0x18,0x3C,0x00}, /* 2 */
    {0x00,0x3C,0x66,0x0C,0x66,0x3C,0x00}, /* 3 */
    {0x00,0x0C,0x1C,0x3C,0x7E,0x0C,0x00}, /* 4 */
    {0x00,0x7E,0x0C,0x3C,0x06,0x3C,0x00}, /* 5 */
    {0x00,0x3C,0x0C,0x3C,0x66,0x3C,0x00}, /* 6 */
    {0x00,0x7E,0x06,0x0C,0x18,0x18,0x00}, /* 7 */
    {0x00,0x3C,0x66,0x3C,0x66,0x3C,0x00}, /* 8 */
    {0x00,0x3C,0x66,0x3E,0x06,0x3C,0x00}, /* 9 */
    {0x00,0x00,0x0C,0x00,0x00,0x0C,0x00}, /* : */
    {0x00,0x00,0x0C,0x00,0x00,0x0C,0x18}, /* ; */
    {0x00,0x06,0x0C,0x18,0x30,0x0C,0x06}, /* < */
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00}, /* = */
    {0x00,0x60,0x30,0x18,0x0C,0x30,0x60}, /* > */
    {0x00,0x3C,0x66,0x0C,0x18,0x00,0x18}, /* ? */
    {0x00,0x3C,0x66,0x7E,0x6A,0x3E,0x00}, /* @ */
    {0x00,0x18,0x18,0x3C,0x66,0x66,0x00}, /* A */
    {0x00,0x7C,0x66,0x7C,0x66,0x7C,0x00}, /* B */
    {0x00,0x3C,0x66,0x06,0x06,0x3C,0x00}, /* C */
    {0x00,0x78,0x6C,0x66,0x6C,0x78,0x00}, /* D */
    {0x00,0x7E,0x06,0x3C,0x06,0x7E,0x00}, /* E */
    {0x00,0x7E,0x06,0x3C,0x06,0x06,0x00}, /* F */
    {0x00,0x3C,0x66,0x0E,0x66,0x3E,0x00}, /* G */
    {0x00,0x66,0x66,0x3C,0x66,0x66,0x00}, /* H */
    {0x00,0x3C,0x0C,0x0C,0x0C,0x3C,0x00}, /* I */
    {0x00,0x1E,0x06,0x06,0x66,0x3C,0x00}, /* J */
    {0x00,0x66,0x6C,0x38,0x6C,0x66,0x00}, /* K */
    {0x00,0x0E,0x06,0x06,0x06,0x7E,0x00}, /* L */
    {0x00,0x66,0x7E,0x7E,0x66,0x66,0x00}, /* M */
    {0x00,0x66,0x7E,0x6E,0x66,0x66,0x00}, /* N */
    {0x00,0x3C,0x66,0x66,0x66,0x3C,0x00}, /* O */
    {0x00,0x3C,0x66,0x3C,0x06,0x06,0x00}, /* P */
    {0x00,0x3C,0x66,0x66,0x3E,0x06,0x3C}, /* Q */
    {0x00,0x18,0x3C,0x66,0x66,0x66,0x00}, /* R */
    {0x00,0x3C,0x66,0x0C,0x18,0x3C,0x00}, /* S */
    {0x00,0x7E,0x0C,0x0C,0x0C,0x0C,0x00}, /* T */
    {0x00,0x66,0x66,0x66,0x66,0x3C,0x00}, /* U */
    {0x00,0x66,0x66,0x66,0x3C,0x18,0x00}, /* V */
    {0x00,0x66,0x66,0x3C,0x7E,0x3C,0x00}, /* W */
    {0x00,0x66,0x66,0x3C,0x0C,0x66,0x00}, /* X */
    {0x00,0x66,0x66,0x3C,0x0C,0x0C,0x00}, /* Y */
    {0x00,0x7E,0x06,0x0C,0x38,0x7E,0x00}, /* Z */
    {0x00,0x3C,0x30,0x30,0x30,0x3C,0x00}, /* [ */
    {0x00,0x60,0x30,0x18,0x0C,0x06,0x00}, /* \ */
    {0x00,0x3C,0x0C,0x0C,0x0C,0x3C,0x00}, /* ] */
    {0x00,0x18,0x30,0x30,0x30,0x30,0x00}, /* ^ */
    {0x00,0x00,0x00,0x00,0x00,0x00,0xFE}, /* _ */
    {0x00,0x30,0x18,0x00,0x00,0x00,0x00}, /* ` */
    {0x00,0x00,0x18,0x0C,0x18,0x3E,0x00}, /* a */
    {0x00,0x0E,0x66,0x3C,0x66,0x3E,0x00}, /* b */
    {0x00,0x00,0x3C,0x06,0x0C,0x38,0x00}, /* c */
    {0x00,0x3C,0x66,0x3C,0x66,0x3E,0x00}, /* d */
    {0x00,0x00,0x3C,0x0C,0x3E,0x0C,0x38}, /* e */
    {0x00,0x1C,0x36,0x30,0x30,0x30,0x00}, /* f */
    {0x00,0x00,0x3A,0x66,0x3E,0x06,0x3C}, /* g */
    {0x00,0x0E,0x66,0x3E,0x66,0x66,0x00}, /* h */
    {0x00,0x0C,0x00,0x0C,0x0C,0x3C,0x00}, /* i */
    {0x00,0x38,0x06,0x06,0x66,0x3C,0x00}, /* j */
    {0x00,0x0E,0x66,0x66,0x3E,0x66,0x00}, /* k */
    {0x00,0x3C,0x0C,0x0C,0x0C,0x3C,0x00}, /* l */
    {0x00,0x00,0x7C,0x7C,0x66,0x66,0x00}, /* m */
    {0x00,0x00,0x3C,0x66,0x66,0x66,0x00}, /* n */
    {0x00,0x00,0x3C,0x66,0x66,0x3C,0x00}, /* o */
    {0x00,0x00,0x3E,0x66,0x3C,0x06,0x3C}, /* p */
    {0x00,0x00,0x3C,0x66,0x0E,0x3C,0x06}, /* q */
    {0x00,0x00,0x3E,0x66,0x0E,0x0C,0x00}, /* r */
    {0x00,0x00,0x3E,0x0C,0x3C,0x06,0x38}, /* s */
    {0x00,0x30,0x30,0x1C,0x06,0x06,0x00}, /* t */
    {0x00,0x00,0x66,0x66,0x66,0x3E,0x00}, /* u */
    {0x00,0x00,0x66,0x66,0x3C,0x18,0x00}, /* v */
    {0x00,0x00,0x66,0x66,0x3C,0x7E,0x00}, /* w */
    {0x00,0x00,0x66,0x3C,0x0C,0x66,0x00}, /* x */
    {0x00,0x00,0x66,0x66,0x3E,0x06,0x3C}, /* y */
    {0x00,0x00,0x7E,0x0C,0x30,0x7E,0x00}, /* z */
    {0x00,0x0C,0x1C,0x30,0x30,0x1C,0x0C}, /* { */
    {0x00,0x0C,0x0C,0x0C,0x0C,0x0C,0x00}, /* | */
    {0x00,0x30,0x18,0x0C,0x0C,0x18,0x30}, /* } */
    {0x00,0x00,0x6C,0x36,0x00,0x00,0x00}, /* ~ */
};

void gfx_achar(int x, int y, int code)
{
    const unsigned char *g;
    int row, col;
    if (code < 32 || code > 126) code = 32;
    x = win_map_x(x);
    y = win_map_y(y);
    g = font5x7[code - 32];
    for (row = 0; row < 7; row++) {
        for (col = 0; col < 5; col++) {
            if (g[row] & (0x10 >> col))
                gfx_plot(x + col, y + row);
        }
    }
}

void gfx_text(int x, int y, const char *s)
{
    if (s == NULL) return;
    while (*s) {
        gfx_achar(x, y, (int)(unsigned char)*s);
        x += 6;
        s++;
    }
}

/* ------------------------------------------------------------------ */
/* Capture / restitution (GET / PUT)                                   */
/* ------------------------------------------------------------------ */

void gfx_capture(int x1, int y1, int x2, int y2, char *buf)
{
    int x, y;
    if (g_fb == NULL || buf == NULL) return;
    x1 = win_map_x(x1);
    y1 = win_map_y(y1);
    x2 = win_map_x(x2);
    y2 = win_map_y(y2);
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= g_width) x2 = g_width - 1;
    if (y2 >= g_height) y2 = g_height - 1;
    for (y = y1; y <= y2; y++) {
        for (x = x1; x <= x2; x++) {
            buf[(y - y1) * (x2 - x1 + 1) + (x - x1)] =
                (char)g_fb[(size_t)y * (size_t)g_width + (size_t)x];
        }
    }
}

void gfx_restore(int x, int y, const char *buf, int w, int h)
{
    int xw, yh;
    if (g_fb == NULL || buf == NULL || w <= 0 || h <= 0) return;
    x = win_map_x(x);
    y = win_map_y(y);
    for (yh = 0; yh < h; yh++) {
        for (xw = 0; xw < w; xw++) {
            put_pixel(x + xw, y + yh, (int)(unsigned char)buf[yh * w + xw]);
        }
    }
}

void gfx_turtle_line(int x1, int y1, int x2, int y2, int color)
{
    int saved = g_fg_color;
    g_fg_color = color;
    gfx_line(x1, y1, x2, y2);
    g_fg_color = saved;
}