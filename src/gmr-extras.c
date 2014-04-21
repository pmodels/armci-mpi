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
  ARMCII_Assert_msg(mreg->window != MPI_WIN_NULL, "A non-null mreg contains a null window.");

  // Calculate displacement from beginning of the window
  if (dst == MPI_BOTTOM) 
    disp = 0;
  else
    disp = (gmr_size_t) ((uint8_t*)dst - (uint8_t*)mreg->slices[proc].base);

  // Perform checks
  MPI_Type_get_true_extent(dst_type, &lb, &extent);
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
  ARMCII_Assert_msg(mreg->window != MPI_WIN_NULL, "A non-null mreg contains a null window.");

  /* built-in types only so no chance of seeing MPI_BOTTOM */
  disp = (gmr_size_t) ((uint8_t*)dst - (uint8_t*)mreg->slices[proc].base);

  // Perform checks
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

  ARMCII_Assert(grp_me >= 0);
  ARMCII_Assert_msg(mreg->window != MPI_WIN_NULL, "A non-null mreg contains a null window.");

  MPI_Win_lock_all(MPI_MODE_NOCHECK, mreg->window);

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
  ARMCII_Assert_msg(mreg->window != MPI_WIN_NULL, "A non-null mreg contains a null window.");

  MPI_Win_unlock_all(mreg->window);

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
  ARMCII_Assert_msg(mreg->window != MPI_WIN_NULL, "A non-null mreg contains a null window.");

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
  ARMCII_Assert_msg(mreg->window != MPI_WIN_NULL, "A non-null mreg contains a null window.");

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
  ARMCII_Assert_msg(mreg->window != MPI_WIN_NULL, "A non-null mreg contains a null window.");

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
int gmr_iput(gmr_t *mreg, void *src, void *dst, int size, int proc) {
  ARMCII_Assert_msg(src != NULL, "Invalid local address");
  return gmr_iput_typed(mreg, src, size, MPI_BYTE, dst, size, MPI_BYTE, proc);
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
    void *dst, int dst_count, MPI_Datatype dst_type, int proc) {

  int        grp_proc;
  gmr_size_t disp;
  MPI_Aint lb, extent;

  grp_proc = ARMCII_Translate_absolute_to_group(&mreg->group, proc);
  ARMCII_Assert(grp_proc >= 0);
  ARMCII_Assert_msg(mreg->window != MPI_WIN_NULL, "A non-null mreg contains a null window.");

  // Calculate displacement from beginning of the window
  if (dst == MPI_BOTTOM)
    disp = 0;
  else
    disp = (gmr_size_t) ((uint8_t*)dst - (uint8_t*)mreg->slices[proc].base);

  // Perform checks
  MPI_Type_get_true_extent(dst_type, &lb, &extent);
  ARMCII_Assert_msg(disp >= 0 && disp < mreg->slices[proc].size, "Invalid remote address");
  ARMCII_Assert_msg(disp + dst_count*extent <= mreg->slices[proc].size, "Transfer is out of range");

#ifdef RMA_PROPER_ATOMICITY
  MPI_Accumulate(src, src_count, src_type, grp_proc, (MPI_Aint) disp, dst_count, dst_type, MPI_REPLACE, mreg->window);
#else
  MPI_Put(src, src_count, src_type, grp_proc, (MPI_Aint) disp, dst_count, dst_type, mreg->window);
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
int gmr_iget(gmr_t *mreg, void *src, void *dst, int size, int proc) {
  ARMCII_Assert_msg(dst != NULL, "Invalid local address");
  return gmr_iget_typed(mreg, src, size, MPI_BYTE, dst, size, MPI_BYTE, proc);
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
    void *dst, int dst_count, MPI_Datatype dst_type, int proc) {

  int        grp_proc;
  gmr_size_t disp;
  MPI_Aint lb, extent;

  grp_proc = ARMCII_Translate_absolute_to_group(&mreg->group, proc);
  ARMCII_Assert(grp_proc >= 0);
  ARMCII_Assert_msg(mreg->window != MPI_WIN_NULL, "A non-null mreg contains a null window.");

  // Calculate displacement from beginning of the window
  if (src == MPI_BOTTOM)
    disp = 0;
  else
    disp = (gmr_size_t) ((uint8_t*)src - (uint8_t*)mreg->slices[proc].base);

  // Perform checks
  MPI_Type_get_true_extent(src_type, &lb, &extent);
  ARMCII_Assert_msg(disp >= 0 && disp < mreg->slices[proc].size, "Invalid remote address");
  ARMCII_Assert_msg(disp + src_count*extent <= mreg->slices[proc].size, "Transfer is out of range");

#ifdef RMA_PROPER_ATOMICITY
  MPI_Get_accumulate(NULL, 0, MPI_DATATYPE_NULL, dst, dst_count, dst_type, grp_proc, (MPI_Aint) disp, src_count, src_type, MPI_NO_OP, mreg->window);
#else
  MPI_Get(dst, dst_count, dst_type, grp_proc, (MPI_Aint) disp, src_count, src_type, mreg->window);
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
int gmr_iaccumulate(gmr_t *mreg, void *src, void *dst, int count, MPI_Datatype type, int proc) {
  ARMCII_Assert_msg(src != NULL, "Invalid local address");
  return gmr_iaccumulate_typed(mreg, src, count, type, dst, count, type, proc);
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
    void *dst, int dst_count, MPI_Datatype dst_type, int proc) {

  int        grp_proc;
  gmr_size_t disp;
  MPI_Aint lb, extent;

  grp_proc = ARMCII_Translate_absolute_to_group(&mreg->group, proc);
  ARMCII_Assert(grp_proc >= 0);
  ARMCII_Assert_msg(mreg->window != MPI_WIN_NULL, "A non-null mreg contains a null window.");

  // Calculate displacement from beginning of the window
  if (dst == MPI_BOTTOM)
    disp = 0;
  else
    disp = (gmr_size_t) ((uint8_t*)dst - (uint8_t*)mreg->slices[proc].base);

  // Perform checks
  MPI_Type_get_true_extent(dst_type, &lb, &extent);
  ARMCII_Assert_msg(disp >= 0 && disp < mreg->slices[proc].size, "Invalid remote address");
  ARMCII_Assert_msg(disp + dst_count*extent <= mreg->slices[proc].size, "Transfer is out of range");

  MPI_Accumulate(src, src_count, src_type, grp_proc, (MPI_Aint) disp, dst_count, dst_type, MPI_SUM, mreg->window);

  return 0;
}
