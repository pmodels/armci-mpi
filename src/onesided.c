/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <mpi.h>

#include <armci.h>
#include <armcix.h>
#include <armci_internals.h>
#include <debug.h>
#include <mem_region.h>


/** Declare the start of a local access epoch.  This allows direct access to
  * data in local memory.
  *
  * @param[in] ptr Pointer to the allocation that will be accessed directly 
  */
void ARMCI_Access_begin(void *ptr) {
  mem_region_t *mreg;

  assert(ARMCII_GLOBAL_STATE.dla_state == ARMCII_DLA_CLOSED);

  mreg = mreg_lookup(ptr, ARMCI_GROUP_WORLD.rank);
  assert(mreg != NULL);

  assert((mreg->access_mode & ARMCIX_MODE_NO_LOAD_STORE) == 0);

  ARMCII_GLOBAL_STATE.dla_state = ARMCII_DLA_OPEN;
  ARMCII_GLOBAL_STATE.dla_mreg  = mreg;

  mreg_lock_ldst(mreg);
}


/** Declare the end of a local access epoch.
  *
  * \note MPI-2 does not allow multiple locks at once, so you can have only one
  * access epoch open at a time and cannot do put/get/acc while in an access
  * region.
  *
  * @param[in] ptr Pointer to the allocation that was accessed directly 
  */
void ARMCI_Access_end(void *ptr) {
  mem_region_t *mreg;

  assert(ARMCII_GLOBAL_STATE.dla_state == ARMCII_DLA_OPEN);

#ifndef NO_SEATBELTS
  /* Extra check to ensure the right mem region is unlocked */
  mreg = mreg_lookup(ptr, ARMCI_GROUP_WORLD.rank);
  assert(mreg != NULL);
  assert(mreg == ARMCII_GLOBAL_STATE.dla_mreg);
#endif

  mreg = ARMCII_GLOBAL_STATE.dla_mreg;
  mreg_unlock(mreg, ARMCI_GROUP_WORLD.rank);

  ARMCII_GLOBAL_STATE.dla_state = ARMCII_DLA_CLOSED;
  ARMCII_GLOBAL_STATE.dla_mreg  = NULL;
}


/** Set the acess mode for the given allocation.  Collective across the
  * allocation's group.  Waits for all processes, finishes all communication,
  * and then sets the new access mode.
  *
  * @param[in] new_mode The new access mode.
  * @param[in] ptr      Pointer within the allocation.
  * @return             Zero upon success, error code otherwise.
  */
int ARMCIX_Mode_set(int new_mode, void *ptr, ARMCI_Group *group) {
  mem_region_t *mreg;

  mreg = mreg_lookup(ptr, ARMCI_GROUP_WORLD.rank);
  assert(mreg != NULL);

  assert(group->comm == mreg->group.comm);

  // Wait for all processes to complete any outstanding communication before we
  // do the mode switch
  MPI_Barrier(mreg->group.comm);

  mreg->access_mode = new_mode;

  return 0;
}


/** Query the access mode for the given allocation.  Non-collective.
  *
  * @param[in] ptr      Pointer within the allocation.
  * @return             Current access mode.
  */
int ARMCIX_Mode_get(void *ptr) {
  mem_region_t *mreg;

  mreg = mreg_lookup(ptr, ARMCI_GROUP_WORLD.rank);
  assert(mreg != NULL);

  return mreg->access_mode;
}


/** One-sided get operation.
  *
  * @param[in] src    Source address (remote)
  * @param[in] dst    Destination address (local)
  * @param[in] size   Number of bytes to transfer
  * @param[in] target Process id to target
  * @return           0 on success, non-zero on failure
  */
int ARMCI_Get(void *src, void *dst, int size, int target) {
  mem_region_t *mreg;
  void **dst_buf;

  ARMCII_Buf_get_prepare(&dst, &dst_buf, 1, size);

  mreg = mreg_lookup(src, target);
  assert(mreg != NULL);

  mreg_lock(mreg, target);
  mreg_get(mreg, src, dst_buf[0], size, target);
  mreg_unlock(mreg, target);

  ARMCII_Buf_get_finish(&dst, dst_buf, 1, size);

  return 0;
}


/** One-sided put operation.
  *
  * @param[in] src    Source address (remote)
  * @param[in] dst    Destination address (local)
  * @param[in] size   Number of bytes to transfer
  * @param[in] target Process id to target
  * @return           0 on success, non-zero on failure
  */
int ARMCI_Put(void *src, void *dst, int size, int target) {
  mem_region_t *mreg;
  void **src_buf;

  ARMCII_Buf_put_prepare(&src, &src_buf, 1, size);

  mreg = mreg_lookup(dst, target);
  assert(mreg != NULL);

  mreg_lock(mreg, target);
  mreg_put(mreg, src_buf[0], dst, size, target);
  mreg_unlock(mreg, target);

  ARMCII_Buf_put_finish(&src, src_buf, 1, size);

  return 0;
}


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
int ARMCI_Acc(int datatype, void *scale, void *src, void *dst, int bytes, int proc) {
  void **src_buf;
  int    count, type_size;
  MPI_Datatype type;
  mem_region_t *mreg;

  ARMCII_Buf_acc_prepare(&src, &src_buf, 1, bytes, datatype, scale);

  mreg = mreg_lookup(dst, proc);
  assert(mreg != NULL);

  ARMCII_Acc_type_translate(datatype, &type, &type_size);
  count = bytes/type_size;

  assert(bytes % type_size == 0);

  mreg_lock(mreg, proc);
  mreg_accumulate(mreg, src_buf[0], dst, count, type, proc);
  mreg_unlock(mreg, proc);

  ARMCII_Buf_acc_finish(&src, src_buf, 1, bytes);

  return 0;
}

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
int ARMCI_Put_flag(void *src, void* dst, int size, int *flag, int value, int proc) {
  ARMCI_Put(src, dst, size, proc);
  ARMCI_Fence(proc);
  ARMCI_Put(&value, flag, sizeof(int), proc);

  return 0;
}
