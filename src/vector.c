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
#ifndef NO_CHECK_OVERLAP
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
#endif

  return 0;
}


/** Check an I/O vector operation destination buffers for overlap.
  *
  * @param[in] iov      Vector of transfer information.
  * @return             Logical true when regions overlap, 0 otherwise.
  */
int ARMCII_Iov_check_dst_overlap(armci_giov_t *iov) {
#ifndef NO_CHECK_OVERLAP
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
#endif

  return 0;
}


/** Check if a set of pointers all corresponds to the same allocation.
  *
  * @param[in] ptrs  An array of count shared pointers valid on proc.
  * @param[in] count Size of the ptrs array.
  * @param[in] proc  Process on which the pointers are valid.
  * @return          Non-zero (true) on success, zero (false) otherwise.
  */
int ARMCII_Iov_check_same_allocation(void **ptrs, int count, int proc) {
  int i;
  mem_region_t *mreg;
  void *base, *extent;

  mreg = mreg_lookup(ptrs[0], proc);
  assert(mreg != NULL);

  base   = mreg->slices[proc].base;
  extent = ((uint8_t*) base) + mreg->slices[proc].size;

  for (i = 1; i < count; i++)
    if ( !(ptrs[i] >= base && ptrs[i] < extent) )
      return 0;

  return 1;
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
  * @param[in] same_alloc  Do remote regions correspond to the same allocation?
  * @param[in] proc        Target process
  * @return                Zero on success, error code otherwise
  */
int ARMCII_Iov_op_dispatch(int op, void **src, void **dst, int count, int size,
    int datatype, int overlapping, int same_alloc, int proc) {

  int i;
  MPI_Datatype type;
  int type_count, type_size;

  if (op == ARMCII_OP_ACC) {
    ARMCII_Acc_type_translate(datatype, &type, &type_size);
    type_count = size/type_size;
    assert(size % type_size == 0);
  } else {
    type = MPI_BYTE;
    MPI_Type_size(type, &type_size);
    type_count = size/type_size;
    assert(size % type_size == 0);
  }

  // SAFE CASE: If remote pointers overlap or remote pointers correspond to
  // multiple allocations, use the safe implementation to avoid invalid MPI
  // use.

  if (overlapping || !same_alloc || ARMCII_GLOBAL_STATE.iov_method == ARMCII_IOV_SAFE) {
    if (overlapping) printf("Warning: IOV remote buffers overlap\n");
    if (!same_alloc) printf("Warning: IOV remote buffers are not within the same allocation\n");
    return ARMCII_Iov_op_safe(op, src, dst, count, type_count, type, proc);
  }

  // OPTIMIZED CASE: It's safe for us to issue all the operations under a
  // single lock.

  else if (ARMCII_GLOBAL_STATE.iov_method == ARMCII_IOV_DTYPE)
    return ARMCII_Iov_op_datatype(op, src, dst, count, type_count, type, proc);
  else
    return ARMCII_Iov_op_onelock(op, src, dst, count, type_count, type, proc);
}


/** Safe implementation of the ARMCI IOV operation
  */
int ARMCII_Iov_op_safe(int op, void **src, void **dst, int count, int elem_count,
    MPI_Datatype type, int proc) {
  
  int i;

  for (i = 0; i < count; i++) {
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
        mreg_put(mreg, src[i], dst[i], elem_count, proc);
        break;
      case ARMCII_OP_GET:
        mreg_get(mreg, src[i], dst[i], elem_count, proc);
        break;
      case ARMCII_OP_ACC:
        mreg_accumulate(mreg, src[i], dst[i], elem_count, type, proc);
        break;
      default:
        ARMCII_Error(__FILE__, __LINE__, __func__, "unknown operation", 100);
        return 1;
    }

    mreg_unlock(mreg, proc);
  }

  return 0;
}


/** Optimized implementation of the ARMCI IOV operation that uses a single
  * lock/unlock pair.
  */
int ARMCII_Iov_op_onelock(int op, void **src, void **dst, int count, int elem_count,
    MPI_Datatype type, int proc) {

  int i;
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
        mreg_put(mreg, src[i], dst[i], elem_count, proc);
        break;
      case ARMCII_OP_GET:
        mreg_get(mreg, src[i], dst[i], elem_count, proc);
        break;
      case ARMCII_OP_ACC:
        mreg_accumulate(mreg, src[i], dst[i], elem_count, type, proc);
        break;
      default:
        ARMCII_Error(__FILE__, __LINE__, __func__, "unknown operation", 100);
        return 1;
    }
  }

  mreg_unlock(mreg, proc);

  return 0;
}


/** Optimized implementation of the ARMCI IOV operation that uses an MPI
  * datatype to achieve a one-sided gather/scatter.
  */
int ARMCII_Iov_op_datatype(int op, void **src, void **dst, int count, int elem_count,
    MPI_Datatype type, int proc) {

    mem_region_t *mreg;
    MPI_Datatype  type_loc, type_rem;
    MPI_Aint      disp_loc[count];
    int           disp_rem[count];
    int           block_len[count];
    void         *dst_win_base;
    int           dst_win_size, i, type_size;
    void        **buf_rem, **buf_loc;
    MPI_Aint      base_rem;

    switch(op) {
      case ARMCII_OP_ACC:
      case ARMCII_OP_PUT:
        buf_rem = dst;
        buf_loc = src;
        break;
      case ARMCII_OP_GET:
        buf_rem = src;
        buf_loc = dst;
        break;
      default:
        ARMCII_Error(__FILE__, __LINE__, __func__, "unknown operation", 100);
        return 1;
    }

    MPI_Type_size(type, &type_size);

    mreg = mreg_lookup(buf_rem[0], proc);
    assert(mreg != NULL);

    dst_win_base = mreg->slices[proc].base;
    dst_win_size = mreg->slices[proc].size;

    MPI_Get_address(dst_win_base, &base_rem);

    for (i = 0; i < count; i++) {
      MPI_Aint target_rem;
      MPI_Get_address(buf_loc[i], &disp_loc[i]);
      MPI_Get_address(buf_rem[i], &target_rem);
      disp_rem[i]  = (target_rem - base_rem)/type_size;
      block_len[i] = elem_count;

      assert((target_rem - base_rem) % type_size == 0);
      assert(disp_rem[i] >= 0 && disp_rem[i] < dst_win_size);
      assert(((uint8_t*)buf_rem[i]) + block_len[i] <= ((uint8_t*)dst_win_base) + dst_win_size);
    }

    MPI_Type_create_hindexed(count, block_len, disp_loc, type, &type_loc);
    //MPI_Type_create_indexed_block(count, elem_count, disp_rem, type, &type_rem);
    MPI_Type_indexed(count, block_len, disp_rem, type, &type_rem);

    MPI_Type_commit(&type_loc);
    MPI_Type_commit(&type_rem);

    mreg_lock(mreg, proc);

    switch(op) {
      case ARMCII_OP_ACC:
        mreg_accumulate_typed(mreg, MPI_BOTTOM, 1, type_loc, MPI_BOTTOM, 1, type_rem, proc);
        break;
      case ARMCII_OP_PUT:
        mreg_put_typed(mreg, MPI_BOTTOM, 1, type_loc, MPI_BOTTOM, 1, type_rem, proc);
        break;
      case ARMCII_OP_GET:
        mreg_get_typed(mreg, MPI_BOTTOM, 1, type_rem, MPI_BOTTOM, 1, type_loc, proc);
        break;
      default:
        ARMCII_Error(__FILE__, __LINE__, __func__, "unknown operation", 100);
        return 1;
    }

    mreg_unlock(mreg, proc);

    MPI_Type_free(&type_loc);
    MPI_Type_free(&type_rem);

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
    int    overlapping, same_alloc;

    if (iov[v].ptr_array_len == 0) continue; // NOP //

    overlapping = ARMCII_Iov_check_dst_overlap(&iov[v]);
    same_alloc  = ARMCII_Iov_check_same_allocation(iov[v].dst_ptr_array, iov[v].ptr_array_len, proc);

    ARMCII_Buf_put_prepare(iov[v].src_ptr_array, &src_buf, iov[v].ptr_array_len, iov[v].bytes);
    ARMCII_Iov_op_dispatch(ARMCII_OP_PUT, src_buf, iov[v].dst_ptr_array, iov[v].ptr_array_len, iov[v].bytes, 0, overlapping, same_alloc, proc);
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
    int    overlapping, same_alloc;

    if (iov[v].ptr_array_len == 0) continue; // NOP //

    overlapping = ARMCII_Iov_check_src_overlap(&iov[v]);
    same_alloc  = ARMCII_Iov_check_same_allocation(iov[v].src_ptr_array, iov[v].ptr_array_len, proc);

    ARMCII_Buf_get_prepare(iov[v].dst_ptr_array, &dst_buf, iov[v].ptr_array_len, iov[v].bytes);
    ARMCII_Iov_op_dispatch(ARMCII_OP_GET, iov[v].src_ptr_array, dst_buf, iov[v].ptr_array_len, iov[v].bytes, 0, overlapping, same_alloc, proc);
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
    int    overlapping, same_alloc;

    if (iov[v].ptr_array_len == 0) continue; // NOP //

    overlapping = ARMCII_Iov_check_dst_overlap(&iov[v]);
    same_alloc  = ARMCII_Iov_check_same_allocation(iov[v].dst_ptr_array, iov[v].ptr_array_len, proc);

    ARMCII_Buf_acc_prepare(iov[v].src_ptr_array, &src_buf, iov[v].ptr_array_len, iov[v].bytes, datatype, scale);
    ARMCII_Iov_op_dispatch(ARMCII_OP_ACC, src_buf, iov[v].dst_ptr_array, iov[v].ptr_array_len, iov[v].bytes, datatype, overlapping, same_alloc, proc);
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


