dnl Process this file with autoconf to produce a configure script

AC_INIT(lcurses, 6, rrt@sc3d.org)
AM_INIT_AUTOMAKE([-Wall -Werror std-options])
AM_SILENT_RULES([yes])

dnl Check for programs
AC_PROG_CC
gl_EARLY
gl_MANYWARN_ALL_GCC([warnings])
# Set up the list of undesired warnings.
nw=
nw="$nw -Wsystem-headers"   # Don’t let system headers trigger warnings
nw="$nw -Wundef"            # All compiler preprocessors support #if UNDEF
nw="$nw -Wtraditional"      # All compilers nowadays support ANSI C
nw="$nw -Wstrict-overflow"  # Use a lower level (see below).
nw="$nw -Wconversion"       # These warnings usually don’t point to mistakes.
nw="$nw -Wsign-conversion"  # Likewise.
nw="$nw -Waggregate-return" # We do this on purpose.
nw="$nw -Wbad-function-cast" # FIXME: We do this on purpose (non-pointer list elements).
nw="$nw -Wtraditional-conversion" # Don't care.
nw="$nw -Wpadded"           # Don't care.
nw="$nw -Wc++-compat"       # Don't care.
nw="$nw -Woverlength-strings" # Don't care.
nw="$nw -Wmissing-format-attribute" # Don't care.
nw="$nw -Wunreachable-code" # Seems buggy.
# Enable all GCC warnings not in this list.
gl_MANYWARN_COMPLEMENT([warnings], [$warnings], [$nw])
for w in $warnings; do
  gl_WARN_ADD([$w])
done
# Add an extra warning
gl_WARN_ADD([-Wstrict-overflow=1], [WARN_FLAGS])
# Add some more safety measures
gl_WARN_ADD([-D_FORTIFY_SOURCE=2], [WARN_FLAGS])
gl_WARN_ADD([-fmudflap], [WARN_FLAGS])
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
AC_SUBST(LUA)
AC_SUBST(LUA_INCLUDE)

dnl Lua libraries
dnl FIXME: Parametrize on version
lualibdir='${libdir}/lua/5.1'
AC_SUBST(lualibdir)

dnl Generate output files
AC_CONFIG_MACRO_DIR(m4)
AC_CONFIG_HEADER(config.h)
AC_CONFIG_FILES(Makefile)
AC_OUTPUT
