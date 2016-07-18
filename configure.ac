#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# Initialization of autoconf.
AC_PREREQ(2.57)
AC_INIT(fam, 2.7.0, fam@oss.sgi.com)
AM_INIT_AUTOMAKE
dnl AC_CONFIG_SRCDIR([acconfig.h])
AM_CONFIG_HEADER([config.h])
AC_LANG(C++)

# Variables used by the FAM source.
FAM_INC='-I$(top_srcdir)/include'
AC_SUBST(FAM_INC)
FAM_CONF='$(sysconfdir)/fam.conf'
AC_SUBST(FAM_CONF)

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_LIBTOOL
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
dnl AC_PROG_RANLIB

# Checks for libraries.
AC_CHECK_LIB([rpcsvc], [pmap_set])

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_DIRENT
AC_CHECK_HEADERS([fcntl.h limits.h linux/imon.h netinet/in.h rpc/rpc.h rpcsvc/mount.h stddef.h stdlib.h string.h syslog.h sys/imon.h sys/param.h sys/select.h sys/statvfs.h sys/syssgi.h sys/time.h sys/types.h sys/un.h unistd.h])

if test "$have_sys_imon_h"; then
	MONITOR_FUNCS=IMonIRIX
elif test "$have_linux_imon_h"; then
	MONITOR_FUNCS=IMonLinux
else
	MONITOR_FUNCS=IMonNone
fi
AC_SUBST(MONITOR_FUNCS)

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_CHECK_TYPES([bool, socklen_t])
AC_C_CONST
AC_TYPE_UID_T
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_CHECK_MEMBERS(struct sockaddr.sa_len, struct sockaddr_un.sun_len)

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_SELECT_ARGTYPES
AC_CHECK_FUNCS([bindresvport _daemonize daemon getgrmember select])

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 include/Makefile
                 lib/Makefile
                 conf/Makefile
                 man/Makefile
                 test/Makefile])
AC_OUTPUT
