/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>

#include <debug.h>

char debug_cat_labels[][MAX_DEBUG_LABEL_LENGTH] = {
  "mem_region",
  "alloc",
  "mutex"
};

unsigned DEBUG_CATS_ENABLED = 0;
//unsigned DEBUG_CATS_ENABLED = DEBUG_CAT_ALLOC;
//unsigned DEBUG_CATS_ENABLED = DEBUG_CAT_ALLOC | DEBUG_CAT_MEM_REGION;
//unsigned DEBUG_CATS_ENABLED = DEBUG_CAT_MUTEX;
//unsigned DEBUG_CATS_ENABLED = DEBUG_CAT_GROUPS;
//unsigned DEBUG_CATS_ENABLED = -1;


void ARMCII_Assert_fail(const char *expr, const char *file, int line, const char *func) {
  int rank;

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  fprintf(stderr, "%d: ARMCI assert fail in %s() [%s:%d]: \"%s\"\n", rank, func, file, line, expr);

#if HAVE_EXECINFO_H
  {
#include <execinfo.h>

    const int SIZE = 100;
    int    j, nframes;
    void  *frames[SIZE];
    char **symbols;

    nframes = backtrace(frames, SIZE);
    symbols = backtrace_symbols(frames, nframes);

    if (symbols == NULL)
      perror("Backtrace failure");

    printf("%d: Backtrace:\n", rank);
    for (j = 0; j < nframes; j++)
      printf("%d:   %d - %s\n", rank, nframes-j-1, symbols[j]);

    free(symbols);
  }
#endif

  MPI_Abort(MPI_COMM_WORLD, -1);
}
