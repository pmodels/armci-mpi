/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#ifndef _ARMCI_H_
#define _ARMCI_H_

#include <mpi.h>

#define ARMCI_MPI 3

enum  ARMCI_Acc_e { ARMCI_ACC_INT /*     int */, ARMCI_ACC_LNG /*           long */,
                    ARMCI_ACC_FLT /*   float */, ARMCI_ACC_DBL /*         double */,
                    ARMCI_ACC_CPL /* complex */, ARMCI_ACC_DCP /* double complex */ };

typedef long armci_size_t;
#define ARMCII_MPI_SIZE_T MPI_LONG

int   ARMCI_Init(void);
int   ARMCI_Init_args(int *argc, char ***argv);
int   ARMCI_Init_thread(int armci_requested);
int   ARMCI_Init_thread_comm(int armci_requested, MPI_Comm comm);
int   ARMCI_Init_mpi_comm(MPI_Comm comm);
int   ARMCI_Initialized(void);

int   ARMCI_Finalize(void);
void  ARMCI_Cleanup(void);

void  ARMCI_Error(const char *msg, int code);

int   ARMCI_Malloc(void **base_ptrs, armci_size_t size);
int   ARMCI_Free(void *ptr);

void *ARMCI_Malloc_local(armci_size_t size);
int   ARMCI_Free_local(void *ptr);

void  ARMCI_Barrier(void);
void  ARMCI_Fence(int proc);
void  ARMCI_AllFence(void);

void  ARMCI_Access_begin(void *ptr); /* NEW API */
void  ARMCI_Access_end(void *ptr);   /* NEW API */

void  ARMCI_Copy(void *src, void *dst, int size);

int   ARMCI_Get(void *src, void *dst, int size, int target);
int   ARMCI_Put(void *src, void *dst, int size, int target);
int   ARMCI_Acc(int datatype, void *scale, void *src, void *dst, int bytes, int proc);

int   ARMCI_PutS(void *src_ptr, int src_stride_ar[/*stride_levels*/],
                 void *dst_ptr, int dst_stride_ar[/*stride_levels*/],
                 int count[/*stride_levels+1*/], int stride_levels, int proc);
int   ARMCI_GetS(void *src_ptr, int src_stride_ar[/*stride_levels*/],
                 void *dst_ptr, int dst_stride_ar[/*stride_levels*/],
                 int count[/*stride_levels+1*/], int stride_levels, int proc);
int   ARMCI_AccS(int datatype, void *scale,
                 void *src_ptr, int src_stride_ar[/*stride_levels*/],
                 void *dst_ptr, int dst_stride_ar[/*stride_levels*/],
                 int count[/*stride_levels+1*/], int stride_levels, int proc);

int   ARMCI_Put_flag(void *src, void* dst, int size, int *flag, int value, int proc);
int   ARMCI_PutS_flag(void *src_ptr, int src_stride_ar[/*stride_levels*/],
                 void *dst_ptr, int dst_stride_ar[/*stride_levels*/],
                 int count[/*stride_levels+1*/], int stride_levels,
                 int *flag, int value, int proc);


typedef struct armci_hdl_s
{
    int target;    /* we do not actually support individual completion */
    int aggregate;
}
armci_hdl_t;

void  ARMCI_INIT_HANDLE(armci_hdl_t *hdl);
void  ARMCI_SET_AGGREGATE_HANDLE(armci_hdl_t* handle);
void  ARMCI_UNSET_AGGREGATE_HANDLE(armci_hdl_t* handle);

int   ARMCI_NbPut(void *src, void *dst, int bytes, int proc, armci_hdl_t *hdl);
int   ARMCI_NbGet(void *src, void *dst, int bytes, int proc, armci_hdl_t *hdl);
int   ARMCI_NbAcc(int datatype, void *scale, void *src, void *dst, int bytes, int proc, armci_hdl_t *hdl);

int   ARMCI_Wait(armci_hdl_t* hdl);
int   ARMCI_Test(armci_hdl_t* hdl);
int   ARMCI_WaitProc(int proc);
int   ARMCI_WaitAll(void);

int   ARMCI_NbPutS(void *src_ptr, int src_stride_ar[/*stride_levels*/],
                   void *dst_ptr, int dst_stride_ar[/*stride_levels*/],
                   int count[/*stride_levels+1*/], int stride_levels, int proc, armci_hdl_t *hdl);
int   ARMCI_NbGetS(void *src_ptr, int src_stride_ar[/*stride_levels*/],
                   void *dst_ptr, int dst_stride_ar[/*stride_levels*/],
                   int count[/*stride_levels+1*/], int stride_levels, int proc, armci_hdl_t *hdl);
int   ARMCI_NbAccS(int datatype, void *scale,
                   void *src_ptr, int src_stride_ar[/*stride_levels*/],
                   void *dst_ptr, int dst_stride_ar[/*stride_levels*/],
                   int count[/*stride_levels+1*/], int stride_levels, int proc, armci_hdl_t *hdl);

void armci_write_strided(void *ptr, int stride_levels, int stride_arr[], int count[], char *buf);
void armci_read_strided(void *ptr, int stride_levels, int stride_arr[], int count[], char *buf);


/** Generalized I/O Vector operations.
  */

typedef struct {
  void **src_ptr_array;  // Source starting addresses of each data segment.
  void **dst_ptr_array;  // Destination starting addresses of each data segment.
  int    bytes;          // The length of each segment in bytes.
  int    ptr_array_len;  // Number of data segment.
} armci_giov_t;

int ARMCI_PutV(armci_giov_t *iov, int iov_len, int proc);
int ARMCI_GetV(armci_giov_t *iov, int iov_len, int proc);
int ARMCI_AccV(int datatype, void *scale, armci_giov_t *iov, int iov_len, int proc);

int ARMCI_NbPutV(armci_giov_t *iov, int iov_len, int proc, armci_hdl_t* handle);
int ARMCI_NbGetV(armci_giov_t *iov, int iov_len, int proc, armci_hdl_t* handle);
int ARMCI_NbAccV(int datatype, void *scale, armci_giov_t *iov, int iov_len, int proc, armci_hdl_t* handle);


/** Scalar/value operations.
  */

int ARMCI_PutValueInt(int src, void *dst, int proc);
int ARMCI_PutValueLong(long src, void *dst, int proc);
int ARMCI_PutValueFloat(float src, void *dst, int proc);
int ARMCI_PutValueDouble(double src, void *dst, int proc);

int ARMCI_NbPutValueInt(int src, void *dst, int proc, armci_hdl_t *hdl);
int ARMCI_NbPutValueLong(long src, void *dst, int proc, armci_hdl_t *hdl);
int ARMCI_NbPutValueFloat(float src, void *dst, int proc, armci_hdl_t *hdl);
int ARMCI_NbPutValueDouble(double src, void *dst, int proc, armci_hdl_t *hdl);

int    ARMCI_GetValueInt(void *src, int proc);
long   ARMCI_GetValueLong(void *src, int proc);
float  ARMCI_GetValueFloat(void *src, int proc);
double ARMCI_GetValueDouble(void *src, int proc);


/** Mutexes
  */

int   ARMCI_Create_mutexes(int count);
int   ARMCI_Destroy_mutexes(void);
void  ARMCI_Lock(int mutex, int proc);
void  ARMCI_Unlock(int mutex, int proc);

/** ARMCI Read-Modify-Write API
  */

enum ARMCI_Rmw_e { ARMCI_FETCH_AND_ADD, ARMCI_FETCH_AND_ADD_LONG,
                   ARMCI_SWAP, ARMCI_SWAP_LONG };

int ARMCI_Rmw(int op, void *ploc, void *prem, int value, int proc);

/** ARMCI Groups API
  */

typedef struct {
  MPI_Comm  comm;
  MPI_Comm  noncoll_pgroup_comm;
  int      *grp_to_abs;
  int      *abs_to_grp;
  int       rank;
  int       size;
} ARMCI_Group;

void ARMCI_Group_create(int grp_size, int *pid_list, ARMCI_Group *group_out);
void ARMCI_Group_create_child(int grp_size, int *pid_list, ARMCI_Group *group_out, ARMCI_Group *group_parent);
void ARMCI_Group_free(ARMCI_Group *group);

int  ARMCI_Group_rank(ARMCI_Group *group, int *rank);
void ARMCI_Group_size(ARMCI_Group *group, int *size);

void ARMCI_Group_set_default(ARMCI_Group *group);
void ARMCI_Group_get_default(ARMCI_Group *group_out);
void ARMCI_Group_get_world(ARMCI_Group *group_out);

int ARMCI_Absolute_id(ARMCI_Group *group,int group_rank);

int ARMCI_Malloc_group(void **ptr_arr, armci_size_t bytes, ARMCI_Group *group);
int ARMCI_Free_group(void *ptr, ARMCI_Group *group);

/** ARMCI Message API is in another file:
  */

#include <message.h>

/** Topology API
  */

enum armci_domain_e { ARMCI_DOMAIN_SMP };

typedef int armci_domain_t;

int armci_domain_nprocs(armci_domain_t domain, int id);
int armci_domain_id(armci_domain_t domain, int glob_proc_id);
int armci_domain_glob_proc_id(armci_domain_t domain, int id, int loc_proc_id);
int armci_domain_my_id(armci_domain_t domain);
int armci_domain_count(armci_domain_t domain);
int armci_domain_same_id(armci_domain_t domain, int proc);

int ARMCI_Same_node(int proc);

/** Odds and ends
  */

int  ARMCI_Uses_shm(void);
void ARMCI_Set_shm_limit(unsigned long shmemlimit);
int  ARMCI_Uses_shm_grp(ARMCI_Group *group);

/** PARMCI -- Profiling Interface
  */

int     PARMCI_Init(void);
int     PARMCI_Init_args(int *argc, char ***argv);
int     PARMCI_Init_thread(int armci_requested);
int     PARMCI_Init_thread_comm(int armci_requested, MPI_Comm comm);
int     PARMCI_Init_mpi_comm(MPI_Comm comm);
int     PARMCI_Initialized(void);
int     PARMCI_Finalize(void);

int     PARMCI_Malloc(void **base_ptrs, armci_size_t size);
int     PARMCI_Free(void *ptr);
void   *PARMCI_Malloc_local(armci_size_t size);
int     PARMCI_Free_local(void *ptr);

void    PARMCI_Barrier(void);
void    PARMCI_Fence(int proc);
void    PARMCI_AllFence(void);
void    PARMCI_Access_begin(void *ptr);
void    PARMCI_Access_end(void *ptr);

int     PARMCI_Get(void *src, void *dst, int size, int target);
int     PARMCI_Put(void *src, void *dst, int size, int target);
int     PARMCI_Acc(int datatype, void *scale, void *src, void *dst, int bytes, int proc);

int     PARMCI_PutS(void *src_ptr, int src_stride_ar[], void *dst_ptr, int dst_stride_ar[],
                 int count[], int stride_levels, int proc);
int     PARMCI_GetS(void *src_ptr, int src_stride_ar[], void *dst_ptr, int dst_stride_ar[],
                 int count[], int stride_levels, int proc);
int     PARMCI_AccS(int datatype, void *scale, void *src_ptr, int src_stride_ar[],
                 void *dst_ptr, int dst_stride_ar[], int count[], int stride_levels, int proc);
int     PARMCI_Put_flag(void *src, void* dst, int size, int *flag, int value, int proc);
int     PARMCI_PutS_flag(void *src_ptr, int src_stride_ar[], void *dst_ptr, int dst_stride_ar[],
                 int count[], int stride_levels, int *flag, int value, int proc);

int     PARMCI_PutV(armci_giov_t *iov, int iov_len, int proc);
int     PARMCI_GetV(armci_giov_t *iov, int iov_len, int proc);
int     PARMCI_AccV(int datatype, void *scale, armci_giov_t *iov, int iov_len, int proc);

int     PARMCI_Wait(armci_hdl_t* hdl);
int     PARMCI_Test(armci_hdl_t* hdl);
int     PARMCI_WaitProc(int proc);
int     PARMCI_WaitAll(void);

int     PARMCI_NbPut(void *src, void *dst, int bytes, int proc, armci_hdl_t *hdl);
int     PARMCI_NbGet(void *src, void *dst, int bytes, int proc, armci_hdl_t *hdl);
int     PARMCI_NbAcc(int datatype, void *scale, void *src, void *dst, int bytes, int proc, armci_hdl_t *hdl);
int     PARMCI_NbPutS(void *src_ptr, int src_stride_ar[], void *dst_ptr, int dst_stride_ar[],
                   int count[], int stride_levels, int proc, armci_hdl_t *hdl);
int     PARMCI_NbGetS(void *src_ptr, int src_stride_ar[], void *dst_ptr, int dst_stride_ar[],
                   int count[], int stride_levels, int proc, armci_hdl_t *hdl);
int     PARMCI_NbAccS(int datatype, void *scale, void *src_ptr, int src_stride_ar[],
                   void *dst_ptr, int dst_stride_ar[], int count[], int stride_levels, int proc, armci_hdl_t *hdl);
int     PARMCI_NbPutV(armci_giov_t *iov, int iov_len, int proc, armci_hdl_t* handle);
int     PARMCI_NbGetV(armci_giov_t *iov, int iov_len, int proc, armci_hdl_t* handle);
int     PARMCI_NbAccV(int datatype, void *scale, armci_giov_t *iov, int iov_len, int proc, armci_hdl_t* handle);

int     PARMCI_PutValueInt(int src, void *dst, int proc);
int     PARMCI_PutValueLong(long src, void *dst, int proc);
int     PARMCI_PutValueFloat(float src, void *dst, int proc);
int     PARMCI_PutValueDouble(double src, void *dst, int proc);
int     PARMCI_NbPutValueInt(int src, void *dst, int proc, armci_hdl_t *hdl);
int     PARMCI_NbPutValueLong(long src, void *dst, int proc, armci_hdl_t *hdl);
int     PARMCI_NbPutValueFloat(float src, void *dst, int proc, armci_hdl_t *hdl);
int     PARMCI_NbPutValueDouble(double src, void *dst, int proc, armci_hdl_t *hdl);

int     PARMCI_GetValueInt(void *src, int proc);
long    PARMCI_GetValueLong(void *src, int proc);
float   PARMCI_GetValueFloat(void *src, int proc);
double  PARMCI_GetValueDouble(void *src, int proc);

int     PARMCI_Create_mutexes(int count);
int     PARMCI_Destroy_mutexes(void);
void    PARMCI_Lock(int mutex, int proc);
void    PARMCI_Unlock(int mutex, int proc);
int     PARMCI_Rmw(int op, void *ploc, void *prem, int value, int proc);

void    parmci_msg_barrier(void);
void    parmci_msg_group_barrier(ARMCI_Group *group);

/* new memdev related to SICM but undocumented */

int ARMCI_Malloc_memdev(void **ptr_arr, armci_size_t bytes, const char* device);
int ARMCI_Malloc_group_memdev(void **ptr_arr, armci_size_t bytes, ARMCI_Group *group, const char *device);
int ARMCI_Free_memdev(void *ptr);

int PARMCI_Malloc_memdev(void **ptr_arr, armci_size_t bytes, const char* device);
int PARMCI_Malloc_group_memdev(void **ptr_arr, armci_size_t bytes, ARMCI_Group *group, const char *device);
int PARMCI_Free_memdev(void *ptr);

#endif /* _ARMCI_H_ */
