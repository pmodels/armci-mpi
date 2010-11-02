#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

check_PROGRAMS += transp1D-c

transp1D_c_SOURCES = $(top_srcdir)/tests/transp1D/transp1D-c.c
transp1D_c_LDADD = -larmci
transp1D_c_DEPENDENCIES = libarmci.la
