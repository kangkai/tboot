#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "battery.h"
#include "libminui/minui.h"

/*
 * battery sprite load posistion, the others should be configured in fbsplash
 * config file, just the same as Pre-OS mode. So there will be two fbsplash
 * config files, one for Pre-OS, one for Charging mode.
 */
#define BAT_SPRITE_POSX		50
#define BAT_SPRITE_POSY		200
#define BAT_SPRITE_START	1
#define BAT_SPRITE_END		3
#define BAT_SPRITE_TIMEOUT	10	/* value in 100ms */

/* Charging mode sprite file */
#define BATTERY_SPRITE_FILE	"/etc/fbsplash/fbcharging-sprite.ppm.bz2"

void charging_mode(const char *sprite_file)
{
	struct battery *bat;
	int capacity;
	char buf[32];

	/* init UI
	if (fbsplash_loadsprite(sprite_file, BAT_SPRITE_POSX, BAT_SPRITE_POSY)
			&& fbsplash_loadsprite(BATTERY_SPRITE_FILE,
				BAT_SPRITE_POSX, BAT_SPRITE_POSY)) {
		pr_error("failed to load sprite file\n");
		return;
	} */
	//fbsplash_animation(BAT_SPRITE_START, BAT_SPRITE_END, BAT_SPRITE_TIMEOUT);

	/* show battery infomation */
	bat = battery_init();
	if (!bat)
		return;
	/* wait for the fist through of battery_status() can get
	 * the correct battery status
	 */
	sleep(3);
	while (1) {
		if(!battery_status(bat) && (bat->capacity != capacity ||
					strcmp(bat->status, buf))) {
			capacity = bat->capacity;
			strncpy(buf, bat->status, sizeof(buf));
			/*
			fbsplash_progress_text(FBBAR_EMPTY, TC_NONE "%s ... "
					TC_GREEN "%d%%", bat->status, bat->capacity);
			*/
			
			pr_debug("battery status: %s, battery capacity:"
					"%d\n", bat->status, bat->capacity);
		}
		/* one time per minute */
		sleep(60);
	}
	battery_free(bat);
}
