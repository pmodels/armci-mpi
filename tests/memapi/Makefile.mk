#
# Copyright (C) 2010. See COPYRIGHT in top-level directory.
#

check_PROGRAMS += \
                  tests/memapi/vmem_hello         \
                  # end

TESTS          += \
                  tests/memapi/vmem_hello         \
                  # end

tests_memapi_vmem_hello_LDADD = libarmci.la -lvmem
