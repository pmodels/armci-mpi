#ifndef _DEBUG_H_
#define _DEBUG_H_

enum debug_cats_e {
  DEBUG_CAT_MEM_REGION
};

#define MAX_DEBUG_LABEL_LENGTH 20
extern char debug_cat_labels[][MAX_DEBUG_LABEL_LENGTH];

extern unsigned DEBUG_CATS_ENABLED;


#ifdef NO_SEATBELTS /* Safety checks and debugging disabled */

#define assert(X) while(0)

static inline void dprint(unsigned category, char *format, ...) {
  return;
}


#else /* DEFAULT: Safety checks and debugging enabled */

#include <assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <mpi.h>

static inline void dprint(unsigned category, char *format, ...) {
  va_list ap;
  int rank;

  if ((category | DEBUG_CATS_ENABLED) == 0)
    return;

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  fprintf(stderr, "%d: ", rank);
  fprintf(stderr, format, ap);
}
#endif /* NO_SEATBELTS */


#endif /* _DEBUG_H_ */
