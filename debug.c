#include <stdio.h>
#include <stdlib.h>

#include "debug.h"

char debug_cat_labels[][MAX_DEBUG_LABEL_LENGTH] = {
  "seg_hdl"
};

unsigned DEBUG_CATS_ENABLED = DEBUG_CAT_SEG_HDL;
