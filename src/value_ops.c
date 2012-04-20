/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>

#include <armci.h>
#include <debug.h>

/* Put value operations */

int PARMCI_PutValueInt(int src, void *dst, int proc) {
  return PARMCI_Put(&src, dst, sizeof(int), proc);
}

int PARMCI_PutValueLong(long src, void *dst, int proc) {
  return PARMCI_Put(&src, dst, sizeof(long), proc);
}

int PARMCI_PutValueFloat(float src, void *dst, int proc) {
  return PARMCI_Put(&src, dst, sizeof(float), proc);
}

int PARMCI_PutValueDouble(double src, void *dst, int proc) {
  return PARMCI_Put(&src, dst, sizeof(double), proc);
}

/* Non-blocking put operations */

int PARMCI_NbPutValueInt(int src, void *dst, int proc, armci_hdl_t *hdl) {
  return PARMCI_NbPut(&src, dst, sizeof(int), proc, hdl);
}

int PARMCI_NbPutValueLong(long src, void *dst, int proc, armci_hdl_t *hdl) {
  return PARMCI_NbPut(&src, dst, sizeof(long), proc, hdl);
}

int PARMCI_NbPutValueFloat(float src, void *dst, int proc, armci_hdl_t *hdl) {
  return PARMCI_NbPut(&src, dst, sizeof(float), proc, hdl);
}

int PARMCI_NbPutValueDouble(double src, void *dst, int proc, armci_hdl_t *hdl) {
  return PARMCI_NbPut(&src, dst, sizeof(double), proc, hdl);
}

/* Get value operations */

int    PARMCI_GetValueInt(void *src, int proc) {
  int val;
  PARMCI_Get(src, &val, sizeof(int), proc);
  return val;
}

long   PARMCI_GetValueLong(void *src, int proc) {
  long val;
  PARMCI_Get(src, &val, sizeof(long), proc);
  return val;
}

float  PARMCI_GetValueFloat(void *src, int proc) {     
  float val;
  PARMCI_Get(src, &val, sizeof(float), proc);
  return val;
}

double PARMCI_GetValueDouble(void *src, int proc) {     
  double val;
  PARMCI_Get(src, &val, sizeof(double), proc);
  return val;
}
