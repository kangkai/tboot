#include <stdio.h>
#include <stdlib.h>
#include "backlight_control.h"
#include "debug.h"
#include "tboot.h"

/*
 * Currently, all supported platform has the same below files
 * It should be change to a more flexible solution.
 */
#define FILE_FB0_BLANK "/sys/class/graphics/fb0/blank"
#define FILE_MAX_BRIGHTNESS "/sys/class/backlight/psb-bl/max_brightness"
#define FILE_BRIGHTNESS "/sys/class/backlight/psb-bl/brightness"

static int lcd_lighton(void);
static int lcd_lightoff(void);
static int set_bl_brightness(int percent);

lcd_state_ret_t lcd_state_on(enum lcd_event ev)
{
	lcd_state_ret_t cur_state = (lcd_state_ret_t)lcd_state_on;
	pr_debug("in lcd_state_on\n");

	switch (ev) {
		case LCD_IDLE:
			cur_state = (lcd_state_ret_t)lcd_state_dim;
			pr_debug("do lcd dim\n");
			set_bl_brightness(atoi(tboot_config_get(LCD_DIM_BRIGHTNESS_KEY)));
			break;
		default:
			break;
	}

	return cur_state;
}

lcd_state_ret_t lcd_state_dim(enum lcd_event ev)
{
	lcd_state_ret_t cur_state = (lcd_state_ret_t)lcd_state_dim;
	pr_debug("in lcd_state_dim\n");

	switch (ev) {
		case LCD_IDLE:
			cur_state = (lcd_state_ret_t)lcd_state_off;
			pr_debug("do light off\n");
			lcd_lightoff();
			break;
		case LCD_COMMAND:
		case LCD_KEY:
		case LCD_USB_EVENT:
			cur_state = (lcd_state_ret_t)lcd_state_on;
			pr_debug("do light off\n");
			lcd_lighton();
			break;
		default:
			break;
	}

	return cur_state;
}

lcd_state_ret_t lcd_state_off(enum lcd_event ev)
{
	lcd_state_ret_t cur_state = (lcd_state_ret_t)lcd_state_off;
	pr_debug("in lcd_state_off\n");

	switch(ev) {
		case LCD_COMMAND:
		case LCD_KEY:
		case LCD_USB_EVENT:
			cur_state = (lcd_state_ret_t)lcd_state_on;
			pr_debug("do light on\n");
			lcd_lighton();
			break;
		default:
			break;
	}

	return cur_state;
}

static int write_fb0_blank(int nu)
{
	FILE *fp = NULL;

	fp = fopen(FILE_FB0_BLANK, "w");
	if (fp == NULL) {
		pr_debug("open %s failed.\n", FILE_FB0_BLANK);
		return -1;
	}

	fprintf(fp, "%d", nu);
	fclose(fp);
	return 0;
}

static int set_bl_brightness(int percent)
{
	int bri = 1;
	int max_bri = 0;
	FILE *fp = NULL;

	if (percent < 0 || percent > 100)
		return -1;

	fp = fopen(FILE_MAX_BRIGHTNESS, "r");
	if (fp == NULL) {
		pr_debug("open %s failed.\n", FILE_MAX_BRIGHTNESS);
		return -1;
	}
	if (1 == fscanf(fp, "%d", &max_bri) && max_bri > 0)
		bri = max_bri * percent / 100;
	fclose(fp);

	fp = fopen(FILE_BRIGHTNESS, "w");
	if (fp == NULL) {
		pr_debug("open %s failed.\n", FILE_BRIGHTNESS);
		return -1;
	}
	fprintf(fp, "%d", bri);
	fclose(fp);
	return 0;
}

static int lcd_lighton(void)
{
	set_bl_brightness(100);
	return write_fb0_blank(0);
}

static int lcd_lightoff(void)
{
	return write_fb0_blank(1);
}
