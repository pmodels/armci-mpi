#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "armci.h"
#include "debug.h"

/********************************************************************/
/** Process space partitioning:                                     */
/********************************************************************/

/* Partition the process space into (roughly) equal size ARMCI groups.  This is
 * a helper function to supplement the existing ARMCI groups API.  This is a
 * collective call.
 */
ARMCIX_Group_partition_t *ARMCIX_Group_partition(int grp_size, ARMCI_Group *parent_grp) {
  ARMCIX_Group_partition_t *g;
  int rank, gid, idx;
  int me, nproc;
  int *procs;
  
  g = malloc(sizeof(ARMCIX_Group_partition_t));
  assert(g != NULL);

  ARMCI_Group_rank(parent_grp, &me);
  ARMCI_Group_size(parent_grp, &nproc);
  
  procs = (int*) malloc(grp_size*sizeof(int));
  
#ifdef ARMCI_GROUPS
  // ARMCI groups are formed collectively across only the processors in the new
  // group so all groups are formed concurrently here and every process has
  // only a valid reference to its own group.
  
  g->ngroups = 1;
  g->groups  = (int*) malloc(g->ngroups * sizeof(int));
  
  for (idx = 0; idx < grp_size && ((me/grp_size)*grp_size + idx) < nproc; idx++) {
    procs[idx] = ((me/grp_size)*grp_size) + idx;
  }

  g->groups[0] = ARMCI_Group_create_child(idx, procs, &g->groups[gid], parent_grp);
  g->mygroup   = 0;
#else
  // Non-ARMCI groups use MPI under the covers and formation is collective
  // across the default group.  So everyone has to participate in the formation
  // of all groups and everyone will have a reference to each group including
  // its own.
  if (nproc % grp_size > 0)
    g->ngroups = nproc / grp_size + 1;
  else
    g->ngroups = nproc / grp_size;

  g->groups  = (ARMCI_Group*) malloc(g->ngroups * sizeof(ARMCI_Group));
  
  for (gid = 0; gid < g->ngroups; gid++) {
    for (rank = gid*grp_size, idx = 0; rank < (gid+1)*grp_size && rank < nproc; rank++, idx++) {
      procs[idx] = rank;
    }

    ARMCI_Group_create_child(idx, procs, &g->groups[gid], parent_grp);

    if (gid == me / grp_size)
      g->mygroup = gid;
  }
#endif

  dprint(DEBUG_CAT_GROUPS, __func__, "ngroups = %d, my_groupid = %d\n", g->ngroups, g->mygroup);

  free(procs);

#define CHECK_MEMBERSHIP
#ifdef CHECK_MEMBERSHIP
  int i;
  for (i = 0; i < g->ngroups; i++) {
    int rank, err;

    err = ARMCI_Group_rank(&g->groups[i], &rank);
    if (err) rank = -1;

    if (i != g->mygroup)
      assert(rank < 0);
    else
      assert(rank >= 0);
  }
#endif

  return g;
}


/* Free a set of groups allocated with gtc_group_partition (collective).
 */
void ARMCIX_Group_partition_free(ARMCIX_Group_partition_t *g) {
  ARMCI_Group_free(&g->groups[g->mygroup]);
  free(g->groups);
  free(g);
}


/* Return my group's handle
 */
ARMCI_Group *ARMCIX_Group_partition_mygroup(ARMCIX_Group_partition_t *g) {
  return &g->groups[g->mygroup];
}
