#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "debug.h"
#include "battery.h"

#define POWER_SUPPLY_DIR	"/sys/class/power_supply"

/* keys in uevent */
#define CHARGE_FULL_DESIGN	"POWER_SUPPLY_CHARGE_FULL_DESIGN"
#define CHARGE_EMPTY_DESIGN	"POWER_SUPPLY_CHARGE_EMPTY_DESIGN"
#define CHARGE_FULL		"POWER_SUPPLY_CHARGE_FULL"
#define CHARGE_EMPTY		"POWER_SUPPLY_CHARGE_EMPTY"
#define CHARGE_NOW		"POWER_SUPPLY_CHARGE_NOW"
#define BATTERY_STATUS		"POWER_SUPPLY_STATUS"
#define BATTERY_CAPACITY	"POWER_SUPPLY_CAPACITY"

#define BUF_SIZE	4096

/*
 * find battery dir in POWER_SUPPLY_DIR
 * I have encountered a device with two battery directories
 * and each directory contains partial contents
 */
static void battery_init_dirs(struct battery *bat)
{
	DIR *dirp;
	struct dirent *dentp;
	char path[PATH_MAX];
	char line[BUF_SIZE];
	FILE *fp;
	int i = 0;

	if (!bat || !bat->dirs) {
		pr_error("please init bat first\n");
		return;
	}
	dirp = opendir(POWER_SUPPLY_DIR);
	if (!dirp) {
		pr_error("failed to open " POWER_SUPPLY_DIR "\n");
		return;
	}

	/* traversal POWER_SUPPLY_DIR */
	while (dentp = readdir(dirp)) {
		if (!strcmp(dentp->d_name, ".") || !strcmp(dentp->d_name, ".."))
			continue;
		if (snprintf(path, PATH_MAX, POWER_SUPPLY_DIR "/%s/type",
					dentp->d_name) < 0) {
			pr_error("construct string failed\n");
			continue;
		}
		pr_debug("got path:%s\n", path);

		/* check if contents of path is "Battery" */
		fp = fopen(path, "r");
		if (!fp) {
			pr_error("open file %s failed\n", path);
			continue;
		}
		if (!fgets(line, sizeof(line), fp)) {
			pr_error("read file %s failed\n", path);
			fclose(fp);
			continue;
		}
		if (strncmp(line, "Battery", 7)) {
			fclose(fp);
			continue;
		}
		fclose(fp);
		/* got it, add to bat->dirs */

		/* extend array if full */
		if (bat->dirs[bat->nr_dirs - 1]) {
			pr_verbose("battery dirs full\n");
			bat->nr_dirs *= 2;
			bat->dirs = realloc(bat->dirs, sizeof(char *) * bat->nr_dirs);
			if (!bat->dirs) {
				pr_error("out of memory\n");
				closedir(dirp);
				return;
			}
		}
		while (bat->dirs[i])
			i++;
		/* got an empty slot */
		bat->dirs[i] = strndup(path, strlen(path) - 5);
	}
	closedir(dirp);
	for (i = 0; i < bat->nr_dirs && bat->dirs[i]; i++)
		pr_debug("battery dir[%d]: %s\n", i, bat->dirs[i]);
}

struct battery *battery_init(void)
{
	struct battery *bat;
	bat = malloc(sizeof(*bat));
	if (!bat) {
		pr_error("out of memory\n");
		return NULL;
	}
	bat->charge_full_design = -1;
	bat->charge_empty_design = 0;
	bat->charge_full = -1;
	bat->charge_empty = 0;
	bat->charge_now = -1;
	bat->status = NULL;
	bat->capacity = -1;
	bat->nr_dirs = 1;	/* in most of case, only one battery directory */
	bat->dirs = malloc(sizeof(char *) * bat->nr_dirs);
	if (!bat->dirs) {
		pr_error("out of memory\n");
		free(bat);
		return NULL;
	}
	battery_init_dirs(bat);
	return bat;
}

void battery_free(struct battery *bat)
{
	int i = 0;
	if (!bat)
		return;
	if (bat->dirs) {
		while (bat->dirs[i] && i < bat->nr_dirs) {
			free(bat->dirs[i]);
			i++;
		}
		free(bat->dirs);
	}
	if (bat->status)
		free(bat->status);
	free(bat);
}

/* get value of key, the format is key=value */
static char *key_value(const char *buf, const char *key)
{
	char *strp;
	int ret;
	if (!buf || !key) {
		return NULL;
	}
	strp = strstr(buf, key);
	if (!strp)
		return NULL;
	strp += strlen(key);
	strp = strchr(strp, '=');
	if (!strp)
		return;
	strp++;
	if (*(strp + strlen(strp) - 1) == '\n')
		*(strp + strlen(strp) - 1) = '\0';
	return strndup(strp, strlen(strp));
}

/* try to find battery capacity in bat->dirs */
int battery_status(struct battery *bat)
{
	char uevent_path[PATH_MAX];
	char *battery_dir;
	char line[BUF_SIZE];
	char *value;
	FILE *fp;

	if (!bat || !bat->dirs)
		return -1;
	while (bat->dirs[bat->wd_index]) {
		/* traversal all battery directories to get battery status */
		battery_dir = bat->dirs[bat->wd_index];
		pr_verbose("battery dir: %s\n", battery_dir);
		if (snprintf(uevent_path, PATH_MAX, "%s/uevent", battery_dir) < 0) {
			pr_error("construct string failed\n");
			return -1;
		}

		fp = fopen(uevent_path, "r");
		if (!fp) {
			pr_error("open file %s failed\n", uevent_path);
			return -1;
		}
		/* read value from uevent */
		while (fgets(line, sizeof(line), fp)) {
			/* get lines we want, even it may be not present */
			if (value = key_value(line, BATTERY_STATUS)) {
				/* has status line */
				bat->status = value;
				if (!strncmp(value, "Full", 4)) {
					/* status line suggests it full charged */
					pr_debug("battery is full charged\n");
					bat->capacity = 100;
					fclose(fp);
					return 0;
				}
				continue;
			} else if (value = key_value(line, BATTERY_CAPACITY)) {
				/* has capacity line */
				bat->capacity = atoi(value);
				/* continue to get battery status */
				if (bat->status) {
					fclose(fp);
					return 0;
				}
			} else if (value = key_value(line, CHARGE_FULL_DESIGN)) {
				bat->charge_full_design = atoi(value);
			} else if (value = key_value(line, CHARGE_FULL)) {
				bat->charge_full = atoi(value);
			} else if (value = key_value(line, CHARGE_EMPTY_DESIGN)) {
				bat->charge_empty_design = atoi(value);
			} else if (value = key_value(line, CHARGE_EMPTY)) {
				bat->charge_empty = atoi(value);
			} else if (value = key_value(line, CHARGE_NOW)) {
				bat->charge_now = atoi(value);
			}
			free(value);
		}
		fclose(fp);
		if (bat->capacity < 0) {
			/* calculate capacity */
			pr_debug("Battery: charge_full_design(%d), charge_empty_design(%d),"
					"charge_full(%d), charge_empty(%d), charge_now(%d)\n",
					bat->charge_full_design, bat->charge_empty_design,
					bat->charge_full, bat->charge_empty, bat->charge_now);
			if (bat->charge_full <= 0 || bat->charge_now <=0 ||
					bat->charge_full == bat->charge_empty) {
				bat->wd_index++;
				continue;
			}
			bat->capacity =  bat->charge_now * 100 /
				(bat->charge_full - bat->charge_empty);
			pr_verbose("got battery capacity: %d\n", bat->capacity);
		}
		if (bat->capacity < 0) {
			bat->wd_index++;
			continue;
		}
		if (!bat->status) {
			/* if we got battery capacity here, but can't
			 * get battery status.
			 */
			pr_debug("The battery driver sucks, can't get"
					" battery status!\n");
			bat->status = strdup("Unknown");
		}
		return 0;
	}
	return -1;
}
