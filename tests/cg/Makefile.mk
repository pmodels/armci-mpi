#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

check_PROGRAMS += tests/cg/cg

tests_cg_cg_SOURCES = $(top_srcdir)/tests/cg/cg.c $(top_srcdir)/tests/cg/compute.c \
	$(top_srcdir)/tests/cg/read_input.c $(top_srcdir)/tests/cg/cg_timing.c
tests_cg_cg_LDADD = -larmci -lm
tests_cg_cg_DEPENDENCIES = libarmci.la
