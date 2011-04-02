dnl Process this file with autoconf to produce a configure script

AC_INIT(lcurses, 5, rrt@sc3d.org)
AM_INIT_AUTOMAKE([-Wall -Werror std-options])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl Check for programs
AC_PROG_CC
gl_EARLY
AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)

dnl Check for header files
AC_HEADER_STDC

dnl Curses
AX_WITH_CURSES
if test "$ax_cv_curses" != "yes"; then
  AC_MSG_ERROR([cannot find curses])
fi
AC_ARG_VAR(CURSES_LIB, [linker flags for curses library])

dnl Lua 5.1
AX_PROG_LUA(501)
AX_LUA_HEADERS
AX_LUA_HEADERS_VERSION(501)
AX_LUA_LIBS
AC_SUBST(LUA)
AC_SUBST(LUA_INCLUDE)
AC_SUBST(LUA_LIB)

dnl Lua libraries
dnl FIXME: Parametrize on version
lualibdir='${libdir}/lua/5.1'
AC_SUBST(lualibdir)

dnl Generate output files
AC_CONFIG_MACRO_DIR(m4)
AC_CONFIG_HEADER(config.h)
AC_CONFIG_FILES(Makefile)
AC_OUTPUT
