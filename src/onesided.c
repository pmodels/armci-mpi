/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

#include <armci.h>
#include <armcix.h>
#include <armci_internals.h>
#include <debug.h>
#include <gmr.h>


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_Access_begin = PARMCI_Access_begin
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_Access_begin ARMCI_Access_begin
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_Access_begin as PARMCI_Access_begin
#endif
/* -- end weak symbols block -- */

/** Declare the start of a local access epoch.  This allows direct access to
  * data in local memory.
  *
  * @param[in] ptr Pointer to the allocation that will be accessed directly 
  */
void PARMCI_Access_begin(void *ptr) {
  gmr_t *mreg;

  mreg = gmr_lookup(ptr, ARMCI_GROUP_WORLD.rank);
  ARMCII_Assert_msg(mreg != NULL, "Invalid remote pointer");

  gmr_sync(mreg);
}


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_Access_end = PARMCI_Access_end
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_Access_end ARMCI_Access_end
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_Access_end as PARMCI_Access_end
#endif
/* -- end weak symbols block -- */

/** Declare the end of a local access epoch.
  *
  * \note MPI-2 does not allow multiple locks at once, so you can have only one
  * access epoch open at a time and cannot do put/get/acc while in an access
  * region.
  *
  * @param[in] ptr Pointer to the allocation that was accessed directly 
  */
void PARMCI_Access_end(void *ptr) {
  gmr_t *mreg;

  mreg = gmr_lookup(ptr, ARMCI_GROUP_WORLD.rank);
  ARMCII_Assert_msg(mreg != NULL, "Invalid remote pointer");

  gmr_sync(mreg);
}


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_Get = PARMCI_Get
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_Get ARMCI_Get
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_Get as PARMCI_Get
#endif
/* -- end weak symbols block -- */

/** One-sided get operation.
  *
  * @param[in] src    Source address (remote)
  * @param[in] dst    Destination address (local)
  * @param[in] size   Number of bytes to transfer
  * @param[in] target Process id to target
  * @return           0 on success, non-zero on failure
  */
int PARMCI_Get(void *src, void *dst, int size, int target) {
  gmr_t *src_mreg, *dst_mreg;

  src_mreg = gmr_lookup(src, target);

  /* If NOGUARD is set, assume the buffer is not shared */
  if (ARMCII_GLOBAL_STATE.shr_buf_method != ARMCII_SHR_BUF_NOGUARD)
    dst_mreg = gmr_lookup(dst, ARMCI_GROUP_WORLD.rank);
  else
    dst_mreg = NULL;

  ARMCII_Assert_msg(src_mreg != NULL, "Invalid remote pointer");

  /* Local operation */
  if (target == ARMCI_GROUP_WORLD.rank && dst_mreg == NULL) {
    ARMCI_Copy(src, dst, size);
  }

  /* Origin buffer is private */
  else if (dst_mreg == NULL) {
    gmr_get(src_mreg, src, dst, size, target, NULL /* handle */);
    gmr_flush(src_mreg, target, 0); /* it's a round trip so w.r.t. flush, local=remote */
  }

  /* COPY: Either origin and target buffers are in the same window and we can't
   * lock the same window twice (MPI semantics) or the user has requested
   * always-copy mode. */
  else {
    void *dst_buf;

    MPI_Alloc_mem(size, MPI_INFO_NULL, &dst_buf);
    ARMCII_Assert(dst_buf != NULL);

    gmr_get(src_mreg, src, dst_buf, size, target, NULL /* handle */);
    gmr_flush(src_mreg, target, 0); /* it's a round trip so w.r.t. flush, local=remote */

    ARMCI_Copy(dst_buf, dst, size);

    MPI_Free_mem(dst_buf);
  }

  return 0;
}


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_Put = PARMCI_Put
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_Put ARMCI_Put
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_Put as PARMCI_Put
#endif
/* -- end weak symbols block -- */

/** One-sided put operation.
  *
  * @param[in] src    Source address (remote)
  * @param[in] dst    Destination address (local)
  * @param[in] size   Number of bytes to transfer
  * @param[in] target Process id to target
  * @return           0 on success, non-zero on failure
  */
int PARMCI_Put(void *src, void *dst, int size, int target) {
  gmr_t *src_mreg, *dst_mreg;

  dst_mreg = gmr_lookup(dst, target);

  /* If NOGUARD is set, assume the buffer is not shared */
  if (ARMCII_GLOBAL_STATE.shr_buf_method != ARMCII_SHR_BUF_NOGUARD)
    src_mreg = gmr_lookup(src, ARMCI_GROUP_WORLD.rank);
  else
    src_mreg = NULL;

  ARMCII_Assert_msg(dst_mreg != NULL, "Invalid remote pointer");

  /* Local operation */
  if (target == ARMCI_GROUP_WORLD.rank && src_mreg == NULL) {
    ARMCI_Copy(src, dst, size);
  }

  /* Origin buffer is private */
  else if (src_mreg == NULL) {
    gmr_put(dst_mreg, src, dst, size, target, NULL /* handle */);
    gmr_flush(dst_mreg, target, 1); /* flush_local */
  }

  /* COPY: Either origin and target buffers are in the same window and we can't
   * lock the same window twice (MPI semantics) or the user has requested
   * always-copy mode. */
  else {
    void *src_buf;

    MPI_Alloc_mem(size, MPI_INFO_NULL, &src_buf);
    ARMCII_Assert(src_buf != NULL);

    ARMCI_Copy(src, src_buf, size);

    gmr_put(dst_mreg, src_buf, dst, size, target, NULL /* handle */);
    gmr_flush(dst_mreg, target, 1); /* flush_local */

    MPI_Free_mem(src_buf);
  }

  return 0;
}


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_Acc = PARMCI_Acc
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_Acc ARMCI_Acc
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_Acc as PARMCI_Acc
#endif
/* -- end weak symbols block -- */

/** One-sided accumulate operation.
  *
  * @param[in] datatype ARMCI data type for the accumulate operation (see armci.h)
  * @param[in] scale    Pointer for a scalar of type datatype that will be used to
  *                     scale values in the source buffer
  * @param[in] src      Source address (remote)
  * @param[in] dst      Destination address (local)
  * @param[in] bytes    Number of bytes to transfer
  * @param[in] proc     Process id to target
  * @return             0 on success, non-zero on failure
  */
int PARMCI_Acc(int datatype, void *scale, void *src, void *dst, int bytes, int proc) {
  void  *src_buf;
  int    count, type_size, scaled;
  MPI_Datatype type;
  gmr_t *src_mreg, *dst_mreg;

  /* If NOGUARD is set, assume the buffer is not shared */
  if (ARMCII_GLOBAL_STATE.shr_buf_method != ARMCII_SHR_BUF_NOGUARD)
    src_mreg = gmr_lookup(src, ARMCI_GROUP_WORLD.rank);
  else
    src_mreg = NULL;

  dst_mreg = gmr_lookup(dst, proc);

  ARMCII_Assert_msg(dst_mreg != NULL, "Invalid remote pointer");

  /* Prepare the input data: Apply scaling if needed and acquire the DLA lock if
   * needed.  We hold the DLA lock if (src_buf == src && src_mreg != NULL). */

  scaled = ARMCII_Buf_acc_is_scaled(datatype, scale);

  if (scaled) {
      MPI_Alloc_mem(bytes, MPI_INFO_NULL, &src_buf);
      ARMCII_Assert(src_buf != NULL);
      ARMCII_Buf_acc_scale(src, src_buf, bytes, datatype, scale);
  } else {
    src_buf = src;
  }

  /* Check if we need to copy: user requested it or same mem region */
  if (   (src_buf == src) /* buf_prepare didn't make a copy */
      && (ARMCII_GLOBAL_STATE.shr_buf_method == ARMCII_SHR_BUF_COPY || src_mreg == dst_mreg) )
  {
    MPI_Alloc_mem(bytes, MPI_INFO_NULL, &src_buf);
    ARMCII_Assert(src_buf != NULL);
    ARMCI_Copy(src, src_buf, bytes);
  }

  ARMCII_Acc_type_translate(datatype, &type, &type_size);
  count = bytes/type_size;

  ARMCII_Assert_msg(bytes % type_size == 0, 
      "Transfer size is not a multiple of the datatype size");

  /* TODO: Support a local accumulate operation more efficiently */

  gmr_accumulate(dst_mreg, src_buf, dst, count, type, proc, NULL /* handle */);
  gmr_flush(dst_mreg, proc, 1); /* flush_local */

  if (src_buf != src)
    MPI_Free_mem(src_buf);

  return 0;
}


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_Put_flag = PARMCI_Put_flag
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_Put_flag ARMCI_Put_flag
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_Put_flag as PARMCI_Put_flag
#endif
/* -- end weak symbols block -- */

/** One-sided copy of data from the source to the destination.  Set a flag on
  * the remote process when the transfer is complete.
  *
  * @param[in] src   Source buffer
  * @param[in] dst   Destination buffer on proc
  * @param[in] size  Number of bytes to transfer
  * @param[in] flag  Address of the flag buffer on proc
  * @param[in] value Value to set the flag to
  * @param[in] proc  Process id of the target
  * @return          0 on success, non-zero on failure
  */
int PARMCI_Put_flag(void *src, void* dst, int size, int *flag, int value, int proc) {
  /* TODO: This can be optimized with a more direct implementation, especially in the
   *       case where RMA is ordered; in that case, the Fence (Flush) is not necessary. */
  PARMCI_Put(src, dst, size, proc);
  PARMCI_Fence(proc);
  PARMCI_Put(&value, flag, sizeof(int), proc);

  return 0;
}
