/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>

#include <armci.h>
#include <armci_internals.h>
#include <gmr.h>
#include <debug.h>


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_Rmw = PARMCI_Rmw
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_Rmw ARMCI_Rmw
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_Rmw as PARMCI_Rmw
#endif
/* -- end weak symbols block -- */

/** Perform atomic read-modify-write on the given integer or long location and
  * return the location's original value.
  *
  * \note ARMCI RMW operations are atomic with respect to other RMW operations,
  * but not with respect to other one-sided operations (get, put, acc, etc).
  *
  * @param[in]  op    Operation to be performed:
  *                     ARMCI_FETCH_AND_ADD (int)
  *                     ARMCI_FETCH_AND_ADD_LONG
  *                     ARMCI_SWAP (int)
  *                     ARMCI_SWAP_LONG
  * @param[out] ploc  Location to store the original value.
  * @param[in]  prem  Location on which to perform atomic operation.
  * @param[in]  value Value to add to remote location (ignored for swap).
  * @param[in]  proc  Process rank for the target buffer.
  */
int PARMCI_Rmw(int op, void *ploc, void *prem, int value, int proc) {

  int is_swap = 0, is_long = 0;
  MPI_Datatype type;
  MPI_Op       rop;
  gmr_t *src_mreg, *dst_mreg;

  /* If NOGUARD is set, assume the buffer is not shared */
  if (ARMCII_GLOBAL_STATE.shr_buf_method != ARMCII_SHR_BUF_NOGUARD)
    src_mreg = gmr_lookup(ploc, ARMCI_GROUP_WORLD.rank);
  else
    src_mreg = NULL;

  dst_mreg = gmr_lookup(prem, proc);

  ARMCII_Assert_msg(dst_mreg != NULL, "Invalid remote pointer");

  if (op == ARMCI_SWAP_LONG || op == ARMCI_FETCH_AND_ADD_LONG) {
    is_long = 1;
    type = MPI_LONG;
  }
  else
    type = MPI_INT;

  if (op == ARMCI_SWAP || op == ARMCI_SWAP_LONG) {
    is_swap = 1;
    rop = MPI_REPLACE;
  }
  else if (op == ARMCI_FETCH_AND_ADD || op == ARMCI_FETCH_AND_ADD_LONG)
    rop = MPI_SUM;
  else
    ARMCII_Error("invalid operation (%d)", op);

  /* We hold the DLA lock if (src_mreg != NULL). */

  if (is_swap) {
    long out_val_l, src_val_l = *((long*)ploc);
    int  out_val_i, src_val_i = *((int*)ploc);

    gmr_fetch_and_op(dst_mreg, 
                     is_long ? (void*) &src_val_l : (void*) &src_val_i /* src */,
                     is_long ? (void*) &out_val_l : (void*) &out_val_i /* out */,
    		     prem /* dst */, type, rop, proc);
    gmr_flush(dst_mreg, proc, 0); /* it's a round trip so w.r.t. flush, local=remote */
    if (is_long)
      *(long*) ploc = out_val_l;
    else
      *(int*) ploc = out_val_i;
  }
  else /* fetch-and-add */ {
    long fetch_val_l, add_val_l = value;
    int  fetch_val_i, add_val_i = value;

    gmr_fetch_and_op(dst_mreg,
                     is_long ? (void*) &add_val_l   : (void*) &add_val_i   /* src */,
                     is_long ? (void*) &fetch_val_l : (void*) &fetch_val_i /* out */,
                     prem /* dst */, type, rop, proc);
    gmr_flush(dst_mreg, proc, 0); /* it's a round trip so w.r.t. flush, local=remote */

    if (is_long)
      *(long*) ploc = fetch_val_l;
    else
      *(int*) ploc = fetch_val_i;
  }

  return 0;
}
