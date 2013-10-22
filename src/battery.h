#ifndef __BATTERY_H
#define __BATTERY_H

struct battery {
	char **dirs;		/* battery sysfs interface */
	int wd_index;		/* which directory contains the battery capacity */
	int nr_dirs;		/* number of battery directories */
	int charge_full_design;	/* design charge value */
	int charge_empty_design;
	int charge_full;	/* last remembered value of charge when
				   battery suggests full */
	int charge_empty;
	int charge_now;
	char *status;		/* charging, full, discharging and etc. */
	int capacity;		/* percent capacity */
};

struct battery *battery_init(void);
int battery_status(struct battery *bat);
void battery_free(struct battery *bat);

#endif
