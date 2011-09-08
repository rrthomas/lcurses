## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = $(LUA_INCLUDE)
AM_CFLAGS = $(WERROR_CFLAGS) $(WARN_CFLAGS)

lualib_LTLIBRARIES = curses_c.la
dist_luadata_DATA = curses.lua

curses_c_la_SOURCES = lcurses.c
curses_c_la_LDFLAGS = -module $(CURSES_LIB)

ChangeLog:
	git2cl > ChangeLog

LUA_ENV = LUA_INIT= LUA_PATH="$(abs_srcdir)/?.lua;;" LUA_CPATH="$(abs_srcdir)/$(objdir)/?$(shrext);;"

EXTRA_DIST = lcurses.html lcurses_c.html

lcurses_c.html: lcurses.c make_lcurses_doc.pl
	$(PERL) make_lcurses_doc.pl

check-local:
	$(LUA_ENV) $(LUA) tests.lua

release: distcheck
	git diff --exit-code && \
	git push && \
	woger lua-l package=$(PACKAGE) package_name=$(PACKAGE_NAME) version="release $(VERSION)" description="Lua bindings for curses" notes=release-notes-$(VERSION)
	git tag -a -m "Release tag" rel-`echo $(VERSION) | sed -e 's/\./-/g'` && \
	git push --tags
	@cat release-notes-$(VERSION) && echo "\n\nDon't forget to release on LuaForge!"
