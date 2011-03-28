dnl Process this file with autoconf to produce a configure script

AC_INIT(lcurses, 4, rrt@sc3d.org)
AM_INIT_AUTOMAKE([-Wall -Werror std-options])

dnl Check for programs
AC_PROG_CC
if test "$GCC" = yes; then
  CFLAGS="$CFLAGS -W -Wmissing-prototypes -Wstrict-prototypes -D_FORTIFY_SOURCE=2"
fi
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

dnl Lua
AX_WITH_LUA
if test -z "$LUA"; then
  AC_MSG_FAILURE([Lua not found])
fi
AX_LUA_VERSION(501, 502)
AX_LUA_HEADERS
AX_LUA_HEADERS_VERSION(501, 502)
AX_LUA_LIBS
if test -z "$LUA_LIB"; then
  AC_MSG_FAILURE([liblua not found])
fi
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
