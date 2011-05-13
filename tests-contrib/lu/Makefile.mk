#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

check_PROGRAMS += tests-contrib/lu/lu tests-contrib/lu/lu-block tests-contrib/lu/lu-b-bc

TESTS          += tests-contrib/lu/lu-block tests-contrib/lu/lu-b-bc

tests_contrib_lu_lu_SOURCES = $(top_srcdir)/tests-contrib/lu/lu.c $(top_srcdir)/tests-contrib/lu/lu_timing.c
tests_contrib_lu_lu_LDADD = -larmci -lm
tests_contrib_lu_lu_DEPENDENCIES = libarmci.la

tests_contrib_lu_lu_block_SOURCES = $(top_srcdir)/tests-contrib/lu/lu-block.c $(top_srcdir)/tests-contrib/lu/lu_timing.c
tests_contrib_lu_lu_block_LDADD = -larmci -lm
tests_contrib_lu_lu_block_DEPENDENCIES = libarmci.la

tests_contrib_lu_lu_b_bc_SOURCES = $(top_srcdir)/tests-contrib/lu/lu-b-bc.c $(top_srcdir)/tests-contrib/lu/lu_timing.c
tests_contrib_lu_lu_b_bc_LDADD = -larmci -lm
tests_contrib_lu_lu_b_bc_DEPENDENCIES = libarmci.la
