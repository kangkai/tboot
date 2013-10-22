#ifndef __DEBUG_H
#define __DEBUG_H

#undef ALOGE
#undef ALOGI
#undef ALOGV
#undef pr_err

#define ALOGE(fmt, ...) \
	do { \
		fprintf(stderr, fmt "\n", ##__VA_ARGS__); \
	} while (0)

#define ALOGI ALOGE
#define ALOGV ALOGE
#endif
