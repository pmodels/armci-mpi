/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#if defined(HAVE_PRAGMA_WEAK)
#pragma weak ARMCI_Init = PARMCI_Init
#pragma weak ARMCI_Init_args = PARMCI_Init_args
#pragma weak ARMCI_Initialized = PARMCI_Initialized
#pragma weak ARMCI_Finalize = PARMCI_Finalize
#pragma weak ARMCI_Malloc = PARMCI_Malloc
#pragma weak ARMCI_Free = PARMCI_Free
#pragma weak ARMCI_Malloc_local = PARMCI_Malloc_local
#pragma weak ARMCI_Free_local = PARMCI_Free_local
#pragma weak ARMCI_Barrier = PARMCI_Barrier
#pragma weak ARMCI_Fence = PARMCI_Fence
#pragma weak ARMCI_AllFence = PARMCI_AllFence
#pragma weak ARMCI_Access_begin = PARMCI_Access_begin
#pragma weak ARMCI_Access_end = PARMCI_Access_end
#pragma weak ARMCI_Get = PARMCI_Get
#pragma weak ARMCI_Put = PARMCI_Put
#pragma weak ARMCI_Acc = PARMCI_Acc
#pragma weak ARMCI_PutS = PARMCI_PutS
#pragma weak ARMCI_GetS = PARMCI_GetS
#pragma weak ARMCI_AccS = PARMCI_AccS
#pragma weak ARMCI_Put_flag = PARMCI_Put_flag
#pragma weak ARMCI_PutS_flag = PARMCI_PutS_flag
#pragma weak ARMCI_PutV = PARMCI_PutV
#pragma weak ARMCI_GetV = PARMCI_GetV
#pragma weak ARMCI_AccV = PARMCI_AccV
#pragma weak ARMCI_Wait = PARMCI_Wait
#pragma weak ARMCI_Test = PARMCI_Test
#pragma weak ARMCI_WaitAll = PARMCI_WaitAll
#pragma weak ARMCI_NbPut = PARMCI_NbPut
#pragma weak ARMCI_NbGet = PARMCI_NbGet
#pragma weak ARMCI_NbAcc = PARMCI_NbAcc
#pragma weak ARMCI_NbPutS = PARMCI_NbPutS
#pragma weak ARMCI_NbGetS = PARMCI_NbGetS
#pragma weak ARMCI_NbAccS = PARMCI_NbAccS
#pragma weak ARMCI_NbPutV = PARMCI_NbPutV
#pragma weak ARMCI_NbGetV = PARMCI_NbGetV
#pragma weak ARMCI_NbAccV = PARMCI_NbAccV
#pragma weak ARMCI_PutValueInt = PARMCI_PutValueInt
#pragma weak ARMCI_PutValueLong = PARMCI_PutValueLong
#pragma weak ARMCI_PutValueFloat = PARMCI_PutValueFloat
#pragma weak ARMCI_PutValueDouble = PARMCI_PutValueDouble
#pragma weak ARMCI_NbPutValueInt = PARMCI_NbPutValueInt
#pragma weak ARMCI_NbPutValueLong = PARMCI_NbPutValueLong
#pragma weak ARMCI_NbPutValueFloat = PARMCI_NbPutValueFloat
#pragma weak ARMCI_NbPutValueDouble = PARMCI_NbPutValueDouble
#pragma weak ARMCI_GetValueInt = PARMCI_GetValueInt
#pragma weak ARMCI_GetValueLong = PARMCI_GetValueLong
#pragma weak ARMCI_GetValueFloat = PARMCI_GetValueFloat
#pragma weak ARMCI_GetValueDouble = PARMCI_GetValueDouble
#pragma weak ARMCI_Create_mutexes = PARMCI_Create_mutexes
#pragma weak ARMCI_Destroy_mutexes = PARMCI_Destroy_mutexes
#pragma weak ARMCI_Lock = PARMCI_Lock
#pragma weak ARMCI_Unlock = PARMCI_Unlock
#pragma weak ARMCI_Rmw = PARMCI_Rmw
#pragma weak armci_msg_barrier = Parmci_msg_barrier
#pragma weak armci_msg_group_barrier = Parmci_msg_group_barrier

#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF PARMCI_Init ARMCI_Init
#pragma _HP_SECONDARY_DEF PARMCI_Init_args ARMCI_Init_args
#pragma _HP_SECONDARY_DEF PARMCI_Initialized ARMCI_Initialized
#pragma _HP_SECONDARY_DEF PARMCI_Finalize ARMCI_Finalize
#pragma _HP_SECONDARY_DEF PARMCI_Malloc ARMCI_Malloc
#pragma _HP_SECONDARY_DEF PARMCI_Free ARMCI_Free
#pragma _HP_SECONDARY_DEF PARMCI_Malloc_local ARMCI_Malloc_local
#pragma _HP_SECONDARY_DEF PARMCI_Free_local ARMCI_Free_local
#pragma _HP_SECONDARY_DEF PARMCI_Barrier ARMCI_Barrier
#pragma _HP_SECONDARY_DEF PARMCI_Fence ARMCI_Fence
#pragma _HP_SECONDARY_DEF PARMCI_AllFence ARMCI_AllFence
#pragma _HP_SECONDARY_DEF PARMCI_Access_begin ARMCI_Access_begin
#pragma _HP_SECONDARY_DEF PARMCI_Access_end ARMCI_Access_end
#pragma _HP_SECONDARY_DEF PARMCI_Get ARMCI_Get
#pragma _HP_SECONDARY_DEF PARMCI_Put ARMCI_Put
#pragma _HP_SECONDARY_DEF PARMCI_Acc ARMCI_Acc
#pragma _HP_SECONDARY_DEF PARMCI_PutS ARMCI_PutS
#pragma _HP_SECONDARY_DEF PARMCI_GetS ARMCI_GetS
#pragma _HP_SECONDARY_DEF PARMCI_AccS ARMCI_AccS
#pragma _HP_SECONDARY_DEF PARMCI_Put_flag ARMCI_Put_flag
#pragma _HP_SECONDARY_DEF PARMCI_PutS_flag ARMCI_PutS_flag
#pragma _HP_SECONDARY_DEF PARMCI_PutV ARMCI_PutV
#pragma _HP_SECONDARY_DEF PARMCI_GetV ARMCI_GetV
#pragma _HP_SECONDARY_DEF PARMCI_AccV ARMCI_AccV
#pragma _HP_SECONDARY_DEF PARMCI_Wait ARMCI_Wait
#pragma _HP_SECONDARY_DEF PARMCI_Test ARMCI_Test
#pragma _HP_SECONDARY_DEF PARMCI_WaitAll ARMCI_WaitAll
#pragma _HP_SECONDARY_DEF PARMCI_NbPut ARMCI_NbPut
#pragma _HP_SECONDARY_DEF PARMCI_NbGet ARMCI_NbGet
#pragma _HP_SECONDARY_DEF PARMCI_NbAcc ARMCI_NbAcc
#pragma _HP_SECONDARY_DEF PARMCI_NbPutS ARMCI_NbPutS
#pragma _HP_SECONDARY_DEF PARMCI_NbGetS ARMCI_NbGetS
#pragma _HP_SECONDARY_DEF PARMCI_NbAccS ARMCI_NbAccS
#pragma _HP_SECONDARY_DEF PARMCI_NbPutV ARMCI_NbPutV
#pragma _HP_SECONDARY_DEF PARMCI_NbGetV ARMCI_NbGetV
#pragma _HP_SECONDARY_DEF PARMCI_NbAccV ARMCI_NbAccV
#pragma _HP_SECONDARY_DEF PARMCI_PutValueInt ARMCI_PutValueInt
#pragma _HP_SECONDARY_DEF PARMCI_PutValueLong ARMCI_PutValueLong
#pragma _HP_SECONDARY_DEF PARMCI_PutValueFloat ARMCI_PutValueFloat
#pragma _HP_SECONDARY_DEF PARMCI_PutValueDouble ARMCI_PutValueDouble
#pragma _HP_SECONDARY_DEF PARMCI_NbPutValueInt ARMCI_NbPutValueInt
#pragma _HP_SECONDARY_DEF PARMCI_NbPutValueLong ARMCI_NbPutValueLong
#pragma _HP_SECONDARY_DEF PARMCI_NbPutValueFloat ARMCI_NbPutValueFloat
#pragma _HP_SECONDARY_DEF PARMCI_NbPutValueDouble ARMCI_NbPutValueDouble
#pragma _HP_SECONDARY_DEF PARMCI_GetValueInt ARMCI_GetValueInt
#pragma _HP_SECONDARY_DEF PARMCI_GetValueLong ARMCI_GetValueLong
#pragma _HP_SECONDARY_DEF PARMCI_GetValueFloat ARMCI_GetValueFloat
#pragma _HP_SECONDARY_DEF PARMCI_GetValueDouble ARMCI_GetValueDouble
#pragma _HP_SECONDARY_DEF PARMCI_Create_mutexes ARMCI_Create_mutexes
#pragma _HP_SECONDARY_DEF PARMCI_Destroy_mutexes ARMCI_Destroy_mutexes
#pragma _HP_SECONDARY_DEF PARMCI_Lock ARMCI_Lock
#pragma _HP_SECONDARY_DEF PARMCI_Unlock ARMCI_Unlock
#pragma _HP_SECONDARY_DEF PARMCI_Rmw ARMCI_Rmw
#pragma _HP_SECONDARY_DEF Parmci_msg_barrier armci_msg_barrier
#pragma _HP_SECONDARY_DEF Parmci_msg_group_barrier armci_msg_group_barrier

#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate ARMCI_Init as PARMCI_Init
#pragma _CRI duplicate ARMCI_Init_args as PARMCI_Init_args
#pragma _CRI duplicate ARMCI_Initialized as PARMCI_Initialized
#pragma _CRI duplicate ARMCI_Finalize as PARMCI_Finalize
#pragma _CRI duplicate ARMCI_Malloc as PARMCI_Malloc
#pragma _CRI duplicate ARMCI_Free as PARMCI_Free
#pragma _CRI duplicate ARMCI_Malloc_local as PARMCI_Malloc_local
#pragma _CRI duplicate ARMCI_Free_local as PARMCI_Free_local
#pragma _CRI duplicate ARMCI_Barrier as PARMCI_Barrier
#pragma _CRI duplicate ARMCI_Fence as PARMCI_Fence
#pragma _CRI duplicate ARMCI_AllFence as PARMCI_AllFence
#pragma _CRI duplicate ARMCI_Access_begin as PARMCI_Access_begin
#pragma _CRI duplicate ARMCI_Access_end as PARMCI_Access_end
#pragma _CRI duplicate ARMCI_Get as PARMCI_Get
#pragma _CRI duplicate ARMCI_Put as PARMCI_Put
#pragma _CRI duplicate ARMCI_Acc as PARMCI_Acc
#pragma _CRI duplicate ARMCI_PutS as PARMCI_PutS
#pragma _CRI duplicate ARMCI_GetS as PARMCI_GetS
#pragma _CRI duplicate ARMCI_AccS as PARMCI_AccS
#pragma _CRI duplicate ARMCI_Put_flag as PARMCI_Put_flag
#pragma _CRI duplicate ARMCI_PutS_flag as PARMCI_PutS_flag
#pragma _CRI duplicate ARMCI_PutV as PARMCI_PutV
#pragma _CRI duplicate ARMCI_GetV as PARMCI_GetV
#pragma _CRI duplicate ARMCI_AccV as PARMCI_AccV
#pragma _CRI duplicate ARMCI_Wait as PARMCI_Wait
#pragma _CRI duplicate ARMCI_Test as PARMCI_Test
#pragma _CRI duplicate ARMCI_WaitAll as PARMCI_WaitAll
#pragma _CRI duplicate ARMCI_NbPut as PARMCI_NbPut
#pragma _CRI duplicate ARMCI_NbGet as PARMCI_NbGet
#pragma _CRI duplicate ARMCI_NbAcc as PARMCI_NbAcc
#pragma _CRI duplicate ARMCI_NbPutS as PARMCI_NbPutS
#pragma _CRI duplicate ARMCI_NbGetS as PARMCI_NbGetS
#pragma _CRI duplicate ARMCI_NbAccS as PARMCI_NbAccS
#pragma _CRI duplicate ARMCI_NbPutV as PARMCI_NbPutV
#pragma _CRI duplicate ARMCI_NbGetV as PARMCI_NbGetV
#pragma _CRI duplicate ARMCI_NbAccV as PARMCI_NbAccV
#pragma _CRI duplicate ARMCI_PutValueInt as PARMCI_PutValueInt
#pragma _CRI duplicate ARMCI_PutValueLong as PARMCI_PutValueLong
#pragma _CRI duplicate ARMCI_PutValueFloat as PARMCI_PutValueFloat
#pragma _CRI duplicate ARMCI_PutValueDouble as PARMCI_PutValueDouble
#pragma _CRI duplicate ARMCI_NbPutValueInt as PARMCI_NbPutValueInt
#pragma _CRI duplicate ARMCI_NbPutValueLong as PARMCI_NbPutValueLong
#pragma _CRI duplicate ARMCI_NbPutValueFloat as PARMCI_NbPutValueFloat
#pragma _CRI duplicate ARMCI_NbPutValueDouble as PARMCI_NbPutValueDouble
#pragma _CRI duplicate ARMCI_GetValueInt as PARMCI_GetValueInt
#pragma _CRI duplicate ARMCI_GetValueLong as PARMCI_GetValueLong
#pragma _CRI duplicate ARMCI_GetValueFloat as PARMCI_GetValueFloat
#pragma _CRI duplicate ARMCI_GetValueDouble as PARMCI_GetValueDouble
#pragma _CRI duplicate ARMCI_Create_mutexes as PARMCI_Create_mutexes
#pragma _CRI duplicate ARMCI_Destroy_mutexes as PARMCI_Destroy_mutexes
#pragma _CRI duplicate ARMCI_Lock as PARMCI_Lock
#pragma _CRI duplicate ARMCI_Unlock as PARMCI_Unlock
#pragma _CRI duplicate ARMCI_Rmw as PARMCI_Rmw
#pragma _CRI duplicate armci_msg_barrier as Parmci_msg_barrier
#pragma _CRI duplicate armci_msg_group_barrier as Parmci_msg_group_barrier

#else /* No weak symbols support */
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
