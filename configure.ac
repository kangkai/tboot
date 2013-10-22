#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.50])
AC_INIT([tboot], [0.9], [kangkai.yin@intel.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_SRCDIR([src/tboot.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_RANLIB

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h limits.h paths.h stdlib.h string.h sys/ioctl.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_FUNC_FORK
AC_CHECK_FUNCS([memset mkfifo pow select strdup strerror strstr])

# Checks for plugins
FILE=src/plugins.h
cat > $FILE << EOF
#ifndef __PLUGINS_H
#define __PLUGINS_H

EOF

PLUGINS=
for lib in $(ls plugins/*.a); do
        PLUGINS="\$(top_builddir)/$lib $PLUGINS"
        lib=$(basename $lib)
        lib=${lib%.a}
        echo "extern void ${lib}_tboot_init(void);" >> $FILE
done
AC_SUBST([PLUGINS])

cat >> $FILE <<EOF

void register_tboot_plugins(void)
{
EOF

for lib in $(ls plugins/*.a); do
        lib=$(basename $lib)
        lib=${lib%.a}
        echo -e "\t${lib}_tboot_init();" >> $FILE
done

echo "}" >> $FILE

if test "x$PLUGINS" == "x${PLUGINS#*libintelprov.a}" ; then
        cat >> $FILE <<EOF

char *tboot_plugin_get_fw_rev(char *buf, int size)
{
        return NULL;
}
EOF
fi

cat >> $FILE <<EOF

#endif
EOF

AC_CONFIG_FILES([Makefile
		 ext4_utils/Makefile
		 libcutils/Makefile
		 libdiskconfig/Makefile
		 libminui/Makefile
		 libsparse/Makefile
		 src/Makefile
		 ])
AC_OUTPUT
