#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <mpi.h>
#include <armci.h>

#define NUM_MUTEXES 1

int main(int argc, char ** argv) {
  int rank, nproc;

  MPI_Init(&argc, &argv);
  ARMCI_Init();

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);

  if (rank == 0) printf("Starting ARMCI mutex contention test with %d processes\n", nproc);

  ARMCI_Create_mutexes(NUM_MUTEXES);

  /*
  for (i = 0; i < nproc; i++)
    for (j = 0; j < NUM_MUTEXES; j++) {
      ARMCI_Lock(j, i);
      ARMCI_Unlock(j, i);
    }
  */

  ARMCI_Lock(0, 0);
  ARMCI_Unlock(0, 0);

  printf(" + %3d done\n", rank);
  fflush(NULL);

  ARMCI_Barrier();

  ARMCI_Destroy_mutexes();

  if (rank == 0) printf("Test complete: PASS.\n");

  ARMCI_Finalize();
  MPI_Finalize();

  return 0;
}
