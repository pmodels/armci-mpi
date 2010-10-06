#include <stdio.h>
#include <stdlib.h>

#include "debug.h"

char debug_cat_labels[][MAX_DEBUG_LABEL_LENGTH] = {
  "mem_region",
  "alloc"
};

unsigned DEBUG_CATS_ENABLED = 0;
