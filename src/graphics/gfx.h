/*
 * gfx.h - Couche graphique C89 pur pour PhoenixBasic
 * ===================================================
 * Emule les primitives graphiques Atari ST via un
 * framebuffer memoire rendu en ANSI (aucune lib externe).
 */

#ifndef GFA_GFX_H
#define GFA_GFX_H

#ifdef __cplusplus
extern "C" {
#endif

/* Initialisation / liberation */
int gfx_init(int width, int height);
void gfx_shutdown(void);
void gfx_update(void);

/* Primitives graphiques */
void gfx_clear(void);
void gfx_color(int fg, int bg);
void gfx_line(int x1, int y1, int x2, int y2);
void gfx_box(int x1, int y1, int x2, int y2);
void gfx_fill_box(int x1, int y1, int x2, int y2);
void gfx_circle(int x, int y, int r);
void gfx_fill_circle(int x, int y, int r);

/* Couleurs Atari ST -> RGB */
unsigned long gfx_st_color(int index);

#ifdef __cplusplus
}
#endif

#endif /* GFA_GFX_H */
