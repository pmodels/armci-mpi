#include <stdio.h>
#include <stdlib.h>

#include "debug.h"

char debug_cat_labels[][MAX_DEBUG_LABEL_LENGTH] = {
  "mem_region",
  "alloc",
  "mutex"
};

unsigned DEBUG_CATS_ENABLED = 0; // | DEBUG_CAT_ALLOC | DEBUG_CAT_MEM_REGION
//unsigned DEBUG_CATS_ENABLED = DEBUG_CAT_ALLOC;
