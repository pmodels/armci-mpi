/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#ifndef HAVE_ARMCI_INTERNALS_H
#define HAVE_ARMCI_INTERNALS_H

#include <armci.h>
#include <mem_region.h>

/* Disable safety checks if the user asks for it */

#ifdef NO_SEATBELTS
#define NO_CHECK_OVERLAP /* Disable checks for overlapping IOV operations */
//#define NO_USE_CTREE     /* Use the slower O(N) check instead of the conflict tree */
#define NO_CHECK_BUFFERS /* Disable checking for shared origin buffers    */

#else
#endif

/* Internal types */

typedef struct {
  int     initialized;    /* Has ARMCI been initialized?            */
  int     debug_alloc;    /* Has ARMCI been initialized?            */

  int     iov_method;     /* Currently selected IOV transfer method */

  int     dla_state;      /* Direct Local Access (load/store) state */
  mem_region_t *dla_mreg; /* Current region exposed for DLA         */
} global_state_t;

enum ARMCII_Op_e { ARMCII_OP_PUT, ARMCII_OP_GET, ARMCII_OP_ACC };

enum ARMCII_Iov_methods_e { ARMCII_IOV_AUTO, ARMCII_IOV_SAFE,
                            ARMCII_IOV_ONELOCK, ARMCII_IOV_DTYPE };

enum ARMCII_Dla_state { ARMCII_DLA_CLOSED, ARMCII_DLA_OPEN, ARMCII_DLA_SUSPENDED };

/* Global data */

extern ARMCI_Group    ARMCI_GROUP_WORLD;
extern ARMCI_Group    ARMCI_GROUP_DEFAULT;
extern MPI_Op         MPI_ABSMIN_OP;
extern MPI_Op         MPI_ABSMAX_OP;
extern global_state_t ARMCII_GLOBAL_STATE;
  
  
/* GOP Operators */

void ARMCII_Absmin_op(void *invec, void *inoutvec, int *len, MPI_Datatype *datatype);
void ARMCII_Absmax_op(void *invec, void *inoutvec, int *len, MPI_Datatype *datatype);


/* Translate betwee ARMCI and MPI ranks */

int  ARMCII_Translate_absolute_to_group(MPI_Comm group_comm, int world_rank);


/* I/O Vector data management and implementation */

void ARMCII_Acc_type_translate(int armci_datatype, MPI_Datatype *type, int *type_size);

int  ARMCII_Iov_check_overlap(void **ptrs, int count, int size);
int  ARMCII_Iov_check_same_allocation(void **ptrs, int count, int proc);

void ARMCII_Strided_to_iov(armci_giov_t *iov,
               void *src_ptr, int src_stride_ar[/*stride_levels*/],
               void *dst_ptr, int dst_stride_ar[/*stride_levels*/], 
               int count[/*stride_levels+1*/], int stride_levels);

int ARMCII_Iov_op_dispatch(int op, void **src, void **dst, int count, int size,
    int datatype, int overlapping, int same_alloc, int proc);

int ARMCII_Iov_op_safe(int op, void **src, void **dst, int count, int elem_count,
    MPI_Datatype type, int proc);
int ARMCII_Iov_op_onelock(int op, void **src, void **dst, int count, int elem_count,
    MPI_Datatype type, int proc);
int ARMCII_Iov_op_datatype(int op, void **src, void **dst, int count, int elem_count,
    MPI_Datatype type, int proc);


/* Shared to private buffer management routines */

int  ARMCII_Buf_put_prepare(void **orig_bufs, void ***new_bufs_ptr, int count, int size);
void ARMCII_Buf_put_finish(void **orig_bufs, void **new_bufs, int count, int size);
int  ARMCII_Buf_acc_prepare(void **orig_bufs, void ***new_bufs_ptr, int count, int size,
                            int datatype, void *scale);
void ARMCII_Buf_acc_finish(void **orig_bufs, void **new_bufs, int count, int size);
int  ARMCII_Buf_get_prepare(void **orig_bufs, void ***new_bufs_ptr, int count, int size);
void ARMCII_Buf_get_finish(void **orig_bufs, void **new_bufs, int count, int size);

#endif /* HAVE_ARMCI_INTERNALS_H */
