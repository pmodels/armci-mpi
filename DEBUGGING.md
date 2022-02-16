# Debugging ARMCI-MPI

I hope you never need to read this, but obviously, debugging is sometimes necessary.

The reality is that MPI-3 RMA implementations still aren't perfect, 
and even if all the latest releases were bug-free, many users are using older builds.

This debugging guide will start with the easiest (least intrusive) options and progress
to the more complicated ones.

## Environment variables

ARMCI-MPI has a number of environment variables to work around known bugs in MPI libraries.
Some of these are so common that we change the settings automatically.

### Allocation

`ARMCI_USE_WIN_ALLOCATE` and `ARMCI_USE_ALLOC_SHM` default 1 but can be set to 0 
to debug whether RMA allocation is a problem.
Never set these unless you need to, because it will negatively impact performance in some cases.

In my experience, `ARMCI_USE_WIN_ALLOCATE` is extremely useful for debugging, whereas 
`ARMCI_USE_ALLOC_SHM` is not.  Many bugs have been mitigated by the former, but none by the latter.

`ARMCI_DEBUG_ALLOC=1` will zero allocations on creation, which will trigger segfaults inside
of the allocation routine rather than in user code, which might be useful.
This option hasn't been useful in many years.

### Datatypes

`ARMCI_STRIDED_METHOD` and `ARMCI_IOV_METHOD` control how non-contigous data is communicated.
These mitigate bugs in MPI datatypes.  NWChem makes heavier use of the former.

Bugs in datatypes lead to crashes during communication operations (`GA_Get`, `GA_Acc`, `GA_Put`)



