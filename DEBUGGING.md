# Debugging ARMCI-MPI

I hope you never need to read this, but obviously, debugging is sometimes necessary.

The reality is that MPI-3 RMA implementations still aren't perfect, 
and even if all the latest releases were bug-free, many users are using older builds.

This debugging guide will start with the easiest (least intrusive) options and progress
to the more complicated ones.

## Environment variables

ARMCI-MPI has a number of environment variables to work around known bugs in MPI libraries.
Some of these are so common that we change the settings automatically.

Always set `ARMCI_VERBOSE=1` when you want to know what options are used.
I almost always run with this, because there's really no reason not to use it.
It adds about 10 lines to the beginning of your output stream.

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
rather than allocation functions.  They can also lead to incorrect results in especially bad
cases where the implementation is silently failing to do the right thing.

`ARMCI_STRIDED_METHOD` supports `DIRECT`, which uses datatypes, and `IOV`, which falls back on  
whatever `ARMCI_IOV_METHOD` is.  `ARMCI_IOV_METHOD` supports `AUTO` and `DIRECT`, which 
use datatypes, as well as `BATCHED` and `CONSRV` (conservative).  The latter two emit one
RMA operation for each contiguous chunk, with `CONSRV` synchronizing after each RMA operation.

The performance of `CONSRV` will be poor.  `BATCHED` is usually slower than `DIRECT` except
on platforms like Blue Gene, where the CPU was slow relative to the network.

Because Open-MPI has had numerous bugs in its datatype implementation over the years,
ARMCI-MPI sets `BATCHED` as the default at compile time when it is used.
Optimistic users can try datatypes by setting `DIRECT` explicitly.
For all other (usually MPICH-based) implementations, `DIRECT` is the default,
which are assumed to be correct except with the Galaxy Express network (Tianhe-2).

Enabling `ARMCI_IOV_CHECKS` will check the input for validity, which should not be useful
when debugging most GA programs.  It takes a rather perverse use case to trigger conflicts
here, which were encountered in the development of ARMCI-MPI but not since.

## MPI libraries

If you are using Open-MPI and having problems, please try an MPICH-based implementation instead.
If this solves the problem, then file a bug with Open-MPI and tell them to fix their
RMA and/or datatypes implementation again.

Open-MPI does not have a monopoly on RMA bugs, and we have observed RMA bugs in MPICH itself and
Cray MPI over the years.  Because ARMCI-MPI is tested regularly at Argonne (or at least I hope it is),
the MPICH bugs should be less common now.  Cray MPI is not tested regularly anymore but we assume
the bugs of the past are resolved.

While Intel MPI was problematic a few years ago, in the past five years, there have been significant
improvements in their implementation of RMA and we can recommend it in most cases.

MVAPICH2 is generally good at RMA and the recommended MPI-3 RMA implementation on InfiniBand platforms.

## Performance

Debugging RMA performance is not a simple task and not covered in this document.

If you are interested in this topic, email Jeff.
