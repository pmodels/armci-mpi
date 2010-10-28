/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#ifndef HAVE_MEM_REGION_H
#define HAVE_MEM_REGION_H

#include <mpi.h>

#include <armci.h>
#include <armcix.h>

typedef struct {
  void *base;
  int   size;
} mem_region_slice_t;

typedef struct mem_region_s {
  MPI_Win              window;
  MPI_Comm             comm;
  armcix_mutex_grp_t   rmw_mutex;
  struct mem_region_s *prev;
  struct mem_region_s *next;
  int                  nslices;
  mem_region_slice_t  *slices;
} mem_region_t;

extern mem_region_t *mreg_list;

mem_region_t *mem_region_create(int local_size, void **base_ptrs, ARMCI_Group *group);
void          mem_region_destroy(mem_region_t *mreg, ARMCI_Group *group);
mem_region_t *mem_region_lookup(void *ptr, int proc);

#endif /* HAVE_MEM_REGION_H */
