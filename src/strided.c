/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <armci.h>
#include <armci_internals.h>
#include <gmr.h>
#include <debug.h>


/** Convert an ARMCI strided access description into an MPI subarray datatype.
  *
  * @param[in]  stride_array    Array of strides
  * @param[in]  count           Array of transfer counts
  * @param[in]  stride_levels   Number of levels of striding
  * @param[in]  old_type        Type of the data element described by count and stride_array
  * @param[out] new_type        New MPI type for the given strided access
  */
void ARMCII_Strided_to_dtype(int stride_array[/*stride_levels*/], int count[/*stride_levels+1*/],
                             int stride_levels, MPI_Datatype old_type, MPI_Datatype *new_type)
{
  int sizes   [stride_levels+1];
  int subsizes[stride_levels+1];
  int starts  [stride_levels+1];
  int i, old_type_size;

  MPI_Type_size(old_type, &old_type_size);

  /* Eliminate counts that don't count (all 1 counts at the end) */
  for (i = stride_levels+1; (i > 0) && (stride_levels > 0) && (count[i-1] == 1); i--)
    stride_levels--;

  /* A correct strided spec should me monotonic increasing and stride_array[i+1] should
     be a multiple of stride_array[i]. */
  if (stride_levels > 0) {
    for (i = 1; i < stride_levels; i++) {
      ARMCII_Assert(stride_array[i] >= stride_array[i-1]);
      /* This assertion is violated by what seems to be valid usage resulting from
       * the new GA API call nga_strided_get during the stride test in GA 5.2.
       * ARMCII_Assert((stride_array[i] % stride_array[i-1]) == 0); */
    }
  }

  /* Test for a contiguous transfer */
  if (stride_levels == 0) {
    int elem_count = count[0]/old_type_size;

    ARMCII_Assert((count[0] % old_type_size) == 0);
    MPI_Type_contiguous(elem_count, old_type, new_type);
  }

  /* Transfer is non-contiguous */
  else {

    for (i = 0; i < stride_levels+1; i++)
      starts[i] = 0;

    sizes   [stride_levels] = stride_array[0]/old_type_size;
    subsizes[stride_levels] = count[0]/old_type_size;

    ARMCII_Assert((stride_array[0] % old_type_size) == 0);
    ARMCII_Assert((count[0] % old_type_size) == 0);

    for (i = 1; i < stride_levels; i++) {
      /* Convert strides into dimensions by dividing out contributions from lower dims */
      sizes   [stride_levels-i] = stride_array[i]/stride_array[i-1];
      subsizes[stride_levels-i] = count[i];

      /* This assertion is violated by what seems to be valid usage resulting from
       * the new GA API call nga_strided_get during the stride test in GA 5.2.  
       * ARMCII_Assert_msg((stride_array[i] % stride_array[i-1]) == 0, "Invalid striding"); */
    }

    sizes   [0] = count[stride_levels];
    subsizes[0] = count[stride_levels];

    MPI_Type_create_subarray(stride_levels+1, sizes, subsizes, starts, MPI_ORDER_C, old_type, new_type);
  }
}


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_PutS = PARMCI_PutS
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_PutS ARMCI_PutS
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_PutS as PARMCI_PutS
#endif
/* -- end weak symbols block -- */

/** Blocking operation that transfers data from the calling process to the
  * memory of the remote process.  The data transfer is strided and blocking.
  *
  * @param[in] src_ptr         Source starting address of the data block to put.
  * @param[in] src_stride_arr  Source array of stride distances in bytes.
  * @param[in] dst_ptr         Destination starting address to put data.
  * @param[in] dst_stride_ar   Destination array of stride distances in bytes.
  * @param[in] count           Block size in each dimension. count[0] should be the
  *                            number of bytes of contiguous data in leading dimension.
  * @param[in] stride_levels   The level of strides.
  * @param[in] proc            Remote process ID (destination).
  *
  * @return                    Zero on success, error code otherwise.
  */
int PARMCI_PutS(void *src_ptr, int src_stride_ar[/*stride_levels*/],
               void *dst_ptr, int dst_stride_ar[/*stride_levels*/], 
               int count[/*stride_levels+1*/], int stride_levels, int proc) {

  int err;

  if (ARMCII_GLOBAL_STATE.strided_method == ARMCII_STRIDED_DIRECT) {
    void         *src_buf = NULL;
    gmr_t *mreg, *gmr_loc = NULL;
    MPI_Datatype src_type, dst_type;

    /* COPY: Guard shared buffers */
    if (ARMCII_GLOBAL_STATE.shr_buf_method == ARMCII_SHR_BUF_COPY) {
      gmr_loc = gmr_lookup(src_ptr, ARMCI_GROUP_WORLD.rank);

      if (gmr_loc != NULL) {
        int i, size;

        for (i = 1, size = count[0]; i < stride_levels+1; i++)
          size *= count[i];

        MPI_Alloc_mem(size, MPI_INFO_NULL, &src_buf);
        ARMCII_Assert(src_buf != NULL);

        armci_write_strided(src_ptr, stride_levels, src_stride_ar, count, src_buf);

        MPI_Type_contiguous(size, MPI_BYTE, &src_type);
      }
    }
    else {
      gmr_loc = gmr_lookup(src_ptr, ARMCI_GROUP_WORLD.rank);
      if (gmr_loc != NULL) {
          gmr_sync(gmr_loc);
      }
    }

    /* NOGUARD: If src_buf hasn't been assigned to a copy, the strided source
     * buffer is going to be used directly. */
    if (src_buf == NULL) { 
        src_buf = src_ptr;
        ARMCII_Strided_to_dtype(src_stride_ar, count, stride_levels, MPI_BYTE, &src_type);
    }

    ARMCII_Strided_to_dtype(dst_stride_ar, count, stride_levels, MPI_BYTE, &dst_type);

    MPI_Type_commit(&src_type);
    MPI_Type_commit(&dst_type);

    mreg = gmr_lookup(dst_ptr, proc);
    ARMCII_Assert_msg(mreg != NULL, "Invalid shared pointer");

    gmr_put_typed(mreg, src_buf, 1, src_type, dst_ptr, 1, dst_type, proc);
    gmr_flush(mreg, proc, 1); /* flush_local */

    MPI_Type_free(&src_type);
    MPI_Type_free(&dst_type);

    /* COPY: Free temporary buffer */
    if (src_buf != src_ptr) {
      MPI_Free_mem(src_buf);
    }

    err = 0;

  } else {
    armci_giov_t iov;

    ARMCII_Strided_to_iov(&iov, src_ptr, src_stride_ar, dst_ptr, dst_stride_ar, count, stride_levels);
    err = PARMCI_PutV(&iov, 1, proc);

    free(iov.src_ptr_array);
    free(iov.dst_ptr_array);
  }

  return err;
}


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_GetS = PARMCI_GetS
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_GetS ARMCI_GetS
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_GetS as PARMCI_GetS
#endif
/* -- end weak symbols block -- */

/** Blocking operation that transfers data from the remote process to the
  * memory of the calling process.  The data transfer is strided and blocking.
  *
  * @param[in] src_ptr         Source starting address of the data block to put.
  * @param[in] src_stride_arr  Source array of stride distances in bytes.
  * @param[in] dst_ptr         Destination starting address to put data.
  * @param[in] dst_stride_ar   Destination array of stride distances in bytes.
  * @param[in] count           Block size in each dimension. count[0] should be the
  *                            number of bytes of contiguous data in leading dimension.
  * @param[in] stride_levels   The level of strides.
  * @param[in] proc            Remote process ID (destination).
  *
  * @return                    Zero on success, error code otherwise.
  */
int PARMCI_GetS(void *src_ptr, int src_stride_ar[/*stride_levels*/],
               void *dst_ptr, int dst_stride_ar[/*stride_levels*/], 
               int count[/*stride_levels+1*/], int stride_levels, int proc) {

  int err;

  if (ARMCII_GLOBAL_STATE.strided_method == ARMCII_STRIDED_DIRECT) {
    void         *dst_buf = NULL;
    gmr_t *mreg, *gmr_loc = NULL;
    MPI_Datatype src_type, dst_type;

    /* COPY: Guard shared buffers */
    if (ARMCII_GLOBAL_STATE.shr_buf_method == ARMCII_SHR_BUF_COPY) {
      gmr_loc = gmr_lookup(dst_ptr, ARMCI_GROUP_WORLD.rank);

      if (gmr_loc != NULL) {
        int i, size;

        for (i = 1, size = count[0]; i < stride_levels+1; i++)
          size *= count[i];

        MPI_Alloc_mem(size, MPI_INFO_NULL, &dst_buf);
        ARMCII_Assert(dst_buf != NULL);

        MPI_Type_contiguous(size, MPI_BYTE, &dst_type);
      }
    }
    else {
      gmr_loc = gmr_lookup(dst_ptr, ARMCI_GROUP_WORLD.rank);
      if (gmr_loc != NULL) {
          gmr_sync(gmr_loc);
      }
    }

    /* NOGUARD: If dst_buf hasn't been assigned to a copy, the strided source
     * buffer is going to be used directly. */
    if (dst_buf == NULL) { 
        dst_buf = dst_ptr;
        ARMCII_Strided_to_dtype(dst_stride_ar, count, stride_levels, MPI_BYTE, &dst_type);
    }

    ARMCII_Strided_to_dtype(src_stride_ar, count, stride_levels, MPI_BYTE, &src_type);

    MPI_Type_commit(&src_type);
    MPI_Type_commit(&dst_type);

    mreg = gmr_lookup(src_ptr, proc);
    ARMCII_Assert_msg(mreg != NULL, "Invalid shared pointer");

    gmr_get_typed(mreg, src_ptr, 1, src_type, dst_buf, 1, dst_type, proc);
    gmr_flush(mreg, proc, 0);

    /* COPY: Finish the transfer */
    if (dst_buf != dst_ptr) {
      armci_read_strided(dst_ptr, stride_levels, dst_stride_ar, count, dst_buf);
      MPI_Free_mem(dst_buf);
    }

    MPI_Type_free(&src_type);
    MPI_Type_free(&dst_type);

    err = 0;

  } else {
    armci_giov_t iov;

    ARMCII_Strided_to_iov(&iov, src_ptr, src_stride_ar, dst_ptr, dst_stride_ar, count, stride_levels);
    err = PARMCI_GetV(&iov, 1, proc);

    free(iov.src_ptr_array);
    free(iov.dst_ptr_array);
  }

  return err;
}


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_AccS = PARMCI_AccS
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_AccS ARMCI_AccS
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_AccS as PARMCI_AccS
#endif
/* -- end weak symbols block -- */

/** Blocking operation that accumulates data from the local process into the
  * memory of the remote process.  The data transfer is strided and blocking.
  *
  * @param[in] datatype        Type of data to be transferred.
  * @param[in] scale           Pointer to the value that input data should be scaled by.
  * @param[in] src_ptr         Source starting address of the data block to put.
  * @param[in] src_stride_arr  Source array of stride distances in bytes.
  * @param[in] dst_ptr         Destination starting address to put data.
  * @param[in] dst_stride_ar   Destination array of stride distances in bytes.
  * @param[in] count           Block size in each dimension. count[0] should be the
  *                            number of bytes of contiguous data in leading dimension.
  * @param[in] stride_levels   The level of strides.
  * @param[in] proc            Remote process ID (destination).
  *
  * @return                    Zero on success, error code otherwise.
  */
int PARMCI_AccS(int datatype, void *scale,
               void *src_ptr, int src_stride_ar[/*stride_levels*/],
               void *dst_ptr, int dst_stride_ar[/*stride_levels*/],
               int count[/*stride_levels+1*/], int stride_levels, int proc) {

  int err;

  if (ARMCII_GLOBAL_STATE.strided_method == ARMCII_STRIDED_DIRECT) {
    void         *src_buf = NULL;
    gmr_t *mreg, *gmr_loc = NULL;
    MPI_Datatype src_type, dst_type, mpi_datatype;
    int          scaled, mpi_datatype_size;
    int          src_size, dst_size;

    ARMCII_Acc_type_translate(datatype, &mpi_datatype, &mpi_datatype_size);
    scaled = ARMCII_Buf_acc_is_scaled(datatype, scale);

    /* SCALE: copy and scale if requested */
    if (scaled) {
      armci_giov_t iov;
      int i, nelem;

      if (ARMCII_GLOBAL_STATE.shr_buf_method != ARMCII_SHR_BUF_NOGUARD)
        gmr_loc = gmr_lookup(src_ptr, ARMCI_GROUP_WORLD.rank);

      for (i = 1, nelem = count[0]/mpi_datatype_size; i < stride_levels+1; i++)
        nelem *= count[i];

      MPI_Alloc_mem(nelem*mpi_datatype_size, MPI_INFO_NULL, &src_buf);
      ARMCII_Assert(src_buf != NULL);

      /* Shoehorn the strided information into an IOV */
      ARMCII_Strided_to_iov(&iov, src_ptr, src_stride_ar, src_ptr, src_stride_ar, count, stride_levels);

      for (i = 0; i < iov.ptr_array_len; i++)
        ARMCII_Buf_acc_scale(iov.src_ptr_array[i], ((uint8_t*)src_buf) + i*iov.bytes, iov.bytes, datatype, scale);

      free(iov.src_ptr_array);
      free(iov.dst_ptr_array);

      MPI_Type_contiguous(nelem, mpi_datatype, &src_type);
    }

    /* COPY: Guard shared buffers */
    else if (ARMCII_GLOBAL_STATE.shr_buf_method == ARMCII_SHR_BUF_COPY) {
      gmr_loc = gmr_lookup(src_ptr, ARMCI_GROUP_WORLD.rank);

      if (gmr_loc != NULL) {
        int i, nelem;

        for (i = 1, nelem = count[0]/mpi_datatype_size; i < stride_levels+1; i++)
          nelem *= count[i];

        MPI_Alloc_mem(nelem*mpi_datatype_size, MPI_INFO_NULL, &src_buf);
        ARMCII_Assert(src_buf != NULL);

        armci_write_strided(src_ptr, stride_levels, src_stride_ar, count, src_buf);

        MPI_Type_contiguous(nelem, mpi_datatype, &src_type);
      }
    }
    else {
      gmr_loc = gmr_lookup(src_ptr, ARMCI_GROUP_WORLD.rank);
      if (gmr_loc != NULL) {
          gmr_sync(gmr_loc);
      }
    }

    /* NOGUARD: If src_buf hasn't been assigned to a copy, the strided source
     * buffer is going to be used directly. */
    if (src_buf == NULL) { 
        src_buf = src_ptr;
        ARMCII_Strided_to_dtype(src_stride_ar, count, stride_levels, mpi_datatype, &src_type);
    }

    ARMCII_Strided_to_dtype(dst_stride_ar, count, stride_levels, mpi_datatype, &dst_type);

    MPI_Type_commit(&src_type);
    MPI_Type_commit(&dst_type);

    MPI_Type_size(src_type, &src_size);
    MPI_Type_size(dst_type, &dst_size);

    ARMCII_Assert(src_size == dst_size);

    mreg = gmr_lookup(dst_ptr, proc);
    ARMCII_Assert_msg(mreg != NULL, "Invalid shared pointer");

    gmr_accumulate_typed(mreg, src_buf, 1, src_type, dst_ptr, 1, dst_type, proc);
    gmr_flush(mreg, proc, 1); /* flush_local */

    MPI_Type_free(&src_type);
    MPI_Type_free(&dst_type);

    /* COPY/SCALE: Free temp buffer */
    if (src_buf != src_ptr) {
      MPI_Free_mem(src_buf);
    }

    err = 0;

  } else {
    armci_giov_t iov;

    ARMCII_Strided_to_iov(&iov, src_ptr, src_stride_ar, dst_ptr, dst_stride_ar, count, stride_levels);
    err = PARMCI_AccV(datatype, scale, &iov, 1, proc);

    free(iov.src_ptr_array);
    free(iov.dst_ptr_array);
  }

  return err;
}

/** Translate a strided operation into a more general IO Vector.
  *
  * @param[in] src_ptr         Source starting address of the data block to put.
  * @param[in] src_stride_arr  Source array of stride distances in bytes.
  * @param[in] dst_ptr         Destination starting address to put data.
  * @param[in] dst_stride_ar   Destination array of stride distances in bytes.
  * @param[in] count           Block size in each dimension. count[0] should be the
  *                            number of bytes of contiguous data in leading dimension.
  * @param[in] stride_levels   The level of strides.
  *
  * @return                    Zero on success, error code otherwise.
  */
void ARMCII_Strided_to_iov(armci_giov_t *iov,
               void *src_ptr, int src_stride_ar[/*stride_levels*/],
               void *dst_ptr, int dst_stride_ar[/*stride_levels*/], 
               int count[/*stride_levels+1*/], int stride_levels) {

  int i;

  iov->bytes = count[0];
  iov->ptr_array_len = 1;

  for (i = 0; i < stride_levels; i++)
    iov->ptr_array_len *= count[i+1];

  iov->src_ptr_array = malloc(iov->ptr_array_len*sizeof(void*));
  iov->dst_ptr_array = malloc(iov->ptr_array_len*sizeof(void*));

  ARMCII_Assert((iov->src_ptr_array != NULL) && (iov->dst_ptr_array != NULL));

  // Case 1: Non-strided transfer
  if (stride_levels == 0) {
    iov->src_ptr_array[0] = src_ptr;
    iov->dst_ptr_array[0] = dst_ptr;

  // Case 2: Strided transfer
  } else {
    int idx[stride_levels];
    int xfer;

    for (i = 0; i < stride_levels; i++)
      idx[i] = 0;

    for (xfer = 0; idx[stride_levels-1] < count[stride_levels]; xfer++) {
      int disp_src = 0;
      int disp_dst = 0;

      ARMCII_Assert(xfer < iov->ptr_array_len);

      // Calculate displacements from base pointers
      for (i = 0; i < stride_levels; i++) {
        disp_src += src_stride_ar[i]*idx[i];
        disp_dst += dst_stride_ar[i]*idx[i];
      }

      // Add to the IO Vector
      iov->src_ptr_array[xfer] = ((uint8_t*)src_ptr) + disp_src;
      iov->dst_ptr_array[xfer] = ((uint8_t*)dst_ptr) + disp_dst;

      // Increment innermost index
      idx[0] += 1;

      // Propagate "carry" overflows outward.  We're done when the outermost
      // index is greater than the requested count.
      for (i = 0; i < stride_levels-1; i++) {
        if (idx[i] >= count[i+1]) {
          idx[i]    = 0;
          idx[i+1] += 1;
        }
      }
    }

    ARMCII_Assert(xfer == iov->ptr_array_len);
  }
}


/** Translate a strided operation into a more general IO Vector iterator.
  *
  * @param[in] src_ptr         Source starting address of the data block to put.
  * @param[in] src_stride_arr  Source array of stride distances in bytes.
  * @param[in] dst_ptr         Destination starting address to put data.
  * @param[in] dst_stride_ar   Destination array of stride distances in bytes.
  * @param[in] count           Block size in each dimension. count[0] should be the
  *                            number of bytes of contiguous data in leading dimension.
  * @param[in] stride_levels   The level of strides.
  *
  * @return                    ARMCI IOV iterator corresponding to the strided parameters.
  */
armcii_iov_iter_t *ARMCII_Strided_to_iov_iter(
               void *src_ptr, int src_stride_ar[/*stride_levels*/],
               void *dst_ptr, int dst_stride_ar[/*stride_levels*/], 
               int count[/*stride_levels+1*/], int stride_levels) {

  int i;
  armcii_iov_iter_t *it = malloc(sizeof(armcii_iov_iter_t));

  ARMCII_Assert(it != NULL);

  it->src = src_ptr;
  it->dst = dst_ptr;
  it->stride_levels = stride_levels;
  it->base_ptr      = malloc(sizeof(int)*(4*stride_levels+1));
  it->was_contiguous= 0;

  ARMCII_Assert( it->base_ptr != NULL );

  it->src_stride_ar = &it->base_ptr[0*stride_levels];
  it->dst_stride_ar = &it->base_ptr[1*stride_levels];
  it->count         = &it->base_ptr[2*stride_levels];
  it->idx           = &it->base_ptr[3*stride_levels+1];

  for (i = 0; i < stride_levels; i++) {
    it->src_stride_ar[i] = src_stride_ar[i];
    it->dst_stride_ar[i] = dst_stride_ar[i];
    it->count[i]         = count[i];
    it->idx[i]           = 0;
  }

  return it;
}


/** Free an iterator.
  * 
  * @param[in]  it      IOV iterator
  */
void ARMCII_Iov_iter_free(armcii_iov_iter_t *it) {
  free(it->base_ptr);
  free(it);
}


/** Query whether the iterator has another iteration.
  * 
  * @param[in]  it      IOV iterator
  *
  * @return             True if another iteration exists
  */
int ARMCII_Iov_iter_has_next(armcii_iov_iter_t *it) {
  return ((it->idx[it->stride_levels-1] < it->count[it->stride_levels]) && (!it->was_contiguous));
}


/** Get the next source/destination pointer pair from the IOV iterator.
  * 
  * @param[in]  it      IOV iterator
  * @param[out] src     Source adress
  * @param[out] dst     Destination adress
  *
  * @return             True if another iteration existed
  */
int ARMCII_Iov_iter_next(armcii_iov_iter_t *it, void **src, void **dst) {

  if (!ARMCII_Iov_iter_has_next(it)) {
    *src = NULL;
    *dst = NULL;
    return 0;
  }

  // Case 1: Non-strided transfer
  if (it->stride_levels == 0) {
    *src = src;
    *dst = dst;
    it->was_contiguous = 1;

  // Case 2: Strided transfer
  } else {
    int i, disp_src = 0, disp_dst = 0;

    // Calculate displacements from base pointers
    for (i = 0; i < it->stride_levels; i++) {
      disp_src += it->src_stride_ar[i]*it->idx[i];
      disp_dst += it->dst_stride_ar[i]*it->idx[i];
    }

    // Add to the IO Vector
    *src = ((uint8_t*)it->src) + disp_src;
    *dst = ((uint8_t*)it->dst) + disp_dst;

    // Increment innermost index
    it->idx[0] += 1;

    // Propagate "carry" overflows outward.  We're done when the outermost
    // index is greater than the requested count.
    for (i = 0; i < it->stride_levels-1; i++) {
      if (it->idx[i] >= it->count[i+1]) {
        it->idx[i]    = 0;
        it->idx[i+1] += 1;
      }
    }
  }

  return 1;
}


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_PutS_flag = PARMCI_PutS_flag
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_PutS_flag ARMCI_PutS_flag
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_PutS_flag as PARMCI_PutS_flag
#endif
/* -- end weak symbols block -- */

/** Blocking operation that transfers data from the calling process to the
  * memory of the remote process.  The data transfer is strided and blocking.
  * After the transfer completes, the given flag is set on the remote process.
  *
  * @param[in] src_ptr         Source starting address of the data block to put.
  * @param[in] src_stride_arr  Source array of stride distances in bytes.
  * @param[in] dst_ptr         Destination starting address to put data.
  * @param[in] dst_stride_ar   Destination array of stride distances in bytes.
  * @param[in] count           Block size in each dimension. count[0] should be the
  *                            number of bytes of contiguous data in leading dimension.
  * @param[in] stride_levels   The level of strides.
  * @param[in] flag            Location of the flag buffer
  * @param[in] value           Value to set the flag to
  * @param[in] proc            Remote process ID (destination).
  *
  * @return                    Zero on success, error code otherwise.
  */
int PARMCI_PutS_flag(void *src_ptr, int src_stride_ar[/*stride_levels*/],
                 void *dst_ptr, int dst_stride_ar[/*stride_levels*/], 
                 int count[/*stride_levels+1*/], int stride_levels, 
                 int *flag, int value, int proc) {

  /* TODO: This can be optimized with a more direct implementation, especially in the
   *       case where RMA is ordered; in that case, the Fence (Flush) is not necessary. */
  PARMCI_PutS(src_ptr, src_stride_ar, dst_ptr, dst_stride_ar, count, stride_levels, proc);
  PARMCI_Fence(proc);
  PARMCI_Put(&value, flag, sizeof(int), proc);

  return 1;
}


/* Pack strided data into a contiguous destination buffer.  This is a local operation.
 *
 * @param[in] src            Pointer to the strided buffer
 * @param[in] stride_levels  Number of levels of striding
 * @param[in] src_stride_arr Array of length stride_levels of stride lengths
 * @param[in] count          Array of length stride_levels+1 of the number of
 *                           units at each stride level (lowest is contiguous)
 * @param[in] dst            Destination contiguous buffer
 */
void armci_write_strided(void *src, int stride_levels, int src_stride_arr[],
                         int count[], char *dst) {
  armci_giov_t iov;
  int i;

  // Shoehorn the strided information into an IOV
  ARMCII_Strided_to_iov(&iov, src, src_stride_arr, src, src_stride_arr, count, stride_levels);

  for (i = 0; i < iov.ptr_array_len; i++)
    ARMCI_Copy(iov.src_ptr_array[i], dst + i*count[0], iov.bytes);

  free(iov.src_ptr_array);
  free(iov.dst_ptr_array);
}


/* Unpack strided data from a contiguous source buffer.  This is a local operation.
 *
 * @param[in] src            Pointer to the contiguous buffer
 * @param[in] stride_levels  Number of levels of striding
 * @param[in] dst_stride_arr Array of length stride_levels of stride lengths
 * @param[in] count          Array of length stride_levels+1 of the number of
 *                           units at each stride level (lowest is contiguous)
 * @param[in] dst            Destination strided buffer
 */
void armci_read_strided(void *dst, int stride_levels, int dst_stride_arr[],
                        int count[], char *src) {
  armci_giov_t iov;
  int i;

  // Shoehorn the strided information into an IOV
  ARMCII_Strided_to_iov(&iov, dst, dst_stride_arr, dst, dst_stride_arr, count, stride_levels);

  for (i = 0; i < iov.ptr_array_len; i++)
    ARMCI_Copy(src + i*count[0], iov.dst_ptr_array[i], iov.bytes);

  free(iov.src_ptr_array);
  free(iov.dst_ptr_array);
}
