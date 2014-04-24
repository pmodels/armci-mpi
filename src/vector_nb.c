/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>

#include <armci.h>
#include <armci_internals.h>
#include <debug.h>
#include <gmr.h>

/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_NbPutV = PARMCI_NbPutV
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_NbPutV ARMCI_NbPutV
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_NbPutV as PARMCI_NbPutV
#endif
/* -- end weak symbols block -- */

/** Generalized I/O vector one-sided put.
  *
  * @param[in] iov      Vector of transfer information.
  * @param[in] iov_len  Length of iov.
  * @param[in] proc     Target process.
  * @return             Success 0, otherwise non-zero.
  */
int PARMCI_NbPutV(armci_giov_t *iov, int iov_len, int proc, armci_hdl_t* handle) {
  int v;

  for (v = 0; v < iov_len; v++) {
    void **src_buf;
    int    overlapping, same_alloc;

    if (iov[v].ptr_array_len == 0) continue; // NOP //
    if (iov[v].bytes == 0) continue; // NOP //

    overlapping = ARMCII_Iov_check_overlap(iov[v].dst_ptr_array, iov[v].ptr_array_len, iov[v].bytes);
    same_alloc  = ARMCII_Iov_check_same_allocation(iov[v].dst_ptr_array, iov[v].ptr_array_len, proc);

    ARMCII_Buf_prepare_read_vec(iov[v].src_ptr_array, &src_buf, iov[v].ptr_array_len, iov[v].bytes);
    ARMCII_Iov_op_dispatch(ARMCII_OP_PUT, src_buf, iov[v].dst_ptr_array, iov[v].ptr_array_len, iov[v].bytes, 0, overlapping, same_alloc, proc);
    ARMCII_Buf_finish_read_vec(iov[v].src_ptr_array, src_buf, iov[v].ptr_array_len, iov[v].bytes);
  }

  if (handle!=NULL) {
      /* Regular (not aggregate) handles merely store the target for future flushing. */
      handle->target = proc;
  }

#ifdef EXPLICIT_PROGRESS
  gmr_progress();
#endif

  return 0;
}


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_NbGetV = PARMCI_NbGetV
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_NbGetV ARMCI_NbGetV
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_NbGetV as PARMCI_NbGetV
#endif
/* -- end weak symbols block -- */

/** Generalized I/O vector one-sided get.
  *
  * @param[in] iov      Vector of transfer information.
  * @param[in] iov_len  Length of iov.
  * @param[in] proc     Target process.
  * @return             Success 0, otherwise non-zero.
  */
int PARMCI_NbGetV(armci_giov_t *iov, int iov_len, int proc, armci_hdl_t* handle) {
  int v;

  for (v = 0; v < iov_len; v++) {
    void **dst_buf;
    int    overlapping, same_alloc;

    if (iov[v].ptr_array_len == 0) continue; // NOP //
    if (iov[v].bytes == 0) continue; // NOP //

    // overlapping = ARMCII_Iov_check_overlap(iov[v].src_ptr_array, iov[v].ptr_array_len, iov[v].bytes);
    overlapping = ARMCII_Iov_check_overlap(iov[v].dst_ptr_array, iov[v].ptr_array_len, iov[v].bytes);
    same_alloc  = ARMCII_Iov_check_same_allocation(iov[v].src_ptr_array, iov[v].ptr_array_len, proc);

    ARMCII_Buf_prepare_write_vec(iov[v].dst_ptr_array, &dst_buf, iov[v].ptr_array_len, iov[v].bytes);
    ARMCII_Iov_op_dispatch(ARMCII_OP_GET, iov[v].src_ptr_array, dst_buf, iov[v].ptr_array_len, iov[v].bytes, 0, overlapping, same_alloc, proc);
    ARMCII_Buf_finish_write_vec(iov[v].dst_ptr_array, dst_buf, iov[v].ptr_array_len, iov[v].bytes);
  }

  if (handle!=NULL) {
      /* Regular (not aggregate) handles merely store the target for future flushing. */
      handle->target = proc;
  }

#ifdef EXPLICIT_PROGRESS
  gmr_progress();
#endif

  return 0;
}


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_NbAccV = PARMCI_NbAccV
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_NbAccV ARMCI_NbAccV
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_NbAccV as PARMCI_NbAccV
#endif
/* -- end weak symbols block -- */

/** Generalized I/O vector one-sided accumulate.
  *
  * @param[in] iov      Vector of transfer information.
  * @param[in] iov_len  Length of iov.
  * @param[in] proc     Target process.
  * @return             Success 0, otherwise non-zero.
  */
int PARMCI_NbAccV(int datatype, void *scale, armci_giov_t *iov, int iov_len, int proc, armci_hdl_t* handle) {
  int v;

  for (v = 0; v < iov_len; v++) {
    void **src_buf;
    int    overlapping, same_alloc;

    if (iov[v].ptr_array_len == 0) continue; // NOP //
    if (iov[v].bytes == 0) continue; // NOP //

    overlapping = ARMCII_Iov_check_overlap(iov[v].dst_ptr_array, iov[v].ptr_array_len, iov[v].bytes);
    same_alloc  = ARMCII_Iov_check_same_allocation(iov[v].dst_ptr_array, iov[v].ptr_array_len, proc);

    ARMCII_Buf_prepare_acc_vec(iov[v].src_ptr_array, &src_buf, iov[v].ptr_array_len, iov[v].bytes, datatype, scale);
    ARMCII_Iov_op_dispatch(ARMCII_OP_ACC, src_buf, iov[v].dst_ptr_array, iov[v].ptr_array_len, iov[v].bytes, datatype, overlapping, same_alloc, proc);
    ARMCII_Buf_finish_acc_vec(iov[v].src_ptr_array, src_buf, iov[v].ptr_array_len, iov[v].bytes);
  }

  if (handle!=NULL) {
      /* Regular (not aggregate) handles merely store the target for future flushing. */
      handle->target = proc;
  }

#ifdef EXPLICIT_PROGRESS
  gmr_progress();
#endif

  return 0;
}
