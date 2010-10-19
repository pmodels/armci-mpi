#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

bin_PROGRAMS += overlap simple

overlap_SOURCES = $(top_srcdir)/tests/non-blocking/overlap.c
simple_SOURCES = $(top_srcdir)/tests/non-blocking/simple.c
