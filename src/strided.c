/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <armci.h>
#include <debug.h>

enum strided_ops_e { STRIDED_PUT, STRIDED_GET, STRIDED_ACC };

int ARMCI_ImplS(void *src_ptr, int src_stride_ar[/*stride_levels*/],
               void *dst_ptr, int dst_stride_ar[/*stride_levels*/], 
               int count[/*stride_levels+1*/], int stride_levels, int proc,
               int strided_op, int datatype, void *scale);

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
int ARMCI_PutS(void *src_ptr, int src_stride_ar[/*stride_levels*/],
               void *dst_ptr, int dst_stride_ar[/*stride_levels*/], 
               int count[/*stride_levels+1*/], int stride_levels, int proc) {

  return ARMCI_ImplS(src_ptr, src_stride_ar,
                     dst_ptr, dst_stride_ar,
                     count, stride_levels, proc, STRIDED_PUT, 0, NULL);
}


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
int ARMCI_GetS(void *src_ptr, int src_stride_ar[/*stride_levels*/],
               void *dst_ptr, int dst_stride_ar[/*stride_levels*/], 
               int count[/*stride_levels+1*/], int stride_levels, int proc) {

  return ARMCI_ImplS(src_ptr, src_stride_ar,
                     dst_ptr, dst_stride_ar,
                     count, stride_levels, proc, STRIDED_GET, 0, NULL);
}


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
int ARMCI_AccS(int datatype, void *scale,
               void *src_ptr, int src_stride_ar[/*stride_levels*/],
               void *dst_ptr, int dst_stride_ar[/*stride_levels*/],
               int count[/*stride_levels+1*/], int stride_levels, int proc) {

  return ARMCI_ImplS(src_ptr, src_stride_ar,
                     dst_ptr, dst_stride_ar,
                     count, stride_levels, proc,
                     STRIDED_ACC, datatype, scale);
}


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
int ARMCI_NbPutS(void *src_ptr, int src_stride_ar[/*stride_levels*/],
               void *dst_ptr, int dst_stride_ar[/*stride_levels*/], 
               int count[/*stride_levels+1*/], int stride_levels, int proc, armci_hdl_t *hdl) {

  return ARMCI_ImplS(src_ptr, src_stride_ar,
                     dst_ptr, dst_stride_ar,
                     count, stride_levels, proc, STRIDED_PUT, 0, NULL);
}


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
int ARMCI_NbGetS(void *src_ptr, int src_stride_ar[/*stride_levels*/],
               void *dst_ptr, int dst_stride_ar[/*stride_levels*/], 
               int count[/*stride_levels+1*/], int stride_levels, int proc, armci_hdl_t *hdl) {

  return ARMCI_ImplS(src_ptr, src_stride_ar,
                     dst_ptr, dst_stride_ar,
                     count, stride_levels, proc, STRIDED_GET, 0, NULL);
}


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
int ARMCI_NbAccS(int datatype, void *scale,
               void *src_ptr, int src_stride_ar[/*stride_levels*/],
               void *dst_ptr, int dst_stride_ar[/*stride_levels*/],
               int count[/*stride_levels+1*/], int stride_levels, int proc, armci_hdl_t *hdl) {

  return ARMCI_ImplS(src_ptr, src_stride_ar,
                     dst_ptr, dst_stride_ar,
                     count, stride_levels, proc,
                     STRIDED_ACC, datatype, scale);
}


/** Underlying implementation of all strided functions.  Supports Put, Get, and Acc.
  *
  * @param[in] src_ptr         Source starting address of the data block to put.
  * @param[in] src_stride_arr  Source array of stride distances in bytes.
  * @param[in] dst_ptr         Destination starting address to put data.
  * @param[in] dst_stride_ar   Destination array of stride distances in bytes.
  * @param[in] count           Block size in each dimension. count[0] should be the
  *                            number of bytes of contiguous data in leading dimension.
  * @param[in] stride_levels   The level of strides.
  * @param[in] proc            Remote process ID (destination).
  * @param[in] strided_op      Desired operation (STRIDED_PUT, STRIDED_GET, STRIDED_ACC).
  * @param[in] datatype        Type of data to be transferred.
  * @param[in] scale           Pointer to the value that input data should be scaled by.
  *
  * @return                    Zero on success, error code otherwise.
  */
int ARMCI_ImplS(void *src_ptr, int src_stride_ar[/*stride_levels*/],
               void *dst_ptr, int dst_stride_ar[/*stride_levels*/], 
               int count[/*stride_levels+1*/], int stride_levels, int proc,
               int strided_op, int datatype, void *scale) {

  int idx[stride_levels];
  int i;

  for (i = 0; i < stride_levels; i++)
      idx[i] = 0;

  // Special case: contiguous put
  if (stride_levels == 0) {
    switch (strided_op) {
      case STRIDED_PUT:
        ARMCI_Put(src_ptr, dst_ptr, count[0], proc);
        break;
      case STRIDED_GET:
        ARMCI_Get(src_ptr, dst_ptr, count[0], proc);
        break;
      case STRIDED_ACC:
        ARMCI_Acc(datatype, scale, src_ptr, dst_ptr, count[0], proc);
        break;
      default:
        ARMCI_Error("unsupported strided operation", 1);
    }
    return 0;
  }

  while(idx[stride_levels-1] < count[stride_levels]) {
    int disp_src = 0;
    int disp_dst = 0;

    // Calculate displacements from base pointers
    for (i = 0; i < stride_levels; i++) {
      disp_src += src_stride_ar[i]*idx[i];
      disp_dst += dst_stride_ar[i]*idx[i];
    }

    // Perform communication.  TODO: Optimize for lock/unlock here
    switch (strided_op) {
      case STRIDED_PUT:
        ARMCI_Put(((uint8_t*)src_ptr) + disp_src,
                  ((uint8_t*)dst_ptr) + disp_dst,
                  count[0], proc);
        break;
      case STRIDED_GET:
        ARMCI_Get(((uint8_t*)src_ptr) + disp_src,
                  ((uint8_t*)dst_ptr) + disp_dst,
                  count[0], proc);
        break;
      case STRIDED_ACC:
        ARMCI_Acc(datatype, scale,
                  ((uint8_t*)src_ptr) + disp_src,
                  ((uint8_t*)dst_ptr) + disp_dst,
                  count[0], proc);
        break;
      default:
        ARMCI_Error("unsupported strided operation", 1);
    }

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

  return 0;
}

