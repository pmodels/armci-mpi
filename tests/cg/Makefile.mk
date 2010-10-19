#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

bin_PROGRAMS += cg

cg_SOURCES = $(top_srcdir)/tests/cg/cg.c $(top_srcdir)/tests/cg/compute.c \
	$(top_srcdir)/tests/cg/read_input.c $(top_srcdir)/tests/cg/cg_timing.c
