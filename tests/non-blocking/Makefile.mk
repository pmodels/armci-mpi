#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

check_PROGRAMS += overlap simple

overlap_SOURCES = $(top_srcdir)/tests/non-blocking/overlap.c
overlap_LDADD = -larmci -lm
overlap_DEPENDENCIES = libarmci.la

simple_SOURCES = $(top_srcdir)/tests/non-blocking/simple.c
simple_LDADD = -larmci -lm
simple_DEPENDENCIES = libarmci.la
