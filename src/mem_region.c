#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#include <armci.h>
#include <debug.h>
#include <mem_region.h>


/** Linked list of shared memory regions.
  */
mem_region_t *mreg_list = NULL;


/** Create a distributed shared memory region. Collective on alloc_comm.
  *
  * @param[in] local_size Size of the local slice of the memory region.
  * @return               Pointer to the memory region object.
  */
mem_region_t *mem_region_create(int local_size, MPI_Comm alloc_comm, MPI_Comm world_comm) {
  int           i;
  int           alloc_me, alloc_nproc;
  int           world_me, world_nproc;
  MPI_Group     world_group, alloc_group;
  mem_region_t *mreg;
  mem_region_slice_t *alloc_slices, mreg_slice;

  MPI_Comm_rank(alloc_comm, &alloc_me);
  MPI_Comm_size(alloc_comm, &alloc_nproc);
  MPI_Comm_rank(world_comm, &world_me);
  MPI_Comm_size(world_comm, &world_nproc);

  mreg = malloc(sizeof(mem_region_t));
  assert(mreg != NULL);

  mreg->slices = malloc(sizeof(mem_region_slice_t)*world_nproc);
  assert(mreg->slices != NULL);
  alloc_slices = malloc(sizeof(mem_region_slice_t)*alloc_nproc);
  assert(alloc_slices != NULL);

  mreg->nslices = world_nproc;
  mreg->prev    = NULL;
  mreg->next    = NULL;

  // Allocate my slice and create the window
  alloc_slices[alloc_me].size = local_size;

  if (local_size == 0)
    alloc_slices[alloc_me].base = NULL;
  else
    MPI_Alloc_mem(local_size, MPI_INFO_NULL, &(alloc_slices[alloc_me].base));

  MPI_Win_create(alloc_slices[alloc_me].base, local_size, 1, MPI_INFO_NULL, alloc_comm, &mreg->window);

  // All-to-all on <base, size> to build up slices vector
  mreg_slice = alloc_slices[alloc_me];
  MPI_Allgather(  &mreg_slice, sizeof(mem_region_slice_t), MPI_BYTE,
                 alloc_slices, sizeof(mem_region_slice_t), MPI_BYTE, alloc_comm);

  // We have to do lookup on global ranks, so shovel the contents of
  // alloc_slices into the mreg->slices array which is indexed by global rank.
  memset(mreg->slices, 0, sizeof(mem_region_slice_t)*world_nproc);

  MPI_Comm_group(world_comm, &world_group);
  MPI_Comm_group(alloc_comm, &alloc_group);

  for (i = 0; i < alloc_nproc; i++) {
    int world_rank;
    MPI_Group_translate_ranks(alloc_group, 1, &i, world_group, &world_rank);
    mreg->slices[world_rank] = alloc_slices[i];
  }

  MPI_Group_free(&world_group);
  MPI_Group_free(&alloc_group);

  // Create the RMW mutex: Keeps RMW operations atomic wrt each other
  mreg->rmw_mutex = ARMCI_Create_mutexes_grp(1);

  // Append the new region onto the region list
  if (mreg_list == NULL) {
    mreg_list = mreg;

  } else {
    mem_region_t *parent = mreg_list;

    while (parent->next != NULL)
      parent = parent->next;

    parent->next = mreg;
    mreg->prev   = parent;
  }

  return mreg;
}


/** Destroy/free a shared memory region.
  *
  * @param[in] ptr Pointer within range of the segment (e.g. base pointer).
  */
void mem_region_destroy(mem_region_t *mreg, MPI_Comm alloc_comm, MPI_Comm world_comm) {
  int   search_proc_in, search_proc_out, search_proc_out_grp;
  void *search_base;
  int   alloc_me, alloc_nproc;
  int   world_me, world_nproc;
  MPI_Group world_group, alloc_group;

  MPI_Comm_rank(alloc_comm, &alloc_me);
  MPI_Comm_size(alloc_comm, &alloc_nproc);
  MPI_Comm_rank(world_comm, &world_me);
  MPI_Comm_size(world_comm, &world_nproc);

  // All-to-all exchange of a <base address, proc> pair.  This is so that we
  // can support passing NULL into ARMCI_Free() which is permitted when a
  // process allocates 0 bytes.  Unfortunately, in this case we still need to
  // identify the mem region and free it.

  if (mreg == NULL)
    search_proc_in = -1;
  else {
    search_proc_in = world_me;
    search_base    = mreg->slices[world_me].base;
  }

  // Collectively decide on who will provide the base address
  MPI_Allreduce(&search_proc_in, &search_proc_out, 1, MPI_INT, MPI_MAX, alloc_comm);
  assert(search_proc_out != -1); // Somebody must pass in non-NULL

  // Translate world rank to group rank
  MPI_Comm_group(world_comm, &world_group);
  MPI_Comm_group(alloc_comm, &alloc_group);

  MPI_Group_translate_ranks(world_group, 1, &search_proc_out, alloc_group, &search_proc_out_grp);

  MPI_Group_free(&world_group);
  MPI_Group_free(&alloc_group);

  // Broadcast the base address
  MPI_Bcast(&search_base, sizeof(void*), MPI_BYTE, search_proc_out_grp, alloc_comm);

  // If we were passed NULL, look up the mem region using the <base, proc> pair
  if (mreg == NULL)
    mreg = mem_region_lookup(search_base, search_proc_out);

  if (mreg == NULL) printf("Err: mreg == NULL.  base=%p proc=%d\n", search_base, search_proc_out);
  assert(mreg != NULL);

  // Remove from the list of mem regions
  if (mreg->prev == NULL) {
    assert(mreg_list == mreg);
    mreg_list = mreg->next;

    if (mreg->next != NULL)
      mreg->next->prev = NULL;

  } else {
    mreg->prev->next = mreg->next;
    if (mreg->next != NULL)
      mreg->next->prev = mreg->prev;
  }

  // Destroy the window and free all buffers
  MPI_Win_free(&mreg->window);

  if (mreg->slices[world_me].base != NULL)
    MPI_Free_mem(mreg->slices[world_me].base);

  free(mreg->slices);
  free(mreg);

  ARMCI_Destroy_mutexes_grp(mreg->rmw_mutex);
}


/** Lookup a shared memory region using an address and process id.
  *
  * @param[in] ptr  Pointer within range of the segment (e.g. base pointer).
  * @param[in] proc Process on which the data lives.
  * @return         Pointer to the mem region object.
  */
mem_region_t *mem_region_lookup(void *ptr, int proc) {
  mem_region_t *mreg;

  mreg = mreg_list;

  while (mreg != NULL) {
    assert(proc < mreg->nslices); // FIXME: Remove when we have groups

    if (proc < mreg->nslices) {
      const u_int8_t *base = mreg->slices[proc].base;
      const int       size = mreg->slices[proc].size;

      if ((u_int8_t*) ptr >= base && (u_int8_t*) ptr < base + size)
        break;
    }

    mreg = mreg->next;
  }

  return mreg;
}
