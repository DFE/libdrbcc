lib_LTLIBRARIES=libdrbcc.la

# Remember to bump library versions before a release.
# Suppose the old version was CURRENT:REVISION:AGE.
# If you have:
# not changed the interface (bug fixes) -> CURRENT : REVISION+1 : AGE
# augmented the interface (new functions) -> CURRENT+1 : 0 : AGE+1
# broken old interface (e.g. removed functions) -> CURRENT+1 : 0 : 0
libdrbcc_la_LDFLAGS= -version-info 0:1:0

libdrbcc_la_SOURCES=drbcc.c drbcc_utils.c drbcc_ll.c drbcc_trace.h drbcc_utils.h drbcc_files.c

libdrbcc_la_CPPFLAGS = $(DRTRACE_CPPFLAGS)

include_HEADERS=drbcc.h drbcc_ll.h drbcc_com.h drbcc_files.h
