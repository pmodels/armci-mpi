#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "debug.h"

#include "armci.h"
#include "seg_hdl.h"


/** Declare the start of a local access epoch.  This allows direct access to
  * data in local memory.  (TODO: Does MPI-2 actually allow this?)
  *
  * @param[in] ptr Pointer to the allocation that will be accessed directly 
  */
void ARMCI_Access_start(void *ptr) {
  int        rank;
  MPI_Group  grp;
  MPI_Win   *access_win;

  access_win = seg_hdl_win_lookup(ptr);
  assert(access_win != NULL);

  MPI_Win_get_group(*access_win, &grp);
  MPI_Group_rank(grp, &rank);
  MPI_Win_lock(MPI_LOCK_EXCLUSIVE, rank, 0, *access_win);
  MPI_Group_free(&grp);
}


/** Declare the end of a local access epoch.
  *
  * TODO: Should we allow multiple accesses at once?
  *
  * @param[in] ptr Pointer to the allocation that was accessed directly 
  */
void ARMCI_Access_end(void *ptr) {
  int        rank;
  MPI_Group  grp;
  MPI_Win   *access_win;

  access_win = seg_hdl_win_lookup(ptr);
  assert(access_win != NULL);

  MPI_Win_get_group(*access_win, &grp);
  MPI_Group_rank(grp, &rank);
  MPI_Win_unlock(rank, *access_win);
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
  int disp;
  MPI_Win *win = seg_hdl_win_lookup(src);

  // Calculate displacement from beginning of the window
  disp = (int) (src - seg_hdl_base_lookup(src));

  MPI_Win_lock(MPI_LOCK_EXCLUSIVE, target, 0, *win);
  MPI_Get(dst, size, MPI_BYTE, target, disp, size, MPI_BYTE, *win);
  MPI_Win_unlock(target, *win);

  return 0;
}


/** One-sided get operation.
  *
  * @param[in] src    Source address (remote)
  * @param[in] dst    Destination address (local)
  * @param[in] size   Number of bytes to transfer
  * @param[in] target Process id to target
  * @return           0 on success, non-zero on failure
  */
int ARMCI_Put(void *src, void *dst, int size, int target) {
  int disp;
  MPI_Win *win = seg_hdl_win_lookup(dst);

  // Calculate displacement from beginning of the window
  disp = (int) (dst - seg_hdl_base_lookup(dst));

  MPI_Win_lock(MPI_LOCK_EXCLUSIVE, target, 0, *win);
  MPI_Put(src, size, MPI_BYTE, target, disp, size, MPI_BYTE, *win);
  MPI_Win_unlock(target, *win);

  return 0;
}


/** One-sided get operation.
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
int ARMCI_Acc(int datatype, void *scale, void* src, void* dst, int bytes, int proc) {
  void *scaled_data = NULL;
  void *src_data, *base;
  int   count, type_size, i;
  MPI_Datatype type;
  MPI_Win *win;

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
    default:
      ARMCI_Error("ARMCI_Acc() unsupported operation", 100);
  }

  if (scaled_data)
    src_data = scaled_data;
  else
    src_data = src;

  // Calculate displacement from window's base address
  base = seg_hdl_base_lookup(dst);
  win  = seg_hdl_win_lookup(dst);

  assert(bytes % type_size == 0);
  assert(dst-base >= 0);

  MPI_Win_lock(MPI_LOCK_EXCLUSIVE, proc, 0, *win);
  MPI_Accumulate(src_data, count, type, proc, dst-base, count, type, MPI_SUM, *win);
  MPI_Win_unlock(proc, *win);

  if (scaled_data != NULL) 
    free(scaled_data);

  return 0;
}
