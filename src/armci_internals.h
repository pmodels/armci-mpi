/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#ifndef HAVE_ARMCI_INTERNALS_H
#define HAVE_ARMCI_INTERNALS_H

#include <armci.h>
#include <armciconf.h>

#if   HAVE_STDINT_H
#  include <stdint.h>
#elif HAVE_INTTYPES_H
#  include <inttypes.h>
#endif

/* Disable safety checks if the user asks for it */

#ifdef NO_SEATBELTS
#define NO_CHECK_OVERLAP /* Disable checks for overlapping IOV operations */
//#define NO_USE_CTREE     /* Use the slower O(N) check instead of the conflict tree */
#define NO_CHECK_BUFFERS /* Disable checking for shared origin buffers    */

#else
#endif

/* Internal types */

enum ARMCII_Op_e { ARMCII_OP_PUT, ARMCII_OP_GET, ARMCII_OP_ACC };

enum ARMCII_Iov_methods_e { ARMCII_IOV_AUTO, ARMCII_IOV_SAFE,
                            ARMCII_IOV_ONELOCK, ARMCII_IOV_DTYPE };

enum ARMCII_Shr_buf_methods_e { ARMCII_SHR_BUF_LOCK, ARMCII_SHR_BUF_COPY, 
                                ARMCII_SHR_BUF_NOGUARD };

extern char ARMCII_Iov_methods_str[][10];
extern char ARMCII_Shr_buf_methods_str[][10];

typedef struct {
  int     init_count;           /* Number of times ARMCI_Init has been called           */
  int     debug_alloc;          /* Do extra debuggin on memory allocation               */
  int     iov_checks_disabled;  /* Disable IOV same allocation and overlapping checks   */
  int     no_mpi_bottom;        /* Don't generate datatypes relative to MPI_BOTTOM      */
  int     verbose;              /* ARMCI should produce extra status output             */

  enum ARMCII_Iov_methods_e     iov_method;     /* IOV transfer method                  */
  enum ARMCII_Shr_buf_methods_e shr_buf_method; /* Shared buffer management method      */
} global_state_t;


/* Global data */

extern ARMCI_Group    ARMCI_GROUP_WORLD;
extern ARMCI_Group    ARMCI_GROUP_DEFAULT;
extern MPI_Op         MPI_ABSMIN_OP;
extern MPI_Op         MPI_ABSMAX_OP;
extern global_state_t ARMCII_GLOBAL_STATE;
  

/* Utility functions */

void ARMCII_Bzero(void *buf, int size);


/* GOP Operators */

void ARMCII_Absmin_op(void *invec, void *inoutvec, int *len, MPI_Datatype *datatype);
void ARMCII_Absmax_op(void *invec, void *inoutvec, int *len, MPI_Datatype *datatype);
void ARMCII_Absv_op(void *invec, void *inoutvec, int *len, MPI_Datatype *datatype);


/* Translate between ARMCI and MPI ranks */

int  ARMCII_Translate_absolute_to_group(MPI_Comm group_comm, int world_rank);


/* I/O Vector data management and implementation */

void ARMCII_Acc_type_translate(int armci_datatype, MPI_Datatype *type, int *type_size);

int  ARMCII_Iov_check_overlap(void **ptrs, int count, int size);
int  ARMCII_Iov_check_same_allocation(void **ptrs, int count, int proc);

void ARMCII_Strided_to_iov(armci_giov_t *iov,
               void *src_ptr, int src_stride_ar[/*stride_levels*/],
               void *dst_ptr, int dst_stride_ar[/*stride_levels*/], 
               int count[/*stride_levels+1*/], int stride_levels);

int ARMCII_Iov_op_dispatch(enum ARMCII_Op_e op, void **src, void **dst, int count, int size,
    int datatype, int overlapping, int same_alloc, int proc);

int ARMCII_Iov_op_safe(enum ARMCII_Op_e op, void **src, void **dst, int count, int elem_count,
    MPI_Datatype type, int proc);
int ARMCII_Iov_op_onelock(enum ARMCII_Op_e op, void **src, void **dst, int count, int elem_count,
    MPI_Datatype type, int proc);
int ARMCII_Iov_op_datatype(enum ARMCII_Op_e op, void **src, void **dst, int count, int elem_count,
    MPI_Datatype type, int proc);
int ARMCII_Iov_op_datatype_no_bottom(enum ARMCII_Op_e op, void **src, void **dst, int count, int elem_count,
    MPI_Datatype type, int proc);


/* Shared to private buffer management routines */

int  ARMCII_Buf_prepare_putv(void **orig_bufs, void ***new_bufs_ptr, int count, int size);
void ARMCII_Buf_finish_putv(void **orig_bufs, void **new_bufs, int count, int size);
int  ARMCII_Buf_prepare_accv(void **orig_bufs, void ***new_bufs_ptr, int count, int size,
                            int datatype, void *scale);
void ARMCII_Buf_finish_accv(void **orig_bufs, void **new_bufs, int count, int size);
int  ARMCII_Buf_prepare_getv(void **orig_bufs, void ***new_bufs_ptr, int count, int size);
void ARMCII_Buf_finish_getv(void **orig_bufs, void **new_bufs, int count, int size);

void *ARMCII_Buf_prepare_acc(void *buf, int size, int datatype, void *scale);

#endif /* HAVE_ARMCI_INTERNALS_H */
