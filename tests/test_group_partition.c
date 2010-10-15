#include <stdio.h>
#include <stdlib.h>

#include <armci.h>

#define PART_SIZE 3

int main(int argc, char **argv) {
  int                      me, nproc;
  ARMCI_Group              g_world;
  ARMCIX_Group_partition_t *gpart;

  MPI_Init(&argc, &argv);
  ARMCI_Init();

  MPI_Comm_rank(MPI_COMM_WORLD, &me);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);

  if (me == 0) printf("ARMCI Group test starting on %d procs\n", nproc);

  ARMCI_Group_get_world(&g_world);
  
  if (me == 0) printf(" + Forming groups of size %d\n", PART_SIZE);

  gpart = ARMCIX_Group_partition(PART_SIZE, &g_world);

  if (me == 0) printf(" + Freeing groups\n");

  ARMCIX_Group_partition_free(gpart);

  ARMCI_Finalize();
  MPI_Finalize();

  return 0;
}
