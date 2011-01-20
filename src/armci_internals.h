/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#ifndef HAVE_ARMCI_INTERNALS_H
#define HAVE_ARMCI_INTERNALS_H

#include <armci.h>

/* Flags -- TODO: These will need configure options eventually */

#ifdef NO_SEATBELTS
#define NO_CHECK_OVERLAP
#define NO_CHECK_BUFFERS

#else
//#define NO_CHECK_OVERLAP /* Disable checks for overlapping IOV operations */
//#define NO_USE_CTREE     /* Use the slower O(N) check instead of the conflict tree */
//#define NO_CHECK_BUFFERS /* Disable checking for shared origin buffers    */
#endif

/* Types */

typedef struct {
  int iov_method;
} global_state_t;

enum ARMCII_Op_e { ARMCII_OP_PUT, ARMCII_OP_GET, ARMCII_OP_ACC };
enum ARMCII_Iov_methods_e { ARMCII_IOV_AUTO, ARMCII_IOV_SAFE,
                            ARMCII_IOV_ONELOCK, ARMCII_IOV_DTYPE };

/* Global data */

extern ARMCI_Group    ARMCI_GROUP_WORLD;
extern ARMCI_Group    ARMCI_GROUP_DEFAULT;
extern global_state_t ARMCII_GLOBAL_STATE;


/* Assorted utility functions */

#define ARMCII_Error(MSG,CODE) ARMCII_Error_impl(__FILE__,__LINE__,__func__,MSG,CODE)
void    ARMCII_Error_impl(const char *file, const int line, const char *func, const char *msg, int code);

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
