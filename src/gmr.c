/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <armci.h>
#include <armcix.h>
#include <armci_internals.h>
#include <debug.h>
#include <gmr.h>

/** Linked list of shared memory regions.
  */
gmr_t *gmr_list = NULL;

#ifdef HAVE_PTHREADS
static pthread_mutex_t gmr_list_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/** Create a distributed shared memory region. Collective on ARMCI group.
  *
  * @param[in]  local_size Size of the local slice of the memory region.
  * @param[out] base_ptrs  Array of base pointers for each process in group.
  * @param[in]  group      Group on which to perform allocation.
  * @return                Pointer to the memory region object.
  */
gmr_t *gmr_create(gmr_size_t local_size, void **base_ptrs, ARMCI_Group *group) {
  int           i;
  int           alloc_me, alloc_nproc;
  int           world_me, world_nproc;
  MPI_Group     world_group, alloc_group;
  gmr_t        *mreg;
  gmr_slice_t  *alloc_slices, gmr_slice;

  ARMCII_Assert(local_size >= 0);
  ARMCII_Assert(group != NULL);

  MPI_Comm_rank(group->comm, &alloc_me);
  MPI_Comm_size(group->comm, &alloc_nproc);

  /* determine if the GMR construction is pointless and exit early */
  /* use max_local_size later for info hints.                      */
  gmr_size_t max_local_size;
  {
    /* if gmr_size_t changes from long, this needs to change... */
    MPI_Allreduce(&local_size, &max_local_size, 1, GMR_MPI_SIZE_T, MPI_MAX, group->comm);

    if (max_local_size==0) {
      for (i = 0; i < alloc_nproc; i++) {
        base_ptrs[i] = NULL;
      }
      return NULL;
    }
  }

  MPI_Comm_rank(ARMCI_GROUP_WORLD.comm, &world_me);
  MPI_Comm_size(ARMCI_GROUP_WORLD.comm, &world_nproc);

  mreg = malloc(sizeof(gmr_t));
  ARMCII_Assert(mreg != NULL);

  mreg->slices = malloc(sizeof(gmr_slice_t)*world_nproc);
  ARMCII_Assert(mreg->slices != NULL);
  alloc_slices = malloc(sizeof(gmr_slice_t)*alloc_nproc);
  ARMCII_Assert(alloc_slices != NULL);

  mreg->group          = *group; /* NOTE: I think it is invalid in GA/ARMCI to
                                    free a group before its allocations.  If
                                    this is not the case, then assignment here
                                    is incorrect and this should really
                                    duplicated the group (communicator). */

  mreg->nslices        = world_nproc;
  mreg->prev           = NULL;
  mreg->next           = NULL;

  /* Allocate my slice of the GMR */
  alloc_slices[alloc_me].size = local_size;

  MPI_Info win_info = MPI_INFO_NULL;
  MPI_Info_create(&win_info);

  /* may be used by both MPI_Alloc_mem and MPI_Win_allocate */
  if (ARMCII_GLOBAL_STATE.use_alloc_shm) {
      MPI_Info_set(win_info, "alloc_shm", "true");
  } else {
      MPI_Info_set(win_info, "alloc_shm", "false");
  }

  /* MPICH info options:
   * which_accumulate_ops (OFI only)
   * optimized_mr (OFI only)
   * perf_preference (unused)
   **/

  /* tell MPICH that the disp_unit is always the same (and always 1) */
  MPI_Info_set(win_info, "same_disp_unit", "true");

  /* tell MPICH that we do not need a contiguous shared memory domain */
  MPI_Info_set(win_info, "alloc_shared_noncontig", "false");

  /* tell MPICH when we are not going to use datatypes */
  if ((ARMCII_GLOBAL_STATE.iov_method == ARMCII_IOV_CONSRV ||
       ARMCII_GLOBAL_STATE.iov_method == ARMCII_IOV_BATCHED) &&
      (ARMCII_GLOBAL_STATE.strided_method == ARMCII_STRIDED_IOV)) {
      MPI_Info_set(win_info, "accumulate_noncontig_dtype", "false");
  }

  /* tell MPICH that nobody is going to send more than the size of the window */
  {
      if (max_local_size > 2147483647) max_local_size = -1;
      char max_local_size_string[16] = {0};
      snprintf(max_local_size_string,sizeof(max_local_size_string)-1,"%ld",max_local_size);
      MPI_Info_set(win_info, "accumulate_max_bytes", max_local_size_string);
  }

  /* tell MPICH to not use shm accumulate, which might work around some bugs
   * in that code path with less of a penalty than disabling shm altogether. */
  if (ARMCII_GLOBAL_STATE.disable_shm_accumulate) {
      MPI_Info_set(win_info, "disable_shm_accumulate", "true");
  }

  /* tell MPI that only one type of access will happen concurrently during accumulate.
     this violates the ARMCI "spec" but is empirically true in applications, e.g. NWChem.
     for example, NXTVAL/GA_Read_inc/ARMCI_Rmw is *always* a fetch-and-add of 1 within
     an epoch.  all other accesses are collective and set the counter to zero.
     all GA_Acc operations are MPI_SUM and no apps rely on location consistency.
     note that location consistency can also be achieved with ARMCI_NO_FLUSH_LOCAL=1. */
  if (ARMCII_GLOBAL_STATE.use_same_op) {
      MPI_Info_set(win_info, "accumulate_ops", "same_op");
  }

  if (strlen(ARMCII_GLOBAL_STATE.rma_ordering) > 0) {
      MPI_Info_set(win_info, "accumulate_ordering", ARMCII_GLOBAL_STATE.rma_ordering);
  }

  /* give hint to CASPER to avoid extra work for lock permission */
  MPI_Info_set(win_info, "epochs_used", "lockall");

  if (ARMCII_GLOBAL_STATE.use_win_allocate == 0) {

      if (local_size == 0) {
        alloc_slices[alloc_me].base = NULL;
      } else {
        MPI_Alloc_mem(local_size, win_info, &(alloc_slices[alloc_me].base));
        ARMCII_Assert(alloc_slices[alloc_me].base != NULL);
      }
      MPI_Win_create(alloc_slices[alloc_me].base, (MPI_Aint) local_size, 1, MPI_INFO_NULL, group->comm, &mreg->window);
  }
  else if (ARMCII_GLOBAL_STATE.use_win_allocate == 1) {

      MPI_Win_allocate( (MPI_Aint) local_size, 1, win_info, group->comm, &(alloc_slices[alloc_me].base), &mreg->window);

      if (local_size == 0) {
        /* TODO: Is this necessary?  Is it a good idea anymore? */
        alloc_slices[alloc_me].base = NULL;
      } else {
        ARMCII_Assert(alloc_slices[alloc_me].base != NULL);
      }
  }
#ifdef HAVE_MEMKIND_H
  else if (ARMCII_GLOBAL_STATE.use_win_allocate == ARMCII_MEMKIND_WINDOW_TYPE) {

      if (local_size == 0) {
        alloc_slices[alloc_me].base = NULL;
      } else {
        ARMCII_Assert(ARMCII_GLOBAL_STATE.memkind_handle != NULL);
        alloc_slices[alloc_me].base = memkind_malloc(ARMCII_GLOBAL_STATE.memkind_handle, local_size);
        if (alloc_slices[alloc_me].base == NULL) {
            ARMCII_Error("MEMKIND failed to allocate memory! (errno=%d)\n", errno);
        }
      }
      MPI_Win_create(alloc_slices[alloc_me].base, (MPI_Aint) local_size, 1, MPI_INFO_NULL, group->comm, &mreg->window);
  }
#endif
  else {
      ARMCII_Error("invalid window type!\n");
  }

  MPI_Info_free(&win_info);

  /* Debugging: Zero out shared memory if enabled */
  if (ARMCII_GLOBAL_STATE.debug_alloc && local_size > 0) {
    ARMCII_Bzero(alloc_slices[alloc_me].base, local_size);
  }

  /* All-to-all on <base, size> to build up slices vector */
  gmr_slice = alloc_slices[alloc_me];
  MPI_Allgather(  &gmr_slice, sizeof(gmr_slice_t), MPI_BYTE,
                 alloc_slices, sizeof(gmr_slice_t), MPI_BYTE, group->comm);

  /* Populate the base pointers array */
  for (i = 0; i < alloc_nproc; i++)
    base_ptrs[i] = alloc_slices[i].base;

  /* We have to do lookup on global ranks, so shovel the contents of
     alloc_slices into the mreg->slices array which is indexed by global rank. */
  memset(mreg->slices, 0, sizeof(gmr_slice_t)*world_nproc);

  MPI_Comm_group(ARMCI_GROUP_WORLD.comm, &world_group);
  MPI_Comm_group(group->comm, &alloc_group);

  for (i = 0; i < alloc_nproc; i++) {
    int world_rank;
    MPI_Group_translate_ranks(alloc_group, 1, &i, world_group, &world_rank);
    mreg->slices[world_rank] = alloc_slices[i];
  }

  free(alloc_slices);
  MPI_Group_free(&world_group);
  MPI_Group_free(&alloc_group);

  MPI_Win_lock_all((ARMCII_GLOBAL_STATE.rma_nocheck) ? MPI_MODE_NOCHECK : 0,
                   mreg->window);

  {
    int unified = 0;
    void    *attr_ptr;
    int     *attr_val;
    int      attr_flag;
    /* this function will always return flag=false in MPI-2 */
    MPI_Win_get_attr(mreg->window, MPI_WIN_MODEL, &attr_ptr, &attr_flag);
    if (attr_flag) {
      attr_val = (int*)attr_ptr;
      if (world_me==0) {
        if ( (*attr_val)==MPI_WIN_SEPARATE ) {
          printf("MPI_WIN_MODEL = MPI_WIN_SEPARATE \n" );
          unified = 0;
        } else if ( (*attr_val)==MPI_WIN_UNIFIED ) {
#ifdef DEBUG
          printf("MPI_WIN_MODEL = MPI_WIN_UNIFIED \n" );
#endif
          unified = 1;
        } else {
          printf("MPI_WIN_MODEL = %d (not UNIFIED or SEPARATE) \n", *attr_val );
          unified = 0;
        }
      }
    } else {
      if (world_me==0) {
        printf("MPI_WIN_MODEL attribute missing \n");
      }
    }
    if (!unified && (ARMCII_GLOBAL_STATE.shr_buf_method == ARMCII_SHR_BUF_NOGUARD) ) {
      if (world_me==0) {
        printf("Please re-run with ARMCI_SHR_BUF_METHOD=COPY\n");
      }
      /* ARMCI_Error("MPI_WIN_SEPARATE with NOGUARD", 1); */
    }
  }

#ifdef HAVE_PTHREADS
  if (ARMCII_GLOBAL_STATE.thread_level == MPI_THREAD_MULTIPLE) {
    int ptrc = pthread_mutex_lock(&gmr_list_mutex);
    ARMCII_Assert(ptrc == 0);
  }
#endif

  /* Append the new region onto the region list */
  if (gmr_list == NULL) {
    gmr_list = mreg;

  } else {
    gmr_t *parent = gmr_list;

    while (parent->next != NULL)
      parent = parent->next;

    parent->next = mreg;
    mreg->prev   = parent;
  }

#ifdef HAVE_PTHREADS
  if (ARMCII_GLOBAL_STATE.thread_level == MPI_THREAD_MULTIPLE) {
    int ptrc = pthread_mutex_unlock(&gmr_list_mutex);
    ARMCII_Assert(ptrc == 0);
  }
#endif

  return mreg;
}


/** Destroy/free a shared memory region.
  *
  * @param[in] ptr   Pointer within range of the segment (e.g. base pointer).
  * @param[in] group Group on which to perform the free.
  */
void gmr_destroy(gmr_t *mreg, ARMCI_Group *group) {
  int   search_proc_in, search_proc_out, search_proc_out_grp;
  void *search_base = NULL;
  int   alloc_me, alloc_nproc;
  int   world_me, world_nproc;

  MPI_Comm_rank(group->comm, &alloc_me);
  MPI_Comm_size(group->comm, &alloc_nproc);
  MPI_Comm_rank(ARMCI_GROUP_WORLD.comm, &world_me);
  MPI_Comm_size(ARMCI_GROUP_WORLD.comm, &world_nproc);

  /* All-to-all exchange of a <base address, proc> pair.  This is so that we
   * can support passing NULL into ARMCI_Free() which is permitted when a
   * process allocates 0 bytes.  Unfortunately, in this case we still need to
   * identify the mem region and free it.
   */

  if (mreg == NULL)
    search_proc_in = -1;
  else {
    search_proc_in = world_me;
    search_base    = mreg->slices[world_me].base;
  }

  /* Collectively decide on who will provide the base address */
  MPI_Allreduce(&search_proc_in, &search_proc_out, 1, MPI_INT, MPI_MAX, group->comm);

  /* Everyone passed NULL.  Nothing to free. */
  if (search_proc_out < 0)
    return;

  /* Translate world rank to group rank */
  search_proc_out_grp = ARMCII_Translate_absolute_to_group(group, search_proc_out);

  /* Broadcast the base address */
  MPI_Bcast(&search_base, sizeof(void*), MPI_BYTE, search_proc_out_grp, group->comm);

  /* If we were passed NULL, look up the mem region using the <base, proc> pair */
  if (mreg == NULL)
    mreg = gmr_lookup(search_base, search_proc_out);

  /* If it's still not found, the user may have passed the wrong group */
  ARMCII_Assert_msg(mreg != NULL, "Could not locate the desired allocation");

#ifdef HAVE_PTHREADS
  if (ARMCII_GLOBAL_STATE.thread_level == MPI_THREAD_MULTIPLE) {
    int ptrc = pthread_mutex_lock(&gmr_list_mutex);
    ARMCII_Assert(ptrc == 0);
  }
#endif

  /* Remove from the list of mem regions */
  if (mreg->prev == NULL) {
    ARMCII_Assert(gmr_list == mreg);
    gmr_list = mreg->next;

    if (mreg->next != NULL)
      mreg->next->prev = NULL;

  } else {
    mreg->prev->next = mreg->next;
    if (mreg->next != NULL)
      mreg->next->prev = mreg->prev;
  }

#ifdef HAVE_PTHREADS
  if (ARMCII_GLOBAL_STATE.thread_level == MPI_THREAD_MULTIPLE) {
    int ptrc = pthread_mutex_unlock(&gmr_list_mutex);
    ARMCII_Assert(ptrc == 0);
  }
#endif

  ARMCII_Assert_msg(mreg->window != MPI_WIN_NULL, "A non-null mreg contains a null window.");
  MPI_Win_unlock_all(mreg->window);

  /* Destroy the window and free all buffers */
  MPI_Win_free(&mreg->window);

  if (ARMCII_GLOBAL_STATE.use_win_allocate == 0) {
    if (mreg->slices[world_me].base != NULL) {
      MPI_Free_mem(mreg->slices[world_me].base);
    }
  }
#ifdef HAVE_MEMKIND_H
  else if (ARMCII_GLOBAL_STATE.use_win_allocate == ARMCII_MEMKIND_WINDOW_TYPE) {
    if (mreg->slices[world_me].base != NULL) {
      ARMCII_Assert(ARMCII_GLOBAL_STATE.memkind_handle != NULL);
      memkind_free(ARMCII_GLOBAL_STATE.memkind_handle, mreg->slices[world_me].base);
    }
  }
#endif

  free(mreg->slices);
  free(mreg);
}


/** Destroy all memory regions (called by finalize).
  *
  * @return Number of mem regions destroyed.
  */
int gmr_destroy_all(void) {
  int count = 0;

  while (gmr_list != NULL) {
    gmr_destroy(gmr_list, &gmr_list->group);
    count++;
  }

  return count;
}

/** Lookup a shared memory region using an address and process id.
  *
  * @param[in] ptr  Pointer within range of the segment (e.g. base pointer).
  * @param[in] proc Process on which the data lives.
  * @return         Pointer to the mem region object.
  */
gmr_t *gmr_lookup(void *ptr, int proc) {
  gmr_t *mreg;

  mreg = gmr_list;

  while (mreg != NULL) {
    ARMCII_Assert(proc < mreg->nslices);

    /* Jeff: Why is uint8_t used here?  .base is (void*). */
    if (proc < mreg->nslices) {
      const uint8_t   *base = mreg->slices[proc].base;
      const gmr_size_t size = mreg->slices[proc].size;

      if ((uint8_t*) ptr >= base && (uint8_t*) ptr < base + size)
        break;
    }

    mreg = mreg->next;
  }

  return mreg;
}


/** One-sided put operation.  Source buffer must be private.
  *
  * @param[in] mreg   Memory region
  * @param[in] src    Source address (local)
  * @param[in] dst    Destination address (remote)
  * @param[in] size   Number of bytes to transfer
  * @param[in] proc   Absolute process id of target process
  * @return           0 on success, non-zero on failure
  */
int gmr_put(gmr_t *mreg, void *src, void *dst, int size, int proc) {
  ARMCII_Assert_msg(src != NULL, "Invalid local address");
  return gmr_put_typed(mreg, src, size, MPI_BYTE, dst, size, MPI_BYTE, proc);
}


/** One-sided put operation with type arguments.  Source buffer must be private.
  *
  * @param[in] mreg      Memory region
  * @param[in] src       Address of source data
  * @param[in] src_count Number of elements of the given type at the source
  * @param[in] src_type  MPI datatype of the source elements
  * @param[in] dst       Address of destination buffer
  * @param[in] dst_count Number of elements of the given type at the destination
  * @param[in] src_type  MPI datatype of the destination elements
  * @param[in] size      Number of bytes to transfer
  * @param[in] proc      Absolute process id of target process
  * @return              0 on success, non-zero on failure
  */
int gmr_put_typed(gmr_t *mreg, void *src, int src_count, MPI_Datatype src_type,
    void *dst, int dst_count, MPI_Datatype dst_type, int proc) {

  int        grp_proc;
  gmr_size_t disp;
  MPI_Aint lb, extent;

  grp_proc = ARMCII_Translate_absolute_to_group(&mreg->group, proc);
  ARMCII_Assert(grp_proc >= 0);
  ARMCII_Assert_msg(mreg->window != MPI_WIN_NULL, "A non-null mreg contains a null window.");

  // Calculate displacement from beginning of the window
  if (dst == MPI_BOTTOM) 
    disp = 0;
  else
    disp = (gmr_size_t) ((uint8_t*)dst - (uint8_t*)mreg->slices[proc].base);

  // Perform checks
  MPI_Type_get_true_extent(dst_type, &lb, &extent);
  ARMCII_Assert_msg(disp >= 0 && disp < mreg->slices[proc].size, "Invalid remote address");
  ARMCII_Assert_msg(disp + dst_count*extent <= mreg->slices[proc].size, "Transfer is out of range");

  if (ARMCII_GLOBAL_STATE.rma_atomicity) {
      MPI_Accumulate(src, src_count, src_type, grp_proc,
                     (MPI_Aint) disp, dst_count, dst_type, MPI_REPLACE, mreg->window);
  } else {
      MPI_Put(src, src_count, src_type, grp_proc,
              (MPI_Aint) disp, dst_count, dst_type, mreg->window);
  }

  return 0;
}


/** One-sided get operation.  Destination buffer must be private.
  *
  * @param[in] mreg   Memory region
  * @param[in] src    Source address (remote)
  * @param[in] dst    Destination address (local)
  * @param[in] size   Number of bytes to transfer
  * @param[in] proc   Absolute process id of target process
  * @return           0 on success, non-zero on failure
  */
int gmr_get(gmr_t *mreg, void *src, void *dst, int size, int proc) {
  ARMCII_Assert_msg(dst != NULL, "Invalid local address");
  return gmr_get_typed(mreg, src, size, MPI_BYTE, dst, size, MPI_BYTE, proc);
}


/** One-sided get operation with type arguments.  Destination buffer must be private.
  *
  * @param[in] mreg      Memory region
  * @param[in] src       Address of source data
  * @param[in] src_count Number of elements of the given type at the source
  * @param[in] src_type  MPI datatype of the source elements
  * @param[in] dst       Address of destination buffer
  * @param[in] dst_count Number of elements of the given type at the destination
  * @param[in] src_type  MPI datatype of the destination elements
  * @param[in] size      Number of bytes to transfer
  * @param[in] proc      Absolute process id of target process
  * @return              0 on success, non-zero on failure
  */
int gmr_get_typed(gmr_t *mreg, void *src, int src_count, MPI_Datatype src_type,
    void *dst, int dst_count, MPI_Datatype dst_type, int proc) {

  int        grp_proc;
  gmr_size_t disp;
  MPI_Aint lb, extent;

  grp_proc = ARMCII_Translate_absolute_to_group(&mreg->group, proc);
  ARMCII_Assert(grp_proc >= 0);
  ARMCII_Assert_msg(mreg->window != MPI_WIN_NULL, "A non-null mreg contains a null window.");

  // Calculate displacement from beginning of the window
  if (src == MPI_BOTTOM) 
    disp = 0;
  else
    disp = (gmr_size_t) ((uint8_t*)src - (uint8_t*)mreg->slices[proc].base);

  // Perform checks
  MPI_Type_get_true_extent(src_type, &lb, &extent);
  ARMCII_Assert_msg(disp >= 0 && disp < mreg->slices[proc].size, "Invalid remote address");
  ARMCII_Assert_msg(disp + src_count*extent <= mreg->slices[proc].size, "Transfer is out of range");

  if (ARMCII_GLOBAL_STATE.rma_atomicity) {
      MPI_Get_accumulate(NULL, 0, MPI_BYTE, dst, dst_count, dst_type, grp_proc,
                         (MPI_Aint) disp, src_count, src_type, MPI_NO_OP, mreg->window);
  } else {
      MPI_Get(dst, dst_count, dst_type, grp_proc,
              (MPI_Aint) disp, src_count, src_type, mreg->window);
  }

  return 0;
}


/** One-sided accumulate operation.  Source buffer must be private.
  *
  * @param[in] mreg     Memory region
  * @param[in] src      Source address (local)
  * @param[in] dst      Destination address (remote)
  * @param[in] type     MPI type of the given buffers
  * @param[in] count    Number of elements of the given type to transfer
  * @param[in] proc     Absolute process id of the target
  * @return             0 on success, non-zero on failure
  */
int gmr_accumulate(gmr_t *mreg, void *src, void *dst, int count, MPI_Datatype type, int proc) {
  ARMCII_Assert_msg(src != NULL, "Invalid local address");
  return gmr_accumulate_typed(mreg, src, count, type, dst, count, type, proc);
}


/** One-sided accumulate operation with typed arguments.  Source buffer must be private.
  *
  * @param[in] mreg      Memory region
  * @param[in] src       Address of source data
  * @param[in] src_count Number of elements of the given type at the source
  * @param[in] src_type  MPI datatype of the source elements
  * @param[in] dst       Address of destination buffer
  * @param[in] dst_count Number of elements of the given type at the destination
  * @param[in] dst_type  MPI datatype of the destination elements
  * @param[in] size      Number of bytes to transfer
  * @param[in] proc      Absolute process id of target process
  * @return              0 on success, non-zero on failure
  */
int gmr_accumulate_typed(gmr_t *mreg, void *src, int src_count, MPI_Datatype src_type,
    void *dst, int dst_count, MPI_Datatype dst_type, int proc) {

  int        grp_proc;
  gmr_size_t disp;
  MPI_Aint lb, extent;

  grp_proc = ARMCII_Translate_absolute_to_group(&mreg->group, proc);
  ARMCII_Assert(grp_proc >= 0);
  ARMCII_Assert_msg(mreg->window != MPI_WIN_NULL, "A non-null mreg contains a null window.");

  // Calculate displacement from beginning of the window
  if (dst == MPI_BOTTOM) 
    disp = 0;
  else
    disp = (gmr_size_t) ((uint8_t*)dst - (uint8_t*)mreg->slices[proc].base);

  // Perform checks
  MPI_Type_get_true_extent(dst_type, &lb, &extent);
  ARMCII_Assert_msg(disp >= 0 && disp < mreg->slices[proc].size, "Invalid remote address");
  ARMCII_Assert_msg(disp + dst_count*extent <= mreg->slices[proc].size, "Transfer is out of range");

  MPI_Accumulate(src, src_count, src_type, grp_proc, (MPI_Aint) disp, dst_count, dst_type, MPI_SUM, mreg->window);

  return 0;
}

