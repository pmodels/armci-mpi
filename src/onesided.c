/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <mpi.h>

#include <armci.h>
#include <armci_internals.h>
#include <debug.h>
#include <mem_region.h>


/** Declare the start of a local access epoch.  This allows direct access to
  * data in local memory.
  *
  * @param[in] ptr Pointer to the allocation that will be accessed directly 
  */
void ARMCI_Access_start(void *ptr) {
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


/** One-sided get operation.
  *
  * @param[in] src    Source address (remote)
  * @param[in] dst    Destination address (local)
  * @param[in] size   Number of bytes to transfer
  * @param[in] target Process id to target
  * @return           0 on success, non-zero on failure
  */
int ARMCI_Get(void *src, void *dst, int size, int target) {
  int disp, grp_target;
  mem_region_t *mreg;

  mreg = mem_region_lookup(src, target);
  assert(mreg != NULL);

  grp_target = ARMCII_Translate_absolute_to_group(mreg->comm, target);
  assert(grp_target >= 0);

  // Calculate displacement from beginning of the window
  disp = (int) ((uint8_t*)src - (uint8_t*)mreg->slices[target].base);

  assert(disp >= 0 && disp < mreg->slices[target].size);
  assert(src >= mreg->slices[target].base);
  assert((uint8_t*)src + size <= (uint8_t*)mreg->slices[target].base + mreg->slices[target].size);

  MPI_Win_lock(MPI_LOCK_EXCLUSIVE, grp_target, 0, mreg->window);
  MPI_Get(dst, size, MPI_BYTE, grp_target, disp, size, MPI_BYTE, mreg->window);
  MPI_Win_unlock(grp_target, mreg->window);

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
  int disp, grp_target;
  mem_region_t *mreg;

  mreg = mem_region_lookup(dst, target);
  assert(mreg != NULL);

  grp_target = ARMCII_Translate_absolute_to_group(mreg->comm, target);
  assert(grp_target >= 0);

  // Calculate displacement from beginning of the window
  disp = (int) ((uint8_t*)dst - (uint8_t*)mreg->slices[target].base);

  assert(disp >= 0 && disp < mreg->slices[target].size);
  assert(dst >= mreg->slices[target].base);
  assert((uint8_t*)dst + size <= (uint8_t*)mreg->slices[target].base + mreg->slices[target].size);

  MPI_Win_lock(MPI_LOCK_EXCLUSIVE, grp_target, 0, mreg->window);
  MPI_Put(src, size, MPI_BYTE, grp_target, disp, size, MPI_BYTE, mreg->window);
  MPI_Win_unlock(grp_target, mreg->window);

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
  void *src_data;
  int   count, type_size, i, disp, grp_proc;
  MPI_Datatype type;
  mem_region_t *mreg;

  switch (datatype) {
    case ARMCI_ACC_INT:
      MPI_Type_size(MPI_INT, &type_size);
      type = MPI_INT;
      count= bytes/type_size;

      if (*((int*)scale) == 1)
        break;
      else {
        int *src_i = (int*) src;
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
        long *src_l = (long*) src;
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
        float *src_f = (float*) src;
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
        double *src_d = (double*) src;
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
        float *src_fc = (float*) src;
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
        double *src_dc = (double*) src;
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
    src_data = src;

  // Calculate displacement from window's base address
  mreg = mem_region_lookup(dst, proc);
  assert(mreg != NULL);

  grp_proc = ARMCII_Translate_absolute_to_group(mreg->comm, proc);
  assert(grp_proc >= 0);

  disp = (int) ((uint8_t*)dst - (uint8_t*)(mreg->slices[proc].base));

  assert(disp >= 0 && disp < mreg->slices[proc].size);
  assert(dst >= mreg->slices[proc].base);
  assert((uint8_t*)dst + bytes <= (uint8_t*)mreg->slices[proc].base + mreg->slices[proc].size);

  MPI_Win_lock(MPI_LOCK_EXCLUSIVE, grp_proc, 0, mreg->window);
  MPI_Accumulate(src_data, count, type, grp_proc, disp, count, type, MPI_SUM, mreg->window);
  MPI_Win_unlock(grp_proc, mreg->window);

  if (scaled_data != NULL) 
    free(scaled_data);

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
