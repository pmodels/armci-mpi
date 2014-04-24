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


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_NbPutS = PARMCI_NbPutS
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_NbPutS ARMCI_NbPutS
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_NbPutS as PARMCI_NbPutS
#endif
/* -- end weak symbols block -- */

/** Non-blocking operation that transfers data from the calling process to the
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
int PARMCI_NbPutS(void *src_ptr, int src_stride_ar[/*stride_levels*/],
               void *dst_ptr, int dst_stride_ar[/*stride_levels*/], 
               int count[/*stride_levels+1*/], int stride_levels, int proc, armci_hdl_t *handle) {

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
    if (src_buf != src_ptr)
      MPI_Free_mem(src_buf);

    err = 0;

  } else {
    armci_giov_t iov;

    ARMCII_Strided_to_iov(&iov, src_ptr, src_stride_ar, dst_ptr, dst_stride_ar, count, stride_levels);
    err = PARMCI_PutV(&iov, 1, proc);

    free(iov.src_ptr_array);
    free(iov.dst_ptr_array);
  }

  if (handle!=NULL) {
      /* Regular (not aggregate) handles merely store the target for future flushing. */
      handle->target = proc;
  }

#ifdef EXPLICIT_PROGRESS
  gmr_progress();
#endif

  return err;
}


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_NbGetS = PARMCI_NbGetS
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_NbGetS ARMCI_NbGetS
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_NbGetS as PARMCI_NbGetS
#endif
/* -- end weak symbols block -- */

/** Non-blocking operation that transfers data from the remote process to the
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
int PARMCI_NbGetS(void *src_ptr, int src_stride_ar[/*stride_levels*/],
               void *dst_ptr, int dst_stride_ar[/*stride_levels*/], 
               int count[/*stride_levels+1*/], int stride_levels, int proc, armci_hdl_t *handle) {

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

  if (handle!=NULL) {
      /* Regular (not aggregate) handles merely store the target for future flushing. */
      handle->target = proc;
  }

#ifdef EXPLICIT_PROGRESS
  gmr_progress();
#endif

  return err;
}


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_NbAccS = PARMCI_NbAccS
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_NbAccS ARMCI_NbAccS
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_NbAccS as PARMCI_NbAccS
#endif
/* -- end weak symbols block -- */

/** Non-blocking operation that accumulates data from the local process into the
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
int PARMCI_NbAccS(int datatype, void *scale,
               void *src_ptr, int src_stride_ar[/*stride_levels*/],
               void *dst_ptr, int dst_stride_ar[/*stride_levels*/],
               int count[/*stride_levels+1*/], int stride_levels, int proc, armci_hdl_t *handle) {

  int err;

  if (ARMCII_GLOBAL_STATE.strided_method == ARMCII_STRIDED_DIRECT) {
    void         *src_buf = NULL;
    gmr_t *mreg, *gmr_loc = NULL;
    MPI_Datatype src_type, dst_type, mpi_datatype;
    int          scaled, mpi_datatype_size;

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

    /* NOGUARD: If src_buf hasn't been assigned to a copy, the strided source
     * buffer is going to be used directly. */
    if (src_buf == NULL) { 
        src_buf = src_ptr;
        ARMCII_Strided_to_dtype(src_stride_ar, count, stride_levels, mpi_datatype, &src_type);
    }

    ARMCII_Strided_to_dtype(dst_stride_ar, count, stride_levels, mpi_datatype, &dst_type);

    MPI_Type_commit(&src_type);
    MPI_Type_commit(&dst_type);

    int src_size, dst_size;

    MPI_Type_size(src_type, &src_size);
    MPI_Type_size(dst_type, &dst_size);

    ARMCII_Assert(src_size == dst_size);

    mreg = gmr_lookup(dst_ptr, proc);
    ARMCII_Assert_msg(mreg != NULL, "Invalid shared pointer");

    gmr_accumulate_typed(mreg, src_buf, 1, src_type, dst_ptr, 1, dst_type, proc);
    gmr_flush(mreg, proc, 0);

    MPI_Type_free(&src_type);
    MPI_Type_free(&dst_type);

    /* COPY/SCALE: Free temp buffer */
    if (src_buf != src_ptr)
      MPI_Free_mem(src_buf);

    err = 0;

  } else {
    armci_giov_t iov;

    ARMCII_Strided_to_iov(&iov, src_ptr, src_stride_ar, dst_ptr, dst_stride_ar, count, stride_levels);
    err = PARMCI_AccV(datatype, scale, &iov, 1, proc);

    free(iov.src_ptr_array);
    free(iov.dst_ptr_array);
  }

  if (handle!=NULL) {
      /* Regular (not aggregate) handles merely store the target for future flushing. */
      handle->target = proc;
  }

#ifdef EXPLICIT_PROGRESS
  gmr_progress();
#endif

  return err;
}
