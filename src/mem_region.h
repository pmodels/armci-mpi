/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#ifndef HAVE_MEM_REGION_H
#define HAVE_MEM_REGION_H

#include <mpi.h>

#include <armci.h>
#include <armcix.h>

enum mreg_lock_states_e { 
  MREG_LOCK_UNLOCKED,  /* Mem region is unlocked */
  MREG_LOCK_EXCLUSIVE, /* Mem region is locked for exclusive access */
  MREG_LOCK_SHARED     /* Mem region is locked for shared (non-conflicting) access */
};

typedef struct {
  void *base;
  int   size;
} mem_region_slice_t;

typedef struct mem_region_s {
  MPI_Win              window;
  ARMCI_Group          group;

  int                  access_mode;
  int                  lock_state;
  armcix_mutex_grp_t   rmw_mutex;

  struct mem_region_s *prev;
  struct mem_region_s *next;
  int                  nslices;
  mem_region_slice_t  *slices;
} mem_region_t;

extern mem_region_t *mreg_list;

mem_region_t *mreg_create(int local_size, void **base_ptrs, ARMCI_Group *group);
void          mreg_destroy(mem_region_t *mreg, ARMCI_Group *group);
int           mreg_destroy_all(void);
mem_region_t *mreg_lookup(void *ptr, int proc);

int mreg_get(mem_region_t *mreg, void *src, void *dst, int size, int target);
int mreg_put(mem_region_t *mreg, void *src, void *dst, int size, int target);
int mreg_accumulate(mem_region_t *mreg, void *src, void *dst, int count, MPI_Datatype type, int proc);

int mreg_get_typed(mem_region_t *mreg, void *src, int src_count, MPI_Datatype src_type,
    void *dst, int dst_count, MPI_Datatype dst_type, int proc);
int mreg_put_typed(mem_region_t *mreg, void *src, int src_count, MPI_Datatype src_type,
    void *dst, int dst_count, MPI_Datatype dst_type, int proc);
int mreg_accumulate_typed(mem_region_t *mreg, void *src, int src_count, MPI_Datatype src_type,
    void *dst, int dst_count, MPI_Datatype dst_type, int proc);

void mreg_lock(mem_region_t *mreg, int proc);
void mreg_lock_ldst(mem_region_t *mreg);
void mreg_unlock(mem_region_t *mreg, int proc);

#endif /* HAVE_MEM_REGION_H */
