#include <pthread.h>
#include <string.h>
#include "minui.h"

#define DEFAULT_FONT_HEIGHT	24

/*
 * create a window
 */
struct window *window_init(int color, int font_height)
{
	struct window *win;

	win = malloc(sizeof(*win));
	if (!win) {
		printf("out of memory\n");
		return NULL;
	}

	win->color = color;
	win->font_height = font_height > 0 ? font_height : DEFAULT_FONT_HEIGHT;

	win->window = NULL;
	win->surface = NULL;
	win->font = NULL;
	win->x = 0;
	win->y = 0;
	win->width = 0;
	win->height = 0;

	return win;
}

/*
 * destroy a window
 */
void window_free(struct window *win)
{
	if (!win)
		return;

	if (win->window)
		win->window->Release(win->window);
	if (win->surface)
		win->surface->Release(win->surface);
	if (win->font)
		win->font->Release(win->font);

	free(win);
}

/*
 * create a menu
 */
struct menu *menu_init(struct window *win, int x, int y, int width, int height,
		       int color, int hl_bg_color, int hl_fg_color)
{
	struct menu *menu;
	int i;

	if (!win || !win->surface || width <= 0 || height <= 0) {
		printf("null window or invalid max_items found\n");
		return NULL;
	}

	menu = malloc(sizeof(*menu));
	if (!menu) {
		printf("out of memory\n");
		return NULL;
	}

	menu->win = win;
	menu->x = x > 0 ? x : 0;
	menu->y = y > 0 ? y : 0;
	menu->width = width;
	menu->height = height;
	menu->max_items = menu->height / win->font_height;
	menu->nr_items = 0;
	menu->items = malloc(sizeof(*(menu->items)) * menu->max_items);
	if (!menu->items) {
		printf("out of memory\n");
		menu_free(menu);
		return NULL;
	}
	for (i = 0; i < menu->max_items; i++)
		menu->items[i] = NULL;

	menu->color = color;
	menu->hl_bg_color = hl_bg_color;
	menu->hl_fg_color = hl_fg_color;
	menu->selected = 0;

	return menu;
}

/*
 * destroy a menu
 */
void menu_free(struct menu *menu)
{
	int i;
	if (!menu)
		return;

	if (menu->win)
		window_free(menu->win);

	if (menu->items) {
		for (i = 0; i < menu->nr_items; i++)
			if (menu->items[i])
				free(menu->items[i]);
		free(menu->items);
	}

	free(menu);
}

int menu_newitem(struct menu *menu, struct textline *line,
		menu_handler handler)
{
	struct menu_item *item;
	if (!menu || !line || !line->text || !strlen(line->text)) {
		printf("invalid value to create a new menu item\n");
		return -1;
	}
	if (menu->nr_items == menu->max_items) {
		printf("menu is full\n");
		return -1;
	}

	item = malloc(sizeof(*item));
	if (!item) {
		printf("out of memory\n");
		return -1;
	}
	item->line = line;
	item->handler = handler;
	menu->items[menu->nr_items] = item;
	menu->nr_items++;

	return 0;
}

/*
 * refresh menu
 */
void menu_refresh(struct menu *menu)
{
	IDirectFBSurface *surface;
	int y;
	int i;
	int color;

	if (!menu || !menu->win || !menu->win->surface)
		return;

	surface = menu->win->surface;

	/* clean up menu area */
	DFBCHECK (surface->SetColor (surface, R(menu->win->color),
				G(menu->win->color), B(menu->win->color), A(menu->win->color)));
	DFBCHECK (surface->FillRectangle (surface, menu->x, menu->y,
				menu->width, menu->height-menu->win->font_height));

	/* render menu items */
	y = menu->y;
	for (i = 0; i < menu->nr_items && menu->items[i]; i++) {
		/* high light selected menu item */
		if (i == menu->selected) {
			color = menu->hl_fg_color;

			/* reverse hilight */
			DFBCHECK (surface->SetColor (surface, R(menu->hl_bg_color),
						     G(menu->hl_bg_color),
						     B(menu->hl_bg_color),
						     A(menu->hl_bg_color)));
			DFBCHECK (surface->FillRectangle (surface, menu->x, y,
							  menu->width,
							  menu->win->font_height));
		} else
			color = menu->items[i]->line->color;

		DFBCHECK (surface->SetColor (surface, R(color),
					G(color), B(color), A(color)));
		DFBCHECK (surface->DrawString (surface, menu->items[i]->line->text, -1,
					0, y, DSTF_TOPLEFT));
		y += menu->win->font_height;
	}

	DFBCHECK (surface->Flip (surface, NULL, DSFLIP_NONE));
}

/*
 * update selected menu item relative current selected one
 */
int menu_update(struct menu *menu, int next)
{
	if (!menu) {
		printf("please init menu first\n");
		return -1;
	}

	return menu_selected(menu, menu->selected + next);
}

/*
 * set selected item of menu
 */
int menu_selected(struct menu *menu, int selected)
{
	if (!menu || selected + menu->nr_items < 0) {
		printf("please init menu first or invalid selected item\n");
		return -1;
	}

	menu->selected = (selected + menu->nr_items) % menu->nr_items;
	menu_refresh(menu);

	return 0;
}

int menu_action(struct menu *menu)
{
	menu_handler handler;
	if (!menu) {
		printf("please init menu first\n");
		return -1;
	}

	if (menu->selected < 0 || menu->selected >= menu->max_items) {
		printf("invalid menu item selected\n");
		return -1;
	}

	handler = menu->items[menu->selected]->handler;
	if (handler)
		return handler();
	else
		return -1;
}

/*
 * create a textline
 */
struct textline *textline_init(int len)
{
	struct textline *line;

	if (len <= 0) {
		printf("invalid len to create textline\n");
		return NULL;
	}

	line = malloc(sizeof(*line));
	if (!line) {
		printf("out of memory\n");
		return NULL;
	}

	line->text = malloc(len);
	if (!line->text) {
		printf("out of memory\n");
		return NULL;
	}

	line->len = len;

	return line;
}

/*
 * destroy a textline
 */
void textline_free(struct textline *line)
{
	if (!line)
		return;
	if (line->text)
		free(line->text);
	free(line);
}

/*
 * create a bar
 */
struct bar *bar_init(int x, int y, int width, int height,
		int flags, int percent, int bg_color, int color,
		int border_width, int border_color)
{
	struct bar *bar;
	bar = malloc(sizeof(*bar));
	if (!bar) {
		printf("out of memory\n");
		return NULL;
	}

	bar->x = x;
	bar->y = y;
	bar->width = width;
	bar->height = height;
	bar->flags = flags;
	bar->percent = percent;
	bar->bg_color = bg_color;
	bar->color = color;
	bar->border_width = border_width;
	bar->border_color = border_color;

	return bar;
}

/*
 * destroy a bar
 */
void bar_free(struct bar *bar)
{
	if (bar)
		free(bar);
}

/*
 * create a text bar
 */
struct textbar *textbar_init(struct window *win, int x, int y,
		int width, int height, int text_color,
		int interval, struct bar *bar)
{
	struct textbar *tb;

	if (!bar || !win) {
		printf("null bar or window found\n");
		return NULL;
	}

	tb = malloc(sizeof(*tb));
	if (!tb) {
		printf("out of memory\n");
		return NULL;
	}

	tb->win = win;
	tb->x = x;
	tb->y = y;
	tb->width = width;
	tb->height = height;
	tb->line = NULL;
	tb->color = text_color;
	tb->interval = interval;
	tb->bar = bar;

	return tb;
}

/*
 * destroy text bar
 */
void textbar_free(struct textbar *tb)
{
	if (!tb)
		return;

	if (tb->line)
		textline_free(tb->line);

	if (tb->bar)
		bar_free(tb->bar);

	if (tb->win)
		window_free(tb->win);

	free(tb);
}

/*
 * render bounce bar
 */
static void *bouncebar_refresh(void *arg)
{
	IDirectFBSurface *surface;
	struct textbar *tb;
	struct bar *bar;
	int x;
	int y;
	int width;
	int height;
	int time_step = 100; // ms
	int pixel_step = 5;

	tb = (struct textbar *)arg;
	if (!tb || !tb->win || !tb->win->surface || !tb->bar) {
		printf("please init textbar first\n");
		return;
	}

	surface = tb->win->surface;
	bar = tb->bar;
	x = bar->x + bar->border_width;
	y = bar->y + bar->border_width;
	height = bar->height - 2 * bar->border_width;
	do {
		/* fill with bg_color */
		DFBCHECK (surface->SetColor (surface, R(bar->bg_color),
					G(bar->bg_color), B(bar->bg_color), A(bar->bg_color)));
		DFBCHECK (surface->FillRectangle (surface, bar->x + bar->border_width,
					y, bar->width - 2 * bar->border_width, height));
		/* fill 1 / 3 width with color */
		DFBCHECK (surface->SetColor (surface, R(bar->color),
					G(bar->color), B(bar->color), A(bar->color)));
		DFBCHECK (surface->FillRectangle (surface, x, y,
					bar->width / 3, height));
		usleep(time_step);
		x += pixel_step;
		if (x + bar->width / 3 > bar->x + bar->width - bar->border_width ||
				x < bar->x + bar->border_width) {
			pixel_step = -pixel_step;
			x += pixel_step;
		}
		DFBCHECK (surface->Flip (surface, NULL, DSFLIP_NONE));
	} while (bar->percent == BAR_BOUNCE);
	return NULL;
}

/*
 * refresh text bar
 */
void textbar_refresh(struct textbar *tb, int text_only)
{
	IDirectFBSurface *surface;
	pthread_t t_bounce;
	struct textline *line;
	struct bar *bar;
	int x;
	int y;
	int width;
	int height;

	if (!tb || !tb->win || !tb->win->surface) {
		printf("please init ui first\n");
		return;
	}

	surface = tb->win->surface;
	x = tb->x;
	y = tb->y;
	width = tb->width;
	height = tb->height;

	bar = tb->bar;
	/* clean up */
	if (text_only && bar->percent)
		/* don't update bar */
		height = bar->y - tb->y;
	DFBCHECK (surface->SetColor (surface, R(tb->win->color),
				G(tb->win->color), B(tb->win->color), A(tb->win->color)));
	DFBCHECK (surface->FillRectangle (surface, x, y,
				width, height));

	/* draw textbar text line, aligned topcenter */
	x = tb->x + tb->width / 2;
	y = bar->y - tb->interval;
	line = tb->line;
	DFBCHECK (surface->SetColor (surface, R(line->color),
				G(line->color), B(line->color), A(line->color)));
	DFBCHECK (surface->DrawString (surface, line->text, -1,
				x, y, DSTF_BOTTOMCENTER));

	if (bar->percent == 0 || text_only) {
		DFBCHECK (surface->Flip (surface, NULL, DSFLIP_NONE));
		return;
	}

	/* draw bar border */
	x = bar->x;
	y = bar->y;
	DFBCHECK (surface->SetColor (surface, R(bar->border_color),
				G(bar->border_color), B(bar->border_color),
				A(bar->border_color)));
	width = bar->width;
	height = bar->height;
	DFBCHECK (surface->DrawRectangle (surface, x, y, width, height));

	/* draw bar background */
	x += bar->border_width;
	y += bar->border_width;
	width -= 2 * bar->border_width;
	height -= 2 * bar->border_width;
	DFBCHECK (surface->SetColor (surface, R(bar->bg_color),
				G(bar->bg_color), B(bar->bg_color),
				A(bar->bg_color)));
	DFBCHECK (surface->FillRectangle (surface, x, y, width, height));

	if (bar->flags & BF_PROGRESS) {
		/* draw progress bar */
		DFBCHECK (surface->SetColor (surface, R(bar->color),
					G(bar->color), B(bar->color), A(bar->color)));
		width = (int)((bar->percent * (bar->width - 2 * bar->border_width)) / 100.0);
		if (width == 0) {
			if (bar->percent > 0)
				width = 1;
			else
				width = -1;
		}

		if (bar->flags & BF_FILLOUT) {
			/* fill out */
			DFBCHECK (surface->FillRectangle (surface, x, y, width, height));
		} else {
			/* empty out */
			width = -width;
			DFBCHECK (surface->FillRectangle (surface, x, y,
						bar->width - 2 * bar->border_width, height));
			DFBCHECK (surface->SetColor (surface, R(bar->bg_color),
						G(bar->bg_color), B(bar->color), A(bar->bg_color)));
			width = bar->width - 2 * bar->border_width - width;
			if (width == 0)
				width = 1;
			DFBCHECK (surface->FillRectangle (surface, x, y, width, height));
		}
	} else {
		if (pthread_create(&t_bounce, NULL, bouncebar_refresh, tb))
			printf("create bounce thread failed\n");
	}

	/* flip to surface */
	DFBCHECK (surface->Flip (surface, NULL, DSFLIP_NONE));
}

/*
 * create a text area on the surface
 */
struct textarea *textarea_init(struct window *win,
		int x, int y, int width, int height,
		int flags, int text_color)
{
	struct textarea *ta;
	int i;
	int font_height;

	if (!win) {
		printf("null window found\n");
		return NULL;
	}

	font_height = win->font_height;
	if (width <= 0 || width < font_height) {
		printf("invalid lines of textarea\n");
		return NULL;
	}

	ta = malloc(sizeof(*ta));
	if (!ta) {
		printf("out of memory\n");
		return NULL;
	}

	ta->win = win;
	ta->x = x;
	ta->y = y;
	ta->width = width;
	ta->height = height;

	ta->nr_lines = ta->height / font_height;
	ta->lines = malloc(sizeof(*(ta->lines)) * ta->nr_lines);
	if (!ta->lines) {
		printf("out of memory\n");
		free(ta);
		return NULL;
	}
	for (i = 0; i < ta->nr_lines; i++)
		ta->lines[i] =
			textline_init(ta->width/ta->win->font_width + 1);

	ta->text_color = text_color;
	ta->cur_line = -1;

	ta->flags = flags;

	return ta;
}

/*
 * destroy text area
 */
void textarea_free(struct textarea *ta)
{
	int i;
	if (!ta)
		return;

	if (ta->lines) {
		for (i = 0; i < ta->nr_lines; i++) {
			if (ta->lines[i])
				textline_free(ta->lines[i]);
		}
		free(ta->lines);
	}

	if (ta->win)
		window_free(ta->win);

	free(ta);
}

/*
 * put a line into text area
 */
void textarea_putline(struct textarea *ta, struct textline *line)
{
	int length = strlen(line->text);
	const char *pos = line->text;
	int ret_str_length;
	int ret_width;
	const char *ret_next_line;

	if (!ta || !line || !strlen(line->text)) {
		printf("null textarea or line\n");
		return;
	}
	if (!ta->lines) {
		printf("null textline\n");
		return;
	}

	while (length) {
		struct textline *l;

		ta->cur_line = (ta->cur_line + 1) % ta->nr_lines;
		l = ta->lines[ta->cur_line];

		DFBCHECK (ta->win->font->GetStringBreak (ta->win->font,
					       pos,
					       length,
					       ta->width,
					       &ret_width,
					       &ret_str_length,
					       &ret_next_line));
		memset(l->text, '\0', l->len);
		memcpy(l->text, pos, min(ret_str_length, l->len));
		/* remove the newline at end of string */
		if (pos[ret_str_length - 1] == '\n')
			l->text[ret_str_length - 1] = 0;
		l->color = line->color;

		length -= ret_str_length;
		pos = ret_next_line;
	}
}

/*
 * render the text area
 */
void textarea_refresh(struct textarea *ta)
{
	IDirectFBSurface *surface;
	int i;
	int step_i;
	int first;
	int x;
	int y;
	int step_y;
	struct textline *line;
	int font_height;

	if (!ta || !ta->win || !ta->win->surface || !ta->lines) {
		printf("please init ui first\n");
		return;
	}

	surface = ta->win->surface;

	font_height = ta->win->font_height;
	/* clean the whole text area */
	DFBCHECK (surface->SetColor (surface, R(ta->win->color),
				G(ta->win->color), B(ta->win->color), A(ta->win->color)));
	DFBCHECK (surface->FillRectangle (surface, ta->x, ta->y, ta->width, ta->height));

	x = ta->x;
	if (ta->flags & TAF_TOPTODOWN) {
		y = ta->y;
		step_y = font_height;
	} else {
		y = ta->y + ta->height - font_height;
		step_y = -font_height;
	}
	if (ta->flags & TAF_LIFO) {
		step_i = -1;
		i = ta->cur_line;
	} else {
		step_i = 1;
		i = (ta->cur_line + 1) % ta->nr_lines;
	}
	first = i;
	line = ta->lines[i];
	do {
		if (!line || !strlen(line->text)) {
			i = (i + step_i + ta->nr_lines) % ta->nr_lines;
			line = ta->lines[i];
			continue;
		}
		DFBCHECK (surface->SetColor (surface, R(line->color),
					G(line->color), B(line->color), A(line->color)));
		DFBCHECK (surface->DrawString (surface, line->text, -1,
				x, y, DSTF_TOPLEFT));
		i = (i + step_i  + ta->nr_lines) % ta->nr_lines;
		line = ta->lines[i];
		y += step_y;
	} while (i != first);

	/* flip to surface */
	DFBCHECK (surface->Flip (surface, NULL, DSFLIP_NONE));
}
