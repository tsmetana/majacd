dnl Written by Tomas Smetana, 28th October 2004
dnl Last modified: never
dnl Process this file with autoconf to produce a configure script.

AC_INIT(majacd,0.1,,)
AC_PREREQ(2.57)
AC_CONFIG_HEADERS(config.h)
AC_GNU_SOURCE
AM_INIT_AUTOMAKE
CFLAGS="${CFLAGS}"
AC_SUBST(CFLAGS)
AC_PROG_CC
AC_HEADER_STDC

AC_CHECK_HEADER([linux/cdrom.h])
AC_ARG_ENABLE([gui], AC_HELP_STRING([--disable-gui], [disable building the GUI]), , have_gui="yes")
if test "x$have_gui" == "xyes"; then
	PKG_CHECK_MODULES(GTK, gtk+-2.0 gdk-2.0, [GUI_DIR="src/gui"] , [])
	AC_SUBST(GUI_DIR)
fi

AC_OUTPUT([
Makefile
src/Makefile
src/gui/Makefile
majacd.spec
])


