/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#include <armci.h>
#include <armci_internals.h>
#include <debug.h>
#include <gmr.h>

#ifdef HAVE_PTHREADS
#include <pthread.h>

#if defined(HAVE_NANOSLEEP)

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif
#include <time.h> /* nanosleep */

#elif defined(HAVE_USLEEP)

#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif
#include <unistd.h> /* usleep */

#else

#warning No naptime available!

#endif

int progress_active;
pthread_t ARMCI_Progress_thread;

static void * progress_function(void * arg)
{
    volatile int * active = (volatile int*)arg;
#if defined(HAVE_NANOSLEEP)
    int naptime = 1000 * ARMCII_GLOBAL_STATE.progress_usleep;
    struct timespec napstruct = { .tv_sec  = 0,
                                  .tv_nsec = naptime };
#elif defined(HAVE_USLEEP)
    int naptime = ARMCII_GLOBAL_STATE.progress_usleep;
#endif

    while(*active) {
        ARMCIX_Progress();
#if defined(HAVE_NANOSLEEP)
        if (naptime) nanosleep(&napstruct,NULL);
#elif defined(HAVE_USLEEP)
        if (naptime) usleep(naptime);
#endif
    }

    pthread_exit(NULL);

    return NULL;
}
#endif

/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_Init_thread = PARMCI_Init_thread
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_Init_thread ARMCI_Init_thread
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_Init_thread as PARMCI_Init_thread
#endif
/* -- end weak symbols block -- */

/** Initialize ARMCI.  MPI must be initialized before this can be called.  It
  * invalid to make ARMCI calls before initialization.  Collective on the world
  * group.
  *
  * @return            Zero on success
  */
int PARMCI_Init_thread(int armci_requested) {

  /* GA/TCGMSG end up calling ARMCI_Init_thread() multiple times. */
  if (ARMCII_GLOBAL_STATE.init_count > 0) {
    ARMCII_GLOBAL_STATE.init_count++;
    return 0;
  }

  /* Check for MPI initialization */
  {
    int mpi_is_init, mpi_is_fin;
    MPI_Initialized(&mpi_is_init);
    MPI_Finalized(&mpi_is_fin);
    if (!mpi_is_init || mpi_is_fin) {
      ARMCII_Error("MPI must be initialized before calling ARMCI_Init");
    }
  }

  /* Check for MPI thread-support */
  {
    int mpi_thread_level;
    MPI_Query_thread(&mpi_thread_level);

    if (mpi_thread_level<armci_requested) {
      ARMCII_Error("MPI thread level below ARMCI thread level!");
    }

    ARMCII_GLOBAL_STATE.thread_level = armci_requested;

#ifdef HAVE_PTHREADS

    /* Check progress thread settings */

    ARMCII_GLOBAL_STATE.progress_thread    = ARMCII_Getenv_bool("ARMCI_PROGRESS_THREAD", 0);
    ARMCII_GLOBAL_STATE.progress_usleep    = ARMCII_Getenv_int("ARMCI_PROGRESS_USLEEP", 0);

    if (ARMCII_GLOBAL_STATE.progress_thread && (mpi_thread_level!=MPI_THREAD_MULTIPLE)) {
        ARMCII_Warning("ARMCI progress thread requires MPI_THREAD_MULTIPLE (%d); progress thread disabled.\n",
                       mpi_thread_level);
        ARMCII_GLOBAL_STATE.progress_thread = 0;
    }

    if (ARMCII_GLOBAL_STATE.progress_thread && (ARMCII_GLOBAL_STATE.progress_usleep < 0)) {
        ARMCII_Warning("ARMCI progress thread is not a time machine. (%d)\n",
                       ARMCII_GLOBAL_STATE.progress_usleep);
        ARMCII_GLOBAL_STATE.progress_usleep = -ARMCII_GLOBAL_STATE.progress_usleep;
    }
  }
#endif

  /* Set defaults */
#ifdef ARMCI_GROUP
  ARMCII_GLOBAL_STATE.noncollective_groups = 1;
#endif
#ifdef NO_SEATBELTS
  ARMCII_GLOBAL_STATE.iov_checks           = 0;
#endif

  /* Check for debugging flags */

  ARMCII_GLOBAL_STATE.debug_alloc          = ARMCII_Getenv_bool("ARMCI_DEBUG_ALLOC", 0);
  {
    int junk;
    junk = ARMCII_Getenv_bool("ARMCI_FLUSH_BARRIERS", -1);
    if (junk != -1) {
      ARMCII_Warning("ARMCI_FLUSH_BARRIERS is deprecated.\n");
    }
  }
  ARMCII_GLOBAL_STATE.verbose              = ARMCII_Getenv_bool("ARMCI_VERBOSE", 0);

  /* Group formation options */

  ARMCII_GLOBAL_STATE.cache_rank_translation=ARMCII_Getenv_bool("ARMCI_CACHE_RANK_TRANSLATION", 1);
  if (ARMCII_Getenv("ARMCI_NONCOLLECTIVE_GROUPS"))
    ARMCII_GLOBAL_STATE.noncollective_groups = ARMCII_Getenv_bool("ARMCI_NONCOLLECTIVE_GROUPS", 0);

  /* Check for IOV flags */

  ARMCII_GLOBAL_STATE.iov_checks           = ARMCII_Getenv_bool("ARMCI_IOV_CHECKS", 0);
  ARMCII_GLOBAL_STATE.iov_batched_limit    = ARMCII_Getenv_int("ARMCI_IOV_BATCHED_LIMIT", 0);

  if (ARMCII_GLOBAL_STATE.iov_batched_limit < 0) {
    ARMCII_Warning("Ignoring invalid value for ARMCI_IOV_BATCHED_LIMIT (%d)\n", ARMCII_GLOBAL_STATE.iov_batched_limit);
    ARMCII_GLOBAL_STATE.iov_batched_limit = 0;
  }

#if defined(OPEN_MPI)
  ARMCII_GLOBAL_STATE.iov_method = ARMCII_IOV_BATCHED;
#else
  /* DIRECT leads to addr=NULL errors when ARMCI_{GetV,PutV} are used
   * Jeff: Is this still true? */
  ARMCII_GLOBAL_STATE.iov_method = ARMCII_IOV_DIRECT;
#endif

  char *var = ARMCII_Getenv("ARMCI_IOV_METHOD");
  if (var != NULL) {
    if (strcmp(var, "AUTO") == 0)
      ARMCII_GLOBAL_STATE.iov_method = ARMCII_IOV_AUTO;
    else if (strcmp(var, "CONSRV") == 0)
      ARMCII_GLOBAL_STATE.iov_method = ARMCII_IOV_CONSRV;
    else if (strcmp(var, "BATCHED") == 0)
      ARMCII_GLOBAL_STATE.iov_method = ARMCII_IOV_BATCHED;
    else if (strcmp(var, "DIRECT") == 0)
      ARMCII_GLOBAL_STATE.iov_method = ARMCII_IOV_DIRECT;
    else if (ARMCI_GROUP_WORLD.rank == 0)
      ARMCII_Warning("Ignoring unknown value for ARMCI_IOV_METHOD (%s)\n", var);
  }

  /* Check for Strided flags */

#if defined(OPEN_MPI)
  ARMCII_GLOBAL_STATE.strided_method = ARMCII_STRIDED_IOV;
#else
  ARMCII_GLOBAL_STATE.strided_method = ARMCII_STRIDED_DIRECT;
#endif

  var = ARMCII_Getenv("ARMCI_STRIDED_METHOD");
  if (var != NULL) {
    if (strcmp(var, "IOV") == 0)
      ARMCII_GLOBAL_STATE.strided_method = ARMCII_STRIDED_IOV;
    else if (strcmp(var, "DIRECT") == 0)
      ARMCII_GLOBAL_STATE.strided_method = ARMCII_STRIDED_DIRECT;
    else if (ARMCI_GROUP_WORLD.rank == 0)
      ARMCII_Warning("Ignoring unknown value for ARMCI_STRIDED_METHOD (%s)\n", var);
  }

#ifdef OPEN_MPI
  if (ARMCII_GLOBAL_STATE.iov_method == ARMCII_IOV_DIRECT ||
      ARMCII_GLOBAL_STATE.strided_method == ARMCII_STRIDED_DIRECT)
      ARMCII_Warning("MPI Datatypes are broken in RMA in older versions of Open-MPI!\n");
#endif

  /* Shared buffer handling method */

  /* The default used to be COPY.  NOGUARD requires MPI_WIN_UNIFIED. */
  ARMCII_GLOBAL_STATE.shr_buf_method = ARMCII_SHR_BUF_NOGUARD;

  var = ARMCII_Getenv("ARMCI_SHR_BUF_METHOD");
  if (var != NULL) {
    if (strcmp(var, "COPY") == 0)
      ARMCII_GLOBAL_STATE.shr_buf_method = ARMCII_SHR_BUF_COPY;
    else if (strcmp(var, "NOGUARD") == 0)
      ARMCII_GLOBAL_STATE.shr_buf_method = ARMCII_SHR_BUF_NOGUARD;
    else if (ARMCI_GROUP_WORLD.rank == 0)
      ARMCII_Warning("Ignoring unknown value for ARMCI_SHR_BUF_METHOD (%s)\n", var);
  }

  /* Use win_allocate or not, to work around MPI-3 RMA implementation bugs (now fixed) in MPICH. */
  ARMCII_GLOBAL_STATE.use_win_allocate=ARMCII_Getenv_int("ARMCI_USE_WIN_ALLOCATE", 1);

  /* Equivalent to ARMCI_Set_shm_limit - determines the size of:
   * - MPI_Win_allocate slab in the case of slab allocation
   * - volatile memory pool in the case of MEMKIND
   *
   * The default must be zero to not break backwards compatibility, although
   * NWChem should always set a value using ARMCI_Set_shm_limit.  */
  ARMCII_GLOBAL_STATE.memory_limit=ARMCII_Getenv_long("ARMCI_SHM_LIMIT", 0);

  /* Poke the MPI progress engine at the end of nonblocking (NB) calls */
  ARMCII_GLOBAL_STATE.explicit_nb_progress=ARMCII_Getenv_bool("ARMCI_EXPLICIT_NB_PROGRESS", 1);

  /* Pass alloc_shm to win_allocate / alloc_mem */
  ARMCII_GLOBAL_STATE.use_alloc_shm=ARMCII_Getenv_bool("ARMCI_USE_ALLOC_SHM", 1);

  /* Enable RMA element-wise atomicity */
  ARMCII_GLOBAL_STATE.rma_atomicity=ARMCII_Getenv_bool("ARMCI_RMA_ATOMICITY", 1);

  /* Flush_local becomes flush */
  ARMCII_GLOBAL_STATE.end_to_end_flush=ARMCII_Getenv_bool("ARMCI_NO_FLUSH_LOCAL", 0);

  /* Use MPI_MODE_NOCHECK assertion */
  ARMCII_GLOBAL_STATE.rma_nocheck=ARMCII_Getenv_bool("ARMCI_RMA_NOCHECK", 1);

  /* Setup groups and communicators */

  MPI_Comm_dup(MPI_COMM_WORLD, &ARMCI_GROUP_WORLD.comm);
  ARMCII_Group_init_from_comm(&ARMCI_GROUP_WORLD);
  ARMCI_GROUP_DEFAULT = ARMCI_GROUP_WORLD;

  /* Create GOP operators */

  MPI_Op_create(ARMCII_Absmin_op, 1 /* commute */, &ARMCI_MPI_ABSMIN_OP);
  MPI_Op_create(ARMCII_Absmax_op, 1 /* commute */, &ARMCI_MPI_ABSMAX_OP);

  MPI_Op_create(ARMCII_Msg_sel_min_op, 1 /* commute */, &ARMCI_MPI_SELMIN_OP);
  MPI_Op_create(ARMCII_Msg_sel_max_op, 1 /* commute */, &ARMCI_MPI_SELMAX_OP);

#ifdef HAVE_MEMKIND_H
  if (ARMCII_GLOBAL_STATE.use_win_allocate == ARMCII_MEMKIND_WINDOW_TYPE) {
      if (ARMCII_GLOBAL_STATE.memory_limit == 0) {
          ARMCII_Error("MEMKIND requires a finite limit to create a memory pool!\n");
      } else {

          if (ARMCII_GLOBAL_STATE.memory_limit < MEMKIND_PMEM_MIN_SIZE) {
              ARMCII_Warning("ARMCI memory limit too low (%zu) for VMEM increasing to MEMKIND_PMEM_MIN_SIZE (%zu)\n",
                             ARMCII_GLOBAL_STATE.memory_limit, MEMKIND_PMEM_MIN_SIZE);
          }

          char * pool_path = ARMCII_Getenv("ARMCI_MEMKIND_POOL_PATH");
          if (pool_path == NULL) {
              ARMCII_Warning("ARMCI_MEMKIND_POOL_PATH not provided - using default (/tmp)\n");
              pool_path = "/tmp";
          }
          ARMCII_Dbg_print(DEBUG_CAT_ALL, "MEMKIND pool_path = %s\n", pool_path);

          /* create the volatile memory pool handle for MEMKIND to use */
          int err = memkind_create_pmem(pool_path, ARMCII_GLOBAL_STATE.memory_limit, &ARMCII_GLOBAL_STATE.memkind_handle);
          if (err) {
              ARMCII_Error("MEMKIND failed to create create a memory pool! (err=%d, errno=%d)\n", err, errno);
          }
      }
  }
#endif

  ARMCII_GLOBAL_STATE.init_count++;

  if (ARMCII_GLOBAL_STATE.verbose) {
    if (ARMCI_GROUP_WORLD.rank == 0) {
      int major, minor;

      MPI_Get_version(&major, &minor);

      printf("ARMCI-MPI initialized with %d process%s, MPI v%d.%d\n", ARMCI_GROUP_WORLD.size, ARMCI_GROUP_WORLD.size > 1 ? "es":"", major, minor);
#ifdef NO_SEATBELTS
      printf("  NO_SEATBELTS           = ENABLED\n");
#endif

#ifdef HAVE_PTHREADS
      printf("  PROGRESS_THREAD        = %s\n", ARMCII_GLOBAL_STATE.progress_thread ? "ENABLED" : "DISABLED");
      if (ARMCII_GLOBAL_STATE.progress_thread) {
          printf("  PROGRESS_USLEEP        = %d\n", ARMCII_GLOBAL_STATE.progress_usleep);
      }
#endif

      if (ARMCII_GLOBAL_STATE.memory_limit > 0) {
          printf("  SHM_LIMIT              = %zu\n", ARMCII_GLOBAL_STATE.memory_limit);
      } else {
          printf("  SHM_LIMIT              = %s\n", "UNLIMITED");
      }

      printf("  ALLOC_SHM used         = %s\n", ARMCII_GLOBAL_STATE.use_alloc_shm ? "TRUE" : "FALSE");

      if (ARMCII_GLOBAL_STATE.use_win_allocate == 0) {
          printf("  WINDOW type used       = %s\n", "CREATE");
      }
      else if (ARMCII_GLOBAL_STATE.use_win_allocate == 1) {
          printf("  WINDOW type used       = %s\n", "ALLOCATE");
      }
#ifdef HAVE_MEMKIND_H
      else if (ARMCII_GLOBAL_STATE.use_win_allocate == ARMCII_MEMKIND_WINDOW_TYPE) {
          printf("  WINDOW type used       = %s\n", "LIBVMEM+CREATE");
      }
#endif
      else {
          ARMCII_Error("You have selected an invalid window type!\n");
      }

      if (ARMCII_GLOBAL_STATE.use_win_allocate == 1) {
          /* Jeff: Using win_allocate leads to correctness issues with some
           *       MPI implementations since 3c4ad2abc8c387fcdec3a7f3f44fa5fd75653ece. */
          /* This is required on Cray systems with CrayMPI 7.0.0 (at least) */
          /* Update (Feb. 2015): Xin and Min found the bug in Fetch_and_op and
           *                     it is fixed upstream. */
          ARMCII_Warning("MPI_Win_allocate can lead to correctness issues.\n");
      }

      printf("  STRIDED_METHOD         = %s\n", ARMCII_Strided_methods_str[ARMCII_GLOBAL_STATE.strided_method]);
      printf("  IOV_METHOD             = %s\n", ARMCII_Iov_methods_str[ARMCII_GLOBAL_STATE.iov_method]);

      if (   ARMCII_GLOBAL_STATE.iov_method == ARMCII_IOV_BATCHED
          || ARMCII_GLOBAL_STATE.iov_method == ARMCII_IOV_AUTO)
      {
        if (ARMCII_GLOBAL_STATE.iov_batched_limit > 0)
          printf("  IOV_BATCHED_LIMIT      = %d\n", ARMCII_GLOBAL_STATE.iov_batched_limit);
        else
          printf("  IOV_BATCHED_LIMIT      = UNLIMITED\n");
      }

      printf("  IOV_CHECKS             = %s\n", ARMCII_GLOBAL_STATE.iov_checks             ? "TRUE" : "FALSE");
      printf("  SHR_BUF_METHOD         = %s\n", ARMCII_Shr_buf_methods_str[ARMCII_GLOBAL_STATE.shr_buf_method]);
      printf("  NONCOLLECTIVE_GROUPS   = %s\n", ARMCII_GLOBAL_STATE.noncollective_groups   ? "TRUE" : "FALSE");
      printf("  CACHE_RANK_TRANSLATION = %s\n", ARMCII_GLOBAL_STATE.cache_rank_translation ? "TRUE" : "FALSE");
      printf("  DEBUG_ALLOC            = %s\n", ARMCII_GLOBAL_STATE.debug_alloc            ? "TRUE" : "FALSE");
      printf("\n");
      fflush(NULL);
    }

    MPI_Barrier(ARMCI_GROUP_WORLD.comm);
  }

#ifdef HAVE_PTHREADS
    /* Create the asynchronous progress thread */
    {
        if(ARMCII_GLOBAL_STATE.progress_thread) {
            progress_active = 1;
            int rc = pthread_create(&ARMCI_Progress_thread, NULL, &progress_function, &progress_active);
            if (rc) {
                ARMCII_Warning("ARMCI progress thread creation failed (%d).\n", rc);
            }
        }
    }
#endif

  return 0;
}


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_Init_args = PARMCI_Init_args
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_Init_args ARMCI_Init_args
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_Init_args as PARMCI_Init_args
#endif
/* -- end weak symbols block -- */

/** Initialize ARMCI.  MPI must be initialized before this can be called.  It
  * is invalid to make ARMCI calls before initialization.  Collective on the
  * world group.
  *
  * @param[inout] argc Command line argument count
  * @param[inout] argv Command line arguments
  * @return            Zero on success
  */
int PARMCI_Init_args(int *argc, char ***argv) {
  return PARMCI_Init_thread(MPI_THREAD_SINGLE);
}


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_Init = PARMCI_Init
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_Init ARMCI_Init
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_Init as PARMCI_Init
#endif
/* -- end weak symbols block -- */

/** Initialize ARMCI.  MPI must be initialized before this can be called.  It
  * is invalid to make ARMCI calls before initialization.  Collective on the
  * world group.
  *
  * @return            Zero on success
  */
int PARMCI_Init(void) {
  return PARMCI_Init_thread(MPI_THREAD_SINGLE);
}


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_Initialized = PARMCI_Initialized
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_Initialized ARMCI_Initialized
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_Initialized as PARMCI_Initialized
#endif
/* -- end weak symbols block -- */

/** Check if ARMCI has been initialized.
  *
  * @return Non-zero if ARMCI has been initialized.
  */
int PARMCI_Initialized(void) {
  return ARMCII_GLOBAL_STATE.init_count > 0;
}


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_Finalize = PARMCI_Finalize
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_Finalize ARMCI_Finalize
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_Finalize as PARMCI_Finalize
#endif
/* -- end weak symbols block -- */

/** Finalize ARMCI.  Must be called before MPI is finalized.  ARMCI calls are
  * not valid after finalization.  Collective on world group.
  *
  * @return            Zero on success
  */
int PARMCI_Finalize(void) {
  int nfreed;

  /* GA/TCGMSG end up calling ARMCI_Finalize() multiple times. */
  if (ARMCII_GLOBAL_STATE.init_count == 0) {
    return 0;
  }

  ARMCII_GLOBAL_STATE.init_count--;

  /* Only finalize on the last matching call */
  if (ARMCII_GLOBAL_STATE.init_count > 0) {
    return 0;
  }

#ifdef HAVE_MEMKIND_H
  if (ARMCII_GLOBAL_STATE.use_win_allocate == ARMCII_MEMKIND_WINDOW_TYPE) {
      ARMCII_Assert(ARMCII_GLOBAL_STATE.memkind_handle != NULL);
      vmem_delete(ARMCII_GLOBAL_STATE.memkind_handle);
  }
#endif

#ifdef HAVE_PTHREADS
    /* Destroy the asynchronous progress thread */
    {
        if(ARMCII_GLOBAL_STATE.progress_thread) {
            progress_active = 0;
            int rc = pthread_join(ARMCI_Progress_thread, NULL);
            if (rc) {
                ARMCII_Warning("ARMCI progress thread join failed (%d).\n", rc);
            }
        }
    }
#endif

  nfreed = gmr_destroy_all();

  if (nfreed > 0 && ARMCI_GROUP_WORLD.rank == 0)
    ARMCII_Warning("Freed %d leaked allocations\n", nfreed);

  /* Free GOP operators */

  MPI_Op_free(&ARMCI_MPI_ABSMIN_OP);
  MPI_Op_free(&ARMCI_MPI_ABSMAX_OP);

  MPI_Op_free(&ARMCI_MPI_SELMIN_OP);
  MPI_Op_free(&ARMCI_MPI_SELMAX_OP);

  ARMCI_Cleanup();

  ARMCI_Group_free(&ARMCI_GROUP_WORLD);

  return 0;
}


/** Cleaup ARMCI resources.  Call finalize instead.
  */
void ARMCI_Cleanup(void) {
  return;
}

