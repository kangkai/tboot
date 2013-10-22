#ifndef __BACKLIGHT_CONTROL_H
#define __BACKLIGHT_CONTROL_H

/*
 * there are three states: on, dim and off
 * and several types of event: COMMAND, KEY, IDLE and USB_EVENT
 * and the state machine looks like
 *
 * on --> LCD_IDLE --> dim
 * dim --> LCD_IDLE --> off
 * dim --> LCD_COMMAND --> on
 * dim --> LCD_KEY --> on
 * dim --> LCD_USB_EVENT --> on
 * off --> LCD_COMMAND --> on
 * off --> LCD_KEY --> on
 * off --> LCD_USB_EVENT --> on
 */

enum lcd_event {
	LCD_COMMAND,
	LCD_KEY,
	LCD_IDLE,
	LCD_USB_EVENT
};

typedef void (*lcd_state_ret_t)(enum lcd_event);
typedef lcd_state_ret_t (*lcd_state_t)(enum lcd_event);

lcd_state_ret_t lcd_state_on(enum lcd_event ev);
lcd_state_ret_t lcd_state_dim(enum lcd_event ev);
lcd_state_ret_t lcd_state_off(enum lcd_event ev);

#endif
