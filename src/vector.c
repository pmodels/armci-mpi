/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <armci.h>
#include <armci_internals.h>
#include <debug.h>
#include <mem_region.h>


/** Check an I/O vector operation source buffers for overlap.
  *
  * @param[in] iov      Vector of transfer information.
  * @return             Logical true when regions overlap, 0 otherwise.
  */
int ARMCII_Iov_check_src_overlap(armci_giov_t *iov) {
  int i, j;
  const int size = iov->bytes;

  for (i = 0; i < iov->ptr_array_len; i++) {
    for (j = i+1; j < iov->ptr_array_len; j++) {
      const uint8_t *src_1     = iov->src_ptr_array[i];
      const uint8_t *src_2_low = iov->src_ptr_array[j];
      const uint8_t *src_2_hi  = ((uint8_t*)iov->src_ptr_array[j]) + size;

      if (src_1 >= src_2_low && src_1 < src_2_hi) {
        printf("%s: Overlapping regions %p and %p (size = %d)\n", __func__, src_1, src_2_low, size);
        return 1;
      }
    }
  }

  return 0;
}


/** Check an I/O vector operation destination buffers for overlap.
  *
  * @param[in] iov      Vector of transfer information.
  * @return             Logical true when regions overlap, 0 otherwise.
  */
int ARMCII_Iov_check_dst_overlap(armci_giov_t *iov) {
  int i, j;
  const int size = iov->bytes;

  for (i = 0; i < iov->ptr_array_len; i++) {
    for (j = i+1; j < iov->ptr_array_len; j++) {
      const uint8_t *dst_1     = iov->dst_ptr_array[i];
      const uint8_t *dst_2_low = iov->dst_ptr_array[j];
      const uint8_t *dst_2_hi  = ((uint8_t*) iov->dst_ptr_array[j]) + size;

      if (dst_1 >= dst_2_low && dst_1 < dst_2_hi) {
        printf("%s: Overlapping regions %p and %p (size = %d)\n", __func__, dst_1, dst_2_low, size);
        return 1;
      }
    }
  }

  return 0;
}


/** Perform an I/O vector operation.  Local buffers must be private.
  *
  * @param[in] op          Operation to be performed (ARMCII_OP_PUT, ...)
  * @param[in] src         Array of source pointers
  * @param[in] dst         Array of destination pointers
  * @param[in] count       Length of pointer arrays
  * @param[in] size        Size of each transfer
  * @param[in] datatype    Data type for accumulate op (ignored for all others)
  * @param[in] overlapping Do remote regions overlap?
  * @param[in] proc        Target process
  * @return                Zero on success, error code otherwise
  */
int ARMCII_Iov_op(int op, void **src, void **dst, int count, int size,
    int datatype, int overlapping, int proc) {

  int i, acc_count, type_size;
  MPI_Datatype type;

  if (op == ARMCII_OP_ACC) {
    ARMCII_Acc_type_translate(datatype, &type, &type_size);
    acc_count = size/type_size;
    assert(size % type_size == 0);
  }

  // CASE Overlapping: If remote pointers overlap, use the safe implementation
  // to avoid invalid MPI use (in MPI-3 this will be undefined so we won't need
  // a special case, but it's an error in MPI-3).

  if (overlapping) {
    for (i = 0; i < count; i++) {
      // TODO: Is it safe to assume that all remote pointers are from the same allocation?
      mem_region_t *mreg;
      void *shr_ptr;

      switch(op) {
        case ARMCII_OP_PUT:
          shr_ptr = dst[i];
          break;
        case ARMCII_OP_GET:
          shr_ptr = src[i];
          break;
        case ARMCII_OP_ACC:
          shr_ptr = dst[i];
          break;
        default:
          ARMCII_Error(__FILE__, __LINE__, __func__, "unknown operation", 100);
          return 1;
      }

      mreg = mreg_lookup(shr_ptr, proc);
      assert(mreg != NULL);

      mreg_lock(mreg, proc);

      switch(op) {
        case ARMCII_OP_PUT:
          mreg_put(mreg, src[i], dst[i], size, proc);
          break;
        case ARMCII_OP_GET:
          mreg_get(mreg, src[i], dst[i], size, proc);
          break;
        case ARMCII_OP_ACC:
          mreg_accumulate(mreg, src[i], dst[i], type, size, proc);
          break;
        default:
          ARMCII_Error(__FILE__, __LINE__, __func__, "unknown operation", 100);
          return 1;
      }

      mreg_unlock(mreg, proc);
    }


  // CASE Non-Overlapping: It's safe for us to issue all the operations under a
  // single lock.

  } else {
    // TODO: Is it safe to assume that all remote pointers are from the same allocation?
    mem_region_t *mreg;
    void *shr_ptr;

    switch(op) {
      case ARMCII_OP_PUT:
        shr_ptr = dst[0];
        break;
      case ARMCII_OP_GET:
        shr_ptr = src[0];
        break;
      case ARMCII_OP_ACC:
        shr_ptr = dst[0];
        break;
      default:
        ARMCII_Error(__FILE__, __LINE__, __func__, "unknown operation", 100);
        return 1;
    }

    mreg = mreg_lookup(shr_ptr, proc);
    assert(mreg != NULL);

    mreg_lock(mreg, proc);

    for (i = 0; i < count; i++) {
      switch(op) {
        case ARMCII_OP_PUT:
          mreg_put(mreg, src[i], dst[i], size, proc);
          break;
        case ARMCII_OP_GET:
          mreg_get(mreg, src[i], dst[i], size, proc);
          break;
        case ARMCII_OP_ACC:
          mreg_accumulate(mreg, src[i], dst[i], type, acc_count, proc);
          break;
      default:
        ARMCII_Error(__FILE__, __LINE__, __func__, "unknown operation", 100);
        return 1;
      }
    }

    mreg_unlock(mreg, proc);
  }

  return 0;
}

/** Generalized I/O vector one-sided put.
  *
  * @param[in] iov      Vector of transfer information.
  * @param[in] iov_len  Length of iov.
  * @param[in] proc     Target process.
  * @return             Success 0, otherwise non-zero.
  */
int ARMCI_PutV(armci_giov_t *iov, int iov_len, int proc) {
  int v;

  for (v = 0; v < iov_len; v++) {
    void **src_buf;
    int    overlapping = ARMCII_Iov_check_dst_overlap(&iov[v]);

    if (iov[v].ptr_array_len == 0) continue; // NOP //

    ARMCII_Buf_put_prepare(iov[v].src_ptr_array, &src_buf, iov[v].ptr_array_len, iov[v].bytes);
    ARMCII_Iov_op(ARMCII_OP_PUT, src_buf, iov[v].dst_ptr_array, iov[v].ptr_array_len, iov[v].bytes, 0, overlapping, proc);
    ARMCII_Buf_put_finish(iov[v].src_ptr_array, src_buf, iov[v].ptr_array_len, iov[v].bytes);
  }

  return 0;
}


/** Generalized I/O vector one-sided get.
  *
  * @param[in] iov      Vector of transfer information.
  * @param[in] iov_len  Length of iov.
  * @param[in] proc     Target process.
  * @return             Success 0, otherwise non-zero.
  */
int ARMCI_GetV(armci_giov_t *iov, int iov_len, int proc) {
  int v;

  for (v = 0; v < iov_len; v++) {
    void **dst_buf;
    int    overlapping = ARMCII_Iov_check_src_overlap(&iov[v]);

    if (iov[v].ptr_array_len == 0) continue; // NOP //

    ARMCII_Buf_get_prepare(iov[v].dst_ptr_array, &dst_buf, iov[v].ptr_array_len, iov[v].bytes);
    ARMCII_Iov_op(ARMCII_OP_GET, iov[v].src_ptr_array, dst_buf, iov[v].ptr_array_len, iov[v].bytes, 0, overlapping, proc);
    ARMCII_Buf_get_finish(iov[v].dst_ptr_array, dst_buf, iov[v].ptr_array_len, iov[v].bytes);
  }

  return 0;
}


/** Generalized I/O vector one-sided accumulate.
  *
  * @param[in] iov      Vector of transfer information.
  * @param[in] iov_len  Length of iov.
  * @param[in] proc     Target process.
  * @return             Success 0, otherwise non-zero.
  */
int ARMCI_AccV(int datatype, void *scale, armci_giov_t *iov, int iov_len, int proc) {
  int v;

  for (v = 0; v < iov_len; v++) {
    void **src_buf;
    int    overlapping = ARMCII_Iov_check_dst_overlap(&iov[v]);

    if (iov[v].ptr_array_len == 0) continue; // NOP //

    ARMCII_Buf_acc_prepare(iov[v].src_ptr_array, &src_buf, iov[v].ptr_array_len, iov[v].bytes, datatype, scale);
    ARMCII_Iov_op(ARMCII_OP_ACC, src_buf, iov[v].dst_ptr_array, iov[v].ptr_array_len, iov[v].bytes, datatype, overlapping, proc);
    ARMCII_Buf_acc_finish(iov[v].src_ptr_array, src_buf, iov[v].ptr_array_len, iov[v].bytes);
  }

  return 0;
}


int ARMCI_NbPutV(armci_giov_t *iov, int iov_len, int proc, armci_hdl_t* handle) {
  return ARMCI_PutV(iov, iov_len, proc);
}

int ARMCI_NbGetV(armci_giov_t *iov, int iov_len, int proc, armci_hdl_t* handle) {
  return ARMCI_GetV(iov, iov_len, proc);
}

int ARMCI_NbAccV(int datatype, void *scale, armci_giov_t *iov, int iov_len, int proc, armci_hdl_t* handle) {
  return ARMCI_AccV(datatype, scale, iov, iov_len, proc);
}


