# ARMCI-MPI Profiling Library

## Build

The build system is trivial but is independent of `configure`.
You need to modify `ARMCI_INSTALL_PREFIX` to match your install
location, and change the compiler and flags in order to generate
a proper shared library.

## Usage

You can `LD_PRELOAD=libarmciprof.so` or you can left-link with
it explicitly when building your application, e.g. NWChem.

## Overhead

The memory overhead of the profiling interface is less than 4 KiB.
The time overhead is negligible.
The small numbers of branches and pointer chases are negligible
compared to what happens in Global Arrays,
the ARMCI-MPI `gmr` interface to MPI-3 RMA, and
the MPI-3 RMA implementation themselves.
If your processor implements `log10` and `log10f` so poorly
that their cost is noticeable, buy a new computer.
Alternatively, you can contribute an optimized implementation
of bin selection that doesn't use these operations.

## Output

Below is a real profile from NWChem.
Aggregate statistics are reported for all operations.
Blocking RMA operations also have a histogram, which
includes the time spent for different message sizes.
We use log-base-10 bins for this.
```
ARMCI-MPI profiling results:
Put called 385074501 times. time per call = 0.000045. total bytes = 864168127040, bandwidth = 0.049871 (GB/s)
size (B)| time (s)->1.0e-07, 1.0e-06, 1.0e-05, 1.0e-04, 1.0e-03, 1.0e-02, 1.0e-01, 1.0e+00, 1.0e+01, 1.0e+02,
10^0: 0, 8609, 2784256, 1772, 23, 0, 0, 0, 0, 0,
10^1: 0, 52569, 115934755, 51004, 960, 59, 0, 0, 0, 0,
10^2: 0, 188333, 227565546, 8528783, 150365, 6270, 297, 0, 0, 0,
10^3: 0, 251333, 23223623, 1743572, 128806, 30916, 1225, 0, 0, 0,
10^4: 0, 1060875, 2488961, 70199, 4147, 275, 0, 0, 0, 0,
10^5: 0, 0, 38620, 550153, 182120, 2257, 0, 0, 0, 0,
10^6: 0, 0, 0, 5681, 18127, 10, 0, 0, 0, 0,
10^7: 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
10^8: 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
Get called 0 times.
Acc called 427948594 times. time per call = 0.000105. total bytes = 12369608230416, bandwidth = 0.275264 (GB/s)
size (B)| time (s)->1.0e-07, 1.0e-06, 1.0e-05, 1.0e-04, 1.0e-03, 1.0e-02, 1.0e-01, 1.0e+00, 1.0e+01, 1.0e+02,
10^0: 0, 97905, 20717399, 429563, 19256, 242, 9, 0, 0, 0,
10^1: 0, 36741, 19040371, 716178, 17641, 225, 4, 0, 0, 0,
10^2: 0, 1840, 123215, 11248, 1789, 71, 6, 0, 0, 0,
10^3: 0, 72330, 60898914, 1806345, 185332, 13237, 3810, 0, 0, 0,
10^4: 0, 1809, 297878555, 22474763, 440042, 30224, 4674, 0, 0, 0,
10^5: 0, 0, 308, 790799, 834242, 59724, 7647, 0, 0, 0,
10^6: 0, 0, 0, 113643, 958966, 146152, 13375, 0, 0, 0,
10^7: 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
10^8: 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
NbPut called 0 times,
NbGet called 54686005 times, total bytes=7648089624560, avg,min,max=0.000585, 0.000006, 5.519342
NbAcc called 0 times,
Rmw called 1455667 times, total bytes=11645336, avg,min,max=0.000270, 0.000002, 0.042148
Barrier called 5789440 times, avg,min,max=0.014946, 0.000004, 15.658547
Wait called 54686005 times, avg,min,max=0.000002, 0.000000, 0.025128
Fence called 5789440 times, avg,min,max=0.000003, 0.000000, 0.012473
Locks called 0 times,
Global memory management called 988800 times, total bytes=526601141768, avg,min,max=0.007586, 0.000116, 0.155639
Local memory management called 404480 times, total bytes=8455736220160, avg,min,max=0.000003, 0.000000, 0.000469
```
