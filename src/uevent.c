#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>

#include "debug.h"
#include "tboot_ui.h"
#include "platform.h"
#include "uevent.h"

#define HOTPLUG_BUFFER_SIZE		1024
#define OBJECT_SIZE			512

#define PATH_MAX			256

/*
 * we currently monitor these events:
 *	1. USB connect/disconnect
 *	2. Battery meter
 *	3. Charger connect/disconnect
 *
 * Uevent example:
 * USB: change@/devices/virtual/usb_mode/usb0
 * Battery: change@/devices/pci0000:00/0000:00:00.4/i2c-1/1-0036/power_supply/max17042_battery
 * Charger: change@/devices/pci0000:00/0000:00:00.4/i2c-1/1-0006/power_supply/smb347-mains
 */

char *uevent_action[] = {
	"change",
#if 0
	"add",
	"remove",
#endif
	NULL
};

enum device_type {
	TYPE_UNKNOWN	= 0,
	TYPE_USB	= 1,
	TYPE_BATTERY	= 2,
	TYPE_CHARGER	= 3
};

struct device_mapping {
	PLATFORM_T plat;
	enum device_type type;
	char *path;
	char *file;
};

struct device_mapping uevent_path[] = {
	/* PR3 old (UMG) kernel interfaces */
	{
		PLATFORM_BLACKBAY,
		TYPE_USB,
		"/devices/virtual/usb_mode/usb0",
		"state"
	},
	{
		PLATFORM_BLACKBAY,
		TYPE_BATTERY,
		"/devices/pci0000:00/0000:00:00.4/i2c-1/1-0036/power_supply/max17042_battery",
		"capacity"
	},
	{
		PLATFORM_BLACKBAY,
		TYPE_CHARGER,
		"/devices/platform/intel_msic/msic_battery/power_supply/msic_charger",
		"online"
	},

	/* PR3 new (MCG/UMG) kernel interfaces */
	{
		PLATFORM_BLACKBAY,
		TYPE_USB,
		"/devices/virtual/usb_mode/usb0",
		"state"
	},
	{
		PLATFORM_BLACKBAY,
		TYPE_BATTERY,
		"/devices/pci0000:00/0000:00:00.4/i2c-1/1-0036/power_supply/max170xx_battery",
		"capacity"
	},
	{
		PLATFORM_BLACKBAY,
		TYPE_CHARGER,
		"/devices/ipc/msic_battery/power_supply/msic_charger",
		"online"
	},

	/* DV10 old (OTC) kernel interfaces */
	{
		PLATFORM_REDRIDGE,
		TYPE_USB,
		"/devices/virtual/usb_mode/usb0",
		"state"
	},
	{
		PLATFORM_REDRIDGE,
		TYPE_BATTERY,
		"/devices/pci0000:00/0000:00:00.4/i2c-1/1-0036/power_supply/max17042_battery",
		"capacity"
	},
	{
		PLATFORM_REDRIDGE,
		TYPE_CHARGER,
		"/devices/pci0000:00/0000:00:00.4/i2c-1/1-0006/power_supply/smb347-mains",
		"online"
	},

	/* DV10 new (MCG since WW19) kernel interfaces */
	{
		PLATFORM_REDRIDGE,
		TYPE_BATTERY,
		"/devices/pci0000:00/0000:00:00.4/i2c-1/1-0036/power_supply/max170xx_battery",
		"capacity"
	},

	/* RedHookBay (MCG 2013_WW05 week release) kernel interfaces */
	{
		PLATFORM_REDHOOKBAY,
		TYPE_USB,
		"/devices/virtual/usb_mode/usb0",
		"state"
	},
	{
		PLATFORM_REDHOOKBAY,
		TYPE_BATTERY,
		"/devices/pci0000:00/0000:00:00.5/i2c-2/2-0036/power_supply/max17047_battery",
		"capacity"
	},
	{
		PLATFORM_REDHOOKBAY,
		TYPE_CHARGER,
		"/devices/pci0000:00/0000:00:00.5/i2c-2/2-006b",
		"online"
	},
	/* end */
	{type:	TYPE_UNKNOWN,}
};

static struct uevent_status {
	int usb_status;
	int charger_status;
	int battery_capacity;
} uevent_status;

int usb_status(void)
{
	return uevent_status.usb_status;
}

int charger_status(void)
{
	return uevent_status.charger_status;
}

int battery_capacity(void)
{
	return uevent_status.battery_capacity;
}

static void parse_uevent(char *uevent)
{
	int ret;
	char *p;
	int i = 0;
	char path[PATH_MAX];
	char buf[16];
	size_t nbytes;
	ssize_t n;
	int fd;
	char *action = uevent_action[0];
	struct device_mapping *dm = &uevent_path[0];
	PLATFORM_T plat = get_current_platform();

	for (i = 0; action != NULL;) {
		if (0 == strncmp(action, uevent, strlen(action)))
			break;
		action = uevent_action[++i];
	}
	if (action == NULL)
		return;

	p = strchr(uevent, '@');
	p++;

	for (i = 0; dm->type != TYPE_UNKNOWN;) {
		if (dm->plat == plat &&
		    0 == strncmp(dm->path, p, strlen(dm->path)))
			break;
		dm = &uevent_path[++i];
	}

	if (dm->type == TYPE_UNKNOWN)
		return;

	snprintf(path, PATH_MAX, "/sys%s/%s", dm->path, dm->file);

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		pr_perror("parse_uevent, open");
		return;
	}
	nbytes = sizeof(buf);
	n = read(fd, buf, nbytes - 1);
	if (n < 0) {
		pr_perror("parse_uevent, read");
		close(fd);
		return;
	}
	buf[n] = '\0';
	pr_verbose("%s, %d: %s\n", path, n, buf);

	if (dm->type == TYPE_USB) {
		if (0 == strncmp(buf, "CONFIGURED", 10)) {
			uevent_status.usb_status = STATUS_CONNECTED;
		} else if (0 == strncmp(buf, "DISCONNECTED", 12)) {
			uevent_status.usb_status = STATUS_DISCONNECTED;
		} else {
			uevent_status.usb_status = STATUS_UNKOWN;
		}
	} else if (dm->type == TYPE_CHARGER) {
		if (atoi(buf))
			uevent_status.charger_status = STATUS_CONNECTED;
		else
			uevent_status.charger_status = STATUS_DISCONNECTED;

	} else if (dm->type == TYPE_BATTERY) {
		uevent_status.battery_capacity = atoi(buf);
	}
}

static void uevent_status_init(void)
{
	struct device_mapping *dm = &uevent_path[0];
	char buf[PATH_MAX];
	int i;
	PLATFORM_T plat = get_current_platform();

	for (i = 0; dm->type != TYPE_UNKNOWN;) {
		if (dm->plat == plat) {
			snprintf(buf, sizeof(buf), "change@%s", dm->path);
			parse_uevent(buf);
		}

		dm = &uevent_path[++i];
	}
}

void *uevent_thread(void *arg)
{
	int sock_fd;
	struct sockaddr_nl snl;
	int ret;

	sock_fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
	if (sock_fd == -1) {
		pr_perror("Couldn't open kobject-uevent netlink socket");
		return NULL;
	}

	memset(&snl, 0, sizeof(struct sockaddr_nl));
	snl.nl_family = AF_NETLINK;
	snl.nl_pid = getpid();
	snl.nl_groups = 1;

	ret = bind(sock_fd, (struct sockaddr *) &snl,
		   sizeof(struct sockaddr_nl));
	if (ret < 0) {
		pr_perror("Error binding to netlink socket");
		close(sock_fd);
		return NULL;
	}

	uevent_status_init();
	/* invoke call back function */
	if (arg)
		((uevent_callback)arg)(NULL);

	while (1) {
		char buffer[HOTPLUG_BUFFER_SIZE + OBJECT_SIZE];
		int buflen;

		buflen = recv(sock_fd, &buffer, sizeof(buffer), 0);
		if (buflen < 0) {
			break;
		}

		if (buflen < strlen("a@/d")) {
			pr_warning("invalid message length\n");
			continue;
		}

		if (strstr(buffer, "@/") == NULL) {
			pr_warning("invalid message length\n");
			continue;
		}

		pr_verbose("%s\n", buffer);
		parse_uevent(buffer);

		/* invoke call back function */
		if (arg)
			((uevent_callback)arg)(NULL);
	}

	pr_warning("%s quit\n", __func__);
	close(sock_fd);
	return NULL;
}
