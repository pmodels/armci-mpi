/* 
* The following is a notice of limited availability of the code, and disclaimer
* which must be included in the prologue of the code and in all source listings
* of the code.
* 
* Copyright (c) 2010  Argonne Leadership Computing Facility, Argonne National 
* Laboratory
* 
* Permission is hereby granted to use, reproduce, prepare derivative works, and
* to redistribute to others.
* 
* 
*                          LICENSE
* 
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
* 
* - Redistributions of source code must retain the above copyright
*   notice, this list of conditions and the following disclaimer.
* 
* - Redistributions in binary form must reproduce the above copyright
*   notice, this list of conditions and the following disclaimer listed
*   in this license in the documentation and/or other materials
*   provided with the distribution.
* 
* - Neither the name of the copyright holders nor the names of its
*   contributors may be used to endorse or promote products derived from
*   this software without specific prior written permission.
* 
* The copyright holders provide no reassurances that the source code
* provided does not infringe any patent, copyright, or any other
* intellectual property rights of third parties.  The copyright holders
* disclaim any liability to any recipient for claims brought against
* recipient by any third party for infringement of that parties
* intellectual property rights.
* 
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
*/

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <armci.h>

#define MAX_XDIM 1024 
#define MAX_YDIM 1024
#define ITERATIONS 100
#define SKIP 10

#ifdef VANILLA_ARMCI
#define ARMCI_Access_begin(X) ((void*)0)
#define ARMCI_Access_end(X) ((void*)0)
#endif 

int main(int argc, char *argv[]) {
   int i, j, rank, nranks;
   int xdim, ydim;
   long bufsize;
   double **buffer, *src_buf;
   double t_start, t_stop;
   int count[2], src_stride, trg_stride, stride_level, peer;
   double expected, actual;
   int provided;

   MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

   MPI_Comm_rank(MPI_COMM_WORLD, &rank);
   MPI_Comm_size(MPI_COMM_WORLD, &nranks);

   if (nranks != 2) {
     printf("Error: need exactly 2 processes\n");
     MPI_Abort(MPI_COMM_WORLD, 1);
   }

   ARMCI_Init_args(&argc, &argv);
   
   buffer = (double **) malloc(sizeof(double *) * nranks);

   bufsize = MAX_XDIM * MAX_YDIM * sizeof(double);
   ARMCI_Malloc((void **) buffer, bufsize);
   src_buf = ARMCI_Malloc_local(bufsize);

   ARMCI_Access_begin(buffer[rank]);
   for (i = 0; i < bufsize/sizeof(double); i++) {
       *(buffer[rank] + i) = 1.0 + rank;
       *(src_buf + i) = 1.0 + rank;
   }
   ARMCI_Access_end(buffer[rank]);

   if(rank == 0) {
     printf("ARMCI_PutS Latency Benchmark\n");
     printf("%30s %22s\n", "Dimensions X-Y (double precision)", "Latency-Remote Completion");
     fflush(stdout);
   }

   src_stride = MAX_XDIM*sizeof(double);
   trg_stride = MAX_XDIM*sizeof(double);
   stride_level = 1;

   ARMCI_Barrier();

   for (xdim = 1; xdim <= MAX_XDIM; xdim *= 2) {

      count[0] = xdim*sizeof(double);

      for (ydim = 1; ydim <= MAX_YDIM; ydim *= 2) {

        count[1] = ydim;
      
        ARMCI_Barrier();

        if (rank == 0) {

          peer = (rank + 1) % nranks;
 
          t_start = MPI_Wtime();

          for (i = 0; i < ITERATIONS; i++)
             ARMCI_PutS(/* (void *) buffer[rank] */ src_buf,
                        &src_stride,
                        (void *) buffer[peer],
                        &trg_stride,
                        count,
                        stride_level,
                        peer); 

          ARMCI_Fence(peer);
          t_stop = MPI_Wtime();

          char temp[10]; 
          sprintf(temp,"[%d,%d]", ydim, xdim);
          printf("%30s %20.2f\n", temp, ((t_stop-t_start)*1.0e6 /* usec */)/ITERATIONS);
          fflush(stdout);

          ARMCI_Barrier();

        } else {
            peer = (rank + nranks-1) % nranks;
            expected = (1.0 + (double) peer);

            ARMCI_Barrier();

            ARMCI_Access_begin(buffer[rank]);

            for (j = 0; j < ydim; j++) {
               for (i = 0; i < xdim; i++) {
                   actual = *(buffer[rank] + i + j*MAX_YDIM);
                   if(actual != expected) {
                      printf("Data validation failed at X: %d Y: %d Expected : %f Actual : %f \n",
                              i, j, expected, actual);
                      fflush(stdout);
                      return -1;
                    }
                }
            }

            for (i = 0; i < bufsize/sizeof(double); i++)
                *(buffer[rank] + i) = 1.0 + rank;

            ARMCI_Access_end(buffer[rank]);
        }
      }
   }

   ARMCI_Barrier();

   ARMCI_Free((void *) buffer[rank]);
   ARMCI_Free_local(src_buf);

   ARMCI_Finalize();
   MPI_Finalize();

   return 0;
}
