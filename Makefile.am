## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = $(LUA_INCLUDE)
AM_CFLAGS = $(WERROR_CFLAGS) $(WARN_CFLAGS)

lualib_LTLIBRARIES = curses.la

curses_la_SOURCES = lcurses.c
curses_la_LDFLAGS = -module $(CURSES_LIB)

ChangeLog:
	git2cl > ChangeLog

# FIXME: Make the following portable
LUA_ENV = LUA_INIT= LUA_PATH="$(abs_srcdir)/?.lua;;" LUA_CPATH="$(abs_srcdir)/.libs/?.so;;"

check-local:
	$(LUA_ENV) $(LUA) tests.lua

release: distcheck
	git diff --exit-code && \
	git push && \
	woger lua-l package=lcurses package_name=lcurses version="release $(VERSION)" description="Lua bindings for curses" notes=release-notes-$(VERSION)
	git tag -a -m "Release tag" rel-`echo $(VERSION) | sed -e 's/\./-/g'` && \
	git push --tags
	@cat release-notes-$(VERSION) && echo "\n\nDon't forget to release on LuaForge!"
