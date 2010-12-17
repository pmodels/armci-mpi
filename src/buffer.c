/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>

#include <armci.h>
#include <armci_internals.h>
#include <mem_region.h>
#include <debug.h>


/** Prepare a set of buffers for use with a put operation.  The returned set of
  * buffers is guaranteed to be in private space.  Copies will be made if needed,
  * the result should be completed by finish.
  *
  * @param[in]  orig_bufs Original set of buffers.
  * @param[out] new_bufs  Pointer to the set of private buffers.
  * @param[in]  count     Number of entries in the buffer list.
  * @param[in]  size      The size of the buffers (all are of the same size).
  * @return               Number of buffers that were moved.
  */
int ARMCII_Buf_put_prepare(void **orig_bufs, void ***new_bufs_ptr, int count, int size) {
  int num_moved = 0;

#ifndef NO_CHECK_BUFFERS
  void **new_bufs = malloc(count*sizeof(void*));
  int i;

  for (i = 0; i < count; i++)
    new_bufs[i] = NULL;

  for (i = 0; i < count; i++) {
    // Check if the source buffer is within a shared region.  If so, copy it
    // into a private buffer.
    mem_region_t *mreg = mreg_lookup(orig_bufs[i], ARMCI_GROUP_WORLD.rank);

    if (mreg != NULL) {
      int ierr = MPI_Alloc_mem(size, MPI_INFO_NULL, &new_bufs[i]);
      assert(ierr == MPI_SUCCESS);

      mreg_lock(mreg, ARMCI_GROUP_WORLD.rank);
      mreg_get(mreg, orig_bufs[i], new_bufs[i], size, ARMCI_GROUP_WORLD.rank);
      mreg_unlock(mreg, ARMCI_GROUP_WORLD.rank);

      num_moved++;
    } else {
      new_bufs[i] = orig_bufs[i];
    }
  }

  *new_bufs_ptr = new_bufs;

#else
  *new_bufs_ptr = orig_bufs;
#endif /* NO_CHECK_BUFFERS */
  
  return num_moved;
}


/** Finish a set of prepared buffers.  Will perform communication and copies as
  * needed to ensure results are in the original buffers.  Temporary space will be
  * freed.
  *
  * @param[in]  orig_bufs Original set of buffers.
  * @param[out] new_bufs  Set of private buffers.
  * @param[in]  count     Number of entries in the buffer list.
  * @param[in]  size      The size of the buffers (all are of the same size).
  */
void ARMCII_Buf_put_finish(void **orig_bufs, void **new_bufs, int count, int size) {
#ifndef NO_CHECK_BUFFERS
  int i;

  for (i = 0; i < count; i++) {
    if (orig_bufs[i] != new_bufs[i]) {
      MPI_Free_mem(new_bufs[i]);
    }
  }

  free(new_bufs);
#endif /* NO_CHECK_BUFFERS */
}


/** Prepare a set of buffers for use with an accumulate operation.  The
  * returned set of buffers is guaranteed to be in private space and scaled.
  * Copies will be made if needed, the result should be completed by finish.
  *
  * @param[in]  orig_bufs Original set of buffers.
  * @param[out] new_bufs  Pointer to the set of private buffers.
  * @param[in]  count     Number of entries in the buffer list.
  * @param[in]  size      The size of the buffers (all are of the same size).
  * @param[in]  datatype  The type of the buffer.
  * @param[in]  scale     Scaling constant to apply to each buffer.
  * @return               Number of buffers that were moved.
  */
int ARMCII_Buf_acc_prepare(void **orig_bufs, void ***new_bufs_ptr, int count, int size,
                            int datatype, void *scale) {

  void **new_bufs = malloc(count*sizeof(void*));
  int i, j, nelem, num_moved = 0;

  for (i = 0; i < count; i++)
    new_bufs[i] = NULL;

  for (i = 0; i < count; i++) {
    void *scaled_data = NULL;
    int   type_size;
    MPI_Datatype type;
    mem_region_t *mreg = NULL;

    // Check if the source buffer is within a shared region.  If so, copy it
    // into a private buffer.
    mreg = mreg_lookup(orig_bufs[i], ARMCI_GROUP_WORLD.rank);

    // Lock if needed so we can directly access the buffer
    if (mreg != NULL)
      mreg_lock(mreg, ARMCI_GROUP_WORLD.rank);

    switch (datatype) {
      case ARMCI_ACC_INT:
        MPI_Type_size(MPI_INT, &type_size);
        type = MPI_INT;
        nelem= size/type_size;

        if (*((int*)scale) == 1)
          break;
        else {
          int *src_i = (int*) orig_bufs[i];
          int *scl_i;
          const int s = *((int*) scale);
          int ierr = MPI_Alloc_mem(size, MPI_INFO_NULL, &scl_i);
          assert(ierr == MPI_SUCCESS);
          scaled_data = scl_i;
          for (j = 0; j < nelem; j++)
            scl_i[j] = src_i[j]*s;
        }
        break;

      case ARMCI_ACC_LNG:
        MPI_Type_size(MPI_LONG, &type_size);
        type = MPI_LONG;
        nelem= size/type_size;

        if (*((long*)scale) == 1)
          break;
        else {
          long *src_l = (long*) orig_bufs[i];
          long *scl_l;
          const long s = *((long*) scale);
          int ierr = MPI_Alloc_mem(size, MPI_INFO_NULL, &scl_l);
          assert(ierr == MPI_SUCCESS);
          scaled_data = scl_l;
          for (j = 0; j < nelem; j++)
            scl_l[j] = src_l[j]*s;
        }
        break;

      case ARMCI_ACC_FLT:
        MPI_Type_size(MPI_FLOAT, &type_size);
        type = MPI_FLOAT;
        nelem= size/type_size;

        if (*((float*)scale) == 1.0)
          break;
        else {
          float *src_f = (float*) orig_bufs[i];
          float *scl_f;
          const float s = *((float*) scale);
          int ierr = MPI_Alloc_mem(size, MPI_INFO_NULL, &scl_f);
          assert(ierr == MPI_SUCCESS);
          scaled_data = scl_f;
          for (j = 0; j < nelem; j++)
            scl_f[j] = src_f[j]*s;
        }
        break;

      case ARMCI_ACC_DBL:
        MPI_Type_size(MPI_DOUBLE, &type_size);
        type = MPI_DOUBLE;
        nelem= size/type_size;

        if (*((double*)scale) == 1.0)
          break;
        else {
          double *src_d = (double*) orig_bufs[i];
          double *scl_d;
          const double s = *((double*) scale);
          int ierr = MPI_Alloc_mem(size, MPI_INFO_NULL, &scl_d);
          assert(ierr == MPI_SUCCESS);
          scaled_data = scl_d;
          for (j = 0; j < nelem; j++)
            scl_d[j] = src_d[j]*s;
        }
        break;

      case ARMCI_ACC_CPL:
        MPI_Type_size(MPI_FLOAT, &type_size);
        type = MPI_FLOAT;
        nelem= size/type_size;

        if (((float*)scale)[0] == 1.0 && ((float*)scale)[1] == 0.0)
          break;
        else {
          float *src_fc = (float*) orig_bufs[i];
          float *scl_fc;
          const float s_r = ((float*)scale)[0];
          const float s_c = ((float*)scale)[1];
          int ierr = MPI_Alloc_mem(size, MPI_INFO_NULL, &scl_fc);
          assert(ierr == MPI_SUCCESS);
          scaled_data = scl_fc;
          for (j = 0; j < nelem; j += 2) {
            // Complex multiplication: (a + bi)*(c + di)
            scl_fc[j]   = src_fc[j]*s_r   - src_fc[j+1]*s_c;
            scl_fc[j+1] = src_fc[j+1]*s_r + src_fc[j]*s_c;
          }
        }
        break;

      case ARMCI_ACC_DCP:
        MPI_Type_size(MPI_DOUBLE, &type_size);
        type = MPI_DOUBLE;
        nelem= size/type_size;

        if (((double*)scale)[0] == 1.0 && ((double*)scale)[1] == 0.0)
          break;
        else {
          double *src_dc = (double*) orig_bufs[i];
          double *scl_dc;
          const double s_r = ((double*)scale)[0];
          const double s_c = ((double*)scale)[1];
          int ierr = MPI_Alloc_mem(size, MPI_INFO_NULL, &scl_dc);
          assert(ierr == MPI_SUCCESS);
          scaled_data = scl_dc;
          for (j = 0; j < nelem; j += 2) {
            // Complex multiplication: (a + bi)*(c + di)
            scl_dc[j]   = src_dc[j]*s_r   - src_dc[j+1]*s_c;
            scl_dc[j+1] = src_dc[j+1]*s_r + src_dc[j]*s_c;
          }
        }
        break;

      default:
        ARMCII_Error(__FILE__, __LINE__, __func__, "unknown data type", 100);
    }

    assert(size % type_size == 0);

    // Scaling was applied, we have a private buffer
    if (scaled_data != NULL) {
      new_bufs[i] = scaled_data;
      num_moved++;

    // Buffer is alread private and not scaled.  Use the original buffer.
    } else if (mreg == NULL) {
      new_bufs[i] = orig_bufs[i];
      
    // Buffer is in shared space, make a private copy
    } else {
      int ierr = MPI_Alloc_mem(size, MPI_INFO_NULL, &new_bufs[i]);
      assert(ierr == MPI_SUCCESS);
      mreg_get(mreg, orig_bufs[i], new_bufs[i], size, ARMCI_GROUP_WORLD.rank);
      num_moved++;
    }

    if (mreg != NULL)
      mreg_unlock(mreg, ARMCI_GROUP_WORLD.rank);
  }

  *new_bufs_ptr = new_bufs;
  
  return num_moved;
}


/** Finish a set of prepared buffers.  Will perform communication and copies as
  * needed to ensure results are in the original buffers.  Temporary space will be
  * freed.
  *
  * @param[in]  orig_bufs Original set of buffers.
  * @param[out] new_bufs  Set of private buffers.
  * @param[in]  count     Number of entries in the buffer list.
  * @param[in]  size      The size of the buffers (all are of the same size).
  */
void ARMCII_Buf_acc_finish(void **orig_bufs, void **new_bufs, int count, int size) {
  ARMCII_Buf_put_finish(orig_bufs, new_bufs, count, size);
}


/** Prepare a set of buffers for use with a get operation.  The returned set of
  * buffers is guaranteed to be in private space.  Copies will be made if needed,
  * the result should be completed by finish.
  *
  * @param[in]  orig_bufs Original set of buffers.
  * @param[out] new_bufs  Pointer to the set of private buffers.
  * @param[in]  count     Number of entries in the buffer list.
  * @param[in]  size      The size of the buffers (all are of the same size).
  * @return               Number of buffers that were moved.
  */
int ARMCII_Buf_get_prepare(void **orig_bufs, void ***new_bufs_ptr, int count, int size) {
  void **new_bufs = malloc(count*sizeof(void*));
  int i, num_moved = 0;

  for (i = 0; i < count; i++)
    new_bufs[i] = NULL;

  for (i = 0; i < count; i++) {
    // Check if the destination buffer is within a shared region.  If not, create
    // a temporary private buffer to hold the result.
    mem_region_t *mreg = mreg_lookup(orig_bufs[i], ARMCI_GROUP_WORLD.rank);

    if (mreg != NULL) {
      int ierr = MPI_Alloc_mem(size, MPI_INFO_NULL, &new_bufs[i]);
      assert(ierr == MPI_SUCCESS);
      num_moved++;
    } else {
      new_bufs[i] = orig_bufs[i];
    }
  }

  *new_bufs_ptr = new_bufs;
  
  return num_moved;
}


/** Finish a set of prepared buffers.  Will perform communication and copies as
  * needed to ensure results are in the original buffers.  Temporary space will be
  * freed.
  *
  * @param[in]  orig_bufs Original set of buffers.
  * @param[out] new_bufs  Set of private buffers.
  * @param[in]  count     Number of entries in the buffer list.
  * @param[in]  size      The size of the buffers (all are of the same size).
  */
void ARMCII_Buf_get_finish(void **orig_bufs, void **new_bufs, int count, int size) {
  int i;

  for (i = 0; i < count; i++) {
    if (orig_bufs[i] != new_bufs[i]) {
      mem_region_t *mreg = mreg_lookup(orig_bufs[i], ARMCI_GROUP_WORLD.rank);
      assert(mreg != NULL);

      mreg_lock(mreg, ARMCI_GROUP_WORLD.rank);
      mreg_put(mreg, new_bufs[i], orig_bufs[i], size, ARMCI_GROUP_WORLD.rank);
      mreg_unlock(mreg, ARMCI_GROUP_WORLD.rank);

      MPI_Free_mem(new_bufs[i]);
    }
  }
}
