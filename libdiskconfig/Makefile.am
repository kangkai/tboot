noinst_LIBRARIES = libdiskconfig.a
libdiskconfig_a_SOURCES = \
	config_mbr.c \
	diskconfig.c \
	write_lst.c \
	debug.h

AM_CFLAGS = \
	-m32 \
	-O2 \
	-D_LARGEFILE64_SOURCE \
	-DLOG_NDEBUG \
	-I$(top_srcdir)/include
