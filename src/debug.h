/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * This is a simplified subset of the ui code in bootable/recovery/
 */

#ifndef __DEBUG_H
#define __DEBUG_H

#include "tboot_ui.h"

#define DEBUG 1
#define VERBOSE_DEBUG 0

#ifndef LOG_TAG
#define LOG_TAG "tboot"
#endif
//#include <cutils/log.h>

#define pr_perror(x) \
	fprintf(stderr, LOG_TAG ": E: %s: %s\n", x, strerror(errno))

/*
 * use "./fastboot oem showtext" to toggle
 *
 * When enabled, pr_* will print the msgs in LCD UI
 */
int oem_showtext;

#define pr_critial(...)	\
	do { \
		pr_error(__VA_ARGS__); \
		exit(-1); \
	} while (0)

#define pr_error(...)						\
	do {							\
		if (oem_showtext)				\
			tboot_ui_error("E: " __VA_ARGS__);	\
		else						\
			fprintf(stderr, LOG_TAG ": E: " __VA_ARGS__);		\
	} while (0)

#define pr_info(...)						\
	do {							\
		if (oem_showtext)				\
			tboot_ui_info("I: " __VA_ARGS__);	\
		else						\
			fprintf(stdout, LOG_TAG ": I: " __VA_ARGS__);		\
	} while (0)

#define pr_warning(...)						\
	do {							\
		if (oem_showtext)				\
			tboot_ui_warn("W: " __VA_ARGS__);	\
		else						\
			fprintf(stderr, LOG_TAG ": W: " __VA_ARGS__);		\
	} while (0)

#if VERBOSE_DEBUG
#ifndef DEBUG
#define DEBUG 1
#else
#undef DEBUG
#define DEBUG 1
#endif
#define pr_verbose(fmt, ...)							\
	do {								\
		if (oem_showtext)					\
			tboot_ui_info("V: %s: " fmt, __func__, ##__VA_ARGS__); \
		else							\
			fprintf(stdout, LOG_TAG ": V: %s: " fmt, __func__, ##__VA_ARGS__); \
	} while (0)

#else
#define pr_verbose(fmt, ...)				do { } while (0)
#endif

#if DEBUG
#define pr_debug(...)						\
	do {							\
		if (oem_showtext)				\
			tboot_ui_info("D: " __VA_ARGS__);	\
		else						\
			fprintf(stdout, LOG_TAG ": D: " __VA_ARGS__);		\
	} while (0)
#else
#define pr_debug(...)				do { } while (0)
#endif

#endif
