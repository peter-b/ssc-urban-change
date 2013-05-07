#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([SSC Urban Change Detection], [1.0], [p.brett@surrey.ac.uk])

AC_CONFIG_SRCDIR([ridge-changemap.c])
AC_CONFIG_AUX_DIR([build-tools])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([foreign silent-rules])
AM_SILENT_RULES

# Checks for programs
AC_PROG_CC
AC_PROG_CC_C99
PKG_PROG_PKG_CONFIG

# Checks for libraries
PKG_CHECK_MODULES([CAIRO], [cairo >= 1.10], [CAIRO=yes], [CAIRO=no])
if test "$CAIRO" = "yes"; then
  PKG_CHECK_MODULES([CAIRO_PNG], [cairo-png >= 1.10], ,
    AC_MSG_ERROR([Cairo PNG support 1.10.0 or later is required.]))
  PKG_CHECK_MODULES([CAIRO_PDF], [cairo-pdf >= 1.10], ,
    AC_MSG_ERROR([Cairo PDF support 1.10.0 or later is required.]))
  PKG_CHECK_MODULES([CAIRO_SVG], [cairo-svg >= 1.10], ,
    AC_MSG_ERROR([Cairo SVG support 1.10.0 or later is required.]))
else
  PKG_CHECK_MODULES([CAIRO], [cairo >= 1.8], [],
    AC_MSG_ERROR([Cairo 1.8.0 or later is required.]))
fi

PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.16], [],
  AC_MSG_ERROR([GLib 2.16.0 or later is required.]))
PKG_CHECK_MODULES([GSL], [gsl >= 1.13], [],
  AC_MSG_ERROR([GNU Scientific Library 1.13.0 or later is required.]))
PKG_CHECK_MODULES([RIDGETOOL], [libridgetool], [],
  AC_MSG_ERROR([SSC Ridge Tools Library is required.]))

AC_CHECK_LIB([m], [sqrt])
AC_CHECK_LIB([tiff], [TIFFOpen])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
