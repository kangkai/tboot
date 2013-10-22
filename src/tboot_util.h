#ifndef __TBOOT_UTIL_H
#define __TBOOT_UTIL_H

#include <stdio.h>
#include <diskconfig/diskconfig.h>

int named_file_write(const char *filename, const unsigned char *what,
		size_t sz);
int kexec_linux(char *basepath, const char *kernel);
int is_valid_blkdev(const char *node);
/* Attribute specification and -Werror prevents most security shenanigans with
 * this function */
int execute_command(const char *fmt, ...) __attribute__((format(printf,1,2)));
int execute_command_oem(FILE **fpp, const char *fmt, ...) __attribute__((format(printf,2,3)));
int execute_command_data(void *data, unsigned sz, const char *fmt, ...)
		__attribute__((format(printf,3,4)));
int mount_partition(struct part_info *ptn);
void die(void);
int erase_partition(struct part_info *ptn);
void apply_sw_update(const char *location, int send_fb_ok);
int check_ext_superblock(struct part_info *ptn, int *sb_present);
int unmount_partition(struct part_info *ptn);
int mount_partition_device(const char *device, const char *type, char *mountpoint);
int ext4_filesystem_checks(const char *device);
void import_kernel_cmdline(void (*callback)(char *name));
char *array_to_string(int argc, char *argv[], char seprator);

#endif
