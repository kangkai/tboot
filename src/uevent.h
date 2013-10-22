#ifndef __UEVENT_H
#define __UEVENT_H

enum status {
	STATUS_DISCONNECTED,
	STATUS_CONNECTED,
	STATUS_UNKOWN,
};

/* call back function to update UI */
typedef void (*uevent_callback)(void *);

/* functions to get uevent status */
int usb_status(void);
int charger_status(void);
int battery_capacity(void);

#endif
