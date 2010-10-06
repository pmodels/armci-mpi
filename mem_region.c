#include <stdio.h>
#include <stdlib.h>
#include "debug.h"
#include <mpi.h>

#include "armci.h"
#include "mem_region.h"

mem_region_t *mreg_list = NULL; // Linked list of mem regions
static int    region_ids  = 0;    // Used to hand out region ids...Won't work with groups.


/** Create a distributed shared memory region.
  *
  * @param[in] local_size Size of the local slice of the memory region.
  * @return               Pointer to the memory region object.
  */
mem_region_t *mem_region_create(int local_size) {
  int           me, nproc;
  mem_region_t *mreg;
  mem_region_slice_t mreg_slice;

  MPI_Comm_rank(MPI_COMM_WORLD, &me);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);

  mreg = malloc(sizeof(mem_region_t));
  assert(mreg != NULL);

  mreg->slices = malloc(sizeof(mem_region_slice_t)*nproc);
  assert(mreg->slices != NULL);

  mreg->id      = region_ids++;
  mreg->nslices = nproc;
  mreg->prev    = NULL;
  mreg->next    = NULL;

  // Allocate my slice and create the window
  mreg->slices[me].size = local_size;

  if (local_size == 0)
    mreg->slices[me].base = NULL;
  else
    MPI_Alloc_mem(local_size, MPI_INFO_NULL, &(mreg->slices[me].base));

  MPI_Win_create(mreg->slices[me].base, local_size, 1, MPI_INFO_NULL, MPI_COMM_WORLD, &mreg->window);

  // All-to-all on <base, size> to build up slices vector
  mreg_slice = mreg->slices[me];
  MPI_Allgather(  &mreg_slice, sizeof(mem_region_slice_t), MPI_BYTE,
                 mreg->slices, sizeof(mem_region_slice_t), MPI_BYTE, MPI_COMM_WORLD);

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
void mem_region_destroy(mem_region_t *mreg) {
  int my_id, our_id;
  int me, nproc;

  MPI_Comm_rank(MPI_COMM_WORLD, &me);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);

  // All-to-all exchange of ids.  This is so that we can support passing NULL
  // into ARMCI_Free() which is permitted when a process allocates 0 bytes.
  // Unfortunately, in this case we still need to identify the mem region and
  // free it.

  if (mreg == NULL)
    my_id = 0;
  else
    my_id = mreg->id;

  MPI_Allreduce(&my_id, &our_id, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);

  assert(mreg == NULL || our_id == my_id);

  // If we were passed NULL, look up the mem region using the id
  if (mreg == NULL)
    mreg = mem_region_lookup_by_id(our_id);

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

  if (mreg->slices[me].base != NULL)
    MPI_Free_mem(mreg->slices[me].base);

  free(mreg->slices);
  free(mreg);
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


/** Lookup a shared memory region using its ID.
  *
  * @param[in] id   Unique ID of the desired mem region.
  * @return         Pointer to the mem region object.
  */
mem_region_t *mem_region_lookup_by_id(int id) {
  mem_region_t *mreg;

  mreg = mreg_list;

  while (mreg != NULL && mreg->id != id) {
    mreg = mreg->next;
  }

  return mreg;
}
