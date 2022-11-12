/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>

#include <armci.h>
#include <armcix.h>
#include <armci_internals.h>
#include <debug.h>


/** The ARMCI world group.  This is accessed from outside via
  * ARMCI_Group_get_world.
  */
ARMCI_Group ARMCI_GROUP_WORLD   = {0};
ARMCI_Group ARMCI_GROUP_DEFAULT = {0};


/** Initialize an ARMCI group's remaining fields using the communicator field.
  */
void ARMCII_Group_init_from_comm(ARMCI_Group *group) {
  if (group->comm != MPI_COMM_NULL) {
    MPI_Comm_size(group->comm, &group->size);
    MPI_Comm_rank(group->comm, &group->rank);

  } else {
    group->rank = -1;
    group->size =  0;
  }

  /* If noncollective groups are in use, create a separate communicator that
    can be used for noncollective group creation with this group as the parent.
    This ensures that calls to MPI_Intercomm_create can't clash with any user
    communication. */

  if (ARMCII_GLOBAL_STATE.noncollective_groups && group->comm != MPI_COMM_NULL)
    MPI_Comm_dup(group->comm, &group->noncoll_pgroup_comm);
  else
    group->noncoll_pgroup_comm = MPI_COMM_NULL;

  /* Check if translation caching is enabled */
  if (ARMCII_GLOBAL_STATE.cache_rank_translation) {
    if (group->comm != MPI_COMM_NULL) {
      int      *ranks, i;
      MPI_Group world_group, sub_group;

      group->abs_to_grp = malloc(sizeof(int)*ARMCI_GROUP_WORLD.size);
      group->grp_to_abs = malloc(sizeof(int)*group->size);
      ranks = malloc(sizeof(int)*ARMCI_GROUP_WORLD.size);

      ARMCII_Assert(group->abs_to_grp != NULL && group->grp_to_abs != NULL && ranks != NULL);

      for (i = 0; i < ARMCI_GROUP_WORLD.size; i++)
        ranks[i] = i;

      MPI_Comm_group(ARMCI_GROUP_WORLD.comm, &world_group);
      MPI_Comm_group(group->comm, &sub_group);

      MPI_Group_translate_ranks(sub_group, group->size, ranks, world_group, group->grp_to_abs);
      MPI_Group_translate_ranks(world_group, ARMCI_GROUP_WORLD.size, ranks, sub_group, group->abs_to_grp);

      MPI_Group_free(&world_group);
      MPI_Group_free(&sub_group);

      free(ranks);
    }
  }
  
  /* Translation caching is disabled */
  else {
    group->abs_to_grp = NULL;
    group->grp_to_abs = NULL;
  }
}


/** Create an ARMCI group that contains a subset of the nodes in the current
  * default group.  Collective across the default group.
  *
  * @param[in]  grp_size         Number of entries in pid_list.
  * @param[in]  pid_list         List of process ids that will be in the new group.
  * @param[out] armci_grp_out    The new ARMCI group.
  * @param[in]  armci_grp_parent The parent of the new ARMCI group.
  */
void ARMCI_Group_create(int grp_size, int *pid_list, ARMCI_Group *group_out) {
  ARMCI_Group_create_child(grp_size, pid_list, group_out, &ARMCI_GROUP_DEFAULT);
}


/** Create an ARMCI group that contains a subset of the nodes in the parent
  * group. Collective across output group.
  *
  * @param[in]  grp_size         Number of entries in pid_list.
  * @param[in]  pid_list         List of process ids that will be in the new group.
  * @param[out] armci_grp_out    The new ARMCI group, only valid on group members.
  * @param[in]  armci_grp_parent The parent of the new ARMCI group.
  */
static inline void ARMCI_Group_create_comm_collective(int grp_size, int *pid_list, ARMCI_Group *armci_grp_out,
    ARMCI_Group *armci_grp_parent) {

  MPI_Group mpi_grp_parent;
  MPI_Group mpi_grp_child;

  MPI_Comm_group(armci_grp_parent->comm, &mpi_grp_parent);
  MPI_Group_incl(mpi_grp_parent, grp_size, pid_list, &mpi_grp_child);

  MPI_Comm_create(armci_grp_parent->comm, mpi_grp_child, &armci_grp_out->comm);
 
  MPI_Group_free(&mpi_grp_parent);
  MPI_Group_free(&mpi_grp_child);
}


/** Create an ARMCI group that contains a subset of the nodes in the parent
  * group. Collective across output group.
  *
  * @param[in]  grp_size         Number of entries in pid_list.
  * @param[in]  pid_list         Sorted list of process ids that will be in the new group.
  * @param[out] armci_grp_out    The new ARMCI group, only valid on group members.
  * @param[in]  armci_grp_parent The parent of the new ARMCI group.
  */
static inline void ARMCI_Group_create_comm_noncollective(int grp_size, int *pid_list, ARMCI_Group *armci_grp_out,
    ARMCI_Group *armci_grp_parent) {

  const int INTERCOMM_TAG = 42;
  int       i, grp_me, me, merge_size;
  MPI_Comm  pgroup, inter_pgroup;

  me    = armci_grp_parent->rank;

  /* CHECK: If I'm not a member, return COMM_NULL */
  grp_me = -1;
  for (i = 0; i < grp_size; i++) {
    if (pid_list[i] == me) {
      grp_me = i;
      break;
    }
  }

  if (grp_me < 0) {
    armci_grp_out->comm = MPI_COMM_NULL;
    return;
  }

  /* CASE: Group size 1 */
  else if (grp_size == 1 && pid_list[0] == me) {
    MPI_Comm_dup(MPI_COMM_SELF, &armci_grp_out->comm);
    return;
  }

  pgroup = MPI_COMM_SELF;

  /* Recursively merge adjacent groups until only one group remains.  */
  for (merge_size = 1; merge_size < grp_size; merge_size *= 2) {
    int      gid        = grp_me / merge_size;
    MPI_Comm pgroup_old = pgroup;

    if (gid % 2 == 0) {
      /* Check if right partner doesn't exist */
      if ((gid+1)*merge_size >= grp_size)
        continue;

      MPI_Intercomm_create(pgroup, 0, armci_grp_parent->noncoll_pgroup_comm, pid_list[(gid+1)*merge_size], INTERCOMM_TAG, &inter_pgroup);
      MPI_Intercomm_merge(inter_pgroup, 0 /* LOW */, &pgroup);
    } else {
      MPI_Intercomm_create(pgroup, 0, armci_grp_parent->noncoll_pgroup_comm, pid_list[(gid-1)*merge_size], INTERCOMM_TAG, &inter_pgroup);
      MPI_Intercomm_merge(inter_pgroup, 1 /* HIGH */, &pgroup);
    }

    MPI_Comm_free(&inter_pgroup);
    if (pgroup_old != MPI_COMM_SELF) MPI_Comm_free(&pgroup_old);
  }

  armci_grp_out->comm = pgroup;
}


/** Create an ARMCI group that contains a subset of the nodes in the parent
  * group. Collective.
  *
  * @param[in]  grp_size         Number of entries in pid_list.
  * @param[in]  pid_list         Sorted list of process ids that will be in the new group.
  * @param[out] armci_grp_out    The new ARMCI group, only valid on group members.
  * @param[in]  armci_grp_parent The parent of the new ARMCI group.
  */
void ARMCI_Group_create_child(int grp_size, int *pid_list, ARMCI_Group *armci_grp_out,
    ARMCI_Group *armci_grp_parent) {

  if (ARMCII_GLOBAL_STATE.noncollective_groups)
    ARMCI_Group_create_comm_noncollective(grp_size, pid_list, armci_grp_out, armci_grp_parent);
  else
    ARMCI_Group_create_comm_collective(grp_size, pid_list, armci_grp_out, armci_grp_parent);

  ARMCII_Group_init_from_comm(armci_grp_out);
}


/** Free an ARMCI group.  Collective across group.
  *
  * @param[in] group The group to be freed
  */
void ARMCI_Group_free(ARMCI_Group *group) {
  if (group->comm != MPI_COMM_NULL) {
    MPI_Comm_free(&group->comm);

    if (ARMCII_GLOBAL_STATE.noncollective_groups)
      MPI_Comm_free(&group->noncoll_pgroup_comm);
  }

  /* If the group has translation caches, free them */
  if (group->abs_to_grp != NULL)
    free(group->abs_to_grp);
  if (group->grp_to_abs != NULL)
    free(group->grp_to_abs);

  group->rank = -1;
  group->size = 0;
}


/** Query the calling process' rank in a given group.
  *
  * @param[in]  group Group to query on.
  * @param[out] rank  Location to store the rank.
  * @return           Zero on success, error code otherwise.
  */
int  ARMCI_Group_rank(ARMCI_Group *group, int *rank) {
  *rank = group->rank;

  if (*rank >= 0)
    return 0;
  else
    return 1;
}


/** Query the size of the given group.
  *
  * @param[in]  group Group to query.
  * @param[out] size  Variable to store the size in.
  */
void ARMCI_Group_size(ARMCI_Group *group, int *size) {
  *size = group->size;
}


/** Set the default group.
  *
  * @param[in] group The new default group
  */
void ARMCI_Group_set_default(ARMCI_Group *group) {
  ARMCI_GROUP_DEFAULT = *group;
}


/** Get the default group.
  *
  * @param[out] group_out Pointer to the default group.
  */
void ARMCI_Group_get_default(ARMCI_Group *group_out) {
  *group_out = ARMCI_GROUP_DEFAULT;
}


/** Fetch the world group.
  *
  * @param[out] group_out Output group.
  */
void ARMCI_Group_get_world(ARMCI_Group *group_out) {
  *group_out = ARMCI_GROUP_WORLD;
}


/** Translate a group process rank to the corresponding process rank in the
  * ARMCI world group.
  *
  * @param[in] group      Group to translate from.
  * @param[in] group_rank Rank of the process in group.
  */
int ARMCI_Absolute_id(ARMCI_Group *group, int group_rank) {
  int       world_rank;
  MPI_Group world_group, sub_group;

  ARMCII_Assert(group_rank >= 0 && group_rank < group->size);

  /* Check if group is the world group */
  if (group->comm == ARMCI_GROUP_WORLD.comm)
    world_rank = group_rank;

  /* Check for translation cache */
  else if (group->grp_to_abs != NULL)
    world_rank = group->grp_to_abs[group_rank];

  else {
    /* Translate the rank */
    MPI_Comm_group(ARMCI_GROUP_WORLD.comm, &world_group);
    MPI_Comm_group(group->comm, &sub_group);

    MPI_Group_translate_ranks(sub_group, 1, &group_rank, world_group, &world_rank);

    MPI_Group_free(&world_group);
    MPI_Group_free(&sub_group);
  }

  /* Check if translation failed */
  if (world_rank == MPI_UNDEFINED)
    return -1;
  else
    return world_rank;
}


/** Split a parent group into multiple child groups.  This is similar to
  * MPI_Comm_split.  Collective across the parent group.
  *
  * @param[in]  parent The parent group.
  * @param[in]  color  The id number of the new group.  Processes are grouped
  *                    together so allthat give the same color will be placed
  *                    in the same new group.
  * @param[in]  key    Relative ordering of processes in the new group.
  * @param[out] new_group Pointer to a handle where group info will be stored.
  */
int ARMCIX_Group_split(ARMCI_Group *parent, int color, int key, ARMCI_Group *new_group) {
  int err;

  err = MPI_Comm_split(parent->comm, color, key, &new_group->comm);

  if (err != MPI_SUCCESS)
    return err;

  ARMCII_Group_init_from_comm(new_group);

  return 0;
}


/** Duplicate an ARMCI group.  Collective across the parent group.
  *
  * @param[in]  parent The parent group.
  * @param[in]  color  The id number of the new group.  Processes are grouped
  *                    together so allthat give the same color will be placed
  *                    in the same new group.
  * @param[in]  key    Relative ordering of processes in the new group.
  * @param[out] new_group Pointer to a handle where group info will be stored.
  */
int ARMCIX_Group_dup(ARMCI_Group *parent, ARMCI_Group *new_group) {
  int err;

  err = MPI_Comm_dup(parent->comm, &new_group->comm);

  if (err != MPI_SUCCESS)
    return err;

  ARMCII_Group_init_from_comm(new_group);

  return 0;
}

/** Get the default group.
  *
  * @param[out] the world communicator used by ARMCI
  */
void ARMCIX_Get_world_comm(MPI_Comm * comm)
{
    *comm = ARMCI_GROUP_WORLD.comm;
}
