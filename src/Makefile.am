AM_CPPFLAGS=-I$(top_srcdir)/lib
bin_PROGRAMS=drbcc
drbcc_SOURCES=main.c dvmon.c dvmon.h trace.h drbcc_sema.c drbcc_sema.h
drbcc_CPPFLAGS = -I$(top_srcdir)/lib $(DRTRACE_CPPFLAGS) $(LIBEDIT_CFLAGS)
drbcc_LDADD = ../lib/libdrbcc.la $(DRTRACE_LDFLAGS) $(LIBEDIT_LIBS)
