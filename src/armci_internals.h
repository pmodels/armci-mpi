/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#ifndef HAVE_ARMCI_INTERNALS_H
#define HAVE_ARMCI_INTERNALS_H

#include <armciconf.h>

#if defined HAVE_STDIO_H
#include <stdio.h>
#endif /* HAVE_STDIO_H */

#if defined HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if defined HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if defined HAVE_STRINGS_H
#include <strings.h>
#endif /* HAVE_STRINGS_H */

#if defined HAVE_STDARG_H
#include <stdarg.h>
#endif /* HAVE_STDARG_H */

#if defined HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif /* HAVE_SYS_TYPES_H */

#if defined HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif /* HAVE_SYS_STAT_H */

#if defined HAVE_TIME_H
#include <time.h>
#endif /* HAVE_TIME_H */

#if defined HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */

#if defined HAVE_TYPES_H
#include <types.h>
#endif /* HAVE_TYPES_H */

#include "armci.h"

extern ARMCI_Group ARMCI_GROUP_WORLD;

#endif /* HAVE_ARMCI_INTERNALS_H */
