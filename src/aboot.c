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
 *  * Neither the name of Google, Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#include <errno.h>
#include <fcntl.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <libgen.h>
#include <linux/limits.h>
#include <signal.h>
#include <sys/vfs.h>

#include "platform.h"
#include "cutils/preos_reboot.h"
#include "cutils/hashmap.h"
#include "diskconfig/diskconfig.h"
#include "tboot_ui.h"
#include "fastboot.h"
#include "tboot.h"
#include "tboot_util.h"
#include "tboot_plugin.h"
#include "debug.h"
#include "buffer.h"

#define CMD_PUSH               "push"
#define CMD_PUSH_USAGE     "Usage:\n    oem push <local-file>    push <local-file> to target\n"


static Hashmap *flash_cmds;
static Hashmap *oem_cmds;

#ifdef USE_GUI
Hashmap *ui_cmds;
#endif

static int aboot_register_cmd(Hashmap *map, char *key, void *callback)
{
	char *k;

	k = strdup(key);
	if (!k) {
		pr_perror("strdup");
		return -1;
	}
	if (hashmapGet(map, k)) {
		pr_error("key collision '%s'\n", k);
		free(k);
		return -1;
	}
	hashmapPut(map, k, callback);
	pr_verbose("Registered plugin function %p (%s) with table %p\n",
			callback, k, map);
	return 0;
}

int aboot_register_flash_cmd(char *key, flash_func callback)
{
	return aboot_register_cmd(flash_cmds, key, callback);
}

int aboot_register_oem_cmd(char *key, oem_func callback)
{
	return aboot_register_cmd(oem_cmds, key, callback);
}

#ifdef USE_GUI
int aboot_register_ui_cmd(char *key, ui_func callback)
{
	return aboot_register_cmd(ui_cmds, key, callback);
}
#endif

static void list_partitions(int os, int part);
/* Erase a named partition by creating a new empty partition on top of
 * its device node. No parameters. */
static void cmd_erase(const char *part_name, void *data, unsigned sz)
{
	struct part_info *ptn;

	pr_info("%s: %s\n", __func__, part_name);

	if (!part_name || strlen(part_name) == 0) {
		list_partitions(0, 1);
		return;
	}

	ptn = find_part(disk_info, part_name);
	if (ptn == NULL) {
		tboot_ui_error("unknown partition: %s.", part_name);
		fastboot_fail("unknown partition name.");
		return;
	}

	pr_debug("Erasing %s.\n", part_name);
	tboot_ui_bouncebar("Erasing %s...", part_name);
	if (erase_partition(ptn)) {
		tboot_ui_error("Erase %s failed.", part_name);
		fastboot_fail("Can't erase partition");
	} else {
		tboot_ui_info("Erase %s finished.", part_name);
		fastboot_okay("");
	}
}


static int cmd_flash_update(void *data, unsigned sz)
{
	struct part_info *cacheptn;

	cacheptn = find_part(disk_info, CACHE_PTN);
	if (!cacheptn) {
		pr_error("Couldn't find " CACHE_PTN " partition. Is your "
				"disk_layout.conf valid?\n");
		return -1;
	}
	if (mount_partition(cacheptn)) {
		pr_error("Couldn't mount " CACHE_PTN "partition\n");
		return -1;
	}
	/* Remove any old copy hanging around */
	unlink("/mnt/" CACHE_PTN "/tboot.update.zip");

	/* Once the update is applied this file is deleted */
	if (named_file_write("/mnt/" CACHE_PTN "/tboot.update.zip",
				data, sz)) {
		pr_error("Couldn't write update package to " CACHE_PTN
				" partition.\n");
		unmount_partition(cacheptn);
		return -1;
	}
	unmount_partition(cacheptn);
	apply_sw_update(CACHE_VOLUME "/tboot.update.zip", 1);
	return -1;
}

static int cmd_flash_system(void *data, unsigned sz)
{
	Volume *v;
	int ret;

	if ((v = volume_for_path("/system")) == NULL) {
		pr_error("Cannot find system volume!\n");
		return -1;
	}

	ret = named_file_write_decompress_gzip(v->device, data, sz);

	if (unlink(FASTBOOT_DOWNLOAD_TMP_FILE) != 0) {
		pr_error("Cannot remove %s\n", FASTBOOT_DOWNLOAD_TMP_FILE);
	}

	return ret;
}

/*
 * buffers for reader and writer
 */
#define SIZE (4096 * 1024)
#define NR_BUFFER 2
static struct buffer *buf[NR_BUFFER] = {NULL, NULL};

/* tell reader if writer has finished its work */
static int writer_finished;

/*
 * reader thread to read from buffer and write to device node
 */
static void *reader(void *args)
{
	int i = 0;
	int empty_counter = 0;
	FILE *fout = (FILE *)args;

	if (!fout) {
		printf("null arg in reader\n");
		pthread_exit((void *)-1);
	}

	while (empty_counter < NR_BUFFER && pipe_broken == 0) {
		pthread_mutex_lock(&buf[i]->mutex);
		if (!buf[i]->len) {
			pthread_mutex_unlock(&buf[i]->mutex);
			if (writer_finished) {
				pr_verbose("writer finished\n");
				empty_counter++;
			}
			i = (i + 1) % NR_BUFFER;
			continue;
		}
		if (fwrite(buf[i]->data, 1, buf[i]->len, fout)
				!= buf[i]->len) {
			pr_error("short write in reader\n");
			pthread_mutex_unlock(&buf[i]->mutex);
			pthread_exit((void *)-1);
		}
		buf[i]->len = 0;
		pthread_mutex_unlock(&buf[i]->mutex);
		i = (i + 1) % NR_BUFFER;
	}

	if (fout) {
		struct stat st;

		if (fstat(fileno(fout), &st)) {
			pr_error("fstat failed.\n");
		}
		if (S_ISFIFO(st.st_mode)) {
			if (pclose(fout))
				pthread_exit((void *)-1);
		}
	}

	pthread_exit((void *)0);
}

static bool list_callback(void *key, void *value, void *context)
{
	char *name;

	if (key) {
		name = strdup(key);
		fastboot_info(name);
		fastboot_info("\n");
		free(name);
	}

	return true;
}

/*
 * list partitions/OS which can be flashed
 */
static void list_partitions(int os, int part)
{
	struct part_info *ptn;
	char *name;
	int i;

	if (!os && !part)
		return;

	/* list os */
	if (os) {
		fastboot_info("FW and raw OS can be flashed:\n");
		hashmapForEach(flash_cmds, list_callback, NULL);
	}

	/* list partitions */
	if (part) {
		fastboot_info("\nPartitions can be flashed/erased:\n");
		fastboot_info("Name        Size(MB)\n");
		fastboot_info("--------------------\n");
		for (i = 0; i < disk_info->num_parts; i++) {
			ptn = &disk_info->part_lst[i];
			if (ptn) {
				char *str;
				if (asprintf(&str, "%-12s%u\n", ptn->name, ptn->len_kb / 1024) == -1)
					continue;
				fastboot_info(str);
				free(str);
			}
		}
	}
	fastboot_info("\n");
	fastboot_okay("");
}

/*
 * if partition is a OS slot, use the old fashion download and flash method.
 * if partition is a real partition of mmc, use streaming flash.
 * cmd is in the below format:
 * part_name:data_len
 *
 * In general, there are two buffers in the same size, two posix threads and
 * an extra pipe which write buffer to device node. We gave a try to use more
 * buffers but can't help, just waste much memory.
 *
 * the data flow looks like below.
 * the main thread read data from usb buffer and write to doubled buffers.
 * the reader thread read from doubled buffers and write to pipe.
 * the pipe writer write data to device node.
 *
 * So when we finished, the main thread should wait for reader thread while
 * the reader thread should wait for pipe writer finished. Otherwise, sync
 * issue may occur.
 */
static void cmd_stream_flash(const char *cmd, void *data, unsigned sz)
{
	char *pc;
	char *part_name = NULL;
	unsigned long len;
	char *device = NULL;
	flash_func cb;
	int free_device = 0;
	struct part_info *ptn = NULL;
	int do_ext_checks = 0;
	void *saved_data = NULL;
	unsigned long size;
	unsigned char *magic;
	int i = 0;
	char *cmd_line = NULL;
	char *cmd_base;
	unsigned long finished = 0;
	pthread_t t_reader;
	void *reader_exit;
	FILE *fout = NULL;
	int percent;
	int joined = 0;
	Volume *vol;

	pr_debug("flash command is: %s\n", cmd);
	part_name = strdup(cmd);
	if (!part_name) {
		fastboot_fail("strdup failed.");
		goto error;
	}

	pc = strchr(part_name, ':');
	if (!pc) {
		fastboot_fail("invalid cmd.");
		goto error;
	}

	len = strtoul(pc + 1, NULL, 16);
	*pc = 0;

	if (strlen(part_name) == 0) {
		list_partitions(1, 1);
		return;
	}

	pr_debug("part_name: %s, data size: 0x%lX\n", part_name, len);

	if (!strcmp(part_name, "disk")) {
		device = disk_info->device;
	} else if ( (cb = hashmapGet(flash_cmds, (char *)part_name)) ) {
		/*
		 * download data first
		 */
		if (download_to(len, &saved_data)) {
			fastboot_fail("download failed.");
			goto error;
		}

		/* Use our table of flash functions registered by platform
		 * specific plugin libraries */
		int cbret;
		tboot_ui_bouncebar("Flashing %s...", part_name);
		cbret = cb(saved_data, len);
		if (cbret) {
			fastboot_fail("flash failed.");
			goto error;
		} else {
			goto finish;
		}
	} else {
		free_device = 1;
		device = find_part_device(disk_info, part_name);
		if (!device) {
			fastboot_fail("unknown partition specified");
			goto error;
		}
		ptn = find_part(disk_info, part_name);

		/* check the filesystem type */
		vol = volume_for_device(device);
		if (!vol) {
			fastboot_fail("invalid fstab or disk layout config");
			goto error;
		}

		/* only ext4 support */
		if (strcmp(vol->fs_type, "ext4")) {
			fastboot_fail("unsupport filesystem type");
			goto error;
		}
	}

	/* response to flashing tool */
	if (streaming_download(NULL, len, 1)) {
		fastboot_fail("protocol handshake failed.");
		goto error;
	}

	/* create buffers */
	for (i = 0; i < NR_BUFFER; i++) {
		if ((buf[i] = buffer_init(SIZE)) == NULL) {
			while(i)
				buffer_free(buf[--i]);
			fastboot_fail("out of memory");
			goto error;
		}
	}

	size = len > buf[0]->size ? buf[0]->size : len;

	/* first through read to get stream magic */
	if (streaming_download((void *)&buf[0]->data, size, 0)) {
		fastboot_fail("download failed.");
		goto error;
	}
	buf[0]->len = size;
	finished += size;

	/*
	 * Check input stream for a gzip header or bzip2 header.
	 * For details, please refer to
	 * http://www.gzip.org/zlib/rfc-gzip.html#file-format
	 * http://en.wikipedia.org/wiki/Bzip2#File_format
	 */
	magic = buf[0]->data;
	pr_verbose("Checking if input in gzip or bzip2 format...\n"
		"4 bytes header value is: %x, %x, %x, %x\n",
		magic[0], magic[1], magic[2], magic[3]);
	if (magic[0] == 0x1f && magic[1] == 0x8b) {
		pr_verbose("Yes, input stream is in gzip format\n");
		cmd_base = "/bin/gzip -c -d | /bin/dd of=%s bs=8192";
	} else if (magic[0] == 'B' && magic[1] == 'Z' &&
			(magic[2] == 'h' || magic[2] == '0') &&
			(magic[3] >= 49 && magic[3] <= 57)) {
		pr_verbose("Yes, input stream is in bzip2 format\n");
		cmd_base = "/bin/bzip2 -c -d | /bin/dd of=%s bs=8192";
	} else {
		pr_verbose("Warning: Input stream isn't in gzip or bzip2 format!\n");
		tboot_ui_warn("Input isn't in gzip or bzip2 format.");
		cmd_base = "/bin/dd of=%s bs=8192";
	}

	if (asprintf(&cmd_line, cmd_base, device) < 0) {
		fastboot_fail("asprintf");
		goto error;
	}

	pr_verbose("command: %s\n", cmd_line);

	if ((fout = popen(cmd_line, "w")) == NULL) {
		fastboot_fail("open for write.");
		goto error;
	}

	writer_finished = 0;
	pipe_broken = 0;
	 /* create reader thread to read */
	if (pthread_create(&t_reader, NULL, reader, (void *)fout)) {
		fastboot_fail("create reader thread failed.");
		goto error;
	}

	/* the first buffer has data already */
	i = 1;
	while (finished < len && pipe_broken == 0) {
		pthread_mutex_lock(&buf[i]->mutex);
		if (buf[i]->len) {	// has valid data, try next
			pthread_mutex_unlock(&buf[i]->mutex);
			i = (i + 1) % NR_BUFFER;
			continue;
		}

		if (finished + size > len)
			size = len - finished;

		/* write to buffer */
		if (streaming_download((void *)&buf[i]->data, size, 0)) {
			fastboot_fail("download failed.");
			pthread_mutex_unlock(&buf[i]->mutex);
			goto error;
		}

		buf[i]->len = size;
		pthread_mutex_unlock(&buf[i]->mutex);
		i = (i + 1) % NR_BUFFER;
		finished += size;
		/* never archive 100% */
		/* update progress bar */
		percent = (double)finished / len * 100;
		/* percent may be zero if len is too large */
		if (percent < 100 && percent > 0)
			tboot_ui_textbar(percent, "Flashing...%d%%", percent);
		else if (percent >= 100)
			/* in case the data is smaller than SIZE */
			tboot_ui_textbar(percent, "Flashing...99%%");
	}

	if (pipe_broken) {
		/* empty the usb buffer, so we can send back fail msgs */
		while (finished < len) {
			if (finished + size > len)
				size = len - finished;
			if (streaming_download((void *)&buf[i]->data, size, 0)) {
				pr_error("we may hang there.\n");
				break;
			}
			finished += size;
		}
		fastboot_fail("broken pipe, image too large?");
		goto error;
	}

	writer_finished = 1;

	/* wait reader thread */
	if (pthread_join(t_reader, &reader_exit)) {
		fastboot_fail("join reader thread failed");
		goto error;
	}

	joined = 1;

	if (finished != len || (int)reader_exit) {
		fastboot_fail("write to device failed.");
		goto error;
	}

	pr_debug("writer write: %ld bytes\n", finished);

	pr_debug("syncing...\n");
	tboot_ui_bouncebar("Syncing...");
	sync();

	/* Check if we wrote to the base device node. If so,
	 * re-sync the partition table in case we wrote out
	 * a new one */
	if (!strcmp(device, disk_info->device)) {
		int fd = open(device, O_RDWR);
		if (fd < 0) {
			fastboot_fail("could not open device node");
			goto error;
		}
		pr_verbose("sync partition table\n");
		ioctl(fd, BLKRRPART, NULL);
		close(fd);
	}

	/* Make sure this is really an ext4 partition before we try to
	 * run some disk checks and resize it, ptn->type isn't sufficient
	 * information */
	if (ptn && ptn->type == PC_PART_TYPE_LINUX) {
		if (check_ext_superblock(ptn, &do_ext_checks)) {
			fastboot_fail("couldn't check superblock");
			goto error;
		}
	}
	if (do_ext_checks) {
		tboot_ui_bouncebar("File system checking...");
		if (ext4_filesystem_checks(device)) {
			fastboot_fail("ext4 filesystem error");
			goto error;
		}
	}

finish:
	tboot_ui_info("Flash %s finished.", part_name);
	fastboot_okay("");
	goto out;

error:
	tboot_ui_error("Flash %s failed.", part_name);

out:
	if (part_name)
		free(part_name);
	if (device && free_device)
		free(device);
	i = 0;
	while (i < NR_BUFFER) {
		buffer_free(buf[i]);
		buf[i++] = NULL;
	}
	if (cmd_line)
		free(cmd_line);
	if (!joined)
		pthread_cancel(t_reader);
}


/* Image command. Allows user to send a single gzipped file which
 * will be decompressed and written to a destination location. Typical
 * usage is to write to a disk device node, in order to flash a raw
 * partition, but can be used to write any file.
 *
 * The parameter part_name can be one of several possibilities:
 *
 * "disk" : Write directly to the disk node specified in disk_layout.conf,
 *          whatever it is named there.
 * <name> : Look in the flash_cmds table and execute the callback function.
 *          If not found, lookup the named partition in disk_layout.conf
 *          and write to its corresponding device node
 */
static void cmd_flash(const char *part_name, void *data, unsigned sz)
{
	FILE *fp = NULL;
	char *cmd = NULL;
	char *device;
	char *cmd_base;
	struct part_info *ptn = NULL;
	unsigned char *data_bytes = (unsigned char *)data;
	int free_device = 0;
	int do_ext_checks = 0;
	flash_func cb;

	/* progress bar */
	unsigned long slice;
	short percent = 0;
	unsigned long finished = 0;
	int success = 0;

	pr_verbose("cmd_flash %s %u\n", part_name, sz);

	if (!strcmp(part_name, "disk")) {
		device = disk_info->device;
	} else if ( (cb = hashmapGet(flash_cmds, (char *)part_name)) ) {
		/* Use our table of flash functions registered by platform
		 * specific plugin libraries */
		int cbret;
		tboot_ui_bouncebar("Flashing %s...", part_name);
		cbret = cb(data, sz);
		if (cbret) {
			pr_error("%s flash failed!\n", part_name);
			tboot_ui_error("Flash %s failed.", part_name);
			fastboot_fail(part_name);
		} else {
			tboot_ui_info("Flash %s finished.", part_name);
			fastboot_okay("");
		}
		return;
	} else {
		free_device = 1;
		device = find_part_device(disk_info, part_name);
		if (!device) {
			fastboot_fail("unknown partition specified");
			tboot_ui_error("unknown partition: %s.", part_name);
			return;
		}
		ptn = find_part(disk_info, part_name);
	}

	pr_debug("Writing %u bytes to destination device: %s\n", sz, device);
	if (!is_valid_blkdev(device)) {
		fastboot_fail("invalid destination node. partition disks?");
		goto out;
	}

	/*
	 * Check input stream for a gzip header or bzip2 header.
	 * For details, please refer to
	 * http://www.gzip.org/zlib/rfc-gzip.html#file-format
	 * http://en.wikipedia.org/wiki/Bzip2#File_format
	 */
	pr_verbose("Checking if input in gzip or bzip2 format...\n"
		"4 bytes header value is: %x, %x, %x, %x\n",
		data_bytes[0], data_bytes[1], data_bytes[2], data_bytes[3]);
	if (sz > 3 && data_bytes[0] == 0x1f &&
			data_bytes[1] == 0x8b && data_bytes[3] == 8) {
		pr_verbose("Yes, input stream is in gzip format\n");
		cmd_base = "/bin/gzip -c -d | /bin/dd of=%s bs=8192";
	} else if (sz > 3 && data_bytes[0] == 'B' && data_bytes[1] == 'Z' &&
			(data_bytes[2] == 'h' || data_bytes[2] == '0') &&
			(data_bytes[3] >= 49 && data_bytes[3] <= 57)) {
		pr_verbose("Yes, input stream is in bzip2 format\n");
		cmd_base = "/bin/bzip2 -c -d | /bin/dd of=%s bs=8192";
	} else {
		pr_verbose("Warning: Input stream isn't in gzip or bzip2 format!\n");
		tboot_ui_warn("Input isn't in gzip or bzip2 format.");
		cmd_base = "/bin/dd of=%s bs=8192";
	}

	if (asprintf(&cmd, cmd_base, device) < 0) {
		pr_perror("asprintf");
		cmd = NULL;
		fastboot_fail("memory allocation error");
		goto out;
	}

	pr_verbose("command: %s\n", cmd);
	fp = popen(cmd, "w");
	if (!fp) {
		pr_perror("popen");
		fastboot_fail("popen failure");
		goto out;
	}
	free(cmd);
	cmd = NULL;

	/* flash progress */
	slice = sz / 100;
	while (1) {
		if (slice == 0) {
			slice = sz;
			percent = 99;
		}

		if (fwrite(data + finished, 1, slice, fp) != slice) {
			pr_perror("fwrite");
			fastboot_fail("image write failure");
			goto out;
		}

		percent++;
		finished += slice;

		/* pr_verbose("flashed %lu bytes, %d\n", finished, percent); */

		/* never archive 100% */
		/* update progress bar */
		if (percent < 100) {
			tboot_ui_textbar(percent, "Flashing...%d%%", percent);
		}

		if (finished > sz) {
			pr_error("finished : %lu bigger than sz: %u\n", finished, sz);
			fastboot_fail("image write failure");
			goto out;
		} else if (finished == sz)
			break;


		if (percent == 100) {
			slice = sz % 100;
			if (slice == 0) {
				break;
			} else {
				/* set percent to 99 */
				percent = 99;
				continue;
			}
		}
	}

	pclose(fp);
	fp = NULL;
	pr_debug("syncing...\n");
	sync();

	pr_debug("wrote %u bytes to %s\n", sz, device);

	/* Check if we wrote to the base device node. If so,
	 * re-sync the partition table in case we wrote out
	 * a new one */
	if (!strcmp(device, disk_info->device)) {
		int fd = open(device, O_RDWR);
		if (fd < 0) {
			fastboot_fail("could not open device node");
			goto out;
		}
		pr_verbose("sync partition table\n");
		ioctl(fd, BLKRRPART, NULL);
		close(fd);
	}

	/* Make sure this is really an ext4 partition before we try to
	 * run some disk checks and resize it, ptn->type isn't sufficient
	 * information */
	if (ptn && ptn->type == PC_PART_TYPE_LINUX) {
		if (check_ext_superblock(ptn, &do_ext_checks)) {
			fastboot_fail("couldn't check superblock");
			goto out;
		}
	}
	if (do_ext_checks) {
		if (ext4_filesystem_checks(device)) {
			fastboot_fail("ext4 filesystem error");
			goto out;
		}
	}

	tboot_ui_info("Flash %s finished.", part_name);
	success = 1;
	fastboot_okay("");
out:
	if (fp)
		pclose(fp);
	if (cmd)
		free(cmd);
	if (device && free_device)
		free(device);
	if (!success)
		tboot_ui_error("Flash %s failed.", part_name);
}

static void cmd_oem(const char *arg, void *data, unsigned sz)
{
	char *command, *saveptr, *str1;
	char *argv[MAX_OEM_ARGS];
	int argc = 0;
	oem_func cb;

	pr_verbose("%s: <%s>\n", __FUNCTION__, arg);

	while (*arg == ' ')
		arg++;

	if (strlen(arg) == 0) {
		pr_info("empty OEM command\n");
		fastboot_info("All available oem commands, call them with '-h|--help' to get help.\n");
		hashmapForEach(oem_cmds, list_callback, NULL);
		fastboot_info("\n");
		fastboot_okay("");
		return;
	}

	command = strdup(arg); /* Can't use strtok() on const strings */
	if (!command) {
		pr_perror("strdup");
		fastboot_fail("memory allocation error");
		return;
	}

	for (str1 = command; argc < MAX_OEM_ARGS; str1 = NULL) {
		argv[argc] = strtok_r(str1, " \t", &saveptr);
		if (!argv[argc])
			break;
		argc++;
	}

	if ( (cb = hashmapGet(oem_cmds, argv[0])) ) {
		int ret;

		ret = cb(argc, argv);
		if (ret) {
			pr_error("oem %s command failed, retval = %d\n",
					argv[0], ret);
			fastboot_fail(argv[0]);
		} else
			fastboot_okay("");
	} else if (strcmp(argv[0], CMD_PUSH) == 0) {
		/* argv[1] the pushed file from host */
		FILE *fp;
		char *file;
		size_t saved;
		char tmpfile[PATH_MAX];
		struct statfs stat;
		unsigned long max_size;
		char push_dir[PATH_MAX];

		if (argc != 2 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
			fastboot_info(CMD_PUSH_USAGE);
			fastboot_okay("");
			return;
		}
		strncpy(push_dir, tboot_config_get(PUSH_DIR_KEY), sizeof(push_dir));
		if (statfs(push_dir, &stat) == -1) {
			pr_warning("get filesystem status failed.\n");
			tboot_ui_warn("Get filesystem status failed.");
		} else {
			/* left 1M bytes to make sure write file successfully */
			max_size = stat.f_bsize * stat.f_bfree - (1 << 20);
			pr_verbose("free size: %lu\n", max_size);
			if (sz > max_size) {
			/* check is there enough space to hold the file */
				fastboot_fail("file size is too large.");
				tboot_ui_error("Push %s failed.", argv[1]);
				goto out;
			}
		}

		file = basename(argv[1]);
		/* remove the last slash */
		if ('/' == push_dir[strlen(push_dir) - 1])
			push_dir[strlen(push_dir) - 1] = '\0';

		snprintf(tmpfile, sizeof(tmpfile), "%s/%s", push_dir, file);

		if (!(fp = fopen(tmpfile, "wb"))) {
			pr_perror("create file");
			fastboot_fail("create file failed.");
			goto err;
		}

		saved = fwrite(data, 1, sz, fp);
		if (saved != sz || ferror(fp)) {
			printf("save to file failed: saved = %u, size = %u\n", saved, sz);
			fastboot_fail("oem push, fwrite");
			goto err;
		} else {
			fastboot_okay(tmpfile);
			tboot_ui_info("Saved file to %s.", tmpfile);
			fclose(fp);
			goto out;
		}

err:
		if (fp)
			fclose(fp);
		tboot_ui_error("Push %s failed.", argv[1]);
	} else {
		fastboot_fail("unknown OEM command");
	}
out:
	if (command)
		free(command);
}

static void cmd_boot(const char *arg, void *data, unsigned sz)
{
	fastboot_fail("boot command stubbed on this platform!");
}

static void cmd_reboot(const char *arg, void *data, unsigned sz)
{
	fastboot_okay("");
	sync();
	pr_info("Rebooting!\n");
	tboot_ui_bouncebar("Rebooting...");
	preos_reboot(PREOS_RB_RESTART2, 0, PREOS_RB_ARG_NORMALOS);
	tboot_ui_error("Reboot failed.");
	pr_error("Reboot failed\n");
}

static void cmd_reboot_bl(const char *arg, void *data, unsigned sz)
{
	fastboot_okay("");
	sync();
	pr_info("Restarting tboot...\n");
	tboot_ui_bouncebar("Restarting tboot...");
	preos_reboot(PREOS_RB_RESTART2, 0, PREOS_RB_ARG_PREOS);
	tboot_ui_error("Restarting tboot failed.");
	pr_error("Restarting tboot failed\n");
}


static void cmd_continue(const char *arg, void *data, unsigned sz)
{
	if (g_update_location) {
		apply_sw_update(g_update_location, 1);
		fastboot_fail("Unable to apply SW update");
	} else {
		start_kernel("kernel");
		fastboot_fail("Unable to boot default kernel!");
	}
}

void aboot_register_commands(void)
{
	fastboot_register("oem", cmd_oem);
	fastboot_register("boot", cmd_boot);
	fastboot_register("reboot", cmd_reboot);
	fastboot_register("reboot-bootloader", cmd_reboot_bl);
	fastboot_register("erase:", cmd_erase);
	fastboot_register("flash:", cmd_stream_flash);
	//fastboot_register("continue", cmd_continue);

	flash_cmds = hashmapCreate(8, strhash, strcompare);
	oem_cmds = hashmapCreate(8, strhash, strcompare);
	if (!flash_cmds || !oem_cmds) {
		pr_error("Memory allocation error\n");
		die();
	}
#ifdef USE_GUI
	ui_cmds = hashmapCreate(8, strhash, strcompare);
	if (!ui_cmds) {
		pr_error("Memory allocation error\n");
		die();
	}
#endif
	//aboot_register_flash_cmd("update", cmd_flash_update);

}

void aboot_cmds_free(void)
{
	if (flash_cmds)
		hashmapFree(flash_cmds);
	if (oem_cmds)
		hashmapFree(oem_cmds);
}
