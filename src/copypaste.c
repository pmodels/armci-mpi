/** WARNING: DO NOT DISTRIBUTE THIS FILE
  *
  * This file contains code that is copy-pasted from the original ARMCI
  * implementation.  It is copyright (c) Pacific Northwest National Laboratory
  * and Battelle Memorial Institute.
  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <armci.h>
#include <armci_internals.h>
#include <debug.h>

#define ARMCI_TAG 42

#define BUF_SIZE  (4*2048)
#define INFO_BUF_SIZE  (BUF_SIZE*sizeof(BUF_SIZE) - sizeof(double))

static double *work=NULL;

static void armci_sel(int type, char *op, void *x, void* mywork, int n)
{
int selected=0;
  switch (type) {
  case ARMCI_INT:
     if(strncmp(op,"min",3) == 0){ 
        if(*(int*)x > *(int*)mywork) selected=1;
     }else
        if(*(int*)x < *(int*)mywork) selected=1;
     break;
  case ARMCI_LONG:
     if(strncmp(op,"min",3) == 0){ 
        if(*(long*)x > *(long*)mywork) selected=1;
     }else
        if(*(long*)x < *(long*)mywork) selected=1;
     break;
  case ARMCI_LONG_LONG:
     if(strncmp(op,"min",3) == 0){ 
        if(*(long long*)x > *(long long*)mywork) selected=1;
     }else
        if(*(long long*)x < *(long long*)mywork) selected=1;
     break;
  case ARMCI_FLOAT:
     if(strncmp(op,"min",3) == 0){ 
        if(*(float*)x > *(float*)mywork) selected=1;
     }else
        if(*(float*)x < *(float*)mywork) selected=1;
     break;
  default:
     if(strncmp(op,"min",3) == 0){
        if(*(double*)x > *(double*)mywork) selected=1;
     }else
        if(*(double*)x < *(double*)mywork) selected=1;
  }
  if(selected)ARMCI_Copy(mywork,x, n);
}

/*\ global for  op with extra info 
\*/
void armci_msg_sel_scope(int scope, void *x, int n, char* op, int type, int contribute)
{
int root, up, left, right;
int tag=ARMCI_TAG;
int len, lenmes, min;

    if (work == NULL) {
      work = (double *)malloc(sizeof(double)*BUF_SIZE);
      ARMCII_Assert(work != NULL);
    }

    min = (strncmp(op,"min",3) == 0);
    if(!min && (strncmp(op,"max",3) != 0))
            ARMCII_Error("armci_msg_gop_info: operation not supported '%s'", op);

    if(!x)ARMCII_Error("armci_msg_gop_info: NULL pointer");

    if(n>INFO_BUF_SIZE)ARMCII_Error("armci_msg_gop_info: info too large");

    len = lenmes = n;

    armci_msg_bintree(scope, &root, &up, &left, &right);

    if (left > -1) {

        /* receive into work if contributing otherwise into x */
        if(contribute)armci_msg_rcv(tag, work, len, &lenmes, left);
        else armci_msg_rcv(tag, x, len, &lenmes, left);

        if(lenmes){
           if(contribute) armci_sel(type, op, x, work, n);
           else contribute =1; /* now we got data to pass */
        }
    }

    if (right > -1) {
        /* receive into work if contributing otherwise into x */
        if(contribute) armci_msg_rcv(tag, work, len, &lenmes, right);
        else armci_msg_rcv(tag, x, len, &lenmes, right);

        if(lenmes){
           if(contribute) armci_sel(type, op, x, work, n);
           else contribute =1; /* now we got data to pass */
        }
    }

    if (ARMCI_GROUP_WORLD.rank != root){
       if(contribute) armci_msg_snd(tag, x, len, up);
       else armci_msg_snd(tag, x, 0, up); /* send 0 bytes */
    }

    /* Now, root broadcasts the result down the binary tree */
    armci_msg_bcast_scope(scope, x, n, root);
}

