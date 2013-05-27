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
  * @param[in] proc     Absolute process id of the target
  * @return             0 on success, non-zero on failure
  */
int gmr_get_accumulate(gmr_t *mreg, void *src, void *out, void *dst, int count, MPI_Datatype type, int proc) {
  ARMCII_Assert_msg(src != NULL && out != NULL, "Invalid local address(es)");
  return gmr_get_accumulate_typed(mreg, src, count, type, out, count, type, dst, count, type, proc);
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
  * @param[in] proc      Absolute process id of target process
  * @return              0 on success, non-zero on failure
  */
int gmr_get_accumulate_typed(gmr_t *mreg, void *src, int src_count, MPI_Datatype src_type,
    void *out, int out_count, MPI_Datatype out_type,
    void *dst, int dst_count, MPI_Datatype dst_type, int proc) {

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

  MPI_Get_accumulate(src, src_count, src_type, out, out_count, out_type, grp_proc, (MPI_Aint) disp, dst_count, dst_type, MPI_SUM, mreg->window);

  return 0;
}

