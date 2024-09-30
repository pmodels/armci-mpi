/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>

#include <armci.h>
#include <debug.h>
#include <gmr.h>

/** Initialize Non-blocking handle.
  */
void ARMCI_INIT_HANDLE(armci_hdl_t *handle) {
  if (handle!=NULL) {
    handle->aggregate =  0;
    handle->target    = -1;
  } else {
    ARMCII_Warning("ARMCI_INIT_HANDLE given NULL handle");
  }
  return;
}


/** Mark a handle as aggregate.
  */
void ARMCI_SET_AGGREGATE_HANDLE(armci_hdl_t *handle) {
  if (handle!=NULL) {
    handle->aggregate =  1;
  } else {
    ARMCII_Warning("ARMCI_SET_AGGREGATE_HANDLE given NULL handle");
  }
  return;
}


/** Clear an aggregate handle.
  */
void ARMCI_UNSET_AGGREGATE_HANDLE(armci_hdl_t *handle) {
  if (handle!=NULL) {
    handle->aggregate =  0;
  } else {
    ARMCII_Warning("ARMCI_UNSET_AGGREGATE_HANDLE given NULL handle");
  }
  return;
}


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_NbPut = PARMCI_NbPut
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_NbPut ARMCI_NbPut
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_NbPut as PARMCI_NbPut
#endif
/* -- end weak symbols block -- */

/** Non-blocking put operation.
  */
int PARMCI_NbPut(void *src, void *dst, int size, int target, armci_hdl_t *handle) {
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
  else {
      gmr_put(dst_mreg, src, dst, size, target);
  }

  if (handle!=NULL) {
      /* Regular (not aggregate) handles merely store the target for future flushing. */
      handle->target = target;
  }

  gmr_progress();

  return 0;
}


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_NbGet = PARMCI_NbGet
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_NbGet ARMCI_NbGet
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_NbGet as PARMCI_NbGet
#endif
/* -- end weak symbols block -- */

/** Non-blocking get operation.  Note: the implementation is not non-blocking
  */
int PARMCI_NbGet(void *src, void *dst, int size, int target, armci_hdl_t *handle) {
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
  else {
    gmr_get(src_mreg, src, dst, size, target);
  }

  if (handle!=NULL) {
      /* Regular (not aggregate) handles merely store the target for future flushing. */
      handle->target = target;
  }

  gmr_progress();

  return 0;
}


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_NbAcc = PARMCI_NbAcc
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_NbAcc ARMCI_NbAcc
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_NbAcc as PARMCI_NbAcc
#endif
/* -- end weak symbols block -- */

/** Non-blocking accumulate operation.  Note: the implementation is not non-blocking
  */
int PARMCI_NbAcc(int datatype, void *scale, void *src, void *dst, int bytes, int target, armci_hdl_t *handle) {
  void  *src_buf;
  int    count, type_size, scaled;
  MPI_Datatype type;
  gmr_t *src_mreg, *dst_mreg;

  /* If NOGUARD is set, assume the buffer is not shared */
  if (ARMCII_GLOBAL_STATE.shr_buf_method != ARMCII_SHR_BUF_NOGUARD)
    src_mreg = gmr_lookup(src, ARMCI_GROUP_WORLD.rank);
  else
    src_mreg = NULL;

  dst_mreg = gmr_lookup(dst, target);

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

  gmr_accumulate(dst_mreg, src_buf, dst, count, type, target);

  if (src_buf != src) {
    /* must wait for local completion to free source buffer */
    gmr_flush(dst_mreg, target, 1); /* flush local only, unlike Fence */
    MPI_Free_mem(src_buf);
  }

  if (handle!=NULL) {
      /* Regular (not aggregate) handles merely store the target for future flushing. */
      handle->target = target;
  }

  gmr_progress();

  return 0;
}


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_Wait = PARMCI_Wait
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_Wait ARMCI_Wait
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_Wait as PARMCI_Wait
#endif
/* -- end weak symbols block -- */

/** Wait for a non-blocking operation to finish.
  */
int PARMCI_Wait(armci_hdl_t* handle) {
  gmr_t *cur_mreg = gmr_list;

  if(handle->aggregate > 0) {
    while (cur_mreg) {
      gmr_flushall(cur_mreg, 1); /* local only */
      cur_mreg = cur_mreg->next;
    }
  }
  else {
    int proc = handle->target;
    while (cur_mreg) {
      gmr_flush(cur_mreg, proc, 1); /* local only */
      cur_mreg = cur_mreg->next;
    }
  }
  return 0;
}


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_Test = PARMCI_Test
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_Test ARMCI_Test
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_Test as PARMCI_Test
#endif
/* -- end weak symbols block -- */

/** Check if a non-blocking operation has finished.
  */
int PARMCI_Test(armci_hdl_t* handle) {
  return PARMCI_Wait(handle);
}


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_WaitProc = PARMCI_WaitProc
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_WaitProc ARMCI_WaitProc
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_WaitProc as PARMCI_WaitProc
#endif
/* -- end weak symbols block -- */

/** Wait for all outstanding non-blocking operations with implicit handles to a particular process to finish.
  */
int PARMCI_WaitProc(int proc) {
  gmr_t *cur_mreg = gmr_list;

  while (cur_mreg) {
    gmr_flush(cur_mreg, proc, 1); /* local only */
    cur_mreg = cur_mreg->next;
  }
  return 0;
}


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_WaitAll = PARMCI_WaitAll
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_WaitAll ARMCI_WaitAll
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_WaitAll as PARMCI_WaitAll
#endif
/* -- end weak symbols block -- */

/** Wait for all non-blocking operations with implicit (NULL) handles to finish.
  */
int PARMCI_WaitAll(void) {
  gmr_t *cur_mreg = gmr_list;

  while (cur_mreg) {
    gmr_flushall(cur_mreg, 1); /* local only */
    cur_mreg = cur_mreg->next;
  }
  return 0;
}


