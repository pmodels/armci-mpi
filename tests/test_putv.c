/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <mpi.h>
#include <armci.h>

#define XDIM 1024
#define YDIM 1024
#define ITERATIONS 10

int main(int argc, char **argv) {

    int i, j, rank, nranks, peer, bufsize, errors;
    double **buffer, *src_buf;
    armci_giov_t iov;

    MPI_Init(&argc, &argv);
    ARMCI_Init();

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nranks);

    buffer = (double **) malloc(sizeof(double *) * nranks);

    bufsize = XDIM * YDIM * sizeof(double);
    ARMCI_Malloc((void **) buffer, bufsize);
    src_buf = ARMCI_Malloc_local(bufsize);

    if (rank == 0)
        printf("ARMCI I/O Vector Put Test:\n");

    peer = (rank+1) % nranks;

    iov.bytes                = XDIM * sizeof(double);
    iov.ptr_array_len        = YDIM;
    iov.src_ptr_array        = malloc(iov.ptr_array_len*sizeof(void*));
    iov.dst_ptr_array        = malloc(iov.ptr_array_len*sizeof(void*));
    for (i = 0; i < iov.ptr_array_len ; i++) {
        iov.src_ptr_array[i] = (void *)&src_buf[i*XDIM];
        iov.dst_ptr_array[i] = (void *)&buffer[peer][i*XDIM];
    }

    ARMCI_Barrier();

    for (i = 0; i < ITERATIONS; i++) {

      for (j = 0; j < XDIM*YDIM; j++) {
        *(src_buf + j) = rank + i;
      }

      ARMCI_PutV(&iov, 1, peer);
    }

    ARMCI_Barrier();

    free(iov.src_ptr_array);
    free(iov.dst_ptr_array);

    ARMCI_Access_begin(buffer[rank]);
    for (i = errors = 0; i < XDIM; i++) {
      for (j = 0; j < YDIM; j++) {
        const double actual   = *(buffer[rank] + i + j*XDIM);
        const double expected = (1.0 + rank) + (1.0 + ((rank+nranks-1)%nranks)) + (ITERATIONS);
        if (actual - expected > 1e-10) {
          printf("%d: Data validation failed at [%d, %d] expected=%f actual=%f\n",
              rank, j, i, expected, actual);
          errors++;
          fflush(stdout);
        }
      }
    }
    ARMCI_Access_end(buffer[rank]);

    ARMCI_Free((void *) buffer[rank]);
    ARMCI_Free_local(src_buf);
    free(buffer);

    ARMCI_Finalize();
    MPI_Finalize();

    if (errors == 0) {
      printf("%d: Success\n", rank);
      return 0;
    } else {
      printf("%d: Fail\n", rank);
      return 1;
    }
}
