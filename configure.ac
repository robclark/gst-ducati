dnl required version of autoconf
AC_PREREQ([2.53])

dnl TODO: fill in your package name and package version here
AC_INIT([gst-ducati],[0.10.0])

dnl required versions of gstreamer and plugins-base
GST_MAJORMINOR=0.10
GST_REQUIRED=0.10.30

AC_CONFIG_SRCDIR([src/gstducati.c])
AC_CONFIG_HEADERS([config.h])

dnl required version of automake
AM_INIT_AUTOMAKE([1.10])

dnl enable mainainer mode by default
AM_MAINTAINER_MODE

dnl sets host_* variables
AC_CANONICAL_HOST

dnl use pretty build output with automake >= 1.11
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])],
  [AM_DEFAULT_VERBOSITY=1
   AC_SUBST(AM_DEFAULT_VERBOSITY)])

dnl check for tools
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_LIBTOOL

PKG_CHECK_MODULES([CHECK], [check], HAVE_CHECK=yes, HAVE_CHECK=no)

dnl Keep correct libtool macros in-tree.
AC_CONFIG_MACRO_DIR([m4])

dnl initialize GStreamer macros
AG_GST_INIT
dnl define an ERROR_CFLAGS Makefile variable
AG_GST_SET_ERROR_CFLAGS($GST_CVS)

dnl set up gettext
dnl the version check needs to stay here because autopoint greps for it
AM_GNU_GETTEXT_VERSION([0.17])
AM_GNU_GETTEXT([external])
AG_GST_GETTEXT([gst-ducati-$GST_MAJORMINOR])

dnl add GStreamer arguments
AG_GST_ARG_DEBUG
AG_GST_ARG_PROFILING
AG_GST_ARG_VALGRIND
AG_GST_ARG_GCOV

AG_GST_ARG_WITH_PKG_CONFIG_PATH
AG_GST_ARG_WITH_PACKAGE_NAME
AG_GST_ARG_WITH_PACKAGE_ORIGIN

dnl required version of libtool
LT_PREREQ([2.2.6])
LT_INIT

dnl give error and exit if we don't have pkgconfig
AC_CHECK_PROG(HAVE_PKGCONFIG, pkg-config, [ ], [
  AC_MSG_ERROR([You need to have pkg-config installed!])
])

dnl Check for tiler memmgr
PKG_CHECK_MODULES([MEMMGR], [libmemmgr])

dnl Check for required codec-engine library..
PKG_CHECK_MODULES(LIBDCE, [libdce >= 1.0.0])

dnl Check for the required version of GStreamer core (and gst-plugins-base)
dnl This will export GST_CFLAGS and GST_LIBS variables for use in Makefile.am
PKG_CHECK_MODULES(GST, [
  gstreamer-0.10 >= $GST_REQUIRED
  gstreamer-base-0.10 >= $GST_REQUIRED
], [
  AC_SUBST(GST_CFLAGS)
  AC_SUBST(GST_LIBS)
], [
  AC_MSG_ERROR([
      You need to install or upgrade the GStreamer development
      packages on your system. On debian-based systems these are
      libgstreamer0.10-dev and libgstreamer-plugins-base0.10-dev.
      on RPM-based systems gstreamer0.10-devel, libgstreamer0.10-devel
      or similar. The minimum version required is $GST_REQUIRED.
  ])
])

dnl set license and copyright notice
GST_LICENSE="LGPL"
AC_DEFINE_UNQUOTED(GST_LICENSE, "$GST_LICENSE", [GStreamer license])
AC_SUBST(GST_LICENSE)

dnl set GStreamer plug-in dir
AG_GST_SET_PLUGINDIR

dnl GST_OPTION_CFLAGS
if test "x$USE_DEBUG" = xyes; then
   PROFILE_CFLAGS="-g"
fi
AC_SUBST(PROFILE_CFLAGS)

DEPRECATED_CFLAGS="-DGST_DISABLE_DEPRECATED"
AC_SUBST(DEPRECATED_CFLAGS)

dnl every flag in GST_OPTION_CFLAGS can be overridden at make time
GST_OPTION_CFLAGS="\$(ERROR_CFLAGS) \$(DEBUG_CFLAGS) \$(PROFILE_CFLAGS) \$(GCOV_CFLAGS) \$(OPT_CFLAGS) \$(DEPRECATED_CFLAGS)"
AC_SUBST(GST_OPTION_CFLAGS)

dnl our libraries need to be versioned correctly
AC_SUBST(GST_LT_LDFLAGS)

dnl FIXME: do we want to rename to GST_ALL_* ?
dnl prefer internal headers to already installed ones
dnl also add builddir include for enumtypes and marshal
dnl add GST_OPTION_CFLAGS, but overridable
GST_CFLAGS="-I\$(top_srcdir)/gst-libs -I\$(top_builddir)/gst-libs $GST_CFLAGS \$(GST_OPTION_CFLAGS)"
AC_SUBST(GST_CFLAGS)
AC_SUBST(GST_LIBS)

dnl LDFLAGS really should only contain flags, not libs - they get added before
dnl whatevertarget_LIBS and -L flags here affect the rest of the linking
GST_ALL_LDFLAGS=""
AC_SUBST(GST_ALL_LDFLAGS)

dnl this really should only contain flags, not libs - they get added before
dnl whatevertarget_LIBS and -L flags here affect the rest of the linking
GST_PLUGIN_LDFLAGS="-module -avoid-version -export-symbols-regex '^[_]*gst_plugin_desc\$\$' $GST_ALL_LDFLAGS"
AC_SUBST(GST_PLUGIN_LDFLAGS)

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT

