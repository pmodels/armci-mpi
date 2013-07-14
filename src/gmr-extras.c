/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#include <armci.h>
#include <armcix.h>
#include <armci_internals.h>
#include <debug.h>
#include <gmr.h>

#if MPI_VERSION >= 3

/** One-sided get-accumulate operation.  Source and output buffer must be private.
  *
  * @param[in] mreg     Memory region
  * @param[in] src      Source address (local)
  * @param[in] out      Result address (local)
  * @param[in] dst      Destination address (remote)
  * @param[in] type     MPI type of the given buffers
  * @param[in] count    Number of elements of the given type to transfer
  * @param[in] op       MPI_Op to apply at the destination
  * @param[in] proc     Absolute process id of the target
  * @return             0 on success, non-zero on failure
  */
int gmr_get_accumulate(gmr_t *mreg, void *src, void *out, void *dst, int count, MPI_Datatype type, MPI_Op op, int proc) {
  ARMCII_Assert_msg(src != NULL && out != NULL, "Invalid local address(es)");
  return gmr_get_accumulate_typed(mreg, src, count, type, out, count, type, dst, count, type, op, proc);
}

/** One-sided get-accumulate operation with typed arguments.  Source and output buffer must be private.
  *
  * @param[in] mreg      Memory region
  * @param[in] src       Address of source data
  * @param[in] src_count Number of elements of the given type at the source
  * @param[in] src_type  MPI datatype of the source elements
  * @param[in] out       Address of output buffer (same process as the source)
  * @param[in] out_count Number of elements of the given type at the ouput
  * @param[in] out_type  MPI datatype of the output elements
  * @param[in] dst       Address of destination buffer
  * @param[in] dst_count Number of elements of the given type at the destination
  * @param[in] dst_type  MPI datatype of the destination elements
  * @param[in] size      Number of bytes to transfer
  * @param[in] op        MPI_Op to apply at the destination
  * @param[in] proc      Absolute process id of target process
  * @return              0 on success, non-zero on failure
  */
int gmr_get_accumulate_typed(gmr_t *mreg, void *src, int src_count, MPI_Datatype src_type,
    void *out, int out_count, MPI_Datatype out_type,
    void *dst, int dst_count, MPI_Datatype dst_type, MPI_Op op, int proc) {

  int        grp_proc;
  gmr_size_t disp;
  MPI_Aint lb, extent;

  grp_proc = ARMCII_Translate_absolute_to_group(&mreg->group, proc);
  ARMCII_Assert(grp_proc >= 0);

  // Calculate displacement from beginning of the window
  if (dst == MPI_BOTTOM) 
    disp = 0;
  else
    disp = (gmr_size_t) ((uint8_t*)dst - (uint8_t*)mreg->slices[proc].base);

  // Perform checks
  MPI_Type_get_true_extent(dst_type, &lb, &extent);
  ARMCII_Assert(mreg->lock_state != GMR_LOCK_UNLOCKED);
  ARMCII_Assert_msg(disp >= 0 && disp < mreg->slices[proc].size, "Invalid remote address");
  ARMCII_Assert_msg(disp + dst_count*extent <= mreg->slices[proc].size, "Transfer is out of range");

  MPI_Get_accumulate(src, src_count, src_type, out, out_count, out_type, grp_proc, (MPI_Aint) disp, dst_count, dst_type, op, mreg->window);

  return 0;
}

/** One-sided fetch-and-op.  Source and output buffer must be private.
  *
  * @param[in] mreg      Memory region
  * @param[in] src       Address of source data
  * @param[in] out       Address of output buffer (same process as the source)
  * @param[in] dst       Address of destination buffer
  * @param[in] type      MPI datatype of the source, output and destination elements
  * @param[in] op        MPI_Op to apply at the destination
  * @param[in] proc      Absolute process id of target process
  * @return              0 on success, non-zero on failure
  */
int gmr_fetch_and_op(gmr_t *mreg, void *src, void *out, void *dst,
		MPI_Datatype type, MPI_Op op, int proc) {

  int        grp_proc;
  gmr_size_t disp;

  grp_proc = ARMCII_Translate_absolute_to_group(&mreg->group, proc);
  ARMCII_Assert(grp_proc >= 0);

  /* built-in types only so no chance of seeing MPI_BOTTOM */
  disp = (gmr_size_t) ((uint8_t*)dst - (uint8_t*)mreg->slices[proc].base);

  // Perform checks
  ARMCII_Assert(mreg->lock_state != GMR_LOCK_UNLOCKED);
  ARMCII_Assert_msg(disp >= 0 && disp < mreg->slices[proc].size, "Invalid remote address");
  ARMCII_Assert_msg(disp <= mreg->slices[proc].size, "Transfer is out of range");

  MPI_Fetch_and_op(src, out, type, grp_proc, (MPI_Aint) disp, op, mreg->window);

  return 0;
}

/** Lock a memory region at all targets so that one-sided operations can be performed.
  *
  * @param[in] mreg     Memory region
  * @return             0 on success, non-zero on failure
  */
int gmr_lockall(gmr_t *mreg) {
  int grp_me   = ARMCII_Translate_absolute_to_group(&mreg->group, ARMCI_GROUP_WORLD.rank);
  int lock_assert = 0;

  ARMCII_Assert(grp_me >= 0);
  ARMCII_Assert(mreg->lock_state == GMR_LOCK_UNLOCKED);

  if (   ( mreg->access_mode & ARMCIX_MODE_CONFLICT_FREE )
      && ( mreg->access_mode & ARMCIX_MODE_NO_LOAD_STORE ) )
  {
    /* Only non-conflicting RMA accesses allowed. */
    lock_assert = MPI_MODE_NOCHECK;
  } else {
    lock_assert = 0;
  }

  MPI_Win_lock_all(lock_assert, mreg->window);

  mreg->lock_state  = GMR_LOCK_ALL;
  mreg->lock_target = -1;

  return 0;
}

/** Unlock a memory region at all targets.
  *
  * @param[in] mreg     Memory region
  * @return             0 on success, non-zero on failure
  */
int gmr_unlockall(gmr_t *mreg) {
  int grp_me   = ARMCII_Translate_absolute_to_group(&mreg->group, ARMCI_GROUP_WORLD.rank);

  ARMCII_Assert(grp_me >= 0);
  ARMCII_Assert(mreg->lock_state == GMR_LOCK_ALL);

  MPI_Win_unlock_all(mreg->window);

  mreg->lock_state  = GMR_LOCK_UNLOCKED;
  mreg->lock_target = -1;

  return 0;
}

/** Flush a memory region for local or remote completion.
  *
  * @param[in] mreg         Memory region
  * @param[in] proc         Absolute process id of the target
  * @param[in] local_only   Only flush the operation locally.
  * @return                 0 on success, non-zero on failure
  */
int gmr_flush(gmr_t *mreg, int proc, int local_only) {
  int grp_proc = ARMCII_Translate_absolute_to_group(&mreg->group, proc);
  int grp_me   = ARMCII_Translate_absolute_to_group(&mreg->group, ARMCI_GROUP_WORLD.rank);

  ARMCII_Assert(grp_proc >= 0 && grp_me >= 0);
  ARMCII_Assert(mreg->lock_state != GMR_LOCK_UNLOCKED);

  if (local_only)
    MPI_Win_flush_local(grp_proc, mreg->window);
  else
    MPI_Win_flush(grp_proc, mreg->window);

  return 0;
}

/** Flush a memory region for remote completion to all targets.
  *
  * @param[in] mreg         Memory region
  * @return                 0 on success, non-zero on failure
  */
int gmr_flushall(gmr_t *mreg, int local_only) {
  int grp_me   = ARMCII_Translate_absolute_to_group(&mreg->group, ARMCI_GROUP_WORLD.rank);

  ARMCII_Assert(grp_me >= 0);
  ARMCII_Assert(mreg->lock_state == GMR_LOCK_ALL);

  if (local_only)
    MPI_Win_flush_local_all(mreg->window);
  else
    MPI_Win_flush_all(mreg->window);

  return 0;
}

/** Sync memory region so that public and private windows are the same.
  *
  * @param[in] mreg         Memory region
  * @return                 0 on success, non-zero on failure
  */
int gmr_sync(gmr_t *mreg) {
  int grp_me   = ARMCII_Translate_absolute_to_group(&mreg->group, ARMCI_GROUP_WORLD.rank);

  ARMCII_Assert(grp_me >= 0);
  ARMCII_Assert(mreg->lock_state == GMR_LOCK_EXCLUSIVE || mreg->lock_state == GMR_LOCK_SHARED ||
                mreg->lock_state == GMR_LOCK_ALL);

  MPI_Win_sync(mreg->window);

  return 0;
}

/** One-sided put operation.  Source buffer must be private.
  *
  * @param[in] mreg   Memory region
  * @param[in] src    Source address (local)
  * @param[in] dst    Destination address (remote)
  * @param[in] size   Number of bytes to transfer
  * @param[in] proc   Absolute process id of target process
  * @return           0 on success, non-zero on failure
  */
int gmr_iput(gmr_t *mreg, void *src, void *dst, int size, int proc, MPI_Request *request) {
  ARMCII_Assert_msg(src != NULL, "Invalid local address");
  return gmr_iput_typed(mreg, src, size, MPI_BYTE, dst, size, MPI_BYTE, proc, request);
}


/** One-sided put operation with type arguments.  Source buffer must be private.
  *
  * @param[in] mreg      Memory region
  * @param[in] src       Address of source data
  * @param[in] src_count Number of elements of the given type at the source
  * @param[in] src_type  MPI datatype of the source elements
  * @param[in] dst       Address of destination buffer
  * @param[in] dst_count Number of elements of the given type at the destination
  * @param[in] src_type  MPI datatype of the destination elements
  * @param[in] size      Number of bytes to transfer
  * @param[in] proc      Absolute process id of target process
  * @return              0 on success, non-zero on failure
  */
int gmr_iput_typed(gmr_t *mreg, void *src, int src_count, MPI_Datatype src_type,
    void *dst, int dst_count, MPI_Datatype dst_type, int proc, MPI_Request *request) {

  int        grp_proc;
  gmr_size_t disp;
  MPI_Aint lb, extent;

  grp_proc = ARMCII_Translate_absolute_to_group(&mreg->group, proc);
  ARMCII_Assert(grp_proc >= 0);

  // Calculate displacement from beginning of the window
  if (dst == MPI_BOTTOM)
    disp = 0;
  else
    disp = (gmr_size_t) ((uint8_t*)dst - (uint8_t*)mreg->slices[proc].base);

  // Perform checks
  MPI_Type_get_true_extent(dst_type, &lb, &extent);
  ARMCII_Assert(mreg->lock_state != GMR_LOCK_UNLOCKED);
  ARMCII_Assert_msg(disp >= 0 && disp < mreg->slices[proc].size, "Invalid remote address");
  ARMCII_Assert_msg(disp + dst_count*extent <= mreg->slices[proc].size, "Transfer is out of range");

#ifdef RMA_PROPER_ATOMICITY
  MPI_Raccumulate(src, src_count, src_type, grp_proc, (MPI_Aint) disp, dst_count, dst_type, MPI_REPLACE, mreg->window, request);
#else
  MPI_Rput(src, src_count, src_type, grp_proc, (MPI_Aint) disp, dst_count, dst_type, mreg->window, request);
#endif

  return 0;
}


/** One-sided get operation.  Destination buffer must be private.
  *
  * @param[in] mreg   Memory region
  * @param[in] src    Source address (remote)
  * @param[in] dst    Destination address (local)
  * @param[in] size   Number of bytes to transfer
  * @param[in] proc   Absolute process id of target process
  * @return           0 on success, non-zero on failure
  */
int gmr_iget(gmr_t *mreg, void *src, void *dst, int size, int proc, MPI_Request *request) {
  ARMCII_Assert_msg(dst != NULL, "Invalid local address");
  return gmr_iget_typed(mreg, src, size, MPI_BYTE, dst, size, MPI_BYTE, proc, request);
}


/** One-sided get operation with type arguments.  Destination buffer must be private.
  *
  * @param[in] mreg      Memory region
  * @param[in] src       Address of source data
  * @param[in] src_count Number of elements of the given type at the source
  * @param[in] src_type  MPI datatype of the source elements
  * @param[in] dst       Address of destination buffer
  * @param[in] dst_count Number of elements of the given type at the destination
  * @param[in] src_type  MPI datatype of the destination elements
  * @param[in] size      Number of bytes to transfer
  * @param[in] proc      Absolute process id of target process
  * @return              0 on success, non-zero on failure
  */
int gmr_iget_typed(gmr_t *mreg, void *src, int src_count, MPI_Datatype src_type,
    void *dst, int dst_count, MPI_Datatype dst_type, int proc, MPI_Request *request) {

  int        grp_proc;
  gmr_size_t disp;
  MPI_Aint lb, extent;

  grp_proc = ARMCII_Translate_absolute_to_group(&mreg->group, proc);
  ARMCII_Assert(grp_proc >= 0);

  // Calculate displacement from beginning of the window
  if (src == MPI_BOTTOM)
    disp = 0;
  else
    disp = (gmr_size_t) ((uint8_t*)src - (uint8_t*)mreg->slices[proc].base);

  // Perform checks
  MPI_Type_get_true_extent(src_type, &lb, &extent);
  ARMCII_Assert(mreg->lock_state != GMR_LOCK_UNLOCKED);
  ARMCII_Assert_msg(disp >= 0 && disp < mreg->slices[proc].size, "Invalid remote address");
  ARMCII_Assert_msg(disp + src_count*extent <= mreg->slices[proc].size, "Transfer is out of range");

#if (MPI_VERSION >= 3) && defined(RMA_PROPER_ATOMICITY)
  /* I can only assume that the 3.1 release - MPICH_CALC_VERSION(3,1,0,3,0) - will fix this. */
#if (MPICH && (MPICH_NUM_VERSION < MPICH_CALC_VERSION(3,1,0,3,0)) )
#warning MPI_Get_accumulate will fail on an improper assertion for ARMCI_IOV_METHOD=DIRECT and/or ARMCI_STRIDED_METHOD=DIRECT \
        unless you apply the patch from http://trac.mpich.org/projects/mpich/ticket/1863
#endif
  MPI_Rget_accumulate(NULL, 0, MPI_BYTE, dst, dst_count, dst_type, grp_proc, (MPI_Aint) disp, src_count, src_type, MPI_NO_OP, mreg->window, request);
#else
  MPI_Rget(dst, dst_count, dst_type, grp_proc, (MPI_Aint) disp, src_count, src_type, mreg->window, request);
#endif

  return 0;
}


/** One-sided accumulate operation.  Source buffer must be private.
  *
  * @param[in] mreg     Memory region
  * @param[in] src      Source address (local)
  * @param[in] dst      Destination address (remote)
  * @param[in] type     MPI type of the given buffers
  * @param[in] count    Number of elements of the given type to transfer
  * @param[in] proc     Absolute process id of the target
  * @return             0 on success, non-zero on failure
  */
int gmr_iaccumulate(gmr_t *mreg, void *src, void *dst, int count, MPI_Datatype type, int proc, MPI_Request *request) {
  ARMCII_Assert_msg(src != NULL, "Invalid local address");
  return gmr_iaccumulate_typed(mreg, src, count, type, dst, count, type, proc, request);
}


/** One-sided accumulate operation with typed arguments.  Source buffer must be private.
  *
  * @param[in] mreg      Memory region
  * @param[in] src       Address of source data
  * @param[in] src_count Number of elements of the given type at the source
  * @param[in] src_type  MPI datatype of the source elements
  * @param[in] dst       Address of destination buffer
  * @param[in] dst_count Number of elements of the given type at the destination
  * @param[in] dst_type  MPI datatype of the destination elements
  * @param[in] size      Number of bytes to transfer
  * @param[in] proc      Absolute process id of target process
  * @return              0 on success, non-zero on failure
  */
int gmr_iaccumulate_typed(gmr_t *mreg, void *src, int src_count, MPI_Datatype src_type,
    void *dst, int dst_count, MPI_Datatype dst_type, int proc, MPI_Request *request) {

  int        grp_proc;
  gmr_size_t disp;
  MPI_Aint lb, extent;

  grp_proc = ARMCII_Translate_absolute_to_group(&mreg->group, proc);
  ARMCII_Assert(grp_proc >= 0);

  // Calculate displacement from beginning of the window
  if (dst == MPI_BOTTOM)
    disp = 0;
  else
    disp = (gmr_size_t) ((uint8_t*)dst - (uint8_t*)mreg->slices[proc].base);

  // Perform checks
  MPI_Type_get_true_extent(dst_type, &lb, &extent);
  ARMCII_Assert(mreg->lock_state != GMR_LOCK_UNLOCKED);
  ARMCII_Assert_msg(disp >= 0 && disp < mreg->slices[proc].size, "Invalid remote address");
  ARMCII_Assert_msg(disp + dst_count*extent <= mreg->slices[proc].size, "Transfer is out of range");

  MPI_Raccumulate(src, src_count, src_type, grp_proc, (MPI_Aint) disp, dst_count, dst_type, MPI_SUM, mreg->window, request);

  return 0;
}

#endif // MPI_VERSION >= 3
