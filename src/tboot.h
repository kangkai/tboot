#ifndef _TBOOT_H_
#define _TBOOT_H_

#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <linux/limits.h>

#include "fstab.h"
#include "backlight_control.h"

#define MEGABYTE	(1024 * 1024)

void disable_autoboot(void);
void start_kernel(const char *kernel);

/* Inspect a volume looking for an automatic SW update. If it's
 * there, provision filesystems and apply it. */
int try_update_sw(Volume *vol, int use_countdown);

/* global libdiskconfig data structure representing the intended layout of
 * the internal disk, as read from /etc/disk_layout.conf */
extern struct disk_info *disk_info;

/* Serialize all disk operations. Grabbed by fastboot any time it is
 * performing a command, and also any worker thread handlers */
extern pthread_mutex_t action_mutex;

/* If set, apply this update on 'fastboot continue' */
extern char *g_update_location;

/* If non-NULL, run this during provisioning checks, which are
 * performed before automatic update packages are applied */
void set_platform_provision_function(int (*fn)(void));

int enable_keypress(void);
int disable_keypress(void);

/* ignore commands when battery capacity is lower than */
#define BATTERY_THRESHOLD	10
int check_battery(void);

#define PREOS_VERSION_LEN	32
#define PREOS_VERSION_LOCATION	"/etc/VERSION"
extern char preos_version[PREOS_VERSION_LEN];

#define PREOS_FSTAB_LOCATION	"/etc/preos.fstab"

/* In disk_layout.conf */
#define CACHE_PTN		"cache"
#define DATA_PTN		"userdata"
#define PLATFORM_PTN		"platform"

/* Volume entry in preos.fstab for the SD card */
//#define SDCARD_VOLUME		"/sdcard"
#define CACHE_VOLUME		"/cache"

/*
 * configuration options for tboot
 *
 * key, type, comment
 * ui_conf, string, specifies tboot UI config path.
 * disk_conf, string, specifies disk layout config path.
 * disk_force, [yes|no], specifies force write partition table.
 * push_dir, string, specifies directory on target device to hosts
 *		files pushed by 'oem push'.
 * battery_threshold, decimal integer number, specifies the smallest
 *		battery capacity to enable fastboot operations.
 */

/* tboot config keys */
#define UI_CONF_KEY "ui_conf"
#define DISK_CONFIG_KEY "disk_conf"
#define PUSH_DIR_KEY "push_dir"
#define ROOTFS_KEY "rootfs"
#define ENABLE_NFS_KEY "enable_nfs"
#define NFSURL_KEY "nfsurl"
#define DISK_FORCE_KEY "disk_force"
#define BAT_THRESHOLD_KEY "battery_threshold"
#define LCD_DIM_TIMEOUT_KEY "lcd_dim_timeout"
#define LCD_OFF_TIMEOUT_KEY "lcd_off_timeout"
#define LCD_DIM_BRIGHTNESS_KEY "lcd_dim_brightness"

char *tboot_config_get(char *key);
char *tboot_config_set(char *key, char *value);
void tboot_config_dump(void);
void tboot_config_keywords(void);

#define RESET_ALARM \
	do { \
		alarm(0); \
		alarm(atoi(tboot_config_get(LCD_DIM_TIMEOUT_KEY))); \
	} while (0)

#define CLEAR_ALARM \
	do { \
		alarm(0); \
	} while (0)

/* for lcd backlight control */
lcd_state_t lcd_state;

/* tell reader and writer if error occurred */
int pipe_broken;

#endif
