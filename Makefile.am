#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

ACLOCAL_AMFLAGS = -I m4
AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/src

lib_LTLIBRARIES = libarmci.la
libarmci_la_SOURCES = $(top_srcdir)/src/debug.c $(top_srcdir)/src/groups.c \
	$(top_srcdir)/src/malloc.c $(top_srcdir)/src/mem_region.c \
	$(top_srcdir)/src/message.c $(top_srcdir)/src/message_gop.c \
	$(top_srcdir)/src/mutex.c $(top_srcdir)/src/mutex_grp_notify.c \
	$(top_srcdir)/src/onesided.c $(top_srcdir)/src/onesided_nb.c \
	$(top_srcdir)/src/rmw.c $(top_srcdir)/src/strided.c \
	$(top_srcdir)/src/topology.c $(top_srcdir)/src/util.c $(top_srcdir)/src/value_ops.c \
	$(top_srcdir)/src/vector.c

libarmci_la_LDFLAGS = -version-info $(libarmci_abi_version)

include_HEADERS = $(top_srcdir)/include/armci.h $(top_srcdir)/include/armci_msg.h \
	$(top_srcdir)/include/armcix.h

bin_PROGRAMS =

if armci_build_benchmarks
include benchmarks/Makefile.mk
endif

# if armci_build_tests
# include tests/Makefile.mk
# endif
