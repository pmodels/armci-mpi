#include <armci.h>

int parmci_calls = 0;

int ARMCI_Init(void) {
  parmci_calls++;
  return PARMCI_Init();
}
   
int ARMCI_Finalize(void) {
  parmci_calls++;
  return PARMCI_Finalize();
}

void ARMCI_Barrier(void) {
  parmci_calls++;
#if defined(_CRAYC) || defined(__PGI)
  PARMCI_Barrier();
  return;
#else
  return PARMCI_Barrier();
#endif
}

int ARMCI_Get(void *src, void *dst, int size, int target) {
    parmci_calls++;
    return PARMCI_Get(src, dst, size, target);
}

int ARMCI_Put(void *src, void *dst, int size, int target) {
    parmci_calls++;
    return PARMCI_Put(src, dst, size, target);
}
