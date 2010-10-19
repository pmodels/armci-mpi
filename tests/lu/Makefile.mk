#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

bin_PROGRAMS += lu lu-block lu-b-bc

lu_SOURCES = $(top_srcdir)/tests/lu/lu.c $(top_srcdir)/tests/lu/lu_timing.c
lu_block_SOURCES = $(top_srcdir)/tests/lu/lu-block.c $(top_srcdir)/tests/lu/lu_timing.c
lu_b_bc_SOURCES = $(top_srcdir)/tests/lu/lu-b-bc.c $(top_srcdir)/tests/lu/lu_timing.c
