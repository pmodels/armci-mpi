#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

bin_PROGRAMS += test_onesided test_mutex test_mutex_rmw test_mutex_trylock test_malloc \
	test_malloc_irreg ARMCI_PutS_latency ARMCI_PutAccS_latency test_groups test_group_split \
	test_malloc_group

test_onesided_SOURCES = $(top_srcdir)/tests/test_onesided.c
test_mutex_SOURCES = $(top_srcdir)/tests/test_mutex.c
test_mutex_rmw_SOURCES = $(top_srcdir)/tests/test_mutex_rmw.c
test_mutex_trylock_SOURCES = $(top_srcdir)/tests/test_mutex_trylock.c
test_malloc_SOURCES = $(top_srcdir)/tests/test_malloc.c
test_malloc_irreg_SOURCES = $(top_srcdir)/tests/test_malloc_irreg.c
ARMCI_PutS_latency_SOURCES = $(top_srcdir)/tests/ARMCI_PutS_latency.c
ARMCI_PutAccS_latency_SOURCES = $(top_srcdir)/tests/ARMCI_PutAccS_latency.c
test_groups_SOURCES = $(top_srcdir)/tests/test_groups.c
test_group_split_SOURCES = $(top_srcdir)/tests/test_group_split.c
test_malloc_group_SOURCES = $(top_srcdir)/tests/test_malloc_group.c

# include armci_test/Makefile.mk
# include cg/Makefile.mk
# include lu/Makefile.mk
# include transp1D/Makefile.mk
# include non-blocking/Makefile.mk
