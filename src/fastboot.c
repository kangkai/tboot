/*
 * Copyright (c) 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#define LOG_TAG "fastboot"

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>

#include "tboot.h"
#include "debug.h"
#include "fastboot.h"
#include "tboot_util.h"
#include "tboot_ui.h"

struct fastboot_cmd {
	struct fastboot_cmd *next;
	const char *prefix;
	unsigned prefix_len;
	void (*handle) (const char *arg, void *data, unsigned sz);
};

struct fastboot_var {
	struct fastboot_var *next;
	const char *name;
	const char *value;
};

static struct fastboot_cmd *cmdlist;

void fastboot_register(const char *prefix,
		       void (*handle) (const char *arg, void *data,
				       unsigned sz))
{
	struct fastboot_cmd *cmd;
	cmd = malloc(sizeof(*cmd));
	if (cmd) {
		cmd->prefix = prefix;
		cmd->prefix_len = strlen(prefix);
		cmd->handle = handle;
		cmd->next = cmdlist;
		cmdlist = cmd;
	}
}

static struct fastboot_var *varlist;

void fastboot_publish(const char *name, const char *value)
{
	struct fastboot_var *var;
	var = malloc(sizeof(*var));
	if (var) {
		var->name = name;
		var->value = value;
		var->next = varlist;
		varlist = var;
	}
}

const char *fastboot_getvar(const char *name)
{
	struct fastboot_var *var;
	for (var = varlist; var; var = var->next)
		if (!strcmp(name, var->name))
			return (var->value);
	return NULL;
}

static unsigned char buffer[4096];

static void *download_base;
static unsigned download_max;
static unsigned download_size;

#define STATE_OFFLINE	0
#define STATE_COMMAND	1
#define STATE_COMPLETE	2
#define STATE_ERROR	3

static unsigned fastboot_state = STATE_OFFLINE;
int fb_fp = -1;
int enable_fp;

static int usb_read(void *_buf, unsigned len)
{
	int r = 0;
	unsigned xfer;
	unsigned char *buf = _buf;
	int count = 0;

	if (fastboot_state == STATE_ERROR)
		goto oops;

	/* pr_verbose("usb_read %d\n", len); */
	while (len > 0) {
		xfer = (len > 4096) ? 4096 : len;

		r = read(fb_fp, buf, xfer);
		if (r < 0) {
			pr_perror("read");
			goto oops;
		}

		count += r;
		buf += r;
		len -= r;

		/* short transfer? */
		if ((unsigned int)r != xfer)
			break;
	}
	/* pr_verbose("usb_read complete\n"); */
	return count;

oops:
	fastboot_state = STATE_ERROR;
	return -1;
}

static int usb_read_progress(void *_buf, unsigned len)
{
	unsigned long slice;
	short percent = 0;
	unsigned long finished = 0;

	if (len < 4096)
		return usb_read(_buf, len);

	slice = len / 100;
	/*
	 * slice should round up to be 4096 aligned, otherwise will be hang
	 * during usb_read, reason unknown.
	 */
#define roundup(x, y) ((((x) + ((y) - 1)) / (y)) * (y))
	slice = roundup(slice, 4096);

	while (1) {
		if (slice == 0) {
			slice = len;
			percent = 99;
		}

		if (usb_read(_buf + finished, slice) != slice) {
			pr_perror("usb_read");
			return -1;
		}

		percent++;
		finished += slice;

#if 0
		{
			printf("----------- finished %d%%: %d, len: %d\n", percent, finished, len);
			fflush(stdout);
		}
#endif

		/* never reach 100% */
		/* update progress bar */
		if (percent < 100) {
			tboot_ui_textbar(percent, "Downloading...%d%%", percent);
		}

		if (finished > len) {
			pr_error("finished : %lu bigger than len: %u\n", finished, len);
			goto out;
		} else if (finished == len)
			break;

		if (finished + slice > len) {
			/* last slice */
			slice = len - finished;
			if (slice == 0) {
				break;
			} else {
				/* set percent to 99 */
				percent = 99;
				continue;
			}
		}
	}

	return finished;
out:
	return -1;
}

static int usb_write(void *buf, unsigned len)
{
	int r;

	if (fastboot_state == STATE_ERROR)
		goto oops;

	r = write(fb_fp, buf, len);
	if (r < 0) {
		pr_perror("write");
		goto oops;
	}

	return r;

oops:
	fastboot_state = STATE_ERROR;
	return -1;
}

/* internal use */
static void _fastboot_ack(const char *code, const char *reason)
{
#define MAX_LEN 64
	size_t size;
	char *str;
	char buf[MAX_LEN];

	if (fastboot_state != STATE_COMMAND)
		return;

	if (reason == 0)
		reason = "";

	do {
		snprintf(buf, sizeof(buf), "%s%s", code, reason);
		reason += usb_write((void *)buf, strlen(buf)) - strlen(code);
	} while (*reason != '\0');
}

/*
 * fastboot_info() may be used to send back multiple packets, so
 * fastboot_state is left as is in STATE_COMMAND
 *
 * That implies to end a session, you need to endup with either
 * fastboot_fail() or fastboot_okay().
 */
void fastboot_info(const char *buf)
{
	pr_verbose("INFO %s\n", buf);
	_fastboot_ack("INFO", buf);
}

void fastboot_fail(const char *reason)
{
	pr_error("ack FAIL %s\n", reason);
	_fastboot_ack("FAIL", reason);
	fastboot_state = STATE_COMPLETE;
}

void fastboot_okay(const char *info)
{
	pr_debug("ack OKAY %s\n", info);
	_fastboot_ack("OKAY", info);
	fastboot_state = STATE_COMPLETE;
}

static void cmd_getvar(const char *arg, void *data, unsigned sz)
{
	struct fastboot_var *var;

	pr_debug("fastboot: cmd_getvar %s\n", arg);
	/* list all variables when no variable specified */
	if (!arg || strlen(arg) == 0) {
		fastboot_info("All available variables:\n");
		for (var = varlist; var; var = var->next) {
			fastboot_info(var->name);
			fastboot_info("\n");
		}
		fastboot_info("\n");
		fastboot_okay("");
		return;
	}
	for (var = varlist; var; var = var->next) {
		if (!strcmp(var->name, arg)) {
			fastboot_okay(var->value);
			return;
		}
	}
	fastboot_okay("");
}

/*
 * handshake of data size will be downloaded
 */
static int response_data(unsigned long len)
{
	char response[64];

	sprintf(response, "DATA%08lx", len);
	if (usb_write(response, strlen(response)) != strlen(response)) {
		pr_error("write response failed\n");
		return -1;
	}

	return 0;
}

/*
 * this is a just wrap of usb_read used by streaming flash
 */
int streaming_download(void **data, unsigned long len, int response)
{
	int r;

	if (response)
		return response_data(len);

	if (usb_read(*data, len) != len) {
		printf("short read\n");
		return -1;
	}

	return 0;
}

/*
 * another download function, it's familiar with cmd_download,
 * but save data in specified area
 */
int download_to(unsigned long size, void **data)
{
	char response[64];
	int r;

	if (size > download_max) {
		fastboot_fail("data too large");
		return -1;
	}

	if (response_data(size))
		return -1;

	r = usb_read_progress(download_base, size);
	if ((r < 0) || ((unsigned int)r != size)) {
		pr_error("fastboot: download_to errro, only got %d bytes\n", r);
		fastboot_state = STATE_ERROR;
		return -1;
	}

	*data = download_base;
	tboot_ui_hidebar("");

	return 0;
}


static void cmd_download(const char *arg, void *data, unsigned sz)
{
	char response[64];
	unsigned len;
	int r;

	len = strtoul(arg, NULL, 16);
	pr_debug("fastboot: cmd_download %d bytes\n", len);

	download_size = 0;
	if (len > download_max) {
		fastboot_fail("data too large");
		return;
	}

	if (response_data(len))
		return;

	r = usb_read_progress(download_base, len);
	if ((r < 0) || ((unsigned int)r != len)) {
		pr_error("fastboot: cmd_download errro only got %d bytes\n", r);
		fastboot_state = STATE_ERROR;
		tboot_ui_error("Download failed.");
		return;
	}
	download_size = len;

	tboot_ui_hidebar("");
	fastboot_okay("");
}

/*
 * pull file to HOST PC from target device
 * first, write file to download_base
 * second, send response
 */
int pull_file(int fd)
{
	struct stat st;
	/*
	 * the response format is
	 * FILEsizedata...
	 * size is in hex, so strlen(FILEsize) is 4 + 8 = 14
	 */
	unsigned int offset = 12;
	char response[64];

	if (fd < 0) {
		fastboot_fail("invalid fd.");
		return -1;
	}

	if (fstat(fd, &st) == -1) {
		pr_perror("fstat");
		fastboot_fail("can't get file size.");
		return -1;
	}

	if (st.st_size > download_max - offset) {
		fastboot_fail("file is too large.");
		return -1;
	}

	if (snprintf(download_base, download_max, "FILE%08llx", (long long)st.st_size) != offset) {
		fastboot_fail("construct response failed.");
		return -1;
	}

	if (read(fd, download_base + offset, download_max - offset) != st.st_size) {
		pr_perror("read");
		fastboot_fail("read file failed.");
		return -1;
	}

	download_size = offset + st.st_size;

	if (usb_write(download_base, download_size) != download_size) {
		fastboot_fail("usb write failed.");
		return -1;
	}

	/* wait for response */
	if (usb_read(response, sizeof(response)) < offset) {
		fastboot_fail("get invalid response");
		return -1;
	}
	response[offset] = '\0';
	pr_debug("get response:%s\n", response);

	if (st.st_size != strtoul(response + 4, 0, 16)) {
		fastboot_fail("saved bytes doesn't equal sent bytes");
		return -1;
	}

	return 0;
}

int is_fastboot_active(void)
{
	return fastboot_state == STATE_COMMAND;
}

static void fastboot_command_loop(void)
{
	struct fastboot_cmd *cmd;
	int r;
	/* commands which blocked by low battery */
	const char *cmds[] = {
		"flash",
		"erase",
		"oem system",
		NULL
	};

	pr_debug("fastboot: processing commands\n");
again:
	while (fastboot_state != STATE_ERROR) {
		memset(buffer, 0, 64);
		/*
		 * Due to adb gadget implementation, it will return -EIO if
		 * SIGALRM received, so this is a problem if it received
		 * SIGALRM while do fastboot action.
		 *
		 * The simple fix is clear alarm before do any fastboot action
		 * without touch kernel
		 *
		 * However, if we clear alarm before usb_read for command, we'll
		 * never got idle, so we just clear alarm during do faction command
		 */
		r = usb_read(buffer, 64);
		if (r < 0)
			break;
		buffer[r] = 0;
		pr_debug("fastboot got command: %s\n", buffer);

		fastboot_state = STATE_COMMAND;
		CLEAR_ALARM;
		lcd_state = (lcd_state_t)lcd_state(LCD_COMMAND);

		for (cmd = cmdlist; cmd; cmd = cmd->next) {
			if (memcmp(buffer, cmd->prefix, cmd->prefix_len))
				continue;

			disable_autoboot();

			/*
			 * check battery status first, battery low shouldn't
			 * happen frequently
			 */
			if (check_battery()) {
				int i = 0;
				while (cmds[i]) {
					if (!strncmp(buffer, cmds[i], strlen(cmds[i]))) {
						int bat_threshold = atoi(tboot_config_get(BAT_THRESHOLD_KEY));

						tboot_ui_warn("Battery < %d%%, ignore operation '%s'.", bat_threshold, cmds[i]);
						fastboot_fail("can't do this operation when battery low");
						goto again;
					}
					i++;
				}
			}

			pthread_mutex_lock(&action_mutex);
			/* disable keypress when processing fastboot command */
			disable_keypress();
			cmd->handle((const char *)buffer + cmd->prefix_len,
				    (void *)download_base, download_size);
			enable_keypress();
			pthread_mutex_unlock(&action_mutex);
			if (fastboot_state == STATE_COMMAND)
				fastboot_fail("unknown reason");

			RESET_ALARM;
			goto again;
		}
		pr_error("unknown command '%s'\n", buffer);
		fastboot_fail("unknown command");
	}
	fastboot_state = STATE_OFFLINE;
	pr_error("fastboot: oops!\n");
}


static int fastboot_handler(void *arg)
{
	for (;;) {
		fb_fp = open("/dev/android_adb", O_RDWR);
		if (fb_fp < 1) {
			pr_error("Can't open ADB device node (%s),"
					" trying again\n",
					strerror(errno));
			sleep(1);
			continue;
		}
		fastboot_command_loop();
		close(fb_fp);
		fb_fp = -1;
	}
	return 0;
}

int fastboot_init(unsigned size)
{
	pr_verbose("fastboot_init()\n");
	download_max = size;
	download_base = malloc(size);
	if (download_base == NULL) {
		pr_error("scratch malloc of %u failed in fastboot."
			" Unable to continue.\n", size);
		die();
	}

	fastboot_register("getvar:", cmd_getvar);
	fastboot_register("download:", cmd_download);
	fastboot_publish("fastboot", "0.6");

	fastboot_handler(NULL);

	return 0;
}
