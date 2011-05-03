/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#ifndef HAVE_GMR_H
#define HAVE_GMR_H

#include <mpi.h>

#include <armci.h>
#include <armcix.h>

enum gmr_lock_states_e { 
  MREG_LOCK_UNLOCKED,    /* Mem region is unlocked */
  MREG_LOCK_EXCLUSIVE,   /* Mem region is locked for exclusive access */
  MREG_LOCK_SHARED,      /* Mem region is locked for shared (non-conflicting) access */
  MREG_LOCK_DLA,         /* Mem region is locked for Direct Local Access */
  MREG_LOCK_DLA_SUSP     /* Mem region is unlocked and DLA is suspended */
};

typedef struct {
  void *base;
  int   size;
} gmr_slice_t;

typedef struct gmr_s {
  MPI_Win                 window;         /* MPI Window for this GMR                                        */
  ARMCI_Group             group;          /* Copy of the ARMCI group on which this GMR was allocated        */

  int                     access_mode;    /* Current access mode                                            */
  enum gmr_lock_states_e  lock_state;     /* State of the lock                                              */
  int                     lock_target;    /* Group (window) rank of the current target (if locked)          */
  int                     dla_lock_count; /* Access count on the DLA lock.  Can unlock when this reaches 0. */
  armcix_mutex_hdl_t      rmw_mutex;      /* Mutex used for Read-Modify-Write operations                    */

  struct gmr_s           *prev;           /* Linked list pointers for GMR list                              */
  struct gmr_s           *next;
  gmr_slice_t            *slices;         /* Array of GMR slices for this allocation                        */
  int                     nslices;
} gmr_t;

extern gmr_t *mreg_list;

gmr_t *mreg_create(int local_size, void **base_ptrs, ARMCI_Group *group);
void   mreg_destroy(gmr_t *mreg, ARMCI_Group *group);
int    mreg_destroy_all(void);
gmr_t *mreg_lookup(void *ptr, int proc);

int mreg_get(gmr_t *mreg, void *src, void *dst, int size, int target);
int mreg_put(gmr_t *mreg, void *src, void *dst, int size, int target);
int mreg_accumulate(gmr_t *mreg, void *src, void *dst, int count, MPI_Datatype type, int proc);

int mreg_get_typed(gmr_t *mreg, void *src, int src_count, MPI_Datatype src_type,
    void *dst, int dst_count, MPI_Datatype dst_type, int proc);
int mreg_put_typed(gmr_t *mreg, void *src, int src_count, MPI_Datatype src_type,
    void *dst, int dst_count, MPI_Datatype dst_type, int proc);
int mreg_accumulate_typed(gmr_t *mreg, void *src, int src_count, MPI_Datatype src_type,
    void *dst, int dst_count, MPI_Datatype dst_type, int proc);

void mreg_lock(gmr_t *mreg, int proc);
void mreg_unlock(gmr_t *mreg, int proc);

void mreg_dla_lock(gmr_t *mreg);
void mreg_dla_unlock(gmr_t *mreg);

#endif /* HAVE_GMR_H */
