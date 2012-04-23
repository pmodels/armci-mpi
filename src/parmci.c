/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

/* If no weak symbols support */
#if !defined(HAVE_PRAGMA_WEAK) && !defined(HAVE_PRAGMA_HP_SEC_DEF) && !defined(HAVE_PRAGMA_CRI_DUP)

#include "armci.h"

int ARMCI_Init(void) {
  return PARMCI_Init();
}

int ARMCI_Init_args(int *argc, char ***argv) {
  return PARMCI_Init_args(argc, argv);
}

int ARMCI_Initialized(void) {
  return PARMCI_Initialized();
}

int ARMCI_Finalize(void) {
  return PARMCI_Finalize();
}

int ARMCI_Malloc(void **base_ptrs, armci_size_t size) {
  return PARMCI_Malloc(base_ptrs, size);
}

int ARMCI_Free(void *ptr) {
  return PARMCI_Free(ptr);
}

void *ARMCI_Malloc_local(armci_size_t size) {
  return PARMCI_Malloc_local(size);
}

int ARMCI_Free_local(void *ptr) {
  return PARMCI_Free_local(ptr);
}

void ARMCI_Barrier(void) {
  return PARMCI_Barrier();
}

void ARMCI_Fence(int proc) {
  return PARMCI_Fence(proc);
}

void ARMCI_AllFence(void) {
  return PARMCI_AllFence();
}

void ARMCI_Access_begin(void *ptr) {
  return PARMCI_Access_begin(ptr);
}

void ARMCI_Access_end(void *ptr) {
  return PARMCI_Access_end(ptr);
}

int ARMCI_Get(void *src, void *dst, int size, int target) {
  return PARMCI_Get(src, dst, size, target);
}

int ARMCI_Put(void *src, void *dst, int size, int target) {
  return PARMCI_Put(src, dst, size, target);
}

int ARMCI_Acc(int datatype, void *scale, void *src, void *dst, int bytes, int proc) {
  return PARMCI_Acc(datatype, scale, src, dst, bytes, proc);
}

int ARMCI_PutS(void *src_ptr, int src_stride_ar[], void *dst_ptr, int dst_stride_ar[], int count[], int stride_levels, int proc) {
  return PARMCI_PutS(src_ptr, src_stride_ar, dst_ptr, dst_stride_ar, count, stride_levels, proc);
}

int ARMCI_GetS(void *src_ptr, int src_stride_ar[], void *dst_ptr, int dst_stride_ar[], int count[], int stride_levels, int proc) {
  return PARMCI_GetS(src_ptr, src_stride_ar, dst_ptr, dst_stride_ar, count, stride_levels, proc);
}

int ARMCI_AccS(int datatype, void *scale, void *src_ptr, int src_stride_ar[], void *dst_ptr, int dst_stride_ar[], int count[], int stride_levels, int proc) {
  return PARMCI_AccS(datatype, scale, src_ptr, src_stride_ar, dst_ptr, dst_stride_ar, count, stride_levels, proc);
}

int ARMCI_Put_flag(void *src, void* dst, int size, int *flag, int value, int proc) {
  return PARMCI_Put_flag(src, dst, size, flag, value, proc);
}

int ARMCI_PutS_flag(void *src_ptr, int src_stride_ar[], void *dst_ptr, int dst_stride_ar[], int count[], int stride_levels, int *flag, int value, int proc) {
  return PARMCI_PutS_flag(src_ptr, src_stride_ar, dst_ptr, dst_stride_ar, count, stride_levels, flag, value, proc);
}

int ARMCI_PutV(armci_giov_t *iov, int iov_len, int proc) {
  return PARMCI_PutV(iov, iov_len, proc);
}

int ARMCI_GetV(armci_giov_t *iov, int iov_len, int proc) {
  return PARMCI_GetV(iov, iov_len, proc);
}

int ARMCI_AccV(int datatype, void *scale, armci_giov_t *iov, int iov_len, int proc) {
  return PARMCI_AccV(datatype, scale, iov, iov_len, proc);
}

int ARMCI_Wait(armci_hdl_t* hdl) {
  return PARMCI_Wait(hdl);
}

int ARMCI_Test(armci_hdl_t* hdl) {
  return PARMCI_Test(hdl);
}

int ARMCI_WaitAll(void) {
  return PARMCI_WaitAll();
}

int ARMCI_NbPut(void *src, void *dst, int bytes, int proc, armci_hdl_t *hdl) {
  return PARMCI_NbPut(src, dst, bytes, proc, hdl);
}

int ARMCI_NbGet(void *src, void *dst, int bytes, int proc, armci_hdl_t *hdl) {
  return PARMCI_NbGet(src, dst, bytes, proc, hdl);
}

int ARMCI_NbAcc(int datatype, void *scale, void *src, void *dst, int bytes, int proc, armci_hdl_t *hdl) {
  return PARMCI_NbAcc(datatype, scale, src, dst, bytes, proc, hdl);
}

int ARMCI_NbPutS(void *src_ptr, int src_stride_ar[], void *dst_ptr, int dst_stride_ar[], int count[], int stride_levels, int proc, armci_hdl_t *hdl) {
  return PARMCI_NbPutS(src_ptr, src_stride_ar, dst_ptr, dst_stride_ar, count, stride_levels, proc, hdl);
}

int ARMCI_NbGetS(void *src_ptr, int src_stride_ar[], void *dst_ptr, int dst_stride_ar[], int count[], int stride_levels, int proc, armci_hdl_t *hdl) {
  return PARMCI_NbGetS(src_ptr, src_stride_ar, dst_ptr, dst_stride_ar, count, stride_levels, proc, hdl);
}

int ARMCI_NbAccS(int datatype, void *scale, void *src_ptr, int src_stride_ar[], void *dst_ptr, int dst_stride_ar[], int count[], int stride_levels, int proc, armci_hdl_t *hdl) {
  return PARMCI_NbAccS(datatype, scale, src_ptr, src_stride_ar, dst_ptr, dst_stride_ar, count, stride_levels, proc, hdl);
}

int ARMCI_NbPutV(armci_giov_t *iov, int iov_len, int proc, armci_hdl_t* handle) {
  return PARMCI_NbPutV(iov, iov_len, proc, handle);
}

int ARMCI_NbGetV(armci_giov_t *iov, int iov_len, int proc, armci_hdl_t* handle) {
  return PARMCI_NbGetV(iov, iov_len, proc, handle);
}

int ARMCI_NbAccV(int datatype, void *scale, armci_giov_t *iov, int iov_len, int proc, armci_hdl_t* handle) {
  return PARMCI_NbAccV(datatype, scale, iov, iov_len, proc, handle);
}

int ARMCI_PutValueInt(int src, void *dst, int proc) {
  return PARMCI_PutValueInt(src, dst, proc);
}

int ARMCI_PutValueLong(long src, void *dst, int proc) {
  return PARMCI_PutValueLong(src, dst, proc);
}

int ARMCI_PutValueFloat(float src, void *dst, int proc) {
  return PARMCI_PutValueFloat(src, dst, proc);
}

int ARMCI_PutValueDouble(double src, void *dst, int proc) {
  return PARMCI_PutValueDouble(src, dst, proc);
}

int ARMCI_NbPutValueInt(int src, void *dst, int proc, armci_hdl_t *hdl) {
  return PARMCI_NbPutValueInt(src, dst, proc, hdl);
}

int ARMCI_NbPutValueLong(long src, void *dst, int proc, armci_hdl_t *hdl) {
  return PARMCI_NbPutValueLong(src, dst, proc, hdl);
}

int ARMCI_NbPutValueFloat(float src, void *dst, int proc, armci_hdl_t *hdl) {
  return PARMCI_NbPutValueFloat(src, dst, proc, hdl);
}

int ARMCI_NbPutValueDouble(double src, void *dst, int proc, armci_hdl_t *hdl) {
  return PARMCI_NbPutValueDouble(src, dst, proc, hdl);
}

int ARMCI_GetValueInt(void *src, int proc) {
  return PARMCI_GetValueInt(src, proc);
}

long ARMCI_GetValueLong(void *src, int proc) {
  return PARMCI_GetValueLong(src, proc);
}

float ARMCI_GetValueFloat(void *src, int proc) {
  return PARMCI_GetValueFloat(src, proc);
}

double ARMCI_GetValueDouble(void *src, int proc) {
  return PARMCI_GetValueDouble(src, proc);
}

int ARMCI_Create_mutexes(int count) {
  return PARMCI_Create_mutexes(count);
}

int ARMCI_Destroy_mutexes(void) {
  return PARMCI_Destroy_mutexes();
}

void ARMCI_Lock(int mutex, int proc) {
  return PARMCI_Lock(mutex, proc);
}

void ARMCI_Unlock(int mutex, int proc) {
  return PARMCI_Unlock(mutex, proc);
}

int ARMCI_Rmw(int op, void *ploc, void *prem, int value, int proc) {
  return PARMCI_Rmw(op, ploc, prem, value, proc);
}

void armci_msg_barrier(void) {
  return parmci_msg_barrier();
}

void armci_msg_group_barrier(ARMCI_Group *group) {
  return parmci_msg_group_barrier(group);
}

#endif
