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
