AM_CPPFLAGS=-I$(top_srcdir)/lib
bin_PROGRAMS=drbcc
drbcc_SOURCES=main.c trace.h drbcc_sema.c drbcc_sema.h
drbcc_CPPFLAGS = -I../lib $(DRTRACE_CPPFLAGS)
drbcc_LDADD = ../lib/libdrbcc.la $(DRTRACE_LDFLAGS)
