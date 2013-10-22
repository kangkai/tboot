#ifndef _THEME_H_
#define _THEME_H_

/* colors */
#define COLOR_BLACK		0x000000ff	// black
#define COLOR_GREY1		0x0a0a0aff	// dark grey
#define COLOR_GREY		0x333333ff	// grey
#define COLOR_WHITE		0xffffffff	// white
#define COLOR_RED		0xff0000ff	// red
#define COLOR_YELLOW		0xffff00ff	// yellow
#define COLOR_GREEN		0x00ff00ff	// green
#define COLOR_ORANGE0		0xecdcc4ff	// light organge yellow
#define COLOR_ORANGE1		0xf7931eff	// organge yellow
#define COLOR_ORANGE2		0xf16522ff	// organge2
#define COLOR_ORANGE3		0xe74922ff	// organge3
#define COLOR_ORANGE4		0xf42414ff	// organge red


/* --------- theme ----------- */
#define COLOR_BG		COLOR_BLACK	// background color
#define COLOR_FG		COLOR_WHITE	// foreground color

/* log color */
#define COLOR_INFO		COLOR_FG
#define COLOR_WARN		COLOR_ORANGE1
#define COLOR_ERROR		COLOR_ORANGE4

/* font height */
#define FONT_HEIGHT		20

/* separator height */
#define OUTTER_SEPARATOR	20
#define INNER_SEPARATOR		5

/* bar config */
#define BAR_HEIGHT		6
#define BAR_BG_COLOR		COLOR_GREY
#define BAR_FG_COLOR		COLOR_ORANGE3
#define BAR_BORDER_COLOR	COLOR_BG
#define BAR_BORDER_WIDTH	1

/* menu */
#define MENU_TITLE_COLOR	COLOR_ORANGE2
#define MENU_HL_FG_COLOR	COLOR_BG
#define MENU_HL_BG_COLOR	COLOR_ORANGE2

#define INFO_LINES		3
#define MENU_LINES		7

#define ROTATE_ARG		90

/*
 * default values
 */
#define WINDOW_DEF_COLOR	0x1111117f

#endif
