#
# agent-ovs: Opflex agent for integrating Group-based policy with OVS
#
###########
#
# Process this file with autoconf to produce a configure script
#
# If you just want to start a build from source control, run
# autogen.sh first.
#

# ---------------------------------------------------------------
# Initialization

AC_INIT([agent-ovs], [1.0.0])
AC_SUBST(MODULE_NAME, ["Group-based Policy Agent for OVS"])

# initialize automake and libtool
AM_INIT_AUTOMAKE([subdir-objects])
AM_CONFIG_HEADER(config.h)
LT_INIT()
AC_PROG_LIBTOOL
AC_CONFIG_MACRO_DIR([m4])

m4_include([m4/ax_boost_base.m4])
m4_include([m4/ax_boost_unit_test_framework.m4])
m4_include([m4/ax_boost_program_options.m4])
m4_include([m4/ax_boost_thread.m4])
m4_include([m4/ax_boost_system.m4])
m4_include([m4/ax_boost_date_time.m4])
m4_include([m4/ax_boost_filesystem.m4])
m4_include([m4/ax_boost_iostreams.m4])
m4_include([m4/ax_boost_asio.m4])
m4_include([m4/ax_boost_random.m4])

dnl ---------------------------------------------------------------
dnl Configure options

dnl Modify the release/build version
AC_ARG_WITH(buildversion,
            AC_HELP_STRING([--with-buildversion], 
                           [Version number of build]),
            [bversion=${withval}],
            [bversion='private'])
AC_SUBST(BVERSION, [${bversion}])

dnl allow to create final builds with assert()s disabled
AC_HEADER_ASSERT

# ---------------------------------------------------------------
# Environment introspection

# check for compiler
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AM_PROG_AS
AC_LANG([C++])


# check for doxygen
AC_CHECK_PROGS(DOXYGEN,doxygen,none)
AM_CONDITIONAL(HAVE_DOXYGEN, [test x$DOXYGEN != 'xnone']) 

# ---------------------------------------------------------------
# Dependency checks

# Checks for header files
AC_STDC_HEADERS

AX_BOOST_BASE([1.53.0], [], AC_MSG_ERROR([Boost is required]))
AX_BOOST_RANDOM
if test "x$ax_cv_boost_random" != xyes; then
   AC_MSG_ERROR([Boost::Random library is required])
fi
AX_BOOST_PROGRAM_OPTIONS
if test "x$ax_cv_boost_program_options" != xyes; then
   AC_MSG_ERROR([Boost::Program_Options library is required])
fi
AX_BOOST_UNIT_TEST_FRAMEWORK
if test "x$ax_cv_boost_unit_test_framework" != xyes; then
   AC_MSG_ERROR([Boost::Test library is required])
fi
AX_BOOST_SYSTEM
if test "x$ax_cv_boost_system" != xyes; then
   AC_MSG_ERROR([Boost::System library is required])
fi
AX_BOOST_DATE_TIME
if test "x$ax_cv_boost_date_time" != xyes; then
   AC_MSG_ERROR([Boost::Date_Time library is required])
fi
AX_BOOST_THREAD
if test "x$ax_cv_boost_thread" != xyes; then
   AC_MSG_ERROR([Boost::Thread library is required])
fi
AX_BOOST_FILESYSTEM
if test "x$ax_cv_boost_filesystem" != xyes; then
   AC_MSG_ERROR([Boost::Filesystem library is required])
fi
AX_BOOST_IOSTREAMS
if test "x$ax_cv_boost_iostreams" != xyes; then
   AC_MSG_ERROR([Boost::IOStreams library is required])
fi
AX_BOOST_ASIO
if test "x$ax_cv_boost_asio" != xyes; then
   AC_MSG_ERROR([Boost::ASIO library is required])
fi

PKG_CHECK_MODULES([libopflex], [libopflex >= 0.1.0])
PKG_CHECK_MODULES([libmodelgbp], [libmodelgbp >= 0.1.0])

# check for libopenvswitch
PKG_CHECK_MODULES([libofproto], [libofproto >= 2.3.90])
PKG_CHECK_MODULES([libopenvswitch], [libopenvswitch >= 2.3.90])

# inotify check
AC_ARG_ENABLE(inotify, "Whether to use inotify for endpoint source")
if test x$enable_inotify = xno; then
    have_inotify=no;
else
    AC_CHECK_HEADERS(sys/inotify.h, HAVE_INOTIFY=no, HAVE_INOTIFY=yes)
fi

# eventfd check
AC_ARG_ENABLE(eventfd, "Whether to use eventfd for endpoint source")
if test x$enable_eventfd = xno; then
    have_eventfd=no;
else
    AC_CHECK_HEADERS(sys/eventfd.h, HAVE_EVENTFD=no, HAVE_EVENTFD=yes)
fi

# getifaddrs check
AC_ARG_ENABLE(getifaddrs, "Whether to use getifaddrs for interface enumeration")
if test x$enable_getifaddrs = xno; then
    have_getifaddrs=no;
else
    AC_CHECK_HEADERS(ifaddrs.h, HAVE_GETIFADDRS=no, HAVE_GETIFADDRS=yes)
fi

# Older versions of autoconf don't define docdir
if test x$docdir = x; then
   AC_SUBST(docdir, ['${prefix}/share/doc/'$PACKAGE])
fi

# ---------------------------------------------------------------
# Output

AC_CONFIG_FILES([\
	Makefile \
	doc/Doxyfile \
	rpm/agent-ovs.spec])
AC_OUTPUT

AC_MSG_NOTICE([
======================================================================
Configuration complete

You may now compile the software by running 'make'
======================================================================])
