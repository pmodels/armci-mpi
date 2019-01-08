#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

check_PROGRAMS += \
                  tests/vmem/hello         \
                  # end

TESTS          += \
                  tests/vmem/hello         \
                  # end

tests_vmem_hello_LDADD = libarmci.la -lvmem
