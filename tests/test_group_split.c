#include <stdio.h>
#include <stdlib.h>

#include <armci.h>
#include <armcix.h>

#define PART_SIZE 1

int main(int argc, char **argv) {
  int          me, nproc;
  int         *procs;
  ARMCI_Group  g_world, g_new;

  MPI_Init(&argc, &argv);
  ARMCI_Init();

  MPI_Comm_rank(MPI_COMM_WORLD, &me);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);

  procs = malloc(sizeof(int)* ( nproc/2 + nproc % 2 ? 1 : 0 ));

  if (me == 0) printf("ARMCI Group test starting on %d procs\n", nproc);

  ARMCI_Group_get_world(&g_world);
  
  if (me == 0) printf(" + Creating odd/even groups\n");

  ARMCIX_Group_split(&g_world, me%2, me, &g_new);

  ARMCI_Group_free(&g_new);

  free(procs);

  ARMCI_Finalize();
  MPI_Finalize();

  return 0;
}
