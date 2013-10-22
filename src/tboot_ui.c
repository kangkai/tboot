/*
 * This is the UI of preos tboot mode
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/limits.h>

#include "debug.h"
#include "directfb.h"
#include "tboot_ui.h"
#include "cutils/preos_reboot.h"
#include "cutils/config_parser.h"


/* how many chars can be rendered in line, maybe exceeds screen width */
#define LINE_LEN_INCHAR	256
/*
 * config file for tboot UI
 * currently, only background color and opacity of window
 * can be configured. Because devices color/lumen may volatile
 * in a large range.
 */
#define CONF_PREFIX	"/etc/preos/confs/tboot-ui"
#define BGIMG_PREFIX	"/etc/preos/imgs/preos-logo"
#define FONT_FILE	"/etc/preos/fonts/font.ttf"

/*
 * UI definitions for phone, which screen width < height
 * the whole screen looks like the below. For tablet, which
 * screen width > height, we'll rotate all the window (90),
 * so it looks just like the phone.
 *  ----------------
 *  system infos
 *  ----------------
 *  menu
 *  ----------------
 *  text bar
 *  ----------------
 *  action logs
 *  ----------------
 */

/*
 * the below values can be configured in file
 */
// colors
#define WINDOW_COLOR_KEY	"window_color"
#define INFOWIN_COLOR_KEY	"infowin_color"
#define MENUWIN_COLOR_KEY	"menuwin_color"
#define LOGWIN_COLOR_KEY	"logwin_color"
#define BARWIN_COLOR_KEY	"barwin_color"

// font height
#define DEFAULT_FONT_HEIGHT_KEY "default_font_height"
#define INFOWIN_FONT_HEIGHT_KEY "infowin_font_height"
#define MENUWIN_FONT_HEIGHT_KEY "menuwin_font_height"
#define LOGWIN_FONT_HEIGHT_KEY  "logwin_font_height"
#define BARWIN_FONT_HEIGHT_KEY  "barwin_font_height"

// separator height
#define COMPONENT_SEPARATOR_HEIGHT_KEY "component_separator_height"
#define WINDOW_SEPARATOR_HEIGHT_KEY    "window_separator_height"

// progress bar height
#define PROGRESS_BAR_HEIGHT_KEY "progress_bar_height"

#define BG_IMAGE_KEY "bg_image"
#define FONT_KEY     "font"

/*
 * ui configuration parameters
 */
// colors
static unsigned int window_color = 0;
static unsigned int infowin_color = 0;
static unsigned int menuwin_color = 0;
static unsigned int logwin_color = 0;
static unsigned int barwin_color = 0;

// font height
static unsigned int default_font_height = 0;
static unsigned int infowin_font_height = 0;
static unsigned int menuwin_font_height = 0;
static unsigned int logwin_font_height = 0;
static unsigned int barwin_font_height = 0;

// separator height
static unsigned int component_separator_height = 0;
static unsigned int window_separator_height = 0;

// progress bar height
static unsigned int progress_bar_height = 0;

static char bg_image[PATH_MAX] = {0};
static char font_file[PATH_MAX] = {0};


/*
 * the percent has the below valid values
 *	[-100, 0): empty out progress bar
 *	0:	hide bar
 *	(0, 100]: fill out progress bar
 *	BAR_BOUNCE: bounce bar
 */
void tboot_ui_textbar(int percent, const char *fmt, ...)
{
	struct textbar *tb;
	struct bar *bar;
	struct textline *line;
	va_list va_args;
	int ret;

	if (!ui.tb) {
		printf("please init ui first\n");
		return;
	}
	if ((percent < -100 || percent > 100) && percent != BAR_BOUNCE)
		return;

	line = textline_init(LINE_LEN_INCHAR);
	if (!line)
		return;

	va_start(va_args, fmt);
	ret = vsnprintf(line->text, line->len, fmt, va_args);
	va_end(va_args);
	if (ret < 0) {
		printf("construct text line failed\n");
		free(line);
		return;
	}
	/*
	 * FIXME: be able to specify line color at runtime?
	 */
	line->color = COLOR_FG;

	tb = ui.tb;
	bar = tb->bar;
	/* free the old line */
	if (tb->line)
		free(tb->line);
	tb->line = line;

	if (percent == bar->percent) {
		/* no need to update bar */
		textbar_refresh(ui.tb, 1);
		return;
	}

	if (percent != BAR_BOUNCE) {
		/* progress tb */
		bar->flags |= BF_PROGRESS;
		if (percent < 0)
			/* empty out */
			bar->flags &= ~(BF_FILLOUT);
		else
			/* fill out */
			bar->flags |= BF_FILLOUT;
	} else {
		/* bounce tb */
		bar->flags &= ~(BF_PROGRESS);
	}

	bar->percent = percent;
	textbar_refresh(ui.tb, 0);
}

/*
 * draw string at somewhere
 */
void tboot_ui_drawstring(struct window *win, int color,
		int x, int y, const char *fmt, ...)
{
	IDirectFBFont *font;
	IDirectFBSurface *surface;
	struct textline *msg;
	va_list va_args;
	int ret;
	int posx;
	int posy;

	if (!win || !win->font || !win->surface) {
		printf("invalid window\n");
		return;
	}

	surface = win->surface;
	msg = textline_init(LINE_LEN_INCHAR);
	if (!msg)
		return;
	va_start(va_args, fmt);
	ret = vsnprintf(msg->text, msg->len, fmt, va_args);
	va_end(va_args);
	if (ret < 0) {
		printf("construct msg line failed\n");
		free(msg);
		return;
	}
	msg->color = color == COLOR_BG ? COLOR_FG : color;
	posx = x > 0 ? x : 0;
	posy = y > 0 ? y : 0;

	DFBCHECK (surface->SetColor (surface, R(win->color),
				G(win->color), B(win->color), A(win->color)));
	DFBCHECK (surface->FillRectangle (surface, posx, posy,
				win->width, win->font_height));
	DFBCHECK (surface->SetColor (surface, R(msg->color),
				G(msg->color), B(msg->color), A(msg->color)));
	DFBCHECK (surface->DrawString (surface, msg->text, -1,
				posx, posy, DSTF_TOPLEFT));
	DFBCHECK (surface->Flip (surface, NULL, DSFLIP_NONE));
}

void tboot_ui_textline(struct textarea *ta, int color, const char *fmt, ...)
{
	struct textline *msg;
	va_list va_args;
	int ret;

	msg = textline_init(LINE_LEN_INCHAR);
	if (!msg)
		return;
	va_start(va_args, fmt);
	ret = vsnprintf(msg->text, msg->len, fmt, va_args);
	va_end(va_args);
	if (ret < 0) {
		printf("construct msg line failed\n");
		free(msg);
		return;
	}

	msg->color = color;
	textarea_putline(ta, msg);
	textline_free(msg);

	/* refresh */
	textarea_refresh(ta);
}

/*
 * create a window
 */
static struct window *tboot_ui_initwindow(struct tboot_ui *ui,
		int color, int font_height)
{
	struct window *win;
	DFBFontDescription font_dsc;

	if (!ui || !ui->dfb) {
		printf("please init ui first\n");
		return NULL;
	}

	win = window_init(color, font_height);
	if (!win) {
		printf("create window failed\n");
		return NULL;
	}

	/* create font */
	font_dsc.flags = DFDESC_HEIGHT;
	font_dsc.height = font_height;
	DFBCHECK (ui->dfb->CreateFont (ui->dfb, font_file, &font_dsc, &(win->font)));
	DFBCHECK (win->font->GetHeight (win->font, &(win->font_height)));
	DFBCHECK (win->font->GetMaxAdvance (win->font, &(win->font_width)));

	return win;
}

/*
 * setup a window
 */
static int tboot_ui_setwindow(struct tboot_ui *ui, struct window *win,
		int x, int y, int width, int height, int rotate_arg)
{
	DFBWindowDescription desc;

	if (!ui || !ui->layer || !ui->dfb || !win) {
		printf("please init ui first\n");
		return -1;
	}
	if (width <=0 || height <=0) {
		printf("invalid width or height to init window\n");
		return -1;
	}

	/* create window */
	desc.flags = DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH |
		DWDESC_HEIGHT | DWDESC_CAPS;
	desc.posx = x > 0 ? x : 0;
	desc.posy = y > 0 ? y : 0;
	desc.width = width;
	desc.height = height;
	desc.caps = DWCAPS_ALPHACHANNEL;
	DFBCHECK (ui->layer->CreateWindow (ui->layer, &desc, &win->window));
	DFBCHECK (win->window->GetPosition (win->window, &win->x, &win->y));
	DFBCHECK (win->window->GetSize (win->window, &win->width, &win->height));
	/* fill window with window color */
	DFBCHECK (win->window->GetSurface (win->window, &win->surface));
	DFBCHECK (win->surface->SetFont (win->surface, win->font));
	DFBCHECK (win->surface->SetColor (win->surface, R(win->color),
				G(win->color), B(win->color), A(win->color)));
	DFBCHECK (win->surface->FillRectangle (win->surface, 0, 0, win->width, win->height));
	/* set window opacity */
	DFBCHECK (win->window->SetOpacity (win->window, A(win->color)));
	/* rotate before flip, so user can't aware of the rotate */
	if (rotate_arg)
		DFBCHECK (win->window->SetRotation (win->window, rotate_arg));
	/* Flip */
	DFBCHECK (win->surface->Flip (win->surface, NULL, DSFLIP_NONE));

	return 0;
}

/*
 * create menu item handlers
 */
static int reboot(void)
{
	printf("reboot\n");
	preos_reboot(PREOS_RB_RESTART2, 0, PREOS_RB_ARG_NORMALOS);
	return -1;
}

static int reboot_preos(void)
{
	printf("reboot to preos\n");
	preos_reboot(PREOS_RB_RESTART2, 0, PREOS_RB_ARG_PREOS);
	return -1;
}

static int poweroff(void)
{
	printf("poweroff\n");
	preos_reboot(PREOS_RB_POWEROFF, 0, NULL);
	return -1;
}

static int tboot_ui_menu(struct menu *menu)
{
	int ret;
	struct textline *line;
	int i;
	char *items[] = {
		"Power off",
		"Reboot to normal OS",
		"Reboot to Pre-OS (why?)",
		NULL,
	};
	menu_handler handlers[] = {
		poweroff,
		reboot,
		reboot_preos,
		NULL,
	};


	if (!menu) {
		printf("please init menu first\n");
		return -1;
	}

	for (i = 0; i < menu->max_items && items[i] && !ret; i++) {
		line = textline_init(LINE_LEN_INCHAR);
		line->color = menu->color;
		if (!strncpy(line->text, items[i], line->len)) {
			textline_free(line);
			ret = -1;
		}
		ret = menu_newitem(menu, line, handlers[i]);
	}
	menu->selected = 0;
	return ret;
}

/*
 * export a interface to create new menu item
 */
void tboot_ui_menu_item(const char *name, menu_handler handler)
{
	struct textline *line;
	struct menu *menu;

	if (!ui.menu) {
		printf("please init menu first\n");
		return;
	}

	menu = ui.menu;
	line = textline_init(LINE_LEN_INCHAR);
	line->color = menu->color;
	if (!strncpy(line->text, name, line->len)) {
		textline_free(line);
		return;
	}

	menu_newitem(menu, line, handler);
	menu_refresh(menu);
}

static int tboot_ui_adjust(struct tboot_ui *ui)
{
	DFBFontDescription font_dsc;
	struct window *win;
	struct textarea *ta_info;
	struct menu *menu;
	struct textarea *ta_log;
	struct textbar *tb;
	struct bar *bar;
	int x;
	int y;
	int width;
	int height;
	int rotate_arg;

	if (!ui) {
		printf("please init ui first\n");
		return -1;
	}

	if (ui->screen_width > ui->screen_height)
		rotate_arg = ROTATE_ARG;
	else
		rotate_arg = 0;

	/*
	 * create system info window
	 */
	win = tboot_ui_initwindow(ui, infowin_color, infowin_font_height);

	if (!rotate_arg)
		width = ui->screen_width;
	else
		width = ui->screen_height;
	height = INFO_LINES * win->font_height;

	if (tboot_ui_setwindow(ui, win, 0, 0, width, height, rotate_arg)) {
		printf("set window for system info failed\n");
		return -1;
	}
	/* left a line for uevent info */
	ta_info = textarea_init(win, 0, 0, win->width, win->height -
			infowin_font_height, TAF_TOPTODOWN | ~TAF_LIFO, COLOR_FG);
	if (!ta_info) {
		printf("create text area failed\n");
		return -1;
	}
	ui->ta_info = ta_info;

	/*
	 * create menu window
	 */
	if (!rotate_arg) {
		x = 0;
		y = win->y + win->height + window_separator_height;
		width = ui->screen_width;
	} else {
		x = win->x + win->height + window_separator_height;
		y = 0;
		width = ui->screen_height;
	}

	win = tboot_ui_initwindow(ui, menuwin_color, menuwin_font_height);
	height = (MENU_LINES + 1) * win->font_height;

	if (tboot_ui_setwindow(ui, win, x, y, width, height, rotate_arg)) {
		printf("setup window for menu failed\n");
		return -1;
	}

	/* dislay a notification */
	DFBCHECK (win->surface->SetColor (win->surface, R(MENU_TITLE_COLOR),
				G(MENU_TITLE_COLOR), B(MENU_TITLE_COLOR), A(MENU_TITLE_COLOR)));
	DFBCHECK (win->surface->DrawString (win->surface,
				"MENU (Vol up/down: select, Power: action)", -1,
				width/2, 0, DSTF_TOPCENTER));

	/* create menu */
	menu = menu_init(win, 0, win->font_height, win->width,
			 win->height, COLOR_FG, MENU_HL_BG_COLOR, MENU_HL_FG_COLOR);
	if (!menu) {
		printf("create menu failed\n");
		return -1;
	}
	/* init menu items */
	if (tboot_ui_menu(menu)) {
		printf("create menu items failed\n");
		menu_free(menu);
		return -1;
	}
	ui->menu = menu;
	menu_refresh(ui->menu);

	/*
	 * create text bar window at bottom of screen
	 */
	win = tboot_ui_initwindow(ui, barwin_color, barwin_font_height);

	height = win->font_height + component_separator_height + progress_bar_height;
	if (!rotate_arg) {
		x = 0;
		y = ui->screen_height - height;
		width = ui->screen_width;
	} else {
		x = ui->screen_width - height;
		y = 0;
		width = ui->screen_height;
	}

	if (tboot_ui_setwindow(ui, win, x, y, width, height, rotate_arg)) {
		printf("setup window for text bar failed\n");
		return -1;
	}
	/* create bar at the bottom center of text bar window*/
	x = win->width / 4;
	y = win->height - progress_bar_height;
	width = win->width / 2;
	bar = bar_init(x, y, width, progress_bar_height, BF_PROGRESS | BF_FILLOUT, 0,
			BAR_BG_COLOR, BAR_FG_COLOR, BAR_BORDER_WIDTH, BAR_BORDER_COLOR);
	if (!bar) {
		printf("create bar failed\n");
		return -1;
	}
	/* create textbar */
	tb = textbar_init(win, 0, 0, win->width, win->height,
			COLOR_FG, component_separator_height, bar);
	if (!tb) {
		printf("create text bar failed\n");
		return -1;
	}
	ui->tb = tb;

	/*
	 * create action log window
	 */
	height =ui->ta_info->win->height + window_separator_height +
		ui->menu->win->height + window_separator_height +
		ui->tb->win->height + window_separator_height;
	if (!rotate_arg) {
		x = 0;
		y = ui->menu->win->y + ui->menu->win->height + window_separator_height;
		width = ui->screen_width;
		height = ui->screen_height - height;
	} else {
		x = ui->menu->win->x + ui->menu->win->height + window_separator_height;
		y = 0;
		width = ui->screen_height;
		height = ui->screen_width - height;
	}

	win = tboot_ui_initwindow(ui, logwin_color, logwin_font_height);
	if (tboot_ui_setwindow(ui, win, x, y, width, height, rotate_arg)) {
		printf("setup window for action logs failed\n");
		return -1;
	}

	ta_log = textarea_init(win, 0, 0, win->width, win->height,
			~TAF_TOPTODOWN | TAF_LIFO, COLOR_FG);
	if (!ta_log) {
		printf("create text area failed\n");
		return -1;
	}

	ui->ta_log = ta_log;

	return 0;
}

/* config parser instance, there is only one config parser for tboot */
static struct config_parser *cp = NULL;

char *tboot_ui_getconfpath(void)
{
	return cp == NULL ? NULL : cp->path;
}

static int tboot_ui_load_config(const char *config, int width, int height)
{
	char *value;
	char fn[PATH_MAX];

	if (snprintf(font_file, sizeof(font_file), "%s", FONT_FILE) < 0) {
		pr_error("snprintf failed.\n");
		return -1;
	}

	if (snprintf(bg_image, sizeof(bg_image), BGIMG_PREFIX "_%d_%d.png",
				width, height) < 0) {
		pr_error("snprintf failed.\n");
		return -1;
	}

	/* get config file path */
	if (config)
		strncpy(fn, config, sizeof(fn));
	else
		snprintf(fn, sizeof(fn), CONF_PREFIX "_%d_%d.conf", width, height);

	if (cp && !strcmp(fn, cp->path))
		return 0;

	cp = config_parser_init(fn);
	if (cp == NULL) {
		pr_error("init config parser failed.\n");
		return -1;
	}

	/* get configured values */
	// colors
	value = config_parser_get(cp, WINDOW_COLOR_KEY);
	window_color = value ? strtol(value, NULL, 0) : WINDOW_DEF_COLOR;
	infowin_color = window_color;
	menuwin_color = window_color;
	logwin_color = window_color;
	barwin_color = window_color;

	value = config_parser_get(cp, INFOWIN_COLOR_KEY);
	if (value)
		infowin_color = strtol(value, NULL, 0);
	value = config_parser_get(cp, MENUWIN_COLOR_KEY);
	if (value)
		menuwin_color = strtol(value, NULL, 0);
	value = config_parser_get(cp, LOGWIN_COLOR_KEY);
	if (value)
		logwin_color = strtol(value, NULL, 0);
	value = config_parser_get(cp, BARWIN_COLOR_KEY);
	if (value)
		barwin_color = strtol(value, NULL, 0);

	// font height
	value = config_parser_get(cp, DEFAULT_FONT_HEIGHT_KEY);
	default_font_height = value ? strtol(value, NULL, 0) : FONT_HEIGHT;
	infowin_font_height = default_font_height;
	menuwin_font_height = default_font_height;
	logwin_font_height = default_font_height;
	barwin_font_height = default_font_height;

	value = config_parser_get(cp, INFOWIN_FONT_HEIGHT_KEY);
	if (value)
		infowin_font_height = strtol(value, NULL, 0);
	value = config_parser_get(cp, MENUWIN_FONT_HEIGHT_KEY);
	if (value)
		menuwin_font_height = strtol(value, NULL, 0);
	value = config_parser_get(cp, LOGWIN_FONT_HEIGHT_KEY);
	if (value)
		logwin_font_height = strtol(value, NULL, 0);
	value = config_parser_get(cp, BARWIN_FONT_HEIGHT_KEY);
	if (value)
		barwin_font_height = strtol(value, NULL, 0);

	// separator height
	value = config_parser_get(cp, COMPONENT_SEPARATOR_HEIGHT_KEY);
	component_separator_height = value ? strtol(value, NULL, 0) : INNER_SEPARATOR;
	value = config_parser_get(cp, WINDOW_SEPARATOR_HEIGHT_KEY);
	window_separator_height = value ? strtol(value, NULL, 0) : OUTTER_SEPARATOR;

	// progress bar height
	value = config_parser_get(cp, PROGRESS_BAR_HEIGHT_KEY);
	progress_bar_height = value ? strtol(value, NULL, 0) : BAR_HEIGHT;

	value = config_parser_get(cp, FONT_KEY);
	if (value)
		strncpy(font_file, value, sizeof(font_file));

	value = config_parser_get(cp, BG_IMAGE_KEY);
	if (value)
		strncpy(bg_image, value, sizeof(bg_image));

out:
	pr_debug("UI configuration\n");
	pr_debug("default window color: 0x%08x\n", window_color);
	pr_debug("sysinfo window color: 0x%08x\n", infowin_color);
	pr_debug("menu window color: 0x%08x\n", menuwin_color);
	pr_debug("log window color: 0x%08x\n", logwin_color);
	pr_debug("progress bar window color: 0x%08x\n", barwin_color);
	pr_debug("default font height: %u\n", default_font_height);
	pr_debug("sysinfo window font height: %u\n", infowin_font_height);
	pr_debug("menu window font height: %u\n", menuwin_font_height);
	pr_debug("log window font height: %u\n", logwin_font_height);
	pr_debug("progress bar window font height: %u\n", barwin_font_height);
	pr_debug("component separator height: %u\n", component_separator_height);
	pr_debug("window separator height: %u\n", window_separator_height);
	pr_debug("progress bar height: %u\n", progress_bar_height);
	pr_debug("font file: %s\n", font_file);
	pr_debug("background image: %s\n", bg_image);

	return 0;
}

int tboot_ui_init(const char *config)
{
	DFBGraphicsDeviceDescription gdesc;
	DFBSurfaceDescription dsc;
	IDirectFBImageProvider *imgprovider;
	DFBFontDescription font_dsc;
	int ret;

	/*
	 * init
	 */
	DFBCHECK (DirectFBInit (NULL, NULL));
	DFBCHECK (DirectFBCreate (&(ui.dfb)));

	/*
	 * get primary layer
	 */
	DFBCHECK (ui.dfb->GetDisplayLayer (ui.dfb, DLID_PRIMARY, &ui.layer));
	DFBCHECK (ui.layer->SetCooperativeLevel(ui.layer, DLSCL_ADMINISTRATIVE));
	DFBCHECK (ui.dfb->GetDeviceDescription (ui.dfb, &gdesc));
	/* device support blending alpha? */
	if (!((gdesc.blitting_flags & DSBLIT_BLEND_ALPHACHANNEL) &&
				(gdesc.blitting_flags & DSBLIT_BLEND_COLORALPHA))) {
		/* if not, use system memory */
		ui.layer_config.flags = DLCONF_BUFFERMODE;
		ui.layer_config.buffermode = DLBM_BACKSYSTEM;
		DFBCHECK (ui.layer->SetConfiguration (ui.layer, &ui.layer_config));
	}
	/* store layer configuration */
	DFBCHECK (ui.layer->GetConfiguration (ui.layer, &ui.layer_config));
	DFBCHECK (ui.layer->EnableCursor (ui.layer, 0));
	/* store screen geometry */
	ui.screen_width = ui.layer_config.width;
	ui.screen_height = ui.layer_config.height;

	/*
	 * load background image
	 */
	if (tboot_ui_load_config(config, ui.screen_width, ui.screen_height)) {
		tboot_ui_exit();
		return -1;
	}

	if (!access(bg_image, R_OK)) {
		dsc.flags = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_CAPS;
		dsc.width = ui.screen_width;
		dsc.height = ui.screen_height;
		dsc.caps = DSCAPS_SHARED;
		DFBCHECK (ui.dfb->CreateSurface (ui.dfb, &dsc, &ui.bg_sur));
		DFBCHECK (ui.dfb->CreateImageProvider (ui.dfb, bg_image, &imgprovider));
		DFBCHECK (imgprovider->RenderTo (imgprovider, ui.bg_sur, NULL));
		imgprovider->Release(imgprovider);
		DFBCHECK (ui.layer->SetBackgroundImage (ui.layer, ui.bg_sur));
		DFBCHECK (ui.layer->SetBackgroundMode (ui.layer, DLBM_IMAGE));
	} else {
		DFBCHECK (ui.layer->SetBackgroundColor (ui.layer, R(COLOR_BG),
					G(COLOR_BG), B(COLOR_BG), A(COLOR_BG)));
		DFBCHECK (ui.layer->SetBackgroundMode (ui.layer, DLBM_COLOR));
	}

	ret = tboot_ui_adjust(&ui);
	if (ret) {
		printf("create UI components failed\n");
		tboot_ui_exit();
		return -1;
	}

	return 0;
}

void tboot_ui_exit(void)
{
	/* destroy config parser */
	config_parser_free(cp);

	/* destroy info window */
	if (ui.ta_info)
		textarea_free(ui.ta_info);

	/* destroy menu window */
	if (ui.menu)
		menu_free(ui.menu);

	/* destroy text bar window */
	if (ui.tb)
		textbar_free(ui.tb);

	/* destroy action logs window */
	if (ui.ta_log)
		textarea_free(ui.ta_log);

	/* release background image */
	if (ui.bg_sur)
		ui.bg_sur->Release(ui.bg_sur);

	/* release others */
	if (ui.layer)
		ui.layer->Release(ui.layer);
	if (ui.dfb)
		ui.dfb->Release(ui.dfb);
}
