/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 * Copyright (C) 2022, NVIDIA.
 */

//#include "armciconf.h"

#include "armci.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <float.h>
#include <math.h>

void ARMCIX_Get_world_comm(MPI_Comm * comm);

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

typedef struct
{
    int64_t num_calls;
    int64_t total_bytes;
    double  total_time;
    double  min_time;
    double  max_time;
}
ARMCII_Statistics_s;

typedef struct
{
    int64_t num_calls;
    int64_t total_bytes;
    int64_t histogram[ARMCI_SIZE_BINS][ARMCI_TIME_BINS];
    double  total_time;
} 
ARMCII_Histogram_s;

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
    // impossible to track bandwidth accurately
    ARMCII_Statistics_s nbput;      // includes C/S/V
    ARMCII_Statistics_s nbget;      // includes C/S/V
    ARMCII_Statistics_s nbacc;      // includes C/S/V
    FILE * file;
    int file_needs_closing;         // true if file is not stderr
    int dump_has_happened;          // because ARMCI is finalized multiple times :-(
}
ARMCII_Profiling_s;

ARMCII_Profiling_s profiling_state = { 0 };

/*********************************************************/

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

static inline void ARMCII_Update_stats(ARMCII_Statistics_s * s, double time, int64_t size)
{
    const double old_min = s->min_time;
    const double old_max = s->max_time;
    if (time < old_min) {
        s->min_time = time;
    }
    if (time > old_max) {
        s->max_time = time;
    }
    s->total_time += time;
    s->total_bytes += size;
    s->num_calls++;
}

static inline void ARMCII_Update_hist(ARMCII_Histogram_s * s, double time, int size)
{
    const int size_bin = ARMCII_Get_size_bin(size);
    const int time_bin = ARMCII_Get_time_bin(time);
    s->histogram[size_bin][time_bin]++;
    s->total_bytes += size;
    s->total_time += time;
    s->num_calls++;
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

/*********************************************************/

static void ARMCII_Print_histogram(FILE * f, char * name, ARMCII_Histogram_s * h)
{
    int64_t n = h->num_calls;
    int64_t b = h->total_bytes;
    double  t = h->total_time;
    if (n > 0) {
        fprintf(f,"%s called %"PRIi64" times. time per call = %lf. total bytes = %"PRIi64", bandwidth = %lf (GB/s)\n",
                   name,     n,                               t/n,               b,                     1.e-9*b/t);
 
        double time_bin = 1.e-7;
        fprintf(f," size (B)| time (s)->");
        for (int i=0; i<ARMCI_TIME_BINS; i++) {
            fprintf(f,"%.1e, ", time_bin);
            time_bin *= 10;
        }
        fprintf(f,"\n");
        int size_bin = 0;
        for (int i=0; i<ARMCI_SIZE_BINS; i++) {
            fprintf(f," 10^%1d: ", size_bin++);
            for (int j=0; j<ARMCI_TIME_BINS; j++) {
                fprintf(f,"%"PRIi64", ", h->histogram[i][j]);
            }
            fprintf(f,"\n");
        }
    } else {
        fprintf(f,"%s called %"PRIi64" times.\n",
                   name,     n);
    }
}

static void ARMCII_Print_statistics(FILE * f, char * name, ARMCII_Statistics_s * s)
{
    const int64_t n   = s->num_calls;
    const int64_t b   = s->total_bytes;
    const double  t   = s->total_time;
    const double  min = s->min_time;
    const double  max = s->max_time;
    if (n>0) {
        if (b>0) {
            fprintf(f,"%s called %"PRIi64" times, total bytes=%"PRIi64", avg,min,max=%lf, %lf, %lf\n",
                       name,     n,                           b,                     t/n, min, max);
        } else {
            fprintf(f,"%s called %"PRIi64" times, avg,min,max=%lf, %lf, %lf\n",
                       name,     n,                           t/n, min, max);
        }
    } else {
            fprintf(f,"%s called %"PRIi64" times,\n",
                       name,     n);
    }
}

static void ARMCII_Reduce_stats(ARMCII_Statistics_s * i, ARMCII_Statistics_s * o, MPI_Comm c)
{
    MPI_Reduce(&i->num_calls,   &o->num_calls,   1,         MPI_INT64_T, MPI_SUM, 0, c);
    MPI_Reduce(&i->total_bytes, &o->total_bytes, 1,         MPI_INT64_T, MPI_SUM, 0, c);
    MPI_Reduce(&i->total_time,  &o->total_time,  1,         MPI_DOUBLE,  MPI_SUM, 0, c);
    MPI_Reduce(&i->max_time,    &o->max_time,    1,         MPI_DOUBLE,  MPI_MAX, 0, c);
    MPI_Reduce(&i->min_time,    &o->min_time,    1,         MPI_DOUBLE,  MPI_MIN, 0, c);
}

static void ARMCII_Reduce_histogram(ARMCII_Histogram_s * i, ARMCII_Histogram_s * o, MPI_Comm c)
{
    const int hist_size = ARMCI_SIZE_BINS*ARMCI_TIME_BINS;
    MPI_Reduce(&i->num_calls,   &o->num_calls,   1,         MPI_INT64_T, MPI_SUM, 0, c);
    MPI_Reduce(&i->total_bytes, &o->total_bytes, 1,         MPI_INT64_T, MPI_SUM, 0, c);
    MPI_Reduce(&i->histogram,   &o->histogram,   hist_size, MPI_INT64_T, MPI_SUM, 0, c);
    MPI_Reduce(&i->total_time,  &o->total_time,  1,         MPI_DOUBLE,  MPI_SUM, 0, c);
}

/*********************************************************/

static void ARMCII_Profile_init(void)
{
    int me;
    MPI_Comm comm;

    ARMCIX_Get_world_comm(&comm);
    MPI_Comm_rank(comm, &me);

    if (me == 0) {
        fprintf(stderr,"ARMCI-MPI profiling initiated\n");
        fflush(stderr);
    }

    profiling_state.rmw.min_time      = DBL_MAX;
    profiling_state.barrier.min_time  = DBL_MAX;
    profiling_state.wait.min_time     = DBL_MAX;
    profiling_state.fence.min_time    = DBL_MAX;
    profiling_state.locks.min_time    = DBL_MAX;
    profiling_state.globmem.min_time  = DBL_MAX;
    profiling_state.localmem.min_time = DBL_MAX;
    profiling_state.nbput.min_time    = DBL_MAX;
    profiling_state.nbget.min_time    = DBL_MAX;
    profiling_state.nbacc.min_time    = DBL_MAX;

    if (me == 0) {
        profiling_state.file_needs_closing = 0;
        const char * filename = getenv("ARMCI_PROFILE_OUTPUT");
        if (filename != NULL) {
            FILE * tryfile = fopen(filename,"w");
            if (tryfile == NULL) {
                profiling_state.file = stderr;
                fprintf(stderr,"ARMCI_PROFILE_OUTPUT=%s could not be opened, using stderr instead\n", filename);
                fflush(stderr);
            } else {
                profiling_state.file = tryfile;
                profiling_state.file_needs_closing = 1;
            }
        } else {
            profiling_state.file = stderr;
        }
    }
}

static void ARMCII_Profile_dump(FILE * f)
{
    if (profiling_state.dump_has_happened) return;

    int me;
    MPI_Comm comm;

    ARMCIX_Get_world_comm(&comm);
    MPI_Comm_rank(comm, &me);

    ARMCII_Profiling_s aggregate_stats = { 0 };

    for (int i=0; i<10; i++) {
        fflush(stdout);
        fflush(stderr);
        MPI_Barrier(comm);
    }

    if (me==0) fprintf(f, "ARMCI-MPI profiling results:\n");

    // yeah, this is dumb, i could do a user-defined reduction but that is probably more code.

    ARMCII_Reduce_histogram(&profiling_state.put, &aggregate_stats.put, comm);
    if (me==0) ARMCII_Print_histogram(f, "Put", &aggregate_stats.put);

    ARMCII_Reduce_histogram(&profiling_state.get, &aggregate_stats.get, comm);
    if (me==0) ARMCII_Print_histogram(f, "Get", &aggregate_stats.get);

    ARMCII_Reduce_histogram(&profiling_state.acc, &aggregate_stats.acc, comm);
    if (me==0) ARMCII_Print_histogram(f, "Acc", &aggregate_stats.acc);

    ARMCII_Reduce_stats(&profiling_state.nbput, &aggregate_stats.nbput, comm);
    if (me==0) ARMCII_Print_statistics(f, "NbPut", &aggregate_stats.nbput);

    ARMCII_Reduce_stats(&profiling_state.nbget, &aggregate_stats.nbget, comm);
    if (me==0) ARMCII_Print_statistics(f, "NbGet", &aggregate_stats.nbget);

    ARMCII_Reduce_stats(&profiling_state.nbacc, &aggregate_stats.nbacc, comm);
    if (me==0) ARMCII_Print_statistics(f, "NbAcc", &aggregate_stats.nbacc);

    ARMCII_Reduce_stats(&profiling_state.rmw, &aggregate_stats.rmw, comm);
    if (me==0) ARMCII_Print_statistics(f, "Rmw", &aggregate_stats.rmw);

    ARMCII_Reduce_stats(&profiling_state.barrier, &aggregate_stats.barrier, comm);
    if (me==0) ARMCII_Print_statistics(f, "Barrier", &aggregate_stats.barrier);

    ARMCII_Reduce_stats(&profiling_state.wait, &aggregate_stats.wait, comm);
    if (me==0) ARMCII_Print_statistics(f, "Wait", &aggregate_stats.wait);

    ARMCII_Reduce_stats(&profiling_state.fence, &aggregate_stats.fence, comm);
    if (me==0) ARMCII_Print_statistics(f, "Fence", &aggregate_stats.fence);

    ARMCII_Reduce_stats(&profiling_state.locks, &aggregate_stats.locks, comm);
    if (me==0) ARMCII_Print_statistics(f, "Locks", &aggregate_stats.locks);

    ARMCII_Reduce_stats(&profiling_state.globmem, &aggregate_stats.globmem, comm);
    if (me==0) ARMCII_Print_statistics(f, "Global memory management", &aggregate_stats.globmem);

    ARMCII_Reduce_stats(&profiling_state.localmem, &aggregate_stats.localmem, comm);
    if (me==0) ARMCII_Print_statistics(f, "Local memory management", &aggregate_stats.localmem);

    fflush(0);

    MPI_Barrier(comm);

    if (me==0) {
        if (profiling_state.file_needs_closing) {
            int rc = fclose(profiling_state.file);
            if (rc) {
                fprintf(stderr,"ARMCI_PROFILE_OUTPUT could not be closed (%d)\n", rc);
                fflush(stderr);
            }
        }
    }

    profiling_state.dump_has_happened = 1;
}

/*********************************************************/

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
  ARMCII_Profile_dump(profiling_state.file);
  return PARMCI_Finalize();
}

int ARMCI_Malloc(void **base_ptrs, armci_size_t size) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_Malloc(base_ptrs, size);
  const double t1 = MPI_Wtime();
  ARMCII_Update_stats(&profiling_state.globmem, t1-t0, size);
  return rc;
}

int ARMCI_Free(void *ptr) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_Free(ptr);
  const double t1 = MPI_Wtime();
  ARMCII_Update_stats(&profiling_state.globmem, t1-t0, 0);
  return rc;
}

int ARMCI_Malloc_memdev(void **base_ptrs, armci_size_t size, const char *device) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_Malloc_memdev(base_ptrs, size, device);
  const double t1 = MPI_Wtime();
  ARMCII_Update_stats(&profiling_state.globmem, t1-t0, size);
  return rc;
}

int ARMCI_Malloc_group_memdev(void **base_ptrs, armci_size_t size, ARMCI_Group *group, const char *device) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_Malloc_group_memdev(base_ptrs, size, group, device);
  const double t1 = MPI_Wtime();
  ARMCII_Update_stats(&profiling_state.globmem, t1-t0, size);
  return rc;
}

int ARMCI_Free_memdev(void *ptr) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_Free_memdev(ptr);
  const double t1 = MPI_Wtime();
  ARMCII_Update_stats(&profiling_state.globmem, t1-t0, 0);
  return rc;
}

void *ARMCI_Malloc_local(armci_size_t size) {
  const double t0 = MPI_Wtime();
  void * rc = PARMCI_Malloc_local(size);
  const double t1 = MPI_Wtime();
  ARMCII_Update_stats(&profiling_state.localmem, t1-t0, size);
  return rc;
}

int ARMCI_Free_local(void *ptr) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_Free_local(ptr);
  const double t1 = MPI_Wtime();
  ARMCII_Update_stats(&profiling_state.localmem, t1-t0, 0);
  return rc;
}

void ARMCI_Barrier(void)
{
  const double t0 = MPI_Wtime();
  PARMCI_Barrier();
  const double t1 = MPI_Wtime();
  ARMCII_Update_stats(&profiling_state.barrier, t1-t0, 0);
}

void ARMCI_Fence(int proc)
{
  const double t0 = MPI_Wtime();
  PARMCI_Fence(proc);
  const double t1 = MPI_Wtime();
  ARMCII_Update_stats(&profiling_state.fence, t1-t0, 0);
}

void ARMCI_AllFence(void)
{
  const double t0 = MPI_Wtime();
  PARMCI_AllFence();
  const double t1 = MPI_Wtime();
  ARMCII_Update_stats(&profiling_state.fence, t1-t0, 0);
}

void ARMCI_Access_begin(void *ptr) {
  PARMCI_Access_begin(ptr);
}

void ARMCI_Access_end(void *ptr) {
  PARMCI_Access_end(ptr);
}

int ARMCI_Get(void *src, void *dst, int size, int target) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_Get(src, dst, size, target);
  const double t1 = MPI_Wtime();
  ARMCII_Update_hist(&profiling_state.get, t1-t0, size);
  return rc;
}

int ARMCI_Put(void *src, void *dst, int size, int target) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_Put(src, dst, size, target);
  const double t1 = MPI_Wtime();
  ARMCII_Update_hist(&profiling_state.put, t1-t0, size);
  return rc;
}

int ARMCI_Acc(int datatype, void *scale, void *src, void *dst, int bytes, int proc) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_Acc(datatype, scale, src, dst, bytes, proc);
  const double t1 = MPI_Wtime();
  ARMCII_Update_hist(&profiling_state.acc, t1-t0, bytes);
  return rc;
}

int ARMCI_PutS(void *src_ptr, int src_stride_ar[], void *dst_ptr, int dst_stride_ar[], int count[], int stride_levels, int proc) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_PutS(src_ptr, src_stride_ar, dst_ptr, dst_stride_ar, count, stride_levels, proc);
  const double t1 = MPI_Wtime();
  const int64_t size = ARMCII_Strided_count(count, stride_levels);
  ARMCII_Update_hist(&profiling_state.put, t1-t0, size);
  return rc;
}

int ARMCI_GetS(void *src_ptr, int src_stride_ar[], void *dst_ptr, int dst_stride_ar[], int count[], int stride_levels, int proc) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_GetS(src_ptr, src_stride_ar, dst_ptr, dst_stride_ar, count, stride_levels, proc);
  const double t1 = MPI_Wtime();
  const int64_t size = ARMCII_Strided_count(count, stride_levels);
  ARMCII_Update_hist(&profiling_state.get, t1-t0, size);
  return rc;
}

int ARMCI_AccS(int datatype, void *scale, void *src_ptr, int src_stride_ar[], void *dst_ptr, int dst_stride_ar[], int count[], int stride_levels, int proc) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_AccS(datatype, scale, src_ptr, src_stride_ar, dst_ptr, dst_stride_ar, count, stride_levels, proc);
  const double t1 = MPI_Wtime();
  const int64_t size = ARMCII_Strided_count(count, stride_levels);
  ARMCII_Update_hist(&profiling_state.acc, t1-t0, size);
  return rc;
}

int ARMCI_Put_flag(void *src, void* dst, int size, int *flag, int value, int proc) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_Put_flag(src, dst, size, flag, value, proc);
  const double t1 = MPI_Wtime();
  ARMCII_Update_hist(&profiling_state.put, t1-t0, size);
  return rc;
}

int ARMCI_PutS_flag(void *src_ptr, int src_stride_ar[], void *dst_ptr, int dst_stride_ar[], int count[], int stride_levels, int *flag, int value, int proc) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_PutS_flag(src_ptr, src_stride_ar, dst_ptr, dst_stride_ar, count, stride_levels, flag, value, proc);
  const double t1 = MPI_Wtime();
  const int64_t size = ARMCII_Strided_count(count, stride_levels);
  ARMCII_Update_hist(&profiling_state.put, t1-t0, size);
  return rc;
}

int ARMCI_PutV(armci_giov_t *iov, int iov_len, int proc) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_PutV(iov, iov_len, proc);
  const double t1 = MPI_Wtime();
  const int64_t size = ARMCII_Vector_count(iov);
  ARMCII_Update_hist(&profiling_state.put, t1-t0, size);
  return rc;
}

int ARMCI_GetV(armci_giov_t *iov, int iov_len, int proc) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_GetV(iov, iov_len, proc);
  const double t1 = MPI_Wtime();
  const int64_t size = ARMCII_Vector_count(iov);
  ARMCII_Update_hist(&profiling_state.get, t1-t0, size);
  return rc;
}

int ARMCI_AccV(int datatype, void *scale, armci_giov_t *iov, int iov_len, int proc) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_AccV(datatype, scale, iov, iov_len, proc);
  const double t1 = MPI_Wtime();
  const int64_t size = ARMCII_Vector_count(iov);
  ARMCII_Update_hist(&profiling_state.acc, t1-t0, size);
  return rc;
}

int ARMCI_Wait(armci_hdl_t* hdl) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_Wait(hdl);
  const double t1 = MPI_Wtime();
  ARMCII_Update_stats(&profiling_state.wait, t1-t0, 0);
  return rc;
}

int ARMCI_Test(armci_hdl_t* hdl) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_Test(hdl);
  const double t1 = MPI_Wtime();
  ARMCII_Update_stats(&profiling_state.wait, t1-t0, 0);
  return rc;
}

int ARMCI_WaitAll(void) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_WaitAll();
  const double t1 = MPI_Wtime();
  ARMCII_Update_stats(&profiling_state.wait, t1-t0, 0);
  return rc;
}

int ARMCI_NbPut(void *src, void *dst, int bytes, int proc, armci_hdl_t *hdl) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_NbPut(src, dst, bytes, proc, hdl);
  const double t1 = MPI_Wtime();
  ARMCII_Update_stats(&profiling_state.nbput, t1-t0, bytes);
  return rc;
}

int ARMCI_NbGet(void *src, void *dst, int bytes, int proc, armci_hdl_t *hdl) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_NbGet(src, dst, bytes, proc, hdl);
  const double t1 = MPI_Wtime();
  ARMCII_Update_stats(&profiling_state.nbget, t1-t0, bytes);
  return rc;
}

int ARMCI_NbAcc(int datatype, void *scale, void *src, void *dst, int bytes, int proc, armci_hdl_t *hdl) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_NbAcc(datatype, scale, src, dst, bytes, proc, hdl);
  const double t1 = MPI_Wtime();
  ARMCII_Update_stats(&profiling_state.nbacc, t1-t0, bytes);
  return rc;
}

int ARMCI_NbPutS(void *src_ptr, int src_stride_ar[], void *dst_ptr, int dst_stride_ar[], int count[], int stride_levels, int proc, armci_hdl_t *hdl) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_NbPutS(src_ptr, src_stride_ar, dst_ptr, dst_stride_ar, count, stride_levels, proc, hdl);
  const double t1 = MPI_Wtime();
  const int64_t size = ARMCII_Strided_count(count, stride_levels);
  ARMCII_Update_stats(&profiling_state.nbput, t1-t0, size);
  return rc;
}

int ARMCI_NbGetS(void *src_ptr, int src_stride_ar[], void *dst_ptr, int dst_stride_ar[], int count[], int stride_levels, int proc, armci_hdl_t *hdl) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_NbGetS(src_ptr, src_stride_ar, dst_ptr, dst_stride_ar, count, stride_levels, proc, hdl);
  const double t1 = MPI_Wtime();
  const int64_t size = ARMCII_Strided_count(count, stride_levels);
  ARMCII_Update_stats(&profiling_state.nbget, t1-t0, size);
  return rc;
}

int ARMCI_NbAccS(int datatype, void *scale, void *src_ptr, int src_stride_ar[], void *dst_ptr, int dst_stride_ar[], int count[], int stride_levels, int proc, armci_hdl_t *hdl) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_NbAccS(datatype, scale, src_ptr, src_stride_ar, dst_ptr, dst_stride_ar, count, stride_levels, proc, hdl);
  const double t1 = MPI_Wtime();
  const int64_t size = ARMCII_Strided_count(count, stride_levels);
  ARMCII_Update_stats(&profiling_state.nbacc, t1-t0, size);
  return rc;
}

int ARMCI_NbPutV(armci_giov_t *iov, int iov_len, int proc, armci_hdl_t* handle) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_NbPutV(iov, iov_len, proc, handle);
  const double t1 = MPI_Wtime();
  const int64_t size = ARMCII_Vector_count(iov);
  ARMCII_Update_stats(&profiling_state.nbput, t1-t0, size);
  return rc;
}

int ARMCI_NbGetV(armci_giov_t *iov, int iov_len, int proc, armci_hdl_t* handle) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_NbGetV(iov, iov_len, proc, handle);
  const double t1 = MPI_Wtime();
  const int64_t size = ARMCII_Vector_count(iov);
  ARMCII_Update_stats(&profiling_state.nbget, t1-t0, size);
  return rc;
}

int ARMCI_NbAccV(int datatype, void *scale, armci_giov_t *iov, int iov_len, int proc, armci_hdl_t* handle) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_NbAccV(datatype, scale, iov, iov_len, proc, handle);
  const double t1 = MPI_Wtime();
  const int64_t size = ARMCII_Vector_count(iov);
  ARMCII_Update_stats(&profiling_state.nbacc, t1-t0, size);
  return rc;
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
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_Create_mutexes(count);
  const double t1 = MPI_Wtime();
  ARMCII_Update_stats(&profiling_state.locks, t1-t0, 0);
  return rc;
}

int ARMCI_Destroy_mutexes(void) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_Destroy_mutexes();
  const double t1 = MPI_Wtime();
  ARMCII_Update_stats(&profiling_state.locks, t1-t0, 0);
  return rc;
}

void ARMCI_Lock(int mutex, int proc) {
  const double t0 = MPI_Wtime();
  PARMCI_Lock(mutex, proc);
  const double t1 = MPI_Wtime();
  ARMCII_Update_stats(&profiling_state.locks, t1-t0, 0);
}

void ARMCI_Unlock(int mutex, int proc) {
  const double t0 = MPI_Wtime();
  PARMCI_Unlock(mutex, proc);
  const double t1 = MPI_Wtime();
  ARMCII_Update_stats(&profiling_state.locks, t1-t0, 0);
}

int ARMCI_Rmw(int op, void *ploc, void *prem, int value, int proc) {
  const double t0 = MPI_Wtime();
  const int rc = PARMCI_Rmw(op, ploc, prem, value, proc);
  const double t1 = MPI_Wtime();
  const int size = (op == ARMCI_FETCH_AND_ADD_LONG || op == ARMCI_SWAP_LONG) ? 8 : 4;
  ARMCII_Update_stats(&profiling_state.rmw, t1-t0, size);
  return rc;
}

void armci_msg_barrier(void) {
  const double t0 = MPI_Wtime();
  parmci_msg_barrier();
  const double t1 = MPI_Wtime();
  ARMCII_Update_stats(&profiling_state.barrier, t1-t0, 0);
}

void armci_msg_group_barrier(ARMCI_Group *group) {
  const double t0 = MPI_Wtime();
  parmci_msg_group_barrier(group);
  const double t1 = MPI_Wtime();
  ARMCII_Update_stats(&profiling_state.barrier, t1-t0, 0);
}
