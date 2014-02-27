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
#include <assert.h>
#include <mpi.h>
#include <armci.h>

#ifdef USE_ARMCI_LONG
#  define INC_TYPE long
#  define ARMCI_OP ARMCI_FETCH_AND_ADD_LONG
#else
#  define INC_TYPE int
#  define ARMCI_OP ARMCI_FETCH_AND_ADD
#endif

int main(int argc, char* argv[])
{
    int provided;
#ifdef __bgp__
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    assert(provided==MPI_THREAD_MULTIPLE);
#else
    MPI_Init_thread(&argc, &argv, MPI_THREAD_SINGLE, &provided);
#endif

    ARMCI_Init();

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int count = ( argc > 1 ? atoi(argv[1]) : 1000000 );

    int * complete = (int *) malloc(sizeof(int) * count);
    for(i=0; i<count; i++) complete[i] = 0;

    void ** base_ptrs = malloc(sizeof(void*)*nproc);
    ARMCI_Malloc(base_ptrs, sizeof(INC_TYPE));

    ARMCI_Access_begin(base_ptrs[rank]);
    *(int*) base_ptrs[rank] = 0;
    ARMCI_Access_end(base_ptrs[rank]);

    ARMCI_Barrier();

    complete = (int *) malloc(sizeof(int) * count);

    if (rank == 0) {
        printf("ARMCI_Rmw Test - in usec \n");
        fflush(stdout);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank>0)
    {
        int target = 0;
        int nrecv = 0;
        INC_TYPE val = -1;
        t0 = MPI_Wtime();
        while(val < count) {
            ARMCI_Rmw(ARMCI_OP, &val, base_ptrs[target], 1, target);
            if (val < count) {
                complete[val] = rank;
                nrecv++;
            }
        }
        t1 = MPI_Wtime();
        tt = (t1-t0);
    }
    MPI_Allreduce(MPI_IN_PLACE, complete, count, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

    dt = (double)tt/nrecv;
    printf("process %d received %d counters in %f seconds (%f per call)\n",
           rank, nrecv, tt, dt);
    fflush(stdout);
    MPI_Barrier(MPI_COMM_WORLD);

    if (0==rank) {
        printf("Checking for fairness...\n", rank);
        fflush(stdout);
        for(i=0; i<count; i++) {
            printf("counter value %d %s received (rank %d) \n", i,
                   0==complete[i] ? "was NOT" : "was",
                   0==complete[i] ? -911 : complete[i] );
        }
        fflush(stdout);
    }
    MPI_Barrier(MPI_COMM_WORLD);

    ARMCI_Free(base_ptrs[rank]);
    free(base_ptrs);

    free(complete);

    ARMCI_Finalize();
    MPI_Finalize();

    return 0;
}
