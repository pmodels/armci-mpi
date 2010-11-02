#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

check_PROGRAMS += lu lu-block lu-b-bc

lu_SOURCES = $(top_srcdir)/tests/lu/lu.c $(top_srcdir)/tests/lu/lu_timing.c
lu_LDADD = -larmci -lm
lu_DEPENDENCIES = libarmci.la

lu_block_SOURCES = $(top_srcdir)/tests/lu/lu-block.c $(top_srcdir)/tests/lu/lu_timing.c
lu_block_LDADD = -larmci -lm
lu_block_DEPENDENCIES = libarmci.la

lu_b_bc_SOURCES = $(top_srcdir)/tests/lu/lu-b-bc.c $(top_srcdir)/tests/lu/lu_timing.c
lu_b_bc_LDADD = -larmci -lm
lu_b_bc_DEPENDENCIES = libarmci.la
