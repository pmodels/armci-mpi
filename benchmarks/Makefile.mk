#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

bin_PROGRAMS += ping-pong ping-pong-mpi ring-flood

ping_pong_SOURCES = $(top_srcdir)/benchmarks/ping-pong.c
ping_pong_mpi_SOURCES = $(top_srcdir)/benchmarks/ping-pong-mpi.c
ring_flood_SOURCES = $(top_srcdir)/benchmarks/ring-flood.c

ping_pong_LDADD = -larmci
ping_pong_DEPENDENCIES = libarmci.la

ping_pong_mpi_LDADD = -larmci
ping_pong_mpi_DEPENDENCIES = libarmci.la

ring_flood_LDADD = -larmci
ring_flood_DEPENDENCIES = libarmci.la
