#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

bin_PROGRAMS += ping-pong ping-pong-mpi ring-flood

ping_pong_SOURCES = $(top_srcdir)/benchmarks/ping-pong.c
ping_pong_mpi_SOURCES = $(top_srcdir)/benchmarks/ping-pong-mpi.c
ring_flood_SOURCES = $(top_srcdir)/benchmarks/ring-flood.c
