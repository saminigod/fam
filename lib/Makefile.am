include $(top_srcdir)/common.am

lib_LTLIBRARIES = libfam.la

libfam_la_SOURCES = \
	Client.c++ \
	Client.h \
	fam.c++

libfam_la_LDFLAGS = -export-symbols fam.sym
libfam_la_LIBADD = -lstdc++

EXTRA_DIST = fam.sym

