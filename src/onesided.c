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
  int           me, grp_rank;
  mem_region_t *mreg;
  MPI_Group     grp;

  MPI_Comm_rank(ARMCI_GROUP_WORLD.comm, &me);

  mreg = mem_region_lookup(ptr, me);
  assert(mreg != NULL);

  MPI_Win_get_group(mreg->window, &grp);
  MPI_Group_rank(grp, &grp_rank);
  MPI_Win_lock(MPI_LOCK_EXCLUSIVE, grp_rank, 0, mreg->window);
  MPI_Group_free(&grp);
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
  int           me, grp_rank;
  mem_region_t *mreg;
  MPI_Group     grp;

  MPI_Comm_rank(ARMCI_GROUP_WORLD.comm, &me);

  mreg = mem_region_lookup(ptr, me);
  assert(mreg != NULL);

  MPI_Win_get_group(mreg->window, &grp);
  MPI_Group_rank(grp, &grp_rank);
  MPI_Win_unlock(grp_rank, mreg->window);
  MPI_Group_free(&grp);
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

  mreg = mem_region_lookup(ptr, ARMCI_GROUP_WORLD.rank);
  assert(mreg != NULL); // TODO: Return failure or bail?

  assert(group->comm == mreg->comm);

  // Wait for all processes to complete any outstanding communication before we
  // do the mode switch
  MPI_Barrier(mreg->comm);

  if (new_mode != ARMCIX_MODE_ALL && new_mode != ARMCIX_MODE_RMA)
    ARMCII_Error(__FILE__, __LINE__, __func__, "Unknown access mode", 100);

  mreg->access_mode = new_mode;

  return 0;
}


/** Query the acess mode for the given allocation.  Non-collective.
  *
  * @param[in] ptr      Pointer within the allocation.
  * @return             Current access mode.
  */
int ARMCIX_Mode_get(void *ptr) {
  mem_region_t *mreg;

  mreg = mem_region_lookup(ptr, ARMCI_GROUP_WORLD.rank);
  assert(mreg != NULL); // TODO: Return failure or bail?

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
  mem_region_t *mreg, *mreg_dst;
  void *dst_buf;

  // Check if the destination buffer is within a shared region.  If so, we'll
  // need to work on a private buffer and put back the result later.
  mreg_dst = mem_region_lookup(dst, ARMCI_GROUP_WORLD.rank);

  if (mreg_dst != NULL) {
    int ierr = MPI_Alloc_mem(size, MPI_INFO_NULL, &dst_buf);
    assert(ierr == MPI_SUCCESS);
  } else {
    dst_buf = dst;
  }

  mreg = mem_region_lookup(src, target);
  assert(mreg != NULL);

  mreg_get(mreg, src, dst_buf, size, target);

  if (mreg_dst != NULL) {
    mreg_put(mreg_dst, dst_buf, dst, size, ARMCI_GROUP_WORLD.rank);
    MPI_Free_mem(dst_buf);
  }

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
  mem_region_t *mreg, *mreg_src;
  void *src_buf;

  // Check if the source buffer is within a shared region.  If so, we'll
  // need to get the data into a private buffer before we can put it.
  mreg_src = mem_region_lookup(src, ARMCI_GROUP_WORLD.rank);

  if (mreg_src != NULL) {
    int ierr = MPI_Alloc_mem(size, MPI_INFO_NULL, &src_buf);
    assert(ierr == MPI_SUCCESS);
    mreg_get(mreg_src, src, src_buf, size, ARMCI_GROUP_WORLD.rank);
  } else {
    src_buf = src;
  }

  mreg = mem_region_lookup(dst, target);
  assert(mreg != NULL);

  mreg_put(mreg, src, dst, size, target);

  if (mreg_src != NULL) {
    MPI_Free_mem(src_buf);
  }

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
  void *scaled_data = NULL;
  void *src_data, *src_buf;
  int   count, type_size, i;
  MPI_Datatype type;
  mem_region_t *mreg, *mreg_src;

  // Check if the source buffer is within a shared region.  If so, we'll
  // need to get the data into a private buffer before we can put it.
  mreg_src = mem_region_lookup(src, ARMCI_GROUP_WORLD.rank);

  if (mreg_src != NULL) {
    int ierr = MPI_Alloc_mem(bytes, MPI_INFO_NULL, &src_buf);
    assert(ierr == MPI_SUCCESS);
    mreg_get(mreg_src, src, src_buf, bytes, ARMCI_GROUP_WORLD.rank);
  } else {
    src_buf = src;
  }

  switch (datatype) {
    case ARMCI_ACC_INT:
      MPI_Type_size(MPI_INT, &type_size);
      type = MPI_INT;
      count= bytes/type_size;

      if (*((int*)scale) == 1)
        break;
      else {
        int *src_i = (int*) src_buf;
        int *scl_i = malloc(bytes);
        const int s = *((int*) scale);
        scaled_data = scl_i;
        for (i = 0; i < count; i++)
          scl_i[i] = src_i[i]*s;
      }
      break;

    case ARMCI_ACC_LNG:
      MPI_Type_size(MPI_LONG, &type_size);
      type = MPI_LONG;
      count= bytes/type_size;

      if (*((long*)scale) == 1)
        break;
      else {
        long *src_l = (long*) src_buf;
        long *scl_l = malloc(bytes);
        const long s = *((long*) scale);
        scaled_data = scl_l;
        for (i = 0; i < count; i++)
          scl_l[i] = src_l[i]*s;
      }
      break;

    case ARMCI_ACC_FLT:
      MPI_Type_size(MPI_FLOAT, &type_size);
      type = MPI_FLOAT;
      count= bytes/type_size;

      if (*((float*)scale) == 1.0)
        break;
      else {
        float *src_f = (float*) src_buf;
        float *scl_f = malloc(bytes);
        const float s = *((float*) scale);
        scaled_data = scl_f;
        for (i = 0; i < count; i++)
          scl_f[i] = src_f[i]*s;
      }
      break;

    case ARMCI_ACC_DBL:
      MPI_Type_size(MPI_DOUBLE, &type_size);
      type = MPI_DOUBLE;
      count= bytes/type_size;

      if (*((double*)scale) == 1.0)
        break;
      else {
        double *src_d = (double*) src_buf;
        double *scl_d = malloc(bytes);
        const double s = *((double*) scale);
        scaled_data = scl_d;
        for (i = 0; i < count; i++)
          scl_d[i] = src_d[i]*s;
      }
      break;

    case ARMCI_ACC_CPL:
      MPI_Type_size(MPI_FLOAT, &type_size);
      type = MPI_FLOAT;
      count= bytes/type_size;

      if (((float*)scale)[0] == 1.0 && ((float*)scale)[1] == 0.0)
        break;
      else {
        float *src_fc = (float*) src_buf;
        float *scl_fc = malloc(bytes);
        const float s_r = ((float*)scale)[0];
        const float s_c = ((float*)scale)[1];
        scaled_data = scl_fc;
        for (i = 0; i < count; i += 2) {
          // Complex multiplication: (a + bi)*(c + di)
          scl_fc[i]   = src_fc[i]*s_r   - src_fc[i+1]*s_c;
          scl_fc[i+1] = src_fc[i+1]*s_r + src_fc[i]*s_c;
        }
      }
      break;

    case ARMCI_ACC_DCP:
      MPI_Type_size(MPI_DOUBLE, &type_size);
      type = MPI_DOUBLE;
      count= bytes/type_size;

      if (((double*)scale)[0] == 1.0 && ((double*)scale)[1] == 0.0)
        break;
      else {
        double *src_dc = (double*) src_buf;
        double *scl_dc = malloc(bytes);
        const double s_r = ((double*)scale)[0];
        const double s_c = ((double*)scale)[1];
        scaled_data = scl_dc;
        for (i = 0; i < count; i += 2) {
          // Complex multiplication: (a + bi)*(c + di)
          scl_dc[i]   = src_dc[i]*s_r   - src_dc[i+1]*s_c;
          scl_dc[i+1] = src_dc[i+1]*s_r + src_dc[i]*s_c;
        }
      }
      break;

    default:
      ARMCII_Error(__FILE__, __LINE__, __func__, "unknown data type", 100);
      return 1;
  }

  assert(bytes % type_size == 0);

  if (scaled_data)
    src_data = scaled_data;
  else
    src_data = src_buf;

  mreg = mem_region_lookup(dst, proc);
  assert(mreg != NULL);

  mreg_accumulate(mreg, src_data, dst, type, count, proc);

  if (scaled_data != NULL) 
    free(scaled_data);

  if (mreg_src != NULL)
    MPI_Free_mem(src_buf);

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
  ARMCI_Put(&value, flag, sizeof(int), proc);

  return 0;
}
