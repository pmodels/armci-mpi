#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <mpi.h>

#include <armci.h>
#include <armcix.h>
#include <armci_internals.h>
#include <debug.h>


/** Raise an internal fatal ARMCI error.
  *
  * @param[in] file Current file name (__FILE__)
  * @param[in] line Current line numeber (__LINE__)
  * @param[in] func Current function name (__func__)
  * @param[in] msg  Message to be printed
  * @param[in] code Exit error code
  */
void ARMCII_Error(const char *file, const int line, const char *func, const char *msg, int code) {
  fprintf(stderr, "ARMCI Error: %s: %s (%s:%d)\n", func, msg, file, line);
  MPI_Abort(ARMCI_GROUP_WORLD.comm, code);
}


/** Translate a world process rank to the corresponding process rank in the
  * ARMCI group.
  *
  * @param[in] group      Group to translate to.
  * @param[in] world_rank Rank of the process in the world group.
  * @return               Rank in group or -1 if not in the group.
  */
int ARMCII_Translate_absolute_to_group(MPI_Comm group_comm, int world_rank) {
  int       group_rank;
  MPI_Group world_group, sub_group;

  MPI_Comm_group(ARMCI_GROUP_WORLD.comm, &world_group);
  MPI_Comm_group(group_comm, &sub_group);

  MPI_Group_translate_ranks(world_group, 1, &world_rank, sub_group, &group_rank);

  MPI_Group_free(&world_group);
  MPI_Group_free(&sub_group);

  return group_rank == MPI_UNDEFINED ? -1 : group_rank;
}
