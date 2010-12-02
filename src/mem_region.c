/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <mpi.h>

#include <armci.h>
#include <armcix.h>
#include <armci_internals.h>
#include <debug.h>
#include <mem_region.h>


/** Linked list of shared memory regions.
  */
mem_region_t *mreg_list = NULL;


/** Create a distributed shared memory region. Collective on ARMCI group.
  *
  * @param[in]  local_size Size of the local slice of the memory region.
  * @param[out] base_ptrs  Array of base pointers for each process in group.
  * @param[in]  group      Group on which to perform allocation.
  * @return                Pointer to the memory region object.
  */
mem_region_t *mem_region_create(int local_size, void **base_ptrs, ARMCI_Group *group) {
  int           i;
  int           alloc_me, alloc_nproc;
  int           world_me, world_nproc;
  MPI_Group     world_group, alloc_group;
  mem_region_t *mreg;
  mem_region_slice_t *alloc_slices, mreg_slice;

  MPI_Comm_rank(group->comm, &alloc_me);
  MPI_Comm_size(group->comm, &alloc_nproc);
  MPI_Comm_rank(ARMCI_GROUP_WORLD.comm, &world_me);
  MPI_Comm_size(ARMCI_GROUP_WORLD.comm, &world_nproc);

  mreg = malloc(sizeof(mem_region_t));
  assert(mreg != NULL);

  mreg->slices = malloc(sizeof(mem_region_slice_t)*world_nproc);
  assert(mreg->slices != NULL);
  alloc_slices = malloc(sizeof(mem_region_slice_t)*alloc_nproc);
  assert(alloc_slices != NULL);

  mreg->comm           = group->comm;
  mreg->nslices        = world_nproc;
  mreg->access_mode    = ARMCIX_MODE_ALL;
  mreg->lock_state     = MREG_LOCK_UNLOCKED;
  mreg->freelist       = NULL;
  mreg->freelist_count = 0;
  mreg->freelist_size  = 0;
  mreg->prev           = NULL;
  mreg->next           = NULL;

  // Allocate my slice and create the window
  alloc_slices[alloc_me].size = local_size;

  if (local_size == 0)
    alloc_slices[alloc_me].base = NULL;
  else
    MPI_Alloc_mem(local_size, MPI_INFO_NULL, &(alloc_slices[alloc_me].base));

  MPI_Win_create(alloc_slices[alloc_me].base, local_size, 1, MPI_INFO_NULL, group->comm, &mreg->window);

  // All-to-all on <base, size> to build up slices vector
  mreg_slice = alloc_slices[alloc_me];
  MPI_Allgather(  &mreg_slice, sizeof(mem_region_slice_t), MPI_BYTE,
                 alloc_slices, sizeof(mem_region_slice_t), MPI_BYTE, group->comm);

  // Populate the base pointers array
  for (i = 0; i < alloc_nproc; i++)
    base_ptrs[i] = alloc_slices[i].base;

  // We have to do lookup on global ranks, so shovel the contents of
  // alloc_slices into the mreg->slices array which is indexed by global rank.
  memset(mreg->slices, 0, sizeof(mem_region_slice_t)*world_nproc);

  MPI_Comm_group(ARMCI_GROUP_WORLD.comm, &world_group);
  MPI_Comm_group(group->comm, &alloc_group);

  for (i = 0; i < alloc_nproc; i++) {
    int world_rank;
    MPI_Group_translate_ranks(alloc_group, 1, &i, world_group, &world_rank);
    mreg->slices[world_rank] = alloc_slices[i];
  }

  MPI_Group_free(&world_group);
  MPI_Group_free(&alloc_group);

  // Create the RMW mutex: Keeps RMW operations atomic wrt each other
  mreg->rmw_mutex = ARMCIX_Create_mutexes_grp(1, group);

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
  * @param[in] ptr   Pointer within range of the segment (e.g. base pointer).
  * @param[in] group Group on which to perform the free.
  */
void mem_region_destroy(mem_region_t *mreg, ARMCI_Group *group) {
  int   search_proc_in, search_proc_out, search_proc_out_grp;
  void *search_base;
  int   alloc_me, alloc_nproc;
  int   world_me, world_nproc;
  MPI_Group world_group, alloc_group;

  MPI_Comm_rank(group->comm, &alloc_me);
  MPI_Comm_size(group->comm, &alloc_nproc);
  MPI_Comm_rank(ARMCI_GROUP_WORLD.comm, &world_me);
  MPI_Comm_size(ARMCI_GROUP_WORLD.comm, &world_nproc);

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
  MPI_Allreduce(&search_proc_in, &search_proc_out, 1, MPI_INT, MPI_MAX, group->comm);
  assert(search_proc_out != -1); // Somebody must pass in non-NULL

  // Translate world rank to group rank
  MPI_Comm_group(ARMCI_GROUP_WORLD.comm, &world_group);
  MPI_Comm_group(group->comm, &alloc_group);

  MPI_Group_translate_ranks(world_group, 1, &search_proc_out, alloc_group, &search_proc_out_grp);

  MPI_Group_free(&world_group);
  MPI_Group_free(&alloc_group);

  // Broadcast the base address
  MPI_Bcast(&search_base, sizeof(void*), MPI_BYTE, search_proc_out_grp, group->comm);

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
  ARMCIX_Destroy_mutexes_grp(mreg->rmw_mutex);

  assert(mreg->freelist_count == 0);

  if (mreg->freelist != NULL)
    free(mreg->freelist);

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
    assert(proc < mreg->nslices);

    if (proc < mreg->nslices) {
      const uint8_t *base = mreg->slices[proc].base;
      const int       size = mreg->slices[proc].size;

      if ((uint8_t*) ptr >= base && (uint8_t*) ptr < base + size)
        break;
    }

    mreg = mreg->next;
  }

  return mreg;
}


/** One-sided put operation.  If the mem region is not locked, it will be
  * automatically locked according to the access mode.  Source buffer must
  * be private.
  *
  * @param[in] mreg   Memory region
  * @param[in] src    Source address (local)
  * @param[in] dst    Destination address (remote)
  * @param[in] size   Number of bytes to transfer
  * @param[in] proc   Absolute process id of target process
  * @return           0 on success, non-zero on failure
  */
int mreg_put(mem_region_t *mreg, void *src, void *dst, int size, int proc) {
  int disp, grp_proc, need_lock;

  grp_proc = ARMCII_Translate_absolute_to_group(mreg->comm, proc);
  assert(grp_proc >= 0);

  // Calculate displacement from beginning of the window
  disp = (int) ((uint8_t*)dst - (uint8_t*)mreg->slices[proc].base);

  assert(disp >= 0 && disp < mreg->slices[proc].size);
  assert(dst >= mreg->slices[proc].base);
  assert((uint8_t*)dst + size <= (uint8_t*)mreg->slices[proc].base + mreg->slices[proc].size);

  need_lock = mreg->lock_state == MREG_LOCK_UNLOCKED;

  if (need_lock) mreg_lock(mreg, MREG_LOCK_EXCLUSIVE, proc);
  MPI_Put(src, size, MPI_BYTE, grp_proc, disp, size, MPI_BYTE, mreg->window);
  if (need_lock) mreg_unlock(mreg, proc);

  return 0;
}


/** One-sided get operation.  If the mem region is not locked, it will be
  * automatically locked according to the access mode.  Destination buffer
  * must be private.
  *
  * @param[in] mreg   Memory region
  * @param[in] src    Source address (remote)
  * @param[in] dst    Destination address (local)
  * @param[in] size   Number of bytes to transfer
  * @param[in] proc   Absolute process id of target process
  * @return           0 on success, non-zero on failure
  */
int mreg_get(mem_region_t *mreg, void *src, void *dst, int size, int proc) {
  int disp, grp_proc, need_lock;

  grp_proc = ARMCII_Translate_absolute_to_group(mreg->comm, proc);
  assert(grp_proc >= 0);

  // Calculate displacement from beginning of the window
  disp = (int) ((uint8_t*)src - (uint8_t*)mreg->slices[proc].base);

  assert(disp >= 0 && disp < mreg->slices[proc].size);
  assert(src >= mreg->slices[proc].base);
  assert((uint8_t*)src + size <= (uint8_t*)mreg->slices[proc].base + mreg->slices[proc].size);

  need_lock = mreg->lock_state == MREG_LOCK_UNLOCKED;

  if (need_lock) mreg_lock(mreg, MREG_LOCK_EXCLUSIVE, proc);
  MPI_Get(dst, size, MPI_BYTE, grp_proc, disp, size, MPI_BYTE, mreg->window);
  if (need_lock) mreg_unlock(mreg, proc);

  return 0;
}


/** One-sided accumulate operation.  If the mem region is not locked, it will be
  * automatically locked according to the access mode.  Source buffer must be
  * private.
  *
  * @param[in] mreg     Memory region
  * @param[in] src      Source address (local)
  * @param[in] dst      Destination address (remote)
  * @param[in] type     MPI type of the given buffers
  * @param[in] count    Number of elements of the given type to transfer
  * @param[in] proc     Absolute process id of the target
  * @return             0 on success, non-zero on failure
  */
int mreg_accumulate(mem_region_t *mreg, void *src, void *dst, MPI_Datatype type, int count, int proc) {
  int disp, grp_proc, type_size, need_lock;

  grp_proc = ARMCII_Translate_absolute_to_group(mreg->comm, proc);
  assert(grp_proc >= 0);

  // Calculate displacement from window's base address
  disp = (int) ((uint8_t*)dst - (uint8_t*)(mreg->slices[proc].base));

  MPI_Type_size(type, &type_size);
  assert(disp >= 0 && disp < mreg->slices[proc].size);
  assert(dst >= mreg->slices[proc].base);
  assert((uint8_t*)dst + (type_size*count) <= (uint8_t*)mreg->slices[proc].base + mreg->slices[proc].size);

  need_lock = mreg->lock_state == MREG_LOCK_UNLOCKED;

  if (need_lock) mreg_lock(mreg, MREG_LOCK_EXCLUSIVE, proc);
  MPI_Accumulate(src, count, type, grp_proc, disp, count, type, MPI_SUM, mreg->window);
  if (need_lock) mreg_unlock(mreg, proc);

  return 0;
}


/** Attach a buffer to a memory region's to-free list.  This is for internally
  * generated buffers that should be freed when a window is unlocked.
  *
  * @param[in] mreg Memory region
  * @param[in] buf  Buffer to be freed (with MPI_Free_mem)
  */
void mreg_freelist_attach(mem_region_t *mreg, void *buf) {
  if (mreg->freelist_count == mreg->freelist_size) {
    void *new_list;

    mreg->freelist_size += 100;
    new_list = realloc(mreg->freelist, mreg->freelist_size*sizeof(void*));
    assert(new_list != NULL);

    mreg->freelist = new_list;
  }

  mreg->freelist[mreg->freelist_count] = buf;
  mreg->freelist_count++;
}


/** Free all buffers attached to a memory region's to-free list.
  *
  * @param[in] mreg Memory region
  */
void mreg_freelist_free(mem_region_t *mreg) {
  for ( ; mreg->freelist_count > 0; mreg->freelist_count--)
    MPI_Free_mem(mreg->freelist[mreg->freelist_count-1]);
}


/** Lock a memory region so that one-sided operations can be performed.
  *
  * @param[in] mreg     Memory region
  * @param[in] mode     Lock mode (exclusive, shared, etc...)
  * @param[in] proc     Absolute process id of the target
  * @return             0 on success, non-zero on failure
  */
void mreg_lock(mem_region_t *mreg, int mode, int proc) {
  int grp_proc = ARMCII_Translate_absolute_to_group(mreg->comm, proc);
  assert(grp_proc >= 0);

  assert(mreg->lock_state == MREG_LOCK_UNLOCKED);

  switch (mode) {
    case MREG_LOCK_EXCLUSIVE:
      MPI_Win_lock(MPI_LOCK_EXCLUSIVE, grp_proc, 0, mreg->window);
      break;
    case MREG_LOCK_SHARED:
      MPI_Win_lock(MPI_LOCK_SHARED, grp_proc, 0, mreg->window);
      break;
    default:
      ARMCII_Error(__FILE__, __LINE__, __func__, "unknown lock mode", 10);
      return;
  }

  mreg->lock_state = mode;
}


/** Unlock a memory region.
  *
  * @param[in] mreg     Memory region
  * @param[in] proc     Absolute process id of the target
  * @return             0 on success, non-zero on failure
  */
void mreg_unlock(mem_region_t *mreg, int proc) {
  int grp_proc = ARMCII_Translate_absolute_to_group(mreg->comm, proc);
  assert(grp_proc >= 0);

  assert(mreg->lock_state != MREG_LOCK_UNLOCKED);

  MPI_Win_unlock(grp_proc, mreg->window);

  mreg->lock_state = MREG_LOCK_UNLOCKED;

  // Free everything on the to-free list
  mreg_freelist_free(mreg);
}
