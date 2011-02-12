#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

check_PROGRAMS += tests/lu/lu tests/lu/lu-block tests/lu/lu-b-bc

TESTS          += tests/lu/lu-block tests/lu/lu-b-bc

tests_lu_lu_SOURCES = $(top_srcdir)/tests/lu/lu.c $(top_srcdir)/tests/lu/lu_timing.c
tests_lu_lu_LDADD = -larmci -lm
tests_lu_lu_DEPENDENCIES = libarmci.la

tests_lu_lu_block_SOURCES = $(top_srcdir)/tests/lu/lu-block.c $(top_srcdir)/tests/lu/lu_timing.c
tests_lu_lu_block_LDADD = -larmci -lm
tests_lu_lu_block_DEPENDENCIES = libarmci.la

tests_lu_lu_b_bc_SOURCES = $(top_srcdir)/tests/lu/lu-b-bc.c $(top_srcdir)/tests/lu/lu_timing.c
tests_lu_lu_b_bc_LDADD = -larmci -lm
tests_lu_lu_b_bc_DEPENDENCIES = libarmci.la
