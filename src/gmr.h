/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#ifndef HAVE_GMR_H
#define HAVE_GMR_H

#include <mpi.h>

#include <armci.h>
#include <armcix.h>

#ifdef HAVE_STDBOOL_H
#include <stdbool.h>
#else
typedef int bool;
#define false 0
#define true  1
#endif

typedef armci_size_t gmr_size_t;
#define GMR_MPI_SIZE_T ARMCII_MPI_SIZE_T

typedef struct {
  void       *base;
  gmr_size_t  size;
} gmr_slice_t;

typedef struct gmr_s {
  MPI_Win                 window;         /* MPI Window for this GMR                                        */
  ARMCI_Group             group;          /* Copy of the ARMCI group on which this GMR was allocated        */

  struct gmr_s           *prev;           /* Linked list pointers for GMR list                              */
  struct gmr_s           *next;
  gmr_slice_t            *slices;         /* Array of GMR slices for this allocation                        */
  int                     nslices;
  bool                    unified;        /* separate/unified attribute of the window                       */
} gmr_t;

extern gmr_t *gmr_list;

gmr_t *gmr_create(gmr_size_t local_size, void **base_ptrs, ARMCI_Group *group);
void   gmr_destroy(gmr_t *mreg, ARMCI_Group *group);
int    gmr_destroy_all(void);
gmr_t *gmr_lookup(void *ptr, int proc);

// blocking
int gmr_fetch_and_op(gmr_t *mreg, void *src, void *out, void *dst, MPI_Datatype type, MPI_Op op, int proc);

// nonblocking
int gmr_get(gmr_t *mreg, void *src, void *dst, int size,
            int target, armci_hdl_t * handle);
int gmr_put(gmr_t *mreg, void *src, void *dst, int size,
            int target, armci_hdl_t * handle);
int gmr_accumulate(gmr_t *mreg, void *src, void *dst, int count, MPI_Datatype type,
                   int proc, armci_hdl_t * handle);
int gmr_get_accumulate(gmr_t *mreg, void *src, void *out, void *dst, int count, MPI_Datatype type,
                       MPI_Op op, int proc, armci_hdl_t * handle);

int gmr_get_typed(gmr_t *mreg, void *src, int src_count, MPI_Datatype src_type,
                  void *dst, int dst_count, MPI_Datatype dst_type,
                  int proc, armci_hdl_t * handle);
int gmr_put_typed(gmr_t *mreg, void *src, int src_count, MPI_Datatype src_type,
                  void *dst, int dst_count, MPI_Datatype dst_type,
                  int proc, armci_hdl_t * handle);
int gmr_accumulate_typed(gmr_t *mreg, void *src, int src_count, MPI_Datatype src_type,
                         void *dst, int dst_count, MPI_Datatype dst_type,
                         int proc, armci_hdl_t * handle);
int gmr_get_accumulate_typed(gmr_t *mreg, void *src, int src_count, MPI_Datatype src_type, 
                             void *out, int out_count, MPI_Datatype out_type,
                             void *dst, int dst_count, MPI_Datatype dst_type,
                             MPI_Op op, int proc, armci_hdl_t * handle);

int gmr_lockall(gmr_t *mreg);
int gmr_unlockall(gmr_t *mreg);
int gmr_flush(gmr_t *mreg, int proc, int local_only);
int gmr_flushall(gmr_t *mreg, int local_only);
int gmr_sync(gmr_t *mreg);
int gmr_wait(armci_hdl_t * handle);

void gmr_progress(void);
void gmr_handle_add_request(armci_hdl_t * handle, MPI_Request req);

#endif /* HAVE_GMR_H */
