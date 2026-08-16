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

/* Primitives VDI etendues */
void gfx_plot(int x, int y);
int  gfx_get_pixel(int x, int y);
void gfx_hline(int y, int x1, int x2);
void gfx_ellipse(int x, int y, int rx, int ry, int fill);
void gfx_polyline(int n, const int *xy);
void gfx_polygon(int n, const int *xy, int fill);
void gfx_flood_fill(int x, int y, int border);
void gfx_bezier(int n, const int *xy);
void gfx_text(int x, int y, const char *s);
void gfx_achar(int x, int y, int code);
void gfx_clip(int x1, int y1, int x2, int y2);
void gfx_clip_reset(void);
void gfx_window(int x0, int y0, int x1, int y1);
void gfx_window_reset(void);
void gfx_color_reg(int n, int val);
void gfx_mode(int mode);
int  gfx_mode_get(void);
void gfx_capture(int x1, int y1, int x2, int y2, char *buf);
void gfx_restore(int x, int y, const char *buf, int w, int h);

/* Etat turtle (pour DRAW) */
void gfx_turtle_line(int x1, int y1, int x2, int y2, int color);
int  gfx_fg_color(void);
void gfx_set_fg(int color);

/* Turtle GFA (DRAW) : execute une chaine de commandes.
   rt = contexte runtime (etats turtle_x/y/angle/pen/color). */
struct gfa_runtime;
void gfa_turtle_exec(struct gfa_runtime *rt, const char *prog);

/* Couleurs Atari ST -> RGB */
unsigned long gfx_st_color(int index);

#ifdef __cplusplus
}
#endif

#endif /* GFA_GFX_H */
