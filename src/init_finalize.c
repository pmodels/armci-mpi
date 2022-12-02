/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <armci.h>
#include <armci_internals.h>
#include <debug.h>
#include <gmr.h>

#ifdef ENABLE_PROGRESS
#ifdef HAVE_PTHREADS
#include <pthread.h>

#if defined(HAVE_NANOSLEEP)

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif
#include <time.h>     /* nanosleep */
#include <sys/time.h> /* nanosleep */

#elif defined(HAVE_USLEEP)

#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h> /* usleep */
#endif

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
#endif /* HAVE_PTHREADS */
#endif /* ENABLE_PROGRESS */

/* Some of these are preprocessor symbols without the namespace... */
enum ARMCII_MPI_Impl_e { ARMCII_MPICH,
                         ARMCII_OPEN_MPI,
                         ARMCII_MVAPICH2,
                         ARMCII_INTEL_MPI,
                         ARMCII_CRAY_MPI,
                         ARMCII_UNKNOWN_MPI };

static void ARMCII_Parse_library_version(char * library_version, enum ARMCII_MPI_Impl_e * impl,
                                         int * major, int * minor, char * patch)
{
    *impl  = ARMCII_UNKNOWN_MPI;
    *major = -1;
    *minor = -1;

    int is_mpich = 0, is_ompi = 0, is_impi = 0;
    {
        char * p = NULL;
        p = strstr(library_version,"MPICH");
        is_mpich = (p != NULL);
        p = strstr(library_version,"Open MPI");
        is_ompi = (p != NULL);
        p = strstr(library_version,"Intel(R) MPI Library");
        is_impi = (p != NULL);
    }

    if (is_mpich) {
      *impl = ARMCII_MPICH;
      int mpich_major = 0;
      int mpich_minor = 0;
      char mpich_patch[4] = {0};
      for (int major = 9; major >= 3; major--) {
        for (int minor = 9; minor >= 0; minor--) {
          char version_string[4] = {0};
          sprintf(version_string,"%d.%d",major,minor);
          char * p = strstr(library_version,version_string);
          if (p != NULL) {
            mpich_major = atoi(p);
            mpich_minor = atoi(p+2);
            strncpy(mpich_patch,p+3,4);
            break;
          }
        }
      }
      *major = mpich_major;
      *minor = mpich_minor;
      strncpy(patch, mpich_patch, sizeof(mpich_patch));
    }

    if (is_ompi) {
      *impl = ARMCII_OPEN_MPI;
      int ompi_major = 0;
      int ompi_minor = 0;
      char ompi_patch[6] = {0}; /* ".PrcX" is max since major=2+ */
      for (int major = 9; major >= 2; major--) {
        for (int minor = 9; minor >= 0; minor--) {
          char version_string[4] = {0};
          sprintf(version_string,"%d.%d",major,minor);
          char * p = strstr(library_version,version_string);
          if (p != NULL) {
            ompi_major = atoi(p);
            ompi_minor = atoi(p+2);
            strncpy(ompi_patch,p+3,4);
            for (int c=0; c<sizeof(ompi_patch); c++) {
              if (ompi_patch[c] == ',') {
                ompi_patch[c] = '\0';
                break;
              }
            }
            break;
          }
        }
      }
      *major = ompi_major;
      *minor = ompi_minor;
      strncpy(patch, ompi_patch, sizeof(ompi_patch));
    }

    if (is_impi) {
      *impl = ARMCII_INTEL_MPI;
      int impi_major = 0;
      int impi_minor = 0;
      char impi_patch[6] = {0}; /* ".PrcX" is max since major=2+ */
      for (int major = 2030; major >= 2000; major--) {
        for (int minor = 30; minor >= 0; minor--) {
          char version_string[7] = {0};
          sprintf(version_string,"%d.%d",major,minor);
          char * p = strstr(library_version,version_string);
          if (p != NULL) {
            impi_major = atoi(p);
            impi_minor = atoi(p+5);
            break;
          }
        }
      }
      *major = impi_major;
      *minor = impi_minor;
      *patch = '\0';
    }
}

/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_Init_thread_comm = PARMCI_Init_thread_comm
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_Init_thread_comm ARMCI_Init_thread_comm
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_Init_thread_comm as PARMCI_Init_thread_comm
#endif
/* -- end weak symbols block -- */

/** Initialize ARMCI.  MPI must be initialized before this can be called.  It
  * invalid to make ARMCI calls before initialization.  Collective on the
  * communicator provided as input.
  *
  * @return            Zero on success
  */
int PARMCI_Init_thread_comm(int armci_requested, MPI_Comm comm) {

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

  ARMCII_GLOBAL_STATE.verbose = ARMCII_Getenv_int("ARMCI_VERBOSE", 0);

  /* Figure out what MPI library we are using, in an attempt to work around bugs. */
  char mpi_library_version[MPI_MAX_LIBRARY_VERSION_STRING] = {0};
  char mpi_library_version_short[32] = {0};
  enum ARMCII_MPI_Impl_e mpi_implementation;
  int mpi_impl_major = 0;
  int mpi_impl_minor = 0;
  char mpi_impl_patch[8] = {0};
  {
    int len;
    MPI_Get_library_version(mpi_library_version, &len);
    /* Truncate after 32 columns of 1 line to simplify parsing. */
    strncpy(mpi_library_version_short, mpi_library_version, 31);
    for (int c=0; c<sizeof(mpi_library_version_short); c++) {
      if (mpi_library_version[c] == '\r' || mpi_library_version[c] == '\n') {
        mpi_library_version_short[c] = '\0';
        break;
      }
    }
    ARMCII_Parse_library_version(mpi_library_version_short, &mpi_implementation,
                                 &mpi_impl_major, &mpi_impl_minor, mpi_impl_patch);
  }

  /* Check for MPI thread-support */
  {
    int mpi_thread_level;
    MPI_Query_thread(&mpi_thread_level);

    if (mpi_thread_level<armci_requested) {
      ARMCII_Error("MPI thread level below ARMCI thread level!");
    }

    ARMCII_GLOBAL_STATE.thread_level = armci_requested;

#ifdef ENABLE_PROGRESS
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
#endif /* HAVE_PTHREADS */
#endif /* ENABLE_PROGRESS */
  }

  /* Check for debugging flags */

  ARMCII_GLOBAL_STATE.debug_alloc          = ARMCII_Getenv_bool("ARMCI_DEBUG_ALLOC", 0);
  {
    int junk;
    junk = ARMCII_Getenv_bool("ARMCI_FLUSH_BARRIERS", -1);
    if (junk != -1) {
      ARMCII_Warning("ARMCI_FLUSH_BARRIERS is deprecated.\n");
    }
  }

  /* Group formation options */

#ifdef ARMCI_GROUP
  ARMCII_GLOBAL_STATE.noncollective_groups = 1;
#endif
  if (ARMCII_Getenv("ARMCI_NONCOLLECTIVE_GROUPS")) {
    ARMCII_GLOBAL_STATE.noncollective_groups = ARMCII_Getenv_bool("ARMCI_NONCOLLECTIVE_GROUPS", 0);
  }
  ARMCII_GLOBAL_STATE.cache_rank_translation=ARMCII_Getenv_bool("ARMCI_CACHE_RANK_TRANSLATION", 1);

  /* Check for IOV flags */

#ifdef NO_SEATBELTS
  ARMCII_GLOBAL_STATE.iov_checks           = 0;
#endif
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
    if (strcmp(var, "IOV") == 0) {
      ARMCII_GLOBAL_STATE.strided_method = ARMCII_STRIDED_IOV;
    } else if (strcmp(var, "DIRECT") == 0) {
      ARMCII_GLOBAL_STATE.strided_method = ARMCII_STRIDED_DIRECT;
    } else if (ARMCI_GROUP_WORLD.rank == 0) {
      ARMCII_Warning("Ignoring unknown value for ARMCI_STRIDED_METHOD (%s)\n", var);
    }
  }

#ifdef OPEN_MPI
  if (ARMCII_GLOBAL_STATE.iov_method == ARMCII_IOV_DIRECT || ARMCII_GLOBAL_STATE.strided_method == ARMCII_STRIDED_DIRECT) {
      ARMCII_Warning("MPI Datatypes are broken in RMA in many versions of Open-MPI!\n");
#if defined(OMPI_MAJOR_VERSION) && (OMPI_MAJOR_VERSION == 4)
      ARMCII_Warning("Open-MPI 4.0.0 RMA with datatypes is definitely broken.  See https://github.com/open-mpi/ompi/issues/6275 for details.\n");
#endif
  }
#endif

  /* Shared buffer handling method */

  /* The default used to be COPY.  NOGUARD requires MPI_WIN_UNIFIED. */
  ARMCII_GLOBAL_STATE.shr_buf_method = ARMCII_SHR_BUF_NOGUARD;

  var = ARMCII_Getenv("ARMCI_SHR_BUF_METHOD");
  if (var != NULL) {
    if (strcmp(var, "COPY") == 0) {
      ARMCII_GLOBAL_STATE.shr_buf_method = ARMCII_SHR_BUF_COPY;
    } else if (strcmp(var, "NOGUARD") == 0) {
      ARMCII_GLOBAL_STATE.shr_buf_method = ARMCII_SHR_BUF_NOGUARD;
    } else if (ARMCI_GROUP_WORLD.rank == 0) {
      ARMCII_Warning("Ignoring unknown value for ARMCI_SHR_BUF_METHOD (%s)\n", var);
    }
  }

  /* Use win_allocate or not, to work around MPI-3 RMA implementation bugs. */
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

  /* Pass alloc_shm=<this> to win_allocate / alloc_mem */
  ARMCII_GLOBAL_STATE.use_alloc_shm=ARMCII_Getenv_bool("ARMCI_USE_ALLOC_SHM", 1);

  /* Pass disable_shm_accumulate=<this> to window constructor */
  ARMCII_GLOBAL_STATE.disable_shm_accumulate=ARMCII_Getenv_bool("ARMCI_DISABLE_SHM_ACC", 0);

  /* Pass accumulate_ops = same_op info key to window constructor */
  ARMCII_GLOBAL_STATE.use_same_op=ARMCII_Getenv_bool("ARMCI_USE_SAME_OP", 0);

  /* Enable RMA element-wise atomicity (affects ARMCI Put/Get) */
  ARMCII_GLOBAL_STATE.rma_atomicity=ARMCII_Getenv_bool("ARMCI_RMA_ATOMICITY", 1);

  /* RMA ordering info key - this is the actual string we pass to MPI */
  ARMCII_Getenv_char(ARMCII_GLOBAL_STATE.rma_ordering, "ARMCI_RMA_ORDERING", "rar,raw,war,waw",
                     sizeof(ARMCII_GLOBAL_STATE.rma_ordering)-1);

  /* Flush_local becomes flush */
  ARMCII_GLOBAL_STATE.end_to_end_flush=ARMCII_Getenv_bool("ARMCI_NO_FLUSH_LOCAL", 0);

  /* Use MPI_MODE_NOCHECK assertion */
  ARMCII_GLOBAL_STATE.rma_nocheck=ARMCII_Getenv_bool("ARMCI_RMA_NOCHECK", 1);

  /* Setup groups and communicators */

  MPI_Comm_dup(comm, &ARMCI_GROUP_WORLD.comm);
  ARMCII_Group_init_from_comm(&ARMCI_GROUP_WORLD);
  ARMCI_GROUP_DEFAULT = ARMCI_GROUP_WORLD;

  /* Create GOP operators */

  MPI_Op_create(ARMCII_Absmin_op, 1 /* commute */, &ARMCI_MPI_ABSMIN_OP);
  MPI_Op_create(ARMCII_Absmax_op, 1 /* commute */, &ARMCI_MPI_ABSMAX_OP);

  MPI_Op_create(ARMCII_Msg_sel_min_op, 1 /* commute */, &ARMCI_MPI_SELMIN_OP);
  MPI_Op_create(ARMCII_Msg_sel_max_op, 1 /* commute */, &ARMCI_MPI_SELMAX_OP);

#ifdef HAVE_MEMKIND_H
  char * pool_path;
  if (ARMCII_GLOBAL_STATE.use_win_allocate == ARMCII_MEMKIND_WINDOW_TYPE) {
      if (ARMCII_GLOBAL_STATE.memory_limit == 0) {
          ARMCII_Error("MEMKIND requires a finite limit to create a memory pool!\n");
      } else {

          if (ARMCII_GLOBAL_STATE.memory_limit < MEMKIND_PMEM_MIN_SIZE) {
              ARMCII_Warning("ARMCI memory limit too low (%zu) for MEMKIND increasing to MEMKIND_PMEM_MIN_SIZE (%zu)\n",
                             ARMCII_GLOBAL_STATE.memory_limit, MEMKIND_PMEM_MIN_SIZE);
          }

          pool_path = ARMCII_Getenv("ARMCI_MEMKIND_POOL_PATH");
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

  if (ARMCII_GLOBAL_STATE.verbose > 0) {
    if (ARMCI_GROUP_WORLD.rank == 0) {
      int major, minor;

      MPI_Get_version(&major, &minor);

      printf("ARMCI-MPI initialized with %d process%s, MPI v%d.%d\n",
             ARMCI_GROUP_WORLD.size, ARMCI_GROUP_WORLD.size > 1 ? "es":"", major, minor);

      /* tell user what MPI library they are using */
      if (ARMCII_GLOBAL_STATE.verbose > 1) {
        printf("=======\n");
        printf("  MPI library version    = %s", mpi_library_version);
        printf("=======\n");
      } else {
        if (mpi_implementation == ARMCII_OPEN_MPI) {
          printf("  Open MPI version       = %d.%d%s\n", mpi_impl_major, mpi_impl_minor, mpi_impl_patch);
        }
        else if (mpi_implementation == ARMCII_MPICH) {
          printf("  MPICH version          = %d.%d%s\n", mpi_impl_major, mpi_impl_minor, mpi_impl_patch);
        }
        else if (mpi_implementation == ARMCII_INTEL_MPI) {
          printf("  Intel MPI version      = %d.%d%s\n", mpi_impl_major, mpi_impl_minor, mpi_impl_patch);
        }
        else if (mpi_implementation == ARMCII_UNKNOWN_MPI) {
          printf("  Unknown MPI version    = %s\n", mpi_library_version);
        }
      }

#ifdef NO_SEATBELTS
      printf("  NO_SEATBELTS           = ENABLED\n");
#endif

#ifdef ENABLE_PROGRESS
#ifdef HAVE_PTHREADS
      printf("  PROGRESS_THREAD        = %s\n", ARMCII_GLOBAL_STATE.progress_thread ? "ENABLED" : "DISABLED");
      if (ARMCII_GLOBAL_STATE.progress_thread) {
          printf("  PROGRESS_USLEEP        = %d\n", ARMCII_GLOBAL_STATE.progress_usleep);
      }
#endif /* HAVE_PTHREADS */
#endif /* ENABLE_PROGRESS */
      printf("  EXPLICIT_NB_PROGRESS   = %s\n", ARMCII_GLOBAL_STATE.explicit_nb_progress ? "ENABLED" : "DISABLED");

      if (ARMCII_GLOBAL_STATE.memory_limit > 0) {
          size_t limit  = ARMCII_GLOBAL_STATE.memory_limit;
          char suffix[4] = {0};
          int    offset = 0;
          if (limit && !(limit & (limit-1))) {
            char * bsuffix[5] = {"", "KiB","MiB","GiB","TiB"};
            for (int i=0; i<4; i++) {
              if (limit % 1024 == 0) {
                offset++;
                limit /= 1024;
              }
            }
            strncpy(suffix, bsuffix[offset], sizeof(suffix));
          } else {
            char * dsuffix[5] = {"","KB","MB","GB","TB"};
            for (int i=0; i<4; i++) {
              if (limit % 1000 == 0) {
                offset++;
                limit /= 1000;
              }
            }
            strncpy(suffix, dsuffix[offset], sizeof(suffix));
          }
          printf("  SHM_LIMIT              = %zu %s\n", limit, suffix);
      } else {
          printf("  SHM_LIMIT              = %s\n", "UNLIMITED");
      }

      if (ARMCII_GLOBAL_STATE.use_win_allocate == 0) {
          printf("  WINDOW type used       = %s\n", "CREATE");
      }
      else if (ARMCII_GLOBAL_STATE.use_win_allocate == 1) {
          printf("  WINDOW type used       = %s\n", "ALLOCATE");
      }
#ifdef HAVE_MEMKIND_H
      else if (ARMCII_GLOBAL_STATE.use_win_allocate == ARMCII_MEMKIND_WINDOW_TYPE) {
          printf("  WINDOW type used       = %s\n", "MEMKIND+CREATE");
          printf("  MEMKIND pool_path      = %s\n", pool_path);
      }
#endif
      else {
          ARMCII_Error("You have selected an invalid window type (%d)!\n", ARMCII_GLOBAL_STATE.use_win_allocate);
      }

      printf("  STRIDED_METHOD         = %s\n", ARMCII_Strided_methods_str[ARMCII_GLOBAL_STATE.strided_method]);
      printf("  IOV_METHOD             = %s\n", ARMCII_Iov_methods_str[ARMCII_GLOBAL_STATE.iov_method]);

      if (ARMCII_GLOBAL_STATE.iov_method == ARMCII_IOV_BATCHED || ARMCII_GLOBAL_STATE.iov_method == ARMCII_IOV_AUTO) {
        if (ARMCII_GLOBAL_STATE.iov_batched_limit > 0) {
          printf("  IOV_BATCHED_LIMIT      = %d\n", ARMCII_GLOBAL_STATE.iov_batched_limit);
        } else {
          printf("  IOV_BATCHED_LIMIT      = UNLIMITED\n");
        }
      }

      /* MPI RMA semantics */
      printf("  RMA_ATOMICITY          = %s\n", ARMCII_GLOBAL_STATE.rma_atomicity          ? "TRUE" : "FALSE");
      printf("  NO_FLUSH_LOCAL         = %s\n", ARMCII_GLOBAL_STATE.end_to_end_flush       ? "TRUE" : "FALSE");
      printf("  RMA_NOCHECK            = %s\n", ARMCII_GLOBAL_STATE.rma_nocheck            ? "TRUE" : "FALSE");

      /* MPI info set on window */
      printf("  USE_ALLOC_SHM          = %s\n", ARMCII_GLOBAL_STATE.use_alloc_shm          ? "TRUE" : "FALSE");
      printf("  DISABLE_SHM_ACC        = %s\n", ARMCII_GLOBAL_STATE.disable_shm_accumulate ? "TRUE" : "FALSE");
      printf("  USE_SAME_OP            = %s\n", ARMCII_GLOBAL_STATE.use_same_op            ? "TRUE" : "FALSE");
      printf("  RMA_ORDERING           = %s\n", ARMCII_GLOBAL_STATE.rma_ordering);

      /* ARMCI-MPI internal options */
      printf("  IOV_CHECKS             = %s\n", ARMCII_GLOBAL_STATE.iov_checks             ? "TRUE" : "FALSE");
      printf("  SHR_BUF_METHOD         = %s\n", ARMCII_Shr_buf_methods_str[ARMCII_GLOBAL_STATE.shr_buf_method]);
      printf("  NONCOLLECTIVE_GROUPS   = %s\n", ARMCII_GLOBAL_STATE.noncollective_groups   ? "TRUE" : "FALSE");
      printf("  CACHE_RANK_TRANSLATION = %s\n", ARMCII_GLOBAL_STATE.cache_rank_translation ? "TRUE" : "FALSE");
      printf("  DEBUG_ALLOC            = %s\n", ARMCII_GLOBAL_STATE.debug_alloc            ? "TRUE" : "FALSE");
      printf("\n");
      fflush(NULL);
    }

    if ((ARMCII_GLOBAL_STATE.use_win_allocate == 1) && (ARMCI_GROUP_WORLD.rank == 0)) {
        /* Jeff: Using win_allocate leads to correctness issues with some MPI implementations.*/
        /* This is required on Cray systems with CrayMPI 7.0.0 (at least). */
        /* Update (Feb. 2015): Xin and Min found the bug in Fetch_and_op and it is fixed in
         *      https://github.com/pmodels/mpich/commit/bad898f9df13b5060cbf43ee4acdb3b7b4c9a0f7 */
        /* Update (Aug. 2022): it has reappeared in MPICH 4.x, per
         *      https://github.com/pmodels/mpich/issues/6110 */
        printf("  Warning: MPI_Win_allocate can lead to correctness issues.\n");
        if ((mpi_implementation == ARMCII_MPICH) && (mpi_impl_major     == 4)) {
          printf("           There is a good chance your implementation is affected!\n");
          printf("           See https://github.com/pmodels/mpich/issues/6110 for details.\n");
        }
        printf("\n");
        fflush(NULL);
    }

    MPI_Barrier(ARMCI_GROUP_WORLD.comm);
  }

#ifdef ENABLE_PROGRESS
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
#endif /* HAVE_PTHREADS */
#endif /* ENABLE_PROGRESS */

  return 0;
}

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
  * is invalid to make ARMCI calls before initialization.  Collective on the
  * world group.
  *
  * @return            Zero on success
  */
int PARMCI_Init_thread(int armci_requested) {
  return PARMCI_Init_thread_comm(armci_requested, MPI_COMM_WORLD);
}


/* -- begin weak symbols block -- */
#if defined(HAVE_PRAGMA_WEAK)
#  pragma weak ARMCI_Init_mpi_comm = PARMCI_Init_mpi_comm
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#  pragma _HP_SECONDARY_DEF PARMCI_Init_mpi_comm ARMCI_Init_mpi_comm
#elif defined(HAVE_PRAGMA_CRI_DUP)
#  pragma _CRI duplicate ARMCI_Init_mpi_comm as PARMCI_Init_mpi_comm
#endif
/* -- end weak symbols block -- */

/** Initialize ARMCI.  MPI must be initialized before this can be called.  It
  * is invalid to make ARMCI calls before initialization.  Collective on the
  * world group.
  *
  * @return            Zero on success
  */
int PARMCI_Init_mpi_comm(MPI_Comm comm) {
  return PARMCI_Init_thread_comm(MPI_THREAD_SINGLE, comm);
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
  return PARMCI_Init_thread_comm(MPI_THREAD_SINGLE, MPI_COMM_WORLD);
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
  return PARMCI_Init_thread_comm(MPI_THREAD_SINGLE, MPI_COMM_WORLD);
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

#ifdef ENABLE_PROGRESS
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
#endif /* HAVE_PTHREADS */
#endif /* ENABLE_PROGRESS */

  nfreed = gmr_destroy_all();

  if (nfreed > 0 && ARMCI_GROUP_WORLD.rank == 0) {
    ARMCII_Warning("Freed %d leaked allocations\n", nfreed);
  }

  /* Free GOP operators */

  MPI_Op_free(&ARMCI_MPI_ABSMIN_OP);
  MPI_Op_free(&ARMCI_MPI_ABSMAX_OP);

  MPI_Op_free(&ARMCI_MPI_SELMIN_OP);
  MPI_Op_free(&ARMCI_MPI_SELMAX_OP);

  ARMCI_Cleanup();

  ARMCI_Group_free(&ARMCI_GROUP_WORLD);

  /* must come after gmr_destroy_all */
#ifdef HAVE_MEMKIND_H
  if (ARMCII_GLOBAL_STATE.use_win_allocate == ARMCII_MEMKIND_WINDOW_TYPE) {
      ARMCII_Assert(ARMCII_GLOBAL_STATE.memkind_handle != NULL);
      int err = memkind_destroy_kind(ARMCII_GLOBAL_STATE.memkind_handle);
      if (err) {
          ARMCII_Error("MEMKIND failed to create destroy a memory pool! (err=%d, errno=%d)\n", err, errno);
      }
  }
#endif

  return 0;
}


/** Cleaup ARMCI resources.  Call finalize instead.
  */
void ARMCI_Cleanup(void) {
  return;
}

