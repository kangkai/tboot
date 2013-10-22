#ifndef __TBOOT_UI_H
#define __TBOOT_UI_H
#include "libminui/minui.h"
#include "theme.h"

struct tboot_ui {
	/* main directfb description handler */
	IDirectFB *dfb;
	IDirectFBDisplayLayer *layer;
	DFBDisplayLayerConfig layer_config;

	/* background image */
	IDirectFBSurface *bg_sur;

	/* system infomation window */
	struct textarea *ta_info;

	/* menu window */
	struct menu *menu;

	/* text bar window */
	struct textbar *tb;

	/* action logs window */
	struct textarea *ta_log;

	int screen_width;
	int screen_height;
} ui;

void tboot_ui_textline(struct textarea *ta, int color, const char *fmt, ...);
void tboot_ui_drawstring(struct window *win, int color,
		int x, int y, const char *fmt, ...);

/* for system infos */
#define tboot_ui_sysinfo(...) \
	tboot_ui_textline(ui.ta_info, COLOR_FG, __VA_ARGS__)
#define tboot_ui_uevent(...) \
	tboot_ui_drawstring(ui.ta_info->win, \
			COLOR_FG, 0, ui.ta_info->win->height - \
			ui.ta_info->win->font_height, __VA_ARGS__)

/* for logs */
#define tboot_ui_info(...) \
	do { \
		tboot_ui_hidebar(""); \
		tboot_ui_textline(ui.ta_log, COLOR_INFO, __VA_ARGS__); \
	} while (0)
#define tboot_ui_warn(...) \
	tboot_ui_textline(ui.ta_log, COLOR_WARN, __VA_ARGS__)
#define tboot_ui_error(...) \
	do { \
		tboot_ui_hidebar(""); \
		tboot_ui_textline(ui.ta_log, COLOR_ERROR, __VA_ARGS__); \
	} while (0)

int tboot_ui_init(const char *config);
void tboot_ui_exit(void);

void tboot_ui_textbar(int percent, const char *fmt, ...);
#define tboot_ui_bouncebar(...) tboot_ui_textbar(BAR_BOUNCE, __VA_ARGS__)
#define tboot_ui_hidebar(...) tboot_ui_textbar(0, __VA_ARGS__)

#define tboot_ui_menu_up() menu_update(ui.menu, -1)
#define tboot_ui_menu_down() menu_update(ui.menu, 1)
#define tboot_ui_menu_action() menu_action(ui.menu)
#define tboot_ui_menu_selected(selected) menu_selected(ui.menu, selected)

void tboot_ui_menu_item(const char *name, menu_handler handler);
char *tboot_ui_getconfpath(void);

#endif
