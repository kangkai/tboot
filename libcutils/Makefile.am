noinst_LIBRARIES = libcutils.a
libcutils_a_SOURCES = \
	hashmap.c \
	config_utils.c \
	load_file.c \
	config_parser.c \
	preos_reboot.c

AM_CFLAGS = \
	-m32 \
	-O2 \
	-DHAVE_PTHREADS \
	-I$(top_srcdir)/include
