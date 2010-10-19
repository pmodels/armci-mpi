#ifndef _ARMCI_MSG_H_
#define _ARMCI_MSG_H_

enum armci_scope_e { SCOPE_ALL, SCOPE_NODE, SCOPE_MASTERS};
enum armci_type_e  { ARMCI_INT, ARMCI_LONG, ARMCI_LONG_LONG, ARMCI_FLOAT, ARMCI_DOUBLE };

int  armci_msg_me();
int  armci_msg_nproc();

void armci_msg_snd(int tag, void* buffer, int len, int to);
void armci_msg_rcv(int tag, void* buffer, int buflen, int *msglen, int from);

void armci_msg_barrier();
void armci_msg_group_barrier(ARMCI_Group *group);

void armci_msg_bcast(void* buffer, int len, int root);
void armci_msg_group_bcast_scope(int scope, void *buf, int len, int root, ARMCI_Group *group);

void armci_msg_bintree(int scope, int* Root, int *Up, int *Left, int *Right);

// FIXME: armci_msg_sel unimplemented
#define armci_msg_sel(x,n,op,type,contribute)\
        armci_msg_sel_scope(SCOPE_ALL,(x),(n),(op),(type),(contribute)) 

void armci_msg_gop_scope(int scope, void *x, int n, char *op, int type);
void armci_msg_igop(int *x, int n, char *op);
void armci_msg_lgop(long *x, int n, char *op);
void armci_msg_llgop(long long *x, int n, char *op);
void armci_msg_fgop(float *x, int n, char *op);
void armci_msg_dgop(double *x, int n, char *op);

void armci_msg_group_gop_scope(int scope, void *x, int n, char *op, int type, ARMCI_Group *group);
void armci_msg_group_igop(int *x, int n, char *op, ARMCI_Group *group);
void armci_msg_group_lgop(long *x, int n, char *op, ARMCI_Group *group);
void armci_msg_group_llgop(long long *x, int n, char *op, ARMCI_Group *group);
void armci_msg_group_fgop(float *x, int n, char *op, ARMCI_Group *group);
void armci_msg_group_dgop(double *x, int n,char *op, ARMCI_Group *group);

#endif /* _ARMCI_MSG_H_ */
