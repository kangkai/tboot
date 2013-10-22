/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _MINUI_H_
#define _MINUI_H_

#include <stdbool.h>
#include "directfb.h"

// input event structure, include <linux/input.h> for the definition.
// see http://www.mjmwired.net/kernel/Documentation/input/ for info.
struct input_event;

typedef int (*ev_callback)(int fd, short revents, void *data);
typedef int (*ev_set_key_callback)(int code, int value, void *data);

int ev_init(ev_callback input_cb, void *data);
void ev_exit(void);
int ev_add_fd(int fd, ev_callback cb, void *data);
int ev_sync_key_state(ev_set_key_callback set_key_cb, void *data);

/* timeout has the same semantics as for poll
 *    0 : don't block
 *  < 0 : block forever
 *  > 0 : block for 'timeout' milliseconds
 */
int ev_wait(int timeout);

int ev_get_input(int fd, short revents, struct input_event *ev);
void ev_dispatch(void);

/* get rgba component of color */
#define R(color) \
	(unsigned char)((color) >> 24)
#define G(color) \
	(unsigned char)((color) >> 16)
#define B(color) \
	(unsigned char)((color) >> 8)
#define A(color) \
	(unsigned char)(color)

/* check for directfb */
#define DFBCHECK(x...)                                             \
    {                                                              \
        DFBResult err = x;                                         \
                                                                   \
        if (err != DFB_OK)                                         \
        {                                                          \
            fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ ); \
            DirectFBErrorFatal( #x, err );                         \
        }                                                          \
    }

/*
 * basic components that can be used to composite UI
 */
/*
 * window
 */
struct window {
	IDirectFBWindow *window;
	IDirectFBSurface *surface;
	IDirectFBFont *font;
	int font_height;
	int font_width;
	int color;
	int x;		// its position value just for read
	int y;
	int width;
	int height;
};
/*
 * functions for window
 */
struct window *window_init(int color, int font_height);
void window_free(struct window *win);

/*
 * A text line which can be put into text area or as a part of text bar.
 * Its position is controled by its parent.
 */
struct textline {
	char *text;
	int len;
	int color;
};
/*
 * functions for textline
 */
struct textline *textline_init(int len);
void textline_free(struct textline *line);

/*
 * menu
 */
typedef int (*menu_handler)(void);
struct menu_item {
	struct textline *line;
	menu_handler handler;
};
struct menu {
	struct window *win;
	struct menu_item **items;
	int max_items;
	int nr_items;
	int color;		// default color of menu items
	int hl_fg_color;	// fg color of selected menu item
	int hl_bg_color;	// bg color of selected menu item
	int selected;
	int x;
	int y;
	int width;
	int height;
};
/*
 * functions for menu
 */
struct menu *menu_init(struct window *win, int x, int y, int width, int height,
		       int color, int hl_bg_color, int hl_fg_color);
void menu_free(struct menu *menu);
int menu_newitem(struct menu *menu, struct textline *line,
		menu_handler handler);
void menu_refresh(struct menu *menu);
int menu_update(struct menu *menu, int next);
int menu_action(struct menu *menu);

/*
 * text area consists of multi lines of text
 * its flags controls how to render the text lines
 *		bit0: TAF_TOPTODOWN, render text from top to down
 *		bit1: TAF_LIFO, the newest line first
 */
#define TAF_TOPTODOWN	0x1
#define TAF_LIFO		(0x1 << 1)
struct textarea {
	struct window *win;
	int text_color;	// default color of text line
	struct textline **lines;
	int nr_lines;
	int cur_line;	// the newest line
	int x;			// text area position
	int y;
	int width;
	int height;
	int flags;
};
/*
 * functions for textarea
 */
struct textarea *textarea_init(struct window *win, int x, int y, int width, int height,
		int flags, int text_color);
void textarea_free(struct textarea *ta);
void textarea_putline(struct textarea *ta, struct textline *line);
void textarea_refresh(struct textarea *ta);


/*
 * bar flags
 *	bit0 is the type of bar
 *	progress bar(1) or bounce bar(0)
 *	bit1 implies how to render the bar
 *  fill out(1) or empty out(1)
 */
#define BF_PROGRESS	0x1
#define BF_FILLOUT	(0x1 << 1)
#define BAR_BOUNCE	101

struct bar {
	int percent;
	int border_color;
	int border_width;
	int bg_color;
	int color;
	int x;
	int y;
	int width;
	int height;	
	int flags;
};
/*
 * functions for bar
 */
struct bar *bar_init(int x, int y, int width, int height,
		int flags, int percent, int bg_color, int color,
		int border_width, int border_color);
void bar_free(struct bar *bar);

/*
 * textbar (text bar) can show a line of text
 * and a progress bar below the text
 */
struct textbar {
	struct window *win;
	struct textline *line;
	int color;
	int interval;	// interval height between text and bar
	struct bar *bar;
	int x;
	int y;
	int width;
	int height;
};
/*
 * functions for textbar
 */
struct textbar *textbar_init(struct window *win, int x, int y,
		int width, int height, int text_color,
		int interval, struct bar *bar);
void textbar_free(struct textbar *tb);
void textbar_refresh(struct textbar *tb, int text_only);

int get_fontheight(IDirectFBSurface *surface);

#define min(x, y) ((x) < (y) ? (x) : (y))

#endif
