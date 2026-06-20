/*
 * gfx.c - Implementation graphique SDL2
 * =====================================
 * Primitives Atari ST emulees en SDL2.
 */

#include "gfx.h"
#include <SDL2/SDL.h>

static SDL_Window   *g_window   = NULL;
static SDL_Renderer *g_renderer = NULL;
static int           g_fg_color = 1;
static int           g_bg_color = 0;

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

unsigned long gfx_st_color(int index)
{
    if (index < 0 || index > 15) return 0xFFFFFF;
    return st_palette[index];
}

static void set_render_color(int color_idx)
{
    unsigned long c;
    if (color_idx < 0) color_idx = 0;
    if (color_idx > 15) color_idx = 15;
    c = st_palette[color_idx];
    SDL_SetRenderDrawColor(g_renderer,
        (unsigned char)((c >> 16) & 0xFF),
        (unsigned char)((c >> 8) & 0xFF),
        (unsigned char)(c & 0xFF), 255);
}

int gfx_init(int width, int height)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return -1;
    g_window = SDL_CreateWindow("PhoenixBasic",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height, SDL_WINDOW_SHOWN);
    if (g_window == NULL) { SDL_Quit(); return -1; }
    g_renderer = SDL_CreateRenderer(g_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (g_renderer == NULL) {
        SDL_DestroyWindow(g_window); g_window = NULL;
        SDL_Quit(); return -1;
    }
    gfx_clear();
    gfx_update();
    return 0;
}

void gfx_shutdown(void)
{
    if (g_renderer) { SDL_DestroyRenderer(g_renderer); g_renderer = NULL; }
    if (g_window)   { SDL_DestroyWindow(g_window); g_window = NULL; }
    SDL_Quit();
}

void gfx_update(void)
{
    if (g_renderer) SDL_RenderPresent(g_renderer);
}

void gfx_clear(void)
{
    if (g_renderer == NULL) return;
    set_render_color(g_bg_color);
    SDL_RenderClear(g_renderer);
}

void gfx_color(int fg, int bg)
{
    g_fg_color = fg;
    g_bg_color = bg;
}

void gfx_line(int x1, int y1, int x2, int y2)
{
    if (g_renderer == NULL) return;
    set_render_color(g_fg_color);
    SDL_RenderDrawLine(g_renderer, x1, y1, x2, y2);
}

void gfx_box(int x1, int y1, int x2, int y2)
{
    SDL_Rect r;
    if (g_renderer == NULL) return;
    set_render_color(g_fg_color);
    r.x = x1; r.y = y1;
    r.w = x2 - x1; r.h = y2 - y1;
    if (r.w < 0) { r.x = x2; r.w = -r.w; }
    if (r.h < 0) { r.y = y2; r.h = -r.h; }
    SDL_RenderDrawRect(g_renderer, &r);
}

void gfx_fill_box(int x1, int y1, int x2, int y2)
{
    SDL_Rect r;
    if (g_renderer == NULL) return;
    set_render_color(g_fg_color);
    r.x = x1; r.y = y1;
    r.w = x2 - x1; r.h = y2 - y1;
    if (r.w < 0) { r.x = x2; r.w = -r.w; }
    if (r.h < 0) { r.y = y2; r.h = -r.h; }
    SDL_RenderFillRect(g_renderer, &r);
}

void gfx_circle(int x, int y, int r)
{
    int px, py;
    if (g_renderer == NULL || r <= 0) return;
    set_render_color(g_fg_color);
    px = r - 1; py = 0;
    while (px >= py) {
        SDL_RenderDrawPoint(g_renderer, x + px, y + py);
        SDL_RenderDrawPoint(g_renderer, x + py, y + px);
        SDL_RenderDrawPoint(g_renderer, x - py, y + px);
        SDL_RenderDrawPoint(g_renderer, x - px, y + py);
        SDL_RenderDrawPoint(g_renderer, x - px, y - py);
        SDL_RenderDrawPoint(g_renderer, x - py, y - px);
        SDL_RenderDrawPoint(g_renderer, x + py, y - px);
        SDL_RenderDrawPoint(g_renderer, x + px, y - py);
        py++;
        if ((px * px + py * py - r * r) > 0) px--;
    }
}

void gfx_fill_circle(int x, int y, int r)
{
    int px, py, i;
    if (g_renderer == NULL || r <= 0) return;
    set_render_color(g_fg_color);
    px = r; py = 0;
    while (px >= py) {
        for (i = x - px; i <= x + px; i++) {
            SDL_RenderDrawPoint(g_renderer, i, y + py);
            SDL_RenderDrawPoint(g_renderer, i, y - py);
        }
        for (i = x - py; i <= x + py; i++) {
            SDL_RenderDrawPoint(g_renderer, i, y + px);
            SDL_RenderDrawPoint(g_renderer, i, y - px);
        }
        py++;
        if ((px * px + py * py - r * r) > 0) px--;
    }
}
