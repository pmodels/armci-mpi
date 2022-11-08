/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 * Copyright (C) 2022, NVIDIA.
 */

#include "armciconf.h"

#ifdef ENABLE_PROFILING

#include "armci.h"
#include "armci_internals.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>

/*******************************************************************************
 Design
 
 ARMCI_PROFILE=VERBOSE prints one line for every call, similar to MKL_VERBOSE=1.
 ARMCI_PROFILE=HISTOGRAM gathers a histogram of message sizes and latencies.
 ARMCI_PROFILE=BASIC collects aggregate bandwidth and timing data.

 Each profile is inclusive of the one below it.

 For histogram, for each RMA operation, we will have a 2D array with the
 following dimensions:
   1. message size in log_10 bins from count=1 to count 10^8 (9 bins)
   2. latency in log_10 bins from 10^-7 to 10^2 (10 bins)

*******************************************************************************/

#define ARMCI_SIZE_BINS  9
#define ARMCI_TIME_BINS 10

static inline int ARMCII_Get_size_bin(int size)
{
    int bin = 0;
    if (size > 0) {
        bin = (int)log10f((int)size);
        if (bin >= ARMCI_SIZE_BINS) {
          bin = ARMCI_SIZE_BINS - 1;
        }
    }
    return bin;
}

static inline int ARMCII_Get_time_bin(double time)
{
    int bin = 0;
    if (time > 0) {
        bin = 6 + (int)log10(time);
        if (bin < 0) {
            bin = 0;
        } else if (bin >= ARMCI_TIME_BINS) {
            bin = ARMCI_TIME_BINS - 1;
        }
    }
    return bin;
}

static inline int64_t ARMCII_Strided_count(int count[], int stride_levels)
{
    int64_t size = 1;
    for (int i=0; i<=stride_levels; i++) {
        size *= count[i];
    }
    return size;
}

static inline int64_t ARMCII_Vector_count(armci_giov_t * v)
{
    return (v->bytes * v->ptr_array_len);
}

typedef struct
{
    int64_t num_calls;
    int64_t histogram[ARMCI_SIZE_BINS][ARMCI_TIME_BINS];
    int64_t total_bytes;
    double  total_time;
} 
ARMCII_Histogram_s;

static void ARMCII_Print_histogram(FILE * f, char * name, ARMCII_Histogram_s * h)
{
    int64_t n = h->num_calls;
    int64_t b = h->total_bytes;
    double  t = h->total_time;
    fprintf(f,"%s called %"PRIi64" times. time per call = %lf. total bytes = %"PRIi64", bandwidth = %lf (GB/s)\n",
               name,     n,                               t/n,               b,                     1.e-9*b/t);
    for (int i=0; i<ARMCI_SIZE_BINS; i++) {
        for (int j=0; j<ARMCI_TIME_BINS; j++) {
            fprintf(f,"%"PRIi64" ", h->histogram[i][j]);
        }
        fprintf(f,"\n");
    }
}

typedef struct
{
    int64_t num_calls;
    double  total_time;
    double  min_time;
    double  max_time;
}
ARMCII_Statistics_s;

static void ARMCII_Print_statistics(FILE * f, char * name, ARMCII_Statistics_s * s)
{
    const int64_t n   = s->num_calls;
    const double  t   = s->total_time;
#if 0
    const double  min = s->min_time;
    const double  max = s->max_time;
    fprintf(f,"%s called %"PRIi64" times. time per call = %lf. min,max = %lf, %lf.\n", name, n, t/n, min, max);
#else
    fprintf(f,"%s called %"PRIi64" times. time per call = %lf.\n", name, n, t/n);
#endif
}

typedef struct
{
    ARMCII_Histogram_s put;         // includes C/S/V
    ARMCII_Histogram_s get;         // includes C/S/V
    ARMCII_Histogram_s acc;         // includes C/S/V
    ARMCII_Statistics_s rmw;
    ARMCII_Statistics_s barrier;    // includes msg barriers
    ARMCII_Statistics_s wait;       // includes waitall
    ARMCII_Statistics_s fence;      // includes allfence
    ARMCII_Statistics_s locks;      // includes unlock
    ARMCII_Statistics_s globmem;    // includes free
    ARMCII_Statistics_s localmem;   // includes free
}
ARMCII_Profiling_s;

ARMCII_Profiling_s profiling_state = { 0 };

static void ARMCII_Profile_init(void)
{
    int me;
    MPI_Comm_rank(ARMCI_GROUP_WORLD.comm, &me);
    if (me == 0) {
        printf("ARMCI-MPI profiling initiated\n");
        fflush(stdout);
    }
}

static void ARMCII_Profile_dump(void)
{
    const int hist_size = ARMCI_SIZE_BINS*ARMCI_TIME_BINS;
    const MPI_Comm comm = ARMCI_GROUP_WORLD.comm;
    int me;
    MPI_Comm_rank(comm, &me);

    ARMCII_Profiling_s aggregate_stats = { 0 };

    fflush(0);

    MPI_Barrier(comm);

    if (me==0) fprintf(stderr, "ARMCI-MPI profiling results:\n");

    // yeah, this is dumb, i could do a user-defined reduction but that is probably more code.

    MPI_Reduce(&profiling_state.put.num_calls,  &aggregate_stats.put.num_calls, 1, MPI_INT64_T, MPI_SUM, 0, comm);
    MPI_Reduce(&profiling_state.put.total_bytes,&aggregate_stats.put.total_bytes, 1, MPI_INT64_T, MPI_SUM, 0, comm);
    MPI_Reduce(&profiling_state.put.histogram,  &aggregate_stats.put.histogram, hist_size, MPI_INT64_T, MPI_SUM, 0, comm);
    MPI_Reduce(&profiling_state.put.total_time, &aggregate_stats.put.total_time, 1, MPI_DOUBLE, MPI_SUM, 0, comm);

    if (me==0) ARMCII_Print_histogram(stderr, "Put", &aggregate_stats.put);

    MPI_Reduce(&profiling_state.get.num_calls,  &aggregate_stats.get.num_calls, 1, MPI_INT64_T, MPI_SUM, 0, comm);
    MPI_Reduce(&profiling_state.get.total_bytes,&aggregate_stats.get.total_bytes, 1, MPI_INT64_T, MPI_SUM, 0, comm);
    MPI_Reduce(&profiling_state.get.histogram,  &aggregate_stats.get.histogram, hist_size, MPI_INT64_T, MPI_SUM, 0, comm);
    MPI_Reduce(&profiling_state.get.total_time, &aggregate_stats.get.total_time, 1, MPI_DOUBLE, MPI_SUM, 0, comm);

    if (me==0) ARMCII_Print_histogram(stderr, "Get", &aggregate_stats.get);

    MPI_Reduce(&profiling_state.acc.num_calls,  &aggregate_stats.acc.num_calls, 1, MPI_INT64_T, MPI_SUM, 0, comm);
    MPI_Reduce(&profiling_state.acc.total_bytes,&aggregate_stats.acc.total_bytes, 1, MPI_INT64_T, MPI_SUM, 0, comm);
    MPI_Reduce(&profiling_state.acc.histogram,  &aggregate_stats.acc.histogram, hist_size, MPI_INT64_T, MPI_SUM, 0, comm);
    MPI_Reduce(&profiling_state.acc.total_time, &aggregate_stats.acc.total_time, 1, MPI_DOUBLE, MPI_SUM, 0, comm);

    if (me==0) ARMCII_Print_histogram(stderr, "Acc", &aggregate_stats.acc);

    MPI_Reduce(&profiling_state.rmw.num_calls,  &aggregate_stats.rmw.num_calls, 1, MPI_INT64_T, MPI_SUM, 0, comm);
    MPI_Reduce(&profiling_state.rmw.total_time, &aggregate_stats.rmw.total_time, 1, MPI_DOUBLE, MPI_SUM, 0, comm);
    MPI_Reduce(&profiling_state.rmw.max_time, &aggregate_stats.rmw.max_time, 1, MPI_DOUBLE, MPI_MAX, 0, comm);
    MPI_Reduce(&profiling_state.rmw.min_time, &aggregate_stats.rmw.min_time, 1, MPI_DOUBLE, MPI_MIN, 0, comm);

    if (me==0) ARMCII_Print_statistics(stderr, "Rmw", &aggregate_stats.rmw);

    MPI_Reduce(&profiling_state.barrier.num_calls,  &aggregate_stats.barrier.num_calls, 1, MPI_INT64_T, MPI_SUM, 0, comm);
    MPI_Reduce(&profiling_state.barrier.total_time, &aggregate_stats.barrier.total_time, 1, MPI_DOUBLE, MPI_SUM, 0, comm);
    MPI_Reduce(&profiling_state.barrier.max_time, &aggregate_stats.barrier.max_time, 1, MPI_DOUBLE, MPI_MAX, 0, comm);
    MPI_Reduce(&profiling_state.barrier.min_time, &aggregate_stats.barrier.min_time, 1, MPI_DOUBLE, MPI_MIN, 0, comm);

    if (me==0) ARMCII_Print_statistics(stderr, "Barrier", &aggregate_stats.barrier);

    MPI_Reduce(&profiling_state.wait.num_calls,  &aggregate_stats.wait.num_calls, 1, MPI_INT64_T, MPI_SUM, 0, comm);
    MPI_Reduce(&profiling_state.wait.total_time, &aggregate_stats.wait.total_time, 1, MPI_DOUBLE, MPI_SUM, 0, comm);
    MPI_Reduce(&profiling_state.wait.max_time, &aggregate_stats.wait.max_time, 1, MPI_DOUBLE, MPI_MAX, 0, comm);
    MPI_Reduce(&profiling_state.wait.min_time, &aggregate_stats.wait.min_time, 1, MPI_DOUBLE, MPI_MIN, 0, comm);

    if (me==0) ARMCII_Print_statistics(stderr, "Wait", &aggregate_stats.wait);

    MPI_Reduce(&profiling_state.fence.num_calls,  &aggregate_stats.fence.num_calls, 1, MPI_INT64_T, MPI_SUM, 0, comm);
    MPI_Reduce(&profiling_state.fence.total_time, &aggregate_stats.fence.total_time, 1, MPI_DOUBLE, MPI_SUM, 0, comm);
    MPI_Reduce(&profiling_state.fence.max_time, &aggregate_stats.fence.max_time, 1, MPI_DOUBLE, MPI_MAX, 0, comm);
    MPI_Reduce(&profiling_state.fence.min_time, &aggregate_stats.fence.min_time, 1, MPI_DOUBLE, MPI_MIN, 0, comm);

    if (me==0) ARMCII_Print_statistics(stderr, "Fence", &aggregate_stats.fence);

    MPI_Reduce(&profiling_state.locks.num_calls,  &aggregate_stats.locks.num_calls, 1, MPI_INT64_T, MPI_SUM, 0, comm);
    MPI_Reduce(&profiling_state.locks.total_time, &aggregate_stats.locks.total_time, 1, MPI_DOUBLE, MPI_SUM, 0, comm);
    MPI_Reduce(&profiling_state.locks.max_time, &aggregate_stats.locks.max_time, 1, MPI_DOUBLE, MPI_MAX, 0, comm);
    MPI_Reduce(&profiling_state.locks.min_time, &aggregate_stats.locks.min_time, 1, MPI_DOUBLE, MPI_MIN, 0, comm);

    if (me==0) ARMCII_Print_statistics(stderr, "Locks", &aggregate_stats.locks);

    MPI_Reduce(&profiling_state.globmem.num_calls,  &aggregate_stats.globmem.num_calls, 1, MPI_INT64_T, MPI_SUM, 0, comm);
    MPI_Reduce(&profiling_state.globmem.total_time, &aggregate_stats.globmem.total_time, 1, MPI_DOUBLE, MPI_SUM, 0, comm);
    MPI_Reduce(&profiling_state.globmem.max_time, &aggregate_stats.globmem.max_time, 1, MPI_DOUBLE, MPI_MAX, 0, comm);
    MPI_Reduce(&profiling_state.globmem.min_time, &aggregate_stats.globmem.min_time, 1, MPI_DOUBLE, MPI_MIN, 0, comm);

    if (me==0) ARMCII_Print_statistics(stderr, "Global memory management", &aggregate_stats.globmem);

    MPI_Reduce(&profiling_state.localmem.num_calls,  &aggregate_stats.localmem.num_calls, 1, MPI_INT64_T, MPI_SUM, 0, comm);
    MPI_Reduce(&profiling_state.localmem.total_time, &aggregate_stats.localmem.total_time, 1, MPI_DOUBLE, MPI_SUM, 0, comm);
    MPI_Reduce(&profiling_state.localmem.max_time, &aggregate_stats.localmem.max_time, 1, MPI_DOUBLE, MPI_MAX, 0, comm);
    MPI_Reduce(&profiling_state.localmem.min_time, &aggregate_stats.localmem.min_time, 1, MPI_DOUBLE, MPI_MIN, 0, comm);

    if (me==0) ARMCII_Print_statistics(stderr, "Local memory management", &aggregate_stats.localmem);

    fflush(0);

    MPI_Barrier(comm);
}

int ARMCI_Init(void) {
  const int rc = PARMCI_Init();
  ARMCII_Profile_init();
  return rc;
}

int ARMCI_Init_args(int *argc, char ***argv) {
  const int rc = PARMCI_Init_args(argc, argv);
  ARMCII_Profile_init();
  return rc;
}

int ARMCI_Init_thread(int armci_requested) {
  const int rc = PARMCI_Init_thread(armci_requested);
  ARMCII_Profile_init();
  return rc;
}

int ARMCI_Init_thread_comm(int armci_requested, MPI_Comm comm) {
  const int rc = PARMCI_Init_thread_comm(armci_requested, comm);
  ARMCII_Profile_init();
  return rc;
}

int ARMCI_Finalize(void) {
  ARMCII_Profile_dump();
  return PARMCI_Finalize();
}

int ARMCI_Malloc(void **base_ptrs, armci_size_t size) {
  profiling_state.globmem.num_calls++;
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_Malloc(base_ptrs, size);
  const double t1 = MPI_Wtime();
  profiling_state.globmem.total_time += (t1-t0);
  return rc;
}

int ARMCI_Free(void *ptr) {
  profiling_state.globmem.num_calls++;
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_Free(ptr);
  const double t1 = MPI_Wtime();
  profiling_state.globmem.total_time += (t1-t0);
  return rc;
}

int ARMCI_Malloc_memdev(void **base_ptrs, armci_size_t size, const char *device) {
  profiling_state.globmem.num_calls++;
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_Malloc_memdev(base_ptrs, size, device);
  const double t1 = MPI_Wtime();
  profiling_state.globmem.total_time += (t1-t0);
  return rc;
}

int ARMCI_Malloc_group_memdev(void **base_ptrs, armci_size_t size, ARMCI_Group *group, const char *device) {
  profiling_state.globmem.num_calls++;
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_Malloc_group_memdev(base_ptrs, size, group, device);
  const double t1 = MPI_Wtime();
  profiling_state.globmem.total_time += (t1-t0);
  return rc;
}

int ARMCI_Free_memdev(void *ptr) {
  profiling_state.globmem.num_calls++;
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_Free_memdev(ptr);
  const double t1 = MPI_Wtime();
  profiling_state.globmem.total_time += (t1-t0);
  return rc;
}

void *ARMCI_Malloc_local(armci_size_t size) {
  profiling_state.localmem.num_calls++;
  const double t0 = MPI_Wtime();
  void * rc = PARMCI_Malloc_local(size);
  const double t1 = MPI_Wtime();
  profiling_state.localmem.total_time += (t1-t0);
  return rc;
}

int ARMCI_Free_local(void *ptr) {
  profiling_state.localmem.num_calls++;
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_Free_local(ptr);
  const double t1 = MPI_Wtime();
  profiling_state.localmem.total_time += (t1-t0);
  return rc;
}

void ARMCI_Barrier(void)
{
  profiling_state.barrier.num_calls++;
  const double t0 = MPI_Wtime();
  PARMCI_Barrier();
  const double t1 = MPI_Wtime();
  profiling_state.barrier.total_time += (t1-t0);
}

void ARMCI_Fence(int proc)
{
  profiling_state.fence.num_calls++;
  const double t0 = MPI_Wtime();
  PARMCI_Fence(proc);
  const double t1 = MPI_Wtime();
  profiling_state.fence.total_time += (t1-t0);
}

void ARMCI_AllFence(void)
{
  profiling_state.fence.num_calls++;
  const double t0 = MPI_Wtime();
  PARMCI_AllFence();
  const double t1 = MPI_Wtime();
  profiling_state.fence.total_time += (t1-t0);
}

void ARMCI_Access_begin(void *ptr) {
  PARMCI_Access_begin(ptr);
}

void ARMCI_Access_end(void *ptr) {
  PARMCI_Access_end(ptr);
}

int ARMCI_Get(void *src, void *dst, int size, int target) {
  profiling_state.get.num_calls++;
  profiling_state.get.total_bytes += size;
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_Get(src, dst, size, target);
  const double t1 = MPI_Wtime();
  const double time = t1-t0;
  profiling_state.get.total_time += time;
  const int size_bin = ARMCII_Get_size_bin(size);
  const int time_bin = ARMCII_Get_time_bin(time);
  profiling_state.get.histogram[size_bin][time_bin]++;
  return rc;
}

int ARMCI_Put(void *src, void *dst, int size, int target) {
  profiling_state.put.num_calls++;
  profiling_state.put.total_bytes += size;
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_Put(src, dst, size, target);
  const double t1 = MPI_Wtime();
  const double time = t1-t0;
  profiling_state.put.total_time += time;
  const int size_bin = ARMCII_Get_size_bin(size);
  const int time_bin = ARMCII_Get_time_bin(time);
  profiling_state.put.histogram[size_bin][time_bin]++;
  return rc;
}

int ARMCI_Acc(int datatype, void *scale, void *src, void *dst, int bytes, int proc) {
  profiling_state.acc.num_calls++;
  profiling_state.acc.total_bytes += bytes;
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_Acc(datatype, scale, src, dst, bytes, proc);
  const double t1 = MPI_Wtime();
  const double time = t1-t0;
  profiling_state.acc.total_time += time;
  const int size_bin = ARMCII_Get_size_bin(bytes);
  const int time_bin = ARMCII_Get_time_bin(time);
  profiling_state.acc.histogram[size_bin][time_bin]++;
  return rc;
}

int ARMCI_PutS(void *src_ptr, int src_stride_ar[], void *dst_ptr, int dst_stride_ar[], int count[], int stride_levels, int proc) {
  profiling_state.put.num_calls++;
  const int64_t size = ARMCII_Strided_count(count, stride_levels);
  profiling_state.put.total_bytes += size;
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_PutS(src_ptr, src_stride_ar, dst_ptr, dst_stride_ar, count, stride_levels, proc);
  const double t1 = MPI_Wtime();
  const double time = t1-t0;
  profiling_state.put.total_time += time;
  const int size_bin = ARMCII_Get_size_bin(size);
  const int time_bin = ARMCII_Get_time_bin(time);
  profiling_state.put.histogram[size_bin][time_bin]++;
  return rc;
}

int ARMCI_GetS(void *src_ptr, int src_stride_ar[], void *dst_ptr, int dst_stride_ar[], int count[], int stride_levels, int proc) {
  profiling_state.get.num_calls++;
  const int64_t size = ARMCII_Strided_count(count, stride_levels);
  profiling_state.get.total_bytes += size;
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_GetS(src_ptr, src_stride_ar, dst_ptr, dst_stride_ar, count, stride_levels, proc);
  const double t1 = MPI_Wtime();
  const double time = t1-t0;
  profiling_state.get.total_time += time;
  const int size_bin = ARMCII_Get_size_bin(size);
  const int time_bin = ARMCII_Get_time_bin(time);
  profiling_state.get.histogram[size_bin][time_bin]++;
  return rc;
}

int ARMCI_AccS(int datatype, void *scale, void *src_ptr, int src_stride_ar[], void *dst_ptr, int dst_stride_ar[], int count[], int stride_levels, int proc) {
  profiling_state.acc.num_calls++;
  const int64_t size = ARMCII_Strided_count(count, stride_levels);
  profiling_state.acc.total_bytes += size;
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_AccS(datatype, scale, src_ptr, src_stride_ar, dst_ptr, dst_stride_ar, count, stride_levels, proc);
  const double t1 = MPI_Wtime();
  const double time = t1-t0;
  profiling_state.acc.total_time += time;
  const int size_bin = ARMCII_Get_size_bin(size);
  const int time_bin = ARMCII_Get_time_bin(time);
  profiling_state.acc.histogram[size_bin][time_bin]++;
  return rc;
}

int ARMCI_Put_flag(void *src, void* dst, int size, int *flag, int value, int proc)
{
  profiling_state.put.num_calls++;
  profiling_state.put.total_bytes += size;
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_Put_flag(src, dst, size, flag, value, proc);
  const double t1 = MPI_Wtime();
  const double time = t1-t0;
  profiling_state.put.total_time += time;
  const int size_bin = ARMCII_Get_size_bin(size);
  const int time_bin = ARMCII_Get_time_bin(time);
  profiling_state.put.histogram[size_bin][time_bin]++;
  return rc;
}

int ARMCI_PutS_flag(void *src_ptr, int src_stride_ar[], void *dst_ptr, int dst_stride_ar[], int count[], int stride_levels, int *flag, int value, int proc)
{
  profiling_state.put.num_calls++;
  const int64_t size = ARMCII_Strided_count(count, stride_levels);
  profiling_state.put.total_bytes += size;
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_PutS_flag(src_ptr, src_stride_ar, dst_ptr, dst_stride_ar, count, stride_levels, flag, value, proc);
  const double t1 = MPI_Wtime();
  const double time = t1-t0;
  profiling_state.put.total_time += time;
  const int size_bin = ARMCII_Get_size_bin(size);
  const int time_bin = ARMCII_Get_time_bin(time);
  profiling_state.put.histogram[size_bin][time_bin]++;
  return rc;
}

int ARMCI_PutV(armci_giov_t *iov, int iov_len, int proc) {
  profiling_state.put.num_calls++;
  const int64_t size = ARMCII_Vector_count(iov);
  profiling_state.put.total_bytes += size;
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_PutV(iov, iov_len, proc);
  const double t1 = MPI_Wtime();
  const double time = t1-t0;
  profiling_state.put.total_time += time;
  const int size_bin = ARMCII_Get_size_bin(size);
  const int time_bin = ARMCII_Get_time_bin(time);
  profiling_state.put.histogram[size_bin][time_bin]++;
  return rc;
}

int ARMCI_GetV(armci_giov_t *iov, int iov_len, int proc) {
  profiling_state.get.num_calls++;
  const int64_t size = ARMCII_Vector_count(iov);
  profiling_state.get.total_bytes += size;
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_GetV(iov, iov_len, proc);
  const double t1 = MPI_Wtime();
  const double time = t1-t0;
  profiling_state.get.total_time += time;
  const int size_bin = ARMCII_Get_size_bin(size);
  const int time_bin = ARMCII_Get_time_bin(time);
  profiling_state.get.histogram[size_bin][time_bin]++;
  return rc;
}

int ARMCI_AccV(int datatype, void *scale, armci_giov_t *iov, int iov_len, int proc) {
  profiling_state.acc.num_calls++;
  const int64_t size = ARMCII_Vector_count(iov);
  profiling_state.acc.total_bytes += size;
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_AccV(datatype, scale, iov, iov_len, proc);
  const double t1 = MPI_Wtime();
  const double time = t1-t0;
  profiling_state.acc.total_time += time;
  const int size_bin = ARMCII_Get_size_bin(size);
  const int time_bin = ARMCII_Get_time_bin(time);
  profiling_state.acc.histogram[size_bin][time_bin]++;
  return rc;
}

int ARMCI_Wait(armci_hdl_t* hdl) {
  profiling_state.wait.num_calls++;
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_Wait(hdl);
  const double t1 = MPI_Wtime();
  profiling_state.wait.total_time += (t1-t0);
  return rc;
}

int ARMCI_Test(armci_hdl_t* hdl) {
  profiling_state.wait.num_calls++;
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_Test(hdl);
  const double t1 = MPI_Wtime();
  profiling_state.wait.total_time += (t1-t0);
  return rc;
}

int ARMCI_WaitAll(void) {
  profiling_state.wait.num_calls++;
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_WaitAll();
  const double t1 = MPI_Wtime();
  profiling_state.wait.total_time += (t1-t0);
  return rc;
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
  profiling_state.locks.num_calls++;
  const double t0 = MPI_Wtime();
  PARMCI_Lock(mutex, proc);
  const double t1 = MPI_Wtime();
  profiling_state.locks.total_time += (t1-t0);
}

void ARMCI_Unlock(int mutex, int proc) {
  profiling_state.locks.num_calls++;
  const double t0 = MPI_Wtime();
  PARMCI_Unlock(mutex, proc);
  const double t1 = MPI_Wtime();
  profiling_state.locks.total_time += (t1-t0);
}

int ARMCI_Rmw(int op, void *ploc, void *prem, int value, int proc) {
  profiling_state.rmw.num_calls++;
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_Rmw(op, ploc, prem, value, proc);
  const double t1 = MPI_Wtime();
  profiling_state.rmw.total_time += (t1-t0);
  return rc;
}

void armci_msg_barrier(void) {
  profiling_state.barrier.num_calls++;
  const double t0 = MPI_Wtime();
  parmci_msg_barrier();
  const double t1 = MPI_Wtime();
  profiling_state.barrier.total_time += (t1-t0);
}

void armci_msg_group_barrier(ARMCI_Group *group) {
  profiling_state.barrier.num_calls++;
  const double t0 = MPI_Wtime();
  parmci_msg_group_barrier(group);
  const double t1 = MPI_Wtime();
  profiling_state.barrier.total_time += (t1-t0);
}

#endif
