#ifndef _ARMCI_MSG_H_
#define _ARMCI_MSG_H_

int  armci_msg_me();
int  armci_msg_nproc();
void armci_msg_bcast(void* buffer, int len, int root);
void armci_msg_dgop(double x[], int n, char *op);

#endif /* _ARMCI_MSG_H_ */
