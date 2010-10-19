dnl
dnl Copyright (C) 2010. See COPYRIGHT in top-level directory.
dnl

AC_PREREQ(2.62)

AC_INIT([armci],[0])
AC_CONFIG_AUX_DIR(m4)
AC_CONFIG_MACRO_DIR(m4)
AM_INIT_AUTOMAKE([-Wall -Werror foreign 1.11 silent-rules])

LT_PREREQ([2.2.6])

# Bug in libtool adds -O2 and -g by default
PAC_PUSH_FLAG(CFLAGS)
LT_INIT(disable-shared)
PAC_POP_FLAG(CFLAGS)

if test -s "$srcdir/VERSION" ; then
    . $srcdir/VERSION
    export ARMCI_VERSION
else
    AC_MSG_ERROR([Version information not found. Configuration aborted.])
fi

# ABI version
AC_SUBST(libarmci_abi_version)

# Release version
# Produce a numeric version assuming the following format:
# Version: [MAJ].[MIN].[REV][EXT][EXT_NUMBER]
# Example: 1.0.7rc1 has
#          MAJ = 1
#          MIN = 0
#          REV = 7
#          EXT = rc
#          EXT_NUMBER = 1
#
# Converting to numeric version will convert EXT to a format number:
#          ALPHA (a) = 0
#          BETA (b)  = 1
#          RC (rc)   = 2
#          PATCH (p) = 3
# Regular releases are treated as patch 0
#
# Numeric version will have 1 digit for MAJ, 2 digits for MIN,
# 2 digits for REV, 1 digit for EXT and 2 digits for EXT_NUMBER.
changequote(<<,>>)
V1=`expr $ARMCI_VERSION : '\([0-9]*\)\.[0-9]*\.*[0-9]*[a-zA-Z]*[0-9]*'`
V2=`expr $ARMCI_VERSION : '[0-9]*\.\([0-9]*\)\.*[0-9]*[a-zA-Z]*[0-9]*'`
V3=`expr $ARMCI_VERSION : '[0-9]*\.[0-9]*\.*\([0-9]*\)[a-zA-Z]*[0-9]*'`
V4=`expr $ARMCI_VERSION : '[0-9]*\.[0-9]*\.*[0-9]*\([a-zA-Z]*\)[0-9]*'`
V5=`expr $ARMCI_VERSION : '[0-9]*\.[0-9]*\.*[0-9]*[a-zA-Z]*\([0-9]*\)'`
changequote([,])

if test "$V2" -le 9 ; then V2=0$V2 ; fi
if test "$V3" = "" ; then V3=0; fi
if test "$V3" -le 9 ; then V3=0$V3 ; fi
if test "$V4" = "a" ; then
    V4=0
elif test "$V4" = "b" ; then
    V4=1
elif test "$V4" = "rc" ; then
    V4=2
elif test "$V4" = "" ; then
    V4=3
    V5=0
elif test "$V4" = "p" ; then
    V4=3
fi
if test "$V5" -le 9 ; then V5=0$V5 ; fi

ARMCI_NUMVERSION=`expr $V1$V2$V3$V4$V5 + 0`
AC_SUBST(ARMCI_NUMVERSION)
AC_SUBST(ARMCI_VERSION)

AC_CONFIG_HEADER(include/armciconf.h)
AH_TOP([/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *  (C) 2010 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */
#ifndef ARMCICONF_H_INCLUDED
#define ARMCICONF_H_INCLUDED
])
AH_BOTTOM([#endif])

## Some of the autoconf macros automatically edit the CFLAGS and
## friends. We save and reset them to avoid this behavior. We want to
## control what flags get set.
PAC_PUSH_ALL_FLAGS
AC_PROG_CC
PAC_POP_ALL_FLAGS

PAC_ARG_STRICT
PAC_CC_FUNCTION_NAME_SYMBOL

## Check if the necessary headers are available
AC_CHECK_HEADERS(stdio.h stdlib.h string.h strings.h unistd.h stdarg.h sys/types.h \
		sys/stat.h time.h errno.h)

## Error checking functionality
AC_ARG_ENABLE(error-checking,
	AC_HELP_STRING([--enable-error-checking],[Enable error checking functionality]),
	enable_error_checking=$enableval,
	enable_error_checking=yes)
if test "$enable_error_checking" = "yes" ; then
   AC_DEFINE(ERROR_CHECKING,1,[Define if error checking is enabled])
fi

## Check if __VA_ARGS__ is defined by the compiler
PAC_C_MACRO_VA_ARGS

## Debugging support
AC_ARG_ENABLE(g, AC_HELP_STRING([--enable-g],[Enable Debugging]),
                 [ debug=$enableval ],
                 [ debug=no ])
AC_MSG_CHECKING(debugging support)
AC_MSG_RESULT($debug)
if test "$debug" = "yes"; then
   CFLAGS="$CFLAGS -g"
else
   AC_DEFINE(NO_SEATBELTS,1,[Define if debugging support is enabled])
fi

## Benchmarks
AC_ARG_ENABLE(benchmarks, AC_HELP_STRING([--enable-benchmarks],[Build Benchmarks]),
                 [ armci_build_benchmarks=$enableval ],
                 [ armci_build_benchmarks=no ])
AC_MSG_CHECKING(building benchmarks)
AC_MSG_RESULT($armci_build_benchmarks)
AM_CONDITIONAL([armci_build_benchmarks],[test "$armci_build_benchmarks" = "yes"])

## Tests
AC_ARG_ENABLE(tests, AC_HELP_STRING([--enable-tests],[Build Tests]),
                 [ armci_build_tests=$enableval ],
                 [ armci_build_tests=no ])
AC_MSG_CHECKING(building tests)
AC_MSG_RESULT($armci_build_tests)
AM_CONDITIONAL([armci_build_tests],[test "$armci_build_tests" = "yes"])

## Documentation
AC_PATH_PROG([DOXYGEN],[doxygen],,$PATH)
AC_SUBST(DOXYGEN)

AC_SUBST(top_srcdir)

## Final output
AC_OUTPUT(Makefile)
