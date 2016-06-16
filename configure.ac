AC_PREREQ(2.60)
# Set name, version and contact
AC_INIT([libdrbcc], [0.1], [FOSS-projects@dresearch-fe.de])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_HEADERS([config.h])

#------------------------------------------------------
# Arguments

AC_ARG_WITH([libdrtrace-path],
	AS_HELP_STRING([--with-libdrtrace-path=PATH], [path prefix to libdrtrace]))

#------------------------------------------------------
# Headers/Libraries

## search for lockdev
AC_CHECK_HEADER(lockdev.h,,AC_MSG_WARN("library 'liblockdev' is missing - using /var/lock for locks"))
AC_CHECK_LIB(lockdev,dev_unlock,,AC_MSG_WARN("library 'liblockdev' is missing - using /var/lock for locks"))

# _AB_AC_FUNC_OPEN(O_BINARY|O_TEXT)
# ---------------------------------
AC_DEFUN([_AB_AC_FUNC_OPEN],
[AC_CACHE_CHECK([whether fcntl.h defines $1], [ab_cv_header_fcntl_h_$1],
[AC_EGREP_CPP([$1],
              [#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
$1
],
              [ab_cv_header_fcntl_h_$1=no],
              [ab_cv_header_fcntl_h_$1=yes])
if test "x$ab_cv_header_fcntl_h_$1" = xno; then
  AC_EGREP_CPP([_$1],
               [#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
_$1
],
                [ab_cv_header_fcntl_h_$1=0],
                [ab_cv_header_fcntl_h_$1=_$1])
fi])
if test "x$ab_cv_header_fcntl_h_$1" != xyes; then
  AC_DEFINE_UNQUOTED([$1], [$ab_cv_header_fcntl_h_$1],
    [Define this to a usable value if the system provides none])
fi[]dnl
])# _AB_AC_FUNC_OPEN


# AB_AC_FUNC_OPEN_BINARY
# ----------------------
# Test whether open accepts O_BINARY in the mode string for binary
# file opening.  This makes no difference on most unices, but some
# OSes convert every newline written to a file to two bytes (CR LF),
# and every CR LF read from a file is silently converted to a newline.
#
AC_DEFUN([AB_AC_FUNC_OPEN_BINARY], [_AB_AC_FUNC_OPEN([O_BINARY])])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AM_PROG_AR

LT_INIT

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([libgen.h unistd.h malloc.h fcntl.h errno.h sys/timeb.h signal.h unistd.h sys/select.h sys/ioctl.h termios.h readline/readline.h pthread.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_VOLATILE
#AC_TYPE_SIZE_T

# Check for additional libraries
AC_CHECK_LIB([pthreads], [pthread_create], [], [AC_CHECK_LIB([pthread], [pthread_create], [], [ AC_MSG_ERROR([*** Could not find pthread lib ***]) ])])
AC_CHECK_LIB([readline], [readline], [], [ AC_MSG_ERROR([*** Could not find libreadline ***]) ])
AC_CHECK_LIB([m], [sqrt], [], [ AC_MSG_ERROR([*** Could not find libm ***]) ])

# Checks for library functions
AC_CHECK_FUNCS([getopt_long memset basename])
AB_AC_FUNC_OPEN_BINARY

# libdrtrace
if test -n "$with_libdrtrace_path" ; then	
	DRTRACE_CPPFLAGS="-I$with_libdrtrace_path/include"
	DRTRACE_LDFLAGS="-L$with_libdrtrace_path/lib -ldrtrace"
	AC_DEFINE_UNQUOTED(HAVE_LIBDRTRACE, 1)
fi
AC_CHECK_LIB([drtrace], [drhip_trace_init], [], []) 

AC_SUBST(DRTRACE_LDFLAGS)
AC_SUBST(DRTRACE_CPPFLAGS)

# Checks for cc params
AC_DEFUN([CC_CHECK_CFLAGS_SILENT], [
  AC_CACHE_VAL(AS_TR_SH([cc_cv_cflags_$1]),
    [ac_save_CFLAGS="$CFLAGS"
     CFLAGS="$CFLAGS $1"
     AC_COMPILE_IFELSE([AC_LANG_SOURCE([int a;],
       [eval "AS_TR_SH([cc_cv_cflags_$1])='yes'"],
       [eval "AS_TR_SH([cc_cv_cflags_$1])='no'"])])
     CFLAGS="$ac_save_CFLAGS"
    ])

  AS_IF([eval test x$]AS_TR_SH([cc_cv_cflags_$1])[ = xyes], [$2], [$3])
])

AC_DEFUN([CC_CHECK_CFLAGS], [
  AC_CACHE_CHECK([if $CC supports $1 flag],
    AS_TR_SH([cc_cv_cflags_$1]),
    CC_CHECK_CFLAGS_SILENT([$1]) dnl Don't execute actions here!
  )

  AS_IF([eval test x$]AS_TR_SH([cc_cv_cflags_$1])[ = xyes], [$2], [$3])
])

CC_CHECK_CFLAGS([-Wall], [CFLAGS="$CFLAGS -Wall"])
CC_CHECK_CFLAGS([-Wextra], [CFLAGS="$CFLAGS -Wextra"])

#-------------------------------------------------------------------
# DOXYGEN SUPPORT

DX_HTML_FEATURE(ON)
DX_CHM_FEATURE(OFF)
DX_CHI_FEATURE(OFF)
DX_MAN_FEATURE(OFF)
DX_RTF_FEATURE(OFF)
DX_XML_FEATURE(OFF)
DX_PDF_FEATURE(ON)
DX_PS_FEATURE(OFF)

DX_INIT_DOXYGEN([$PACKAGE_NAME],[doxygen.cfg])

#-------------------------------------------------------------------
# OK, let's go
AC_CONFIG_FILES([Makefile lib/Makefile src/Makefile])
AC_OUTPUT

