/*************************************************************************
 * Library: lcurses - Lua 5.1 interface to the curses library            *
 *                                                                       *
 * (c) Reuben Thomas <rrt@sc3d.org> 2009-2010                            *
 * (c) Tiago Dionizio <tiago.dionizio AT gmail.com> 2004-2007            *
 *                                                                       *
 * Permission is hereby granted, free of charge, to any person obtaining *
 * a copy of this software and associated documentation files (the       *
 * "Software"), to deal in the Software without restriction, including   *
 * without limitation the rights to use, copy, modify, merge, publish,   *
 * distribute, sublicense, and/or sell copies of the Software, and to    *
 * permit persons to whom the Software is furnished to do so, subject to *
 * the following conditions:                                             *
 *                                                                       *
 * The above copyright notice and this permission notice shall be        *
 * included in all copies or substantial portions of the Software.       *
 *                                                                       *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       *
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  *
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  *
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     *
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                *
 ************************************************************************/

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#ifdef HAVE_NCURSES_H
#include <ncurses.h>
#else
#include <curses.h>
#endif
#include <term.h>


/*
** =======================================================
** defines
** =======================================================
*/
static const char *STDSCR_REGISTRY     = "curses:stdscr";
static const char *WINDOWMETA          = "curses:window";
static const char *CHSTRMETA           = "curses:chstr";
static const char *RIPOFF_TABLE        = "curses:ripoffline";

#define B(v) ((((int) (v)) == ERR))

/* ======================================================= */

#define LC_NUMBER(v)                        \
    static int lc_ ## v(lua_State *L)       \
    {                                       \
        lua_pushnumber(L, v());             \
        return 1;                           \
    }

#define LC_NUMBER2(n,v)                     \
    static int lc_ ## n(lua_State *L)       \
    {                                       \
        lua_pushnumber(L, v);               \
        return 1;                           \
    }

/* ======================================================= */

#define LC_STRING(v)                        \
    static int lc_ ## v(lua_State *L)       \
    {                                       \
        lua_pushstring(L, v());             \
        return 1;                           \
    }

#define LC_STRING2(n,v)                     \
    static int lc_ ## n(lua_State *L)       \
    {                                       \
        lua_pushstring(L, v);               \
        return 1;                           \
    }

/* ======================================================= */

#define LC_BOOL(v)                          \
    static int lc_ ## v(lua_State *L)       \
    {                                       \
        lua_pushboolean(L, v());            \
        return 1;                           \
    }

#define LC_BOOL2(n,v)                       \
    static int lc_ ## n(lua_State *L)       \
    {                                       \
        lua_pushboolean(L, v);              \
        return 1;                           \
    }

/* ======================================================= */

#define LC_BOOLOK(v)                        \
    static int lc_ ## v(lua_State *L)       \
    {                                       \
        lua_pushboolean(L, B(v()));         \
        return 1;                           \
    }

#define LC_BOOLOK2(n,v)                     \
    static int lc_ ## n(lua_State *L)       \
    {                                       \
        lua_pushboolean(L, B(v));           \
        return 1;                           \
    }

/* ======================================================= */

#define LCW_BOOLOK(n)                       \
    static int lcw_ ## n(lua_State *L)      \
    {                                       \
        WINDOW *w = lcw_check(L, 1);        \
        lua_pushboolean(L, B(n(w)));        \
        return 1;                           \
    }


/*
** =======================================================
** privates
** =======================================================
*/
static void lcw_new(lua_State *L, WINDOW *nw)
{
    if (nw)
    {
        WINDOW **w = lua_newuserdata(L, sizeof(WINDOW*));
        luaL_getmetatable(L, WINDOWMETA);
        lua_setmetatable(L, -2);
        *w = nw;
    }
    else
    {
        lua_pushliteral(L, "failed to create window");
        lua_error(L);
    }
}

static WINDOW **lcw_get(lua_State *L, int offset)
{
    WINDOW **w = (WINDOW**)luaL_checkudata(L, offset, WINDOWMETA);
    if (w == NULL) luaL_argerror(L, offset, "bad curses window");
    return w;
}

static WINDOW *lcw_check(lua_State *L, int offset)
{
    WINDOW **w = lcw_get(L, offset);
    if (*w == NULL) luaL_argerror(L, offset, "attempt to use closed curses window");
    return *w;
}

static int lcw_tostring(lua_State *L)
{
    WINDOW **w = lcw_get(L, 1);
    char buff[34];
    if (*w == NULL)
        strcpy(buff, "closed");
    else
        sprintf(buff, "%p", lua_touserdata(L, 1));
    lua_pushfstring(L, "curses window (%s)", buff);
    return 1;
}

/*
** =======================================================
** chtype handling
** =======================================================
*/
static chtype lc_checkch(lua_State *L, int offset)
{
    if (lua_type(L, offset) == LUA_TNUMBER)
        return (chtype)luaL_checknumber(L, offset);
    if (lua_type(L, offset) == LUA_TSTRING)
        return *lua_tostring(L, offset);

    luaL_typerror(L, offset, "chtype");
    /* never executes */
    return (chtype)0;
}

static chtype lc_optch(lua_State *L, int offset, chtype def)
{
    if (lua_isnoneornil(L, offset))
        return def;
    return lc_checkch(L, offset);
}

/****c* classes/chstr
 * FUNCTION
 *   Line drawing buffer.
 *
 * SEE ALSO
 *   curses.new_chstr
 ****/

typedef struct
{
    unsigned int len;
    chtype str[1];
} chstr;
#define CHSTR_SIZE(len) (sizeof(chstr) + len * sizeof(chtype))


/* create new chstr object and leave it in the lua stack */
static chstr* chstr_new(lua_State *L, int len)
{
    if (len < 1)
    {
        lua_pushliteral(L, "invalid chstr length");
        lua_error(L);
    }
    {
        chstr *cs = lua_newuserdata(L, CHSTR_SIZE(len));
        luaL_getmetatable(L, CHSTRMETA);
        lua_setmetatable(L, -2);
        cs->len = len;
        return cs;
    }
}

/* get chstr from lua (convert if needed) */
static chstr* lc_checkchstr(lua_State *L, int offset)
{
    chstr *cs = (chstr*)luaL_checkudata(L, offset, CHSTRMETA);
    if (cs) return cs;

    luaL_argerror(L, offset, "bad curses chstr");
    return NULL;
}

/****f* curses/curses.new_chstr
 * FUNCTION
 *   Create a new line drawing buffer instance.
 *
 * SEE ALSO
 *   chstr
 ****/
static int lc_new_chstr(lua_State *L)
{
    int len = luaL_checkint(L, 1);
    chstr* ncs = chstr_new(L, len);
    memset(ncs->str, ' ', len*sizeof(chtype));
    return 1;
}

/* change the contents of the chstr */
static int chstr_set_str(lua_State *L)
{
    chstr *cs = lc_checkchstr(L, 1);
    int offset = luaL_checkint(L, 2);
    const char *str = luaL_checkstring(L, 3);
    int len = lua_strlen(L, 3);
    int attr = (chtype)luaL_optnumber(L, 4, A_NORMAL);
    int rep = luaL_optint(L, 5, 1);
    int i;

    if (offset < 0)
        return 0;

    while (rep-- > 0 && offset <= (int)cs->len)
    {
        if (offset + len - 1 > (int)cs->len)
            len = cs->len - offset + 1;

        for (i = 0; i < len; ++i)
            cs->str[offset + i] = str[i] | attr;
        offset += len;
    }

    return 0;
}


/****m* chstr/set_ch
 * FUNCTION
 *   Set a character in the buffer.
 *
 * SYNOPSIS
 *   chstr:set_ch(offset, char, attribute [, repeat])
 *
 * EXAMPLE
 *   Set the buffer with 'a's where the first one is capitalized
 *   and has bold.
 *       size = 10
 *       str = curses.new_chstr(10)
 *       str:set_ch(0, 'A', curses.A_BOLD)
 *       str:set_ch(1, 'a', curses.A_NORMAL, size - 1)
 ****/
static int chstr_set_ch(lua_State *L)
{
    chstr* cs = lc_checkchstr(L, 1);
    int offset = luaL_checkint(L, 2);
    chtype ch = lc_checkch(L, 3);
    int attr = (chtype)luaL_optnumber(L, 4, A_NORMAL);
    int rep = luaL_optint(L, 5, 1);

    while (rep-- > 0)
    {
        if (offset < 0 || offset >= (int) cs->len)
            return 0;

        cs->str[offset] = ch | attr;

        ++offset;
    }
    return 0;
}

/* get information from the chstr */
static int chstr_get(lua_State *L)
{
    chstr* cs = lc_checkchstr(L, 1);
    int offset = luaL_checkint(L, 2);
    chtype ch;

    if (offset < 0 || offset >= (int) cs->len)
        return 0;

    ch = cs->str[offset];

    lua_pushnumber(L, ch & A_CHARTEXT);
    lua_pushnumber(L, ch & A_ATTRIBUTES);
    lua_pushnumber(L, ch & A_COLOR);
    return 3;
}

/* retrieve chstr length */
static int chstr_len(lua_State *L)
{
    chstr *cs = lc_checkchstr(L, 1);
    lua_pushnumber(L, cs->len);
    return 1;
}

/* duplicate chstr */
static int chstr_dup(lua_State *L)
{
    chstr *cs = lc_checkchstr(L, 1);
    chstr *ncs = chstr_new(L, cs->len);

    memcpy(ncs->str, cs->str, CHSTR_SIZE(cs->len));
    return 1;
}

/*
** =======================================================
** initscr
** =======================================================
*/

#define CCR(n, v)                       \
    lua_pushstring(L, n);               \
    lua_pushnumber(L, v);               \
    lua_settable(L, lua_upvalueindex(1));

#define CC(s)       CCR(#s, s)
#define CC2(s, v)   CCR(#s, v)

/*
** these values may be fixed only after initialization, so this is
** called from lc_initscr, after the curses driver is initialized
**
** curses table is kept at upvalue position 1, in case the global
** name is changed by the user or even in the registration phase by
** the developer
**
** some of these values are not constant so need to register
** them directly instead of using a table
*/
static void register_curses_constants(lua_State *L)
{
    /* colors */
    CC(COLOR_BLACK)     CC(COLOR_RED)       CC(COLOR_GREEN)
    CC(COLOR_YELLOW)    CC(COLOR_BLUE)      CC(COLOR_MAGENTA)
    CC(COLOR_CYAN)      CC(COLOR_WHITE)

    /* alternate character set */
    CC(ACS_BLOCK)       CC(ACS_BOARD)

    CC(ACS_BTEE)        CC(ACS_TTEE)
    CC(ACS_LTEE)        CC(ACS_RTEE)
    CC(ACS_LLCORNER)    CC(ACS_LRCORNER)
    CC(ACS_URCORNER)    CC(ACS_ULCORNER)

    CC(ACS_LARROW)      CC(ACS_RARROW)
    CC(ACS_UARROW)      CC(ACS_DARROW)

    CC(ACS_HLINE)       CC(ACS_VLINE)

    CC(ACS_BULLET)      CC(ACS_CKBOARD)     CC(ACS_LANTERN)
    CC(ACS_DEGREE)      CC(ACS_DIAMOND)

    CC(ACS_PLMINUS)     CC(ACS_PLUS)
    CC(ACS_S1)          CC(ACS_S9)

    /* attributes */
    CC(A_NORMAL)        CC(A_STANDOUT)      CC(A_UNDERLINE)
    CC(A_REVERSE)       CC(A_BLINK)         CC(A_DIM)
    CC(A_BOLD)          CC(A_PROTECT)       CC(A_INVIS)
    CC(A_ALTCHARSET)    CC(A_CHARTEXT)
    CC(A_ATTRIBUTES)

    /* key functions */
    CC(KEY_BREAK)       CC(KEY_DOWN)        CC(KEY_UP)
    CC(KEY_LEFT)        CC(KEY_RIGHT)       CC(KEY_HOME)
    CC(KEY_BACKSPACE)

    CC(KEY_DL)          CC(KEY_IL)          CC(KEY_DC)
    CC(KEY_IC)          CC(KEY_EIC)         CC(KEY_CLEAR)
    CC(KEY_EOS)         CC(KEY_EOL)         CC(KEY_SF)
    CC(KEY_SR)          CC(KEY_NPAGE)       CC(KEY_PPAGE)
    CC(KEY_STAB)        CC(KEY_CTAB)        CC(KEY_CATAB)
    CC(KEY_ENTER)       CC(KEY_SRESET)      CC(KEY_RESET)
    CC(KEY_PRINT)       CC(KEY_LL)          CC(KEY_A1)
    CC(KEY_A3)          CC(KEY_B2)          CC(KEY_C1)
    CC(KEY_C3)          CC(KEY_BTAB)        CC(KEY_BEG)
    CC(KEY_CANCEL)      CC(KEY_CLOSE)       CC(KEY_COMMAND)
    CC(KEY_COPY)        CC(KEY_CREATE)      CC(KEY_END)
    CC(KEY_EXIT)        CC(KEY_FIND)        CC(KEY_HELP)
    CC(KEY_MARK)        CC(KEY_MESSAGE)     CC(KEY_MOUSE)
    CC(KEY_MOVE)        CC(KEY_NEXT)        CC(KEY_OPEN)
    CC(KEY_OPTIONS)     CC(KEY_PREVIOUS)    CC(KEY_REDO)
    CC(KEY_REFERENCE)   CC(KEY_REFRESH)     CC(KEY_REPLACE)
    CC(KEY_RESIZE)      CC(KEY_RESTART)     CC(KEY_RESUME)
    CC(KEY_SAVE)        CC(KEY_SBEG)        CC(KEY_SCANCEL)
    CC(KEY_SCOMMAND)    CC(KEY_SCOPY)       CC(KEY_SCREATE)
    CC(KEY_SDC)         CC(KEY_SDL)         CC(KEY_SELECT)
    CC(KEY_SEND)        CC(KEY_SEOL)        CC(KEY_SEXIT)
    CC(KEY_SFIND)       CC(KEY_SHELP)       CC(KEY_SHOME)
    CC(KEY_SIC)         CC(KEY_SLEFT)       CC(KEY_SMESSAGE)
    CC(KEY_SMOVE)       CC(KEY_SNEXT)       CC(KEY_SOPTIONS)
    CC(KEY_SPREVIOUS)   CC(KEY_SPRINT)      CC(KEY_SREDO)
    CC(KEY_SREPLACE)    CC(KEY_SRIGHT)      CC(KEY_SRSUME)
    CC(KEY_SSAVE)       CC(KEY_SSUSPEND)    CC(KEY_SUNDO)
    CC(KEY_SUSPEND)     CC(KEY_UNDO)

    /* KEY_Fx  0 <= x <= 63 */
    CC(KEY_F0)              CC2(KEY_F1, KEY_F(1))   CC2(KEY_F2, KEY_F(2))
    CC2(KEY_F3, KEY_F(3))   CC2(KEY_F4, KEY_F(4))   CC2(KEY_F5, KEY_F(5))
    CC2(KEY_F6, KEY_F(6))   CC2(KEY_F7, KEY_F(7))   CC2(KEY_F8, KEY_F(8))
    CC2(KEY_F9, KEY_F(9))   CC2(KEY_F10, KEY_F(10)) CC2(KEY_F11, KEY_F(11))
    CC2(KEY_F12, KEY_F(12))
}

/*
** make sure screen is restored (and cleared) at exit
** (for the situations where program is aborted without a
** proper cleanup)
*/
static void cleanup(void)
{
    if (!isendwin())
    {
        wclear(stdscr);
        wrefresh(stdscr);
        endwin();
    }
}

static int lc_initscr(lua_State *L)
{
    WINDOW *w;

    /* initialize curses */
    w = initscr();

    /* no longer used, so clean it up */
    lua_pushstring(L, RIPOFF_TABLE);
    lua_pushnil(L);
    lua_settable(L, LUA_REGISTRYINDEX);

    /* failed to initialize */
    if (w == NULL)
        return 0;

    /* return stdscr - main window */
    lcw_new(L, w);

    /* save main window on registry */
    lua_pushstring(L, STDSCR_REGISTRY);
    lua_pushvalue(L, -2);
    lua_rawset(L, LUA_REGISTRYINDEX);

    /* setup curses constants - curses.xxx numbers */
    register_curses_constants(L);

    /* install cleanup handler to help in debugging and screen trashing */
    atexit(cleanup);

    return 1;
}

/* FIXME: Avoid cast to void. */
static int lc_endwin(lua_State *L)
{
    (void) L;
    endwin();
    return 0;
}

LC_BOOL(isendwin)

static int lc_stdscr(lua_State *L)
{
    lua_pushstring(L, STDSCR_REGISTRY);
    lua_rawget(L, LUA_REGISTRYINDEX);
    return 1;
}

LC_NUMBER2(COLS, COLS)
LC_NUMBER2(LINES, LINES)

/*
** =======================================================
** color
** =======================================================
*/

LC_BOOLOK(start_color)
LC_BOOL(has_colors)

static int lc_init_pair(lua_State *L)
{
    short pair = luaL_checkint(L, 1);
    short f = luaL_checkint(L, 2);
    short b = luaL_checkint(L, 3);

    lua_pushboolean(L, B(init_pair(pair, f, b)));
    return 1;
}

static int lc_pair_content(lua_State *L)
{
    short pair = luaL_checkint(L, 1);
    short f;
    short b;
    int ret = pair_content(pair, &f, &b);

    if (ret == ERR)
        return 0;

    lua_pushnumber(L, f);
    lua_pushnumber(L, b);
    return 2;
}

LC_NUMBER2(COLORS, COLORS)
LC_NUMBER2(COLOR_PAIRS, COLOR_PAIRS)

static int lc_COLOR_PAIR(lua_State *L)
{
    int n = luaL_checkint(L, 1);
    lua_pushnumber(L, COLOR_PAIR(n));
    return 1;
}

/*
** =======================================================
** termattrs
** =======================================================
*/

LC_NUMBER(baudrate)
LC_NUMBER(erasechar)
LC_BOOL(has_ic)
LC_BOOL(has_il)
LC_NUMBER(killchar)

static int lc_termattrs(lua_State *L)
{
    if (lua_gettop(L) < 1)
    {
        lua_pushnumber(L, termattrs());
    }
    else
    {
        int a = luaL_checkint(L, 1);
        lua_pushboolean(L, termattrs() & a);
    }
    return 1;
}

LC_STRING(termname)
LC_STRING(longname)

/*
** =======================================================
** kernel
** =======================================================
*/

/* there is no easy way to implement this... */
static lua_State *rip_L = NULL;
static int ripoffline_cb(WINDOW* w, int cols)
{
    static int line = 0;
    int top = lua_gettop(rip_L);

    /* better be safe */
    if (!lua_checkstack(rip_L, 5))
        return 0;

    /* get the table from the registry */
    lua_pushstring(rip_L, RIPOFF_TABLE);
    lua_gettable(rip_L, LUA_REGISTRYINDEX);

    /* get user callback function */
    if (lua_isnil(rip_L, -1)) {
        lua_pop(rip_L, 1);
        return 0;
    }

    lua_rawgeti(rip_L, -1, ++line); /* function to be called */
    lcw_new(rip_L, w);              /* create window object */
    lua_pushnumber(rip_L, cols);    /* push number of columns */

    lua_pcall(rip_L, 2,  0, 0);     /* call the lua function */

    lua_settop(rip_L, top);
    return 1;
}

static int lc_ripoffline(lua_State *L)
{
    static int rip = 0;
    int top_line = lua_toboolean(L, 1);

    if (!lua_isfunction(L, 2))
    {
        lua_pushliteral(L, "invalid callback passed as second parameter");
        lua_error(L);
    }

    /* need to save the lua state somewhere... */
    rip_L = L;

    /* get the table where we are going to save the callbacks */
    lua_pushstring(L, RIPOFF_TABLE);
    lua_gettable(L, LUA_REGISTRYINDEX);

    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        lua_newtable(L);

        lua_pushstring(L, RIPOFF_TABLE);
        lua_pushvalue(L, -2);
        lua_settable(L, LUA_REGISTRYINDEX);
    }

    /* save function callback in registry table */
    lua_pushvalue(L, 2);
    lua_rawseti(L, -2, ++rip);

    /* and tell curses we are going to take the line */
    lua_pushboolean(L, B(ripoffline(top_line ? 1 : -1, ripoffline_cb)));
    return 1;
}

static int lc_curs_set(lua_State *L)
{
    int vis = luaL_checkint(L, 1);
    int state = curs_set(vis);
    if (state == ERR)
        return 0;

    lua_pushnumber(L, state);
    return 1;
}

static int lc_napms(lua_State *L)
{
    int ms = luaL_checkint(L, 1);
    lua_pushboolean(L, B(napms(ms)));
    return 1;
}

/*
** =======================================================
** beep
** =======================================================
*/
LC_BOOLOK(beep)
LC_BOOLOK(flash)


/*
** =======================================================
** window
** =======================================================
*/

static int lc_newwin(lua_State *L)
{
    int nlines  = luaL_checkint(L, 1);
    int ncols   = luaL_checkint(L, 2);
    int begin_y = luaL_checkint(L, 3);
    int begin_x = luaL_checkint(L, 4);

    lcw_new(L, newwin(nlines, ncols, begin_y, begin_x));
    return 1;
}

static int lcw_delwin(lua_State *L)
{
    WINDOW **w = lcw_get(L, 1);
    if (*w != NULL && *w != stdscr)
    {
        delwin(*w);
        *w = NULL;
    }
    return 0;
}

static int lcw_mvwin(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkint(L, 2);
    int x = luaL_checkint(L, 3);
    lua_pushboolean(L, B(mvwin(w, y, x)));
    return 1;
}

static int lcw_subwin(lua_State *L)
{
    WINDOW *orig = lcw_check(L, 1);
    int nlines  = luaL_checkint(L, 2);
    int ncols   = luaL_checkint(L, 3);
    int begin_y = luaL_checkint(L, 4);
    int begin_x = luaL_checkint(L, 5);

    lcw_new(L, subwin(orig, nlines, ncols, begin_y, begin_x));
    return 1;
}

static int lcw_derwin(lua_State *L)
{
    WINDOW *orig = lcw_check(L, 1);
    int nlines  = luaL_checkint(L, 2);
    int ncols   = luaL_checkint(L, 3);
    int begin_y = luaL_checkint(L, 4);
    int begin_x = luaL_checkint(L, 5);

    lcw_new(L, derwin(orig, nlines, ncols, begin_y, begin_x));
    return 1;
}

static int lcw_mvderwin(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int par_y = luaL_checkint(L, 2);
    int par_x = luaL_checkint(L, 3);
    lua_pushboolean(L, B(mvderwin(w, par_y, par_x)));
    return 1;
}

static int lcw_dupwin(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    lcw_new(L, dupwin(w));
    return 1;
}

static int lcw_wsyncup(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    wsyncup(w);
    return 0;
}

static int lcw_syncok(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int bf = lua_toboolean(L, 2);
    lua_pushboolean(L, B(syncok(w, bf)));
    return 1;
}

static int lcw_wcursyncup(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    wcursyncup(w);
    return 0;
}

static int lcw_wsyncdown(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    wsyncdown(w);
    return 0;
}

/*
** =======================================================
** refresh
** =======================================================
*/
LCW_BOOLOK(wrefresh)
LCW_BOOLOK(wnoutrefresh)
LCW_BOOLOK(redrawwin)

static int lcw_wredrawln(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int beg_line = luaL_checkint(L, 2);
    int num_lines = luaL_checkint(L, 3);
    lua_pushboolean(L, B(wredrawln(w, beg_line, num_lines)));
    return 1;
}

LC_BOOLOK(doupdate)

/*
** =======================================================
** move
** =======================================================
*/

static int lcw_wmove(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkint(L, 2);
    int x = luaL_checkint(L, 3);
    lua_pushboolean(L, B(wmove(w, y, x)));
    return 1;
}

/*
** =======================================================
** scroll
** =======================================================
*/

static int lcw_wscrl(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int n = luaL_checkint(L, 2);
    lua_pushboolean(L, B(wscrl(w, n)));
    return 1;
}

/*
** =======================================================
** touch
** =======================================================
*/

static int lcw_touch(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int changed;
    if (lua_isnoneornil(L, 2))
        changed = TRUE;
    else
        changed = lua_toboolean(L, 2);

    if (changed)
        lua_pushboolean(L, B(touchwin(w)));
    else
        lua_pushboolean(L, B(untouchwin(w)));
    return 1;
}

static int lcw_touchline(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkint(L, 2);
    int n = luaL_checkint(L, 3);
    int changed;
    if (lua_isnoneornil(L, 4))
        changed = TRUE;
    else
        changed = lua_toboolean(L, 4);
    lua_pushboolean(L, B(wtouchln(w, y, n, changed)));
    return 1;
}

static int lcw_is_linetouched(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int line = luaL_checkint(L, 2);
    lua_pushboolean(L, is_linetouched(w, line));
    return 1;
}

static int lcw_is_wintouched(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    lua_pushboolean(L, is_wintouched(w));
    return 1;
}

/*
** =======================================================
** getyx
** =======================================================
*/

static int lcw_getyx(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y, x;
    getyx(w, y, x);
    lua_pushnumber(L, y);
    lua_pushnumber(L, x);
    return 2;
}

static int lcw_getparyx(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y, x;
    getparyx(w, y, x);
    lua_pushnumber(L, y);
    lua_pushnumber(L, x);
    return 2;
}

static int lcw_getbegyx(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y, x;
    getbegyx(w, y, x);
    lua_pushnumber(L, y);
    lua_pushnumber(L, x);
    return 2;
}

static int lcw_getmaxyx(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y, x;
    getmaxyx(w, y, x);
    lua_pushnumber(L, y);
    lua_pushnumber(L, x);
    return 2;
}

/*
** =======================================================
** border
** =======================================================
*/

static int lcw_wborder(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    chtype ls = lc_optch(L, 2, 0);
    chtype rs = lc_optch(L, 3, 0);
    chtype ts = lc_optch(L, 4, 0);
    chtype bs = lc_optch(L, 5, 0);
    chtype tl = lc_optch(L, 6, 0);
    chtype tr = lc_optch(L, 7, 0);
    chtype bl = lc_optch(L, 8, 0);
    chtype br = lc_optch(L, 9, 0);

    lua_pushnumber(L, B(wborder(w, ls, rs, ts, bs, tl, tr, bl, br)));
    return 1;
}

static int lcw_box(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    chtype verch = lc_checkch(L, 2);
    chtype horch = lc_checkch(L, 3);

    lua_pushnumber(L, B(box(w, verch, horch)));
    return 1;
}

static int lcw_whline(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    chtype ch = lc_checkch(L, 2);
    int n = luaL_checkint(L, 3);

    lua_pushnumber(L, B(whline(w, ch, n)));
    return 1;
}

static int lcw_wvline(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    chtype ch = lc_checkch(L, 2);
    int n = luaL_checkint(L, 3);

    lua_pushnumber(L, B(wvline(w, ch, n)));
    return 1;
}


static int lcw_mvwhline(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkint(L, 2);
    int x = luaL_checkint(L, 3);
    chtype ch = lc_checkch(L, 4);
    int n = luaL_checkint(L, 5);

    lua_pushnumber(L, B(mvwhline(w, y, x, ch, n)));
    return 1;
}

static int lcw_mvwvline(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkint(L, 2);
    int x = luaL_checkint(L, 3);
    chtype ch = lc_checkch(L, 4);
    int n = luaL_checkint(L, 5);

    lua_pushnumber(L, B(mvwvline(w, y, x, ch, n)));
    return 1;
}

/*
** =======================================================
** clear
** =======================================================
*/

LCW_BOOLOK(werase)
LCW_BOOLOK(wclear)
LCW_BOOLOK(wclrtobot)
LCW_BOOLOK(wclrtoeol)

/*
** =======================================================
** slk
** =======================================================
*/
static int lc_slk_init(lua_State *L)
{
    int fmt = luaL_checkint(L, 1);
    lua_pushboolean(L, B(slk_init(fmt)));
    return 1;
}

static int lc_slk_set(lua_State *L)
{
    int labnum = luaL_checkint(L, 1);
    const char* label = luaL_checkstring(L, 2);
    int fmt = luaL_checkint(L, 3);

    lua_pushboolean(L, B(slk_set(labnum, label, fmt)));
    return 1;
}

LC_BOOLOK(slk_refresh)
LC_BOOLOK(slk_noutrefresh)

static int lc_slk_label(lua_State *L)
{
    int labnum = luaL_checkint(L, 1);
    lua_pushstring(L, slk_label(labnum));
    return 1;
}

LC_BOOLOK(slk_clear)
LC_BOOLOK(slk_restore)
LC_BOOLOK(slk_touch)

static int lc_slk_attron(lua_State *L)
{
    chtype attrs = lc_checkch(L, 1);
    lua_pushboolean(L, B(slk_attron(attrs)));
    return 1;
}

static int lc_slk_attroff(lua_State *L)
{
    chtype attrs = lc_checkch(L, 1);
    lua_pushboolean(L, B(slk_attroff(attrs)));
    return 1;
}

static int lc_slk_attrset(lua_State *L)
{
    chtype attrs = lc_checkch(L, 1);
    lua_pushboolean(L, B(slk_attrset(attrs)));
    return 1;
}

/*
** =======================================================
** addch
** =======================================================
*/

static int lcw_waddch(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    chtype ch = lc_checkch(L, 2);
    lua_pushboolean(L, B(waddch(w, ch)));
    return 1;
}

static int lcw_mvwaddch(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkint(L, 2);
    int x = luaL_checkint(L, 3);
    chtype ch = lc_checkch(L, 4);

    lua_pushboolean(L, B(mvwaddch(w, y, x, ch)));
    return 1;
}

static int lcw_wechochar(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    chtype ch = lc_checkch(L, 2);

    lua_pushboolean(L, B(wechochar(w, ch)));
    return 1;
}

/*
** =======================================================
** addchstr
** =======================================================
*/

static int lcw_waddchnstr(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int n = luaL_optint(L, 3, -1);
    chstr *cs = lc_checkchstr(L, 2);

    if (n < 0 || n > (int) cs->len)
        n = cs->len;

    lua_pushboolean(L, B(waddchnstr(w, cs->str, n)));
    return 1;
}

static int lcw_mvwaddchnstr(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkint(L, 2);
    int x = luaL_checkint(L, 3);
    int n = luaL_optint(L, 5, -1);
    chstr *cs = lc_checkchstr(L, 4);

    if (n < 0 || n > (int) cs->len)
        n = cs->len;

    lua_pushboolean(L, B(mvwaddchnstr(w, y, x, cs->str, n)));
    return 1;
}

/*
** =======================================================
** addstr
** =======================================================
*/

static int lcw_waddnstr(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    const char *str = luaL_checkstring(L, 2);
    int n = luaL_optint(L, 3, -1);

    if (n < 0) n = lua_strlen(L, 2);

    lua_pushboolean(L, B(waddnstr(w, str, n)));
    return 1;
}

static int lcw_mvwaddnstr(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkint(L, 2);
    int x = luaL_checkint(L, 3);
    const char *str = luaL_checkstring(L, 4);
    int n = luaL_optint(L, 5, -1);

    if (n < 0) n = lua_strlen(L, 4);

    lua_pushboolean(L, B(mvwaddnstr(w, y, x, str, n)));
    return 1;
}

/*
** =======================================================
** bkgd
** =======================================================
*/

static int lcw_wbkgdset(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    chtype ch = lc_checkch(L, 2);
    wbkgdset(w, ch);
    return 0;
}

static int lcw_wbkgd(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    chtype ch = lc_checkch(L, 2);
    lua_pushboolean(L, B(wbkgd(w, ch)));
    return 1;
}

static int lcw_getbkgd(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    lua_pushnumber(L, B(getbkgd(w)));
    return 1;
}

/*
** =======================================================
** inopts
** =======================================================
*/

static int lc_cbreak(lua_State *L)
{
    if (lua_isnoneornil(L, 1) || lua_toboolean(L, 1))
        lua_pushboolean(L, B(cbreak()));
    else
        lua_pushboolean(L, B(nocbreak()));
    return 1;
}

static int lc_echo(lua_State *L)
{
    if (lua_isnoneornil(L, 1) || lua_toboolean(L, 1))
        lua_pushboolean(L, B(echo()));
    else
        lua_pushboolean(L, B(noecho()));
    return 1;
}

static int lc_raw(lua_State *L)
{
    if (lua_isnoneornil(L, 1) || lua_toboolean(L, 1))
        lua_pushboolean(L, B(raw()));
    else
        lua_pushboolean(L, B(noraw()));
    return 1;
}

static int lc_halfdelay(lua_State *L)
{
    int tenths = luaL_checkint(L, 1);
    lua_pushboolean(L, B(halfdelay(tenths)));
    return 1;
}

static int lcw_intrflush(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int bf = lua_toboolean(L, 2);
    lua_pushboolean(L, B(intrflush(w, bf)));
    return 1;
}

static int lcw_keypad(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int bf = lua_isnoneornil(L, 2) ? 1 : lua_toboolean(L, 2);
    lua_pushboolean(L, B(keypad(w, bf)));
    return 1;
}

static int lcw_meta(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int bf = lua_toboolean(L, 2);
    lua_pushboolean(L, B(meta(w, bf)));
    return 1;
}

static int lcw_nodelay(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int bf = lua_toboolean(L, 2);
    lua_pushboolean(L, B(nodelay(w, bf)));
    return 1;
}

static int lcw_timeout(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int delay = luaL_checkint(L, 2);
    wtimeout(w, delay);
    return 0;
}

static int lcw_notimeout(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int bf = lua_toboolean(L, 2);
    lua_pushboolean(L, B(notimeout(w, bf)));
    return 1;
}

/*
** =======================================================
** outopts
** =======================================================
*/

static int lc_nl(lua_State *L)
{
    if (lua_isnoneornil(L, 1) || lua_toboolean(L, 1))
        lua_pushboolean(L, B(nl()));
    else
        lua_pushboolean(L, B(nonl()));
    return 1;
}

static int lcw_clearok(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int bf = lua_toboolean(L, 2);
    lua_pushboolean(L, B(clearok(w, bf)));
    return 1;
}

static int lcw_idlok(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int bf = lua_toboolean(L, 2);
    lua_pushboolean(L, B(idlok(w, bf)));
    return 1;
}

static int lcw_leaveok(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int bf = lua_toboolean(L, 2);
    lua_pushboolean(L, B(leaveok(w, bf)));
    return 1;
}

static int lcw_scrollok(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int bf = lua_toboolean(L, 2);
    lua_pushboolean(L, B(scrollok(w, bf)));
    return 1;
}

static int lcw_idcok(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int bf = lua_toboolean(L, 2);
    idcok(w, bf);
    return 0;
}

static int lcw_immedok(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int bf = lua_toboolean(L, 2);
    immedok(w, bf);
    return 0;
}

static int lcw_wsetscrreg(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int top = luaL_checkint(L, 2);
    int bot = luaL_checkint(L, 3);
    lua_pushboolean(L, B(wsetscrreg(w, top, bot)));
    return 1;
}

/*
** =======================================================
** overlay
** =======================================================
*/

static int lcw_overlay(lua_State *L)
{
    WINDOW *srcwin = lcw_check(L, 1);
    WINDOW *dstwin = lcw_check(L, 2);

    lua_pushboolean(L, B(overlay(srcwin, dstwin)));
    return 1;
}

static int lcw_overwrite(lua_State *L)
{
    WINDOW *srcwin = lcw_check(L, 1);
    WINDOW *dstwin = lcw_check(L, 2);

    lua_pushboolean(L, B(overwrite(srcwin, dstwin)));
    return 1;
}

static int lcw_copywin(lua_State *L)
{
    WINDOW *srcwin = lcw_check(L, 1);
    WINDOW *dstwin = lcw_check(L, 2);
    int sminrow = luaL_checkint(L, 3);
    int smincol = luaL_checkint(L, 4);
    int dminrow = luaL_checkint(L, 5);
    int dmincol = luaL_checkint(L, 6);
    int dmaxrow = luaL_checkint(L, 7);
    int dmaxcol = luaL_checkint(L, 8);
    int woverlay = lua_toboolean(L, 9);

    lua_pushboolean(L, B(copywin(srcwin, dstwin, sminrow,
        smincol, dminrow, dmincol, dmaxrow, dmaxcol, woverlay)));

    return 1;
}

/*
** =======================================================
** util
** =======================================================
*/

static int lc_unctrl(lua_State *L)
{
    chtype c = (chtype)luaL_checknumber(L, 1);
    lua_pushstring(L, unctrl(c));
    return 1;
}

static int lc_keyname(lua_State *L)
{
    int c = luaL_checkint(L, 1);
    lua_pushstring(L, keyname(c));
    return 1;
}

static int lc_delay_output(lua_State *L)
{
    int ms = luaL_checkint(L, 1);
    lua_pushboolean(L, B(delay_output(ms)));
    return 1;
}

static int lc_flushinp(lua_State *L)
{
    lua_pushboolean(L, B(flushinp()));
    return 1;
}

/*
** =======================================================
** delch
** =======================================================
*/

LCW_BOOLOK(wdelch)

static int lcw_mvwdelch(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkint(L, 2);
    int x = luaL_checkint(L, 3);

    lua_pushboolean(L, B(mvwdelch(w, y, x)));
    return 1;
}

/*
** =======================================================
** deleteln
** =======================================================
*/

LCW_BOOLOK(wdeleteln)
LCW_BOOLOK(winsertln)

static int lcw_winsdelln(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int n = luaL_checkint(L, 2);
    lua_pushboolean(L, B(winsdelln(w, n)));
    return 1;
}

/*
** =======================================================
** getch
** =======================================================
*/

static int lcw_wgetch(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int c = wgetch(w);

    if (c == ERR) return 0;

    lua_pushnumber(L, c);
    return 1;
}

static int lcw_mvwgetch(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkint(L, 2);
    int x = luaL_checkint(L, 3);
    int c;

    if (wmove(w, y, x) == ERR) return 0;

    c = wgetch(w);

    if (c == ERR) return 0;

    lua_pushnumber(L, c);
    return 1;
}

static int lc_ungetch(lua_State *L)
{
    int c = luaL_checkint(L, 1);
    lua_pushboolean(L, B(ungetch(c)));
    return 1;
}

/*
** =======================================================
** getstr
** =======================================================
*/

static int lcw_wgetnstr(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int n = luaL_optint(L, 2, 0);
    char buf[LUAL_BUFFERSIZE];

    if (n == 0 || n >= LUAL_BUFFERSIZE) n = LUAL_BUFFERSIZE - 1;
    if (wgetnstr(w, buf, n) == ERR)
        return 0;

    lua_pushstring(L, buf);
    return 1;
}

static int lcw_mvwgetnstr(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkint(L, 2);
    int x = luaL_checkint(L, 3);
    int n = luaL_optint(L, 4, -1);
    char buf[LUAL_BUFFERSIZE];

    if (n == 0 || n >= LUAL_BUFFERSIZE) n = LUAL_BUFFERSIZE - 1;
    if (mvwgetnstr(w, y, x, buf, n) == ERR)
        return 0;

    lua_pushstring(L, buf);
    return 1;
}

/*
** =======================================================
** inch
** =======================================================
*/

static int lcw_winch(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    lua_pushnumber(L, winch(w));
    return 1;
}

static int lcw_mvwinch(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkint(L, 2);
    int x = luaL_checkint(L, 3);
    lua_pushnumber(L, mvwinch(w, y, x));
    return 1;
}

/*
** =======================================================
** inchstr
** =======================================================
*/

static int lcw_winchnstr(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int n = luaL_checkint(L, 2);
    chstr *cs = chstr_new(L, n);

    if (winchnstr(w, cs->str, n) == ERR)
        return 0;

    return 1;
}

static int lcw_mvwinchnstr(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkint(L, 2);
    int x = luaL_checkint(L, 3);
    int n = luaL_checkint(L, 4);
    chstr *cs = chstr_new(L, n);

    if (mvwinchnstr(w, y, x, cs->str, n) == ERR)
        return 0;

    return 1;
}

/*
** =======================================================
** instr
** =======================================================
*/

static int lcw_winnstr(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int n = luaL_checkint(L, 2);
    char buf[LUAL_BUFFERSIZE];

    if (n >= LUAL_BUFFERSIZE) n = LUAL_BUFFERSIZE - 1;
    if (winnstr(w, buf, n) == ERR)
        return 0;

    lua_pushlstring(L, buf, n);
    return 1;
}

static int lcw_mvwinnstr(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkint(L, 2);
    int x = luaL_checkint(L, 3);
    int n = luaL_checkint(L, 4);
    char buf[LUAL_BUFFERSIZE];

    if (n >= LUAL_BUFFERSIZE) n = LUAL_BUFFERSIZE - 1;
    if (mvwinnstr(w, y, x, buf, n) == ERR)
        return 0;

    lua_pushlstring(L, buf, n);
    return 1;
}

/*
** =======================================================
** insch
** =======================================================
*/

static int lcw_winsch(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    chtype ch = lc_checkch(L, 2);
    lua_pushboolean(L, B(winsch(w, ch)));
    return 1;
}

static int lcw_mvwinsch(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkint(L, 2);
    int x = luaL_checkint(L, 3);
    chtype ch = lc_checkch(L, 4);
    lua_pushboolean(L, B(mvwinsch(w, y, x, ch)));
    return 1;
}

/*
** =======================================================
** insstr
** =======================================================
*/

static int lcw_winsstr(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    const char *str = luaL_checkstring(L, 2);
    lua_pushboolean(L, B(winsnstr(w, str, lua_strlen(L, 2))));
    return 1;
}

static int lcw_mvwinsstr(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkint(L, 2);
    int x = luaL_checkint(L, 3);
    const char *str = luaL_checkstring(L, 4);
    lua_pushboolean(L, B(mvwinsnstr(w, y, x, str, lua_strlen(L, 2))));
    return 1;
}

static int lcw_winsnstr(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    const char *str = luaL_checkstring(L, 2);
    int n = luaL_checkint(L, 3);
    lua_pushboolean(L, B(winsnstr(w, str, n)));
    return 1;
}

static int lcw_mvwinsnstr(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int y = luaL_checkint(L, 2);
    int x = luaL_checkint(L, 3);
    const char *str = luaL_checkstring(L, 4);
    int n = luaL_checkint(L, 5);
    lua_pushboolean(L, B(mvwinsnstr(w, y, x, str, n)));
    return 1;
}

/*
** =======================================================
** pad
** =======================================================
*/

static int lc_newpad(lua_State *L)
{
    int nlines = luaL_checkint(L, 1);
    int ncols = luaL_checkint(L, 2);
    lcw_new(L, newpad(nlines, ncols));
    return 1;
}

static int lcw_subpad(lua_State *L)
{
    WINDOW *orig = lcw_check(L, 1);
    int nlines  = luaL_checkint(L, 2);
    int ncols   = luaL_checkint(L, 3);
    int begin_y = luaL_checkint(L, 4);
    int begin_x = luaL_checkint(L, 5);

    lcw_new(L, subpad(orig, nlines, ncols, begin_y, begin_x));
    return 1;
}

static int lcw_prefresh(lua_State *L)
{
    WINDOW *p = lcw_check(L, 1);
    int pminrow = luaL_checkint(L, 2);
    int pmincol = luaL_checkint(L, 3);
    int sminrow = luaL_checkint(L, 4);
    int smincol = luaL_checkint(L, 5);
    int smaxrow = luaL_checkint(L, 6);
    int smaxcol = luaL_checkint(L, 7);

    lua_pushboolean(L, B(prefresh(p, pminrow, pmincol,
        sminrow, smincol, smaxrow, smaxcol)));
    return 1;
}

static int lcw_pnoutrefresh(lua_State *L)
{
    WINDOW *p = lcw_check(L, 1);
    int pminrow = luaL_checkint(L, 2);
    int pmincol = luaL_checkint(L, 3);
    int sminrow = luaL_checkint(L, 4);
    int smincol = luaL_checkint(L, 5);
    int smaxrow = luaL_checkint(L, 6);
    int smaxcol = luaL_checkint(L, 7);

    lua_pushboolean(L, B(pnoutrefresh(p, pminrow, pmincol,
        sminrow, smincol, smaxrow, smaxcol)));
    return 1;
}


static int lcw_pechochar(lua_State *L)
{
    WINDOW *p = lcw_check(L, 1);
    chtype ch = lc_checkch(L, 2);

    lua_pushboolean(L, B(pechochar(p, ch)));
    return 1;
}

/*
** =======================================================
** attr
** =======================================================
*/

static int lcw_wattroff(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int attrs = luaL_checkint(L, 2);
    lua_pushboolean(L, B(wattroff(w, attrs)));
    return 1;
}

static int lcw_wattron(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int attrs = luaL_checkint(L, 2);
    lua_pushboolean(L, B(wattron(w, attrs)));
    return 1;
}

static int lcw_wattrset(lua_State *L)
{
    WINDOW *w = lcw_check(L, 1);
    int attrs = luaL_checkint(L, 2);
    lua_pushboolean(L, B(wattrset(w, attrs)));
    return 1;
}

LCW_BOOLOK(wstandend)
LCW_BOOLOK(wstandout)


/*
** =======================================================
** query terminfo database
** =======================================================
*/

static char ti_capname[32];

static int ti_getflag (lua_State *L)
{
    int res;

    strlcpy (ti_capname, luaL_checkstring (L, 1), sizeof (ti_capname));
    res = tigetflag (ti_capname);
    if (-1 == res)
        return luaL_error (L, "`%s' is not a boolean capability", ti_capname);
    else
        lua_pushboolean (L, res);
    return 1;
}

static int ti_getnum (lua_State *L)
{
    int res;

    strlcpy (ti_capname, luaL_checkstring (L, 1), sizeof (ti_capname));
    res = tigetnum (ti_capname);
    if (-2 == res)
        return luaL_error (L, "`%s' is not a numeric capability", ti_capname);
    else if (-1 == res)
        lua_pushnil (L);
    else
        lua_pushnumber(L, res);
    return 1;
}

static int ti_getstr (lua_State *L)
{
    const char *res;

    strlcpy (ti_capname, luaL_checkstring (L, 1), sizeof (ti_capname));
    res = tigetstr (ti_capname);
    if ((char *) -1 == res)
        return luaL_error (L, "`%s' is not a string capability", ti_capname);
    else if (NULL == res)
        lua_pushnil (L);
    else
        lua_pushstring(L, res);
    return 1;
}


/*
** =======================================================
** register functions
** =======================================================
*/
/* chstr members */
static const luaL_reg chstrlib[] =
{
    { "len",        chstr_len       },
    { "set_ch",     chstr_set_ch    },
    { "set_str",    chstr_set_str   },
    { "get",        chstr_get       },
    { "dup",        chstr_dup       },

    { NULL, NULL }
};

#define EWF(name) { #name, lcw_ ## name },
static const luaL_reg windowlib[] =
{
    /* window */
    { "close", lcw_delwin  },
    { "sub", lcw_subwin },
    { "derive", lcw_derwin },
    { "move_window", lcw_mvwin },
    { "move_derived", lcw_mvderwin },
    { "clone", lcw_dupwin },
    { "syncup", lcw_wsyncup },
    { "syncdown", lcw_wsyncdown },
    { "syncok", lcw_syncok },
    { "cursyncup", lcw_wcursyncup },

    /* inopts */
    EWF(intrflush)
    EWF(keypad)
    EWF(meta)
    EWF(nodelay)
    EWF(timeout)
    EWF(notimeout)

    /* outopts */
    EWF(clearok)
    EWF(idlok)
    EWF(leaveok)
    EWF(scrollok)
    EWF(idcok)
    EWF(immedok)
    EWF(wsetscrreg)

    /* pad */
    EWF(subpad)
    EWF(prefresh)
    EWF(pnoutrefresh)
    EWF(pechochar)

    /* move */
    { "move", lcw_wmove },

    /* scroll */
    { "scrl", lcw_wscrl },

    /* refresh */
    { "refresh", lcw_wrefresh },
    { "noutrefresh", lcw_wnoutrefresh },
    { "redrawwin", lcw_redrawwin },
    { "redrawln", lcw_wredrawln },

    /* clear */
    { "erase", lcw_werase },
    { "clear", lcw_wclear },
    { "clrtobot", lcw_wclrtobot },
    { "clrtoeol", lcw_wclrtoeol },

    /* touch */
    { "touch", lcw_touch },
    { "touchline", lcw_touchline },
    { "is_linetouched", lcw_is_linetouched },
    { "is_wintouched", lcw_is_wintouched },

    /* attrs */
    { "attroff", lcw_wattroff },
    { "attron", lcw_wattron },
    { "attrset", lcw_wattrset },
    { "standout", lcw_wstandout },
    { "standend", lcw_wstandend },

    /* getch */
    { "getch", lcw_wgetch },
    { "mvgetch", lcw_mvwgetch },

    /* getyx */
    EWF(getyx)
    EWF(getparyx)
    EWF(getbegyx)
    EWF(getmaxyx)

    /* border */
    { "border", lcw_wborder },
    { "box", lcw_box },
    { "hline", lcw_whline },
    { "vline", lcw_wvline },
    { "mvhline", lcw_mvwhline },
    { "mvvline", lcw_mvwvline },

    /* addch */
    { "addch", lcw_waddch },
    { "mvaddch", lcw_mvwaddch },
    { "echoch", lcw_wechochar },

    /* addchstr */
    { "addchstr", lcw_waddchnstr },
    { "mvaddchstr", lcw_mvwaddchnstr },

    /* addstr */
    { "addstr", lcw_waddnstr },
    { "mvaddstr", lcw_mvwaddnstr },

    /* bkgd */
    EWF(wbkgdset)
    EWF(wbkgd)
    EWF(getbkgd)

    /* overlay */
    { "overlay", lcw_overlay },
    { "overwrite", lcw_overwrite },
    { "copywin", lcw_copywin },

    /* delch */
    { "delch", lcw_wdelch },
    { "mvdelch", lcw_mvwdelch },

    /* deleteln */
    { "deleteln", lcw_wdeleteln },
    { "insertln", lcw_winsertln },
    EWF(winsdelln)

    /* getstr */
    { "getstr", lcw_wgetnstr },
    { "mvgetstr", lcw_mvwgetnstr },

    /* inch */
    EWF(winch)
    EWF(mvwinch)
    EWF(winchnstr)
    EWF(mvwinchnstr)

    /* instr */
    EWF(winnstr)
    EWF(mvwinnstr)

    /* insch */
    EWF(winsch)
    EWF(mvwinsch)

    /* insstr */
    EWF(winsstr)
    EWF(winsnstr)
    EWF(mvwinsstr)
    EWF(mvwinsnstr)

    /* misc */
    {"__gc",        lcw_delwin  }, /* rough safety net */
    {"__tostring",  lcw_tostring},
    {NULL, NULL}
};

#define ECF(name) { #name, lc_ ## name },
static const luaL_reg curseslib[] =
{
    /* chstr helper function */
    { "new_chstr",      lc_new_chstr    },

    /* initscr */
    { "endwin",         lc_endwin       },
    { "isendwin",       lc_isendwin     },
    { "stdscr",         lc_stdscr       },
    { "cols",           lc_COLS         },
    { "lines",          lc_LINES        },

    /* color */
    { "start_color",    lc_start_color  },
    { "has_colors",     lc_has_colors   },
    { "init_pair",      lc_init_pair    },
    { "pair_content",   lc_pair_content },
    { "colors",         lc_COLORS       },
    { "color_pairs",    lc_COLOR_PAIRS  },
    { "color_pair",     lc_COLOR_PAIR   },

    /* termattrs */
    { "baudrate",       lc_baudrate     },
    { "erasechar",      lc_erasechar    },
    { "killchar",       lc_killchar     },
    { "has_ic",         lc_has_ic       },
    { "has_il",         lc_has_il       },
    { "termattrs",      lc_termattrs    },
    { "termname",       lc_termname     },
    { "longname",       lc_longname     },

    /* kernel */
    { "ripoffline",     lc_ripoffline   },
    { "napms",          lc_napms        },
    { "curs_set",       lc_curs_set     },

    /* beep */
    { "beep",           lc_beep         },
    { "flash",          lc_flash        },

    /* window */
    { "newwin",         lc_newwin       },

    /* pad */
    { "newpad",         lc_newpad       },

    /* refresh */
    { "doupdate",       lc_doupdate     },

    /* inopts */
    { "cbreak",         lc_cbreak       },
    { "echo",           lc_echo         },
    { "raw",            lc_raw          },
    { "halfdelay",      lc_halfdelay    },

    /* util */
    { "unctrl",         lc_unctrl       },
    { "keyname",        lc_keyname      },
    { "delay_output",   lc_delay_output },
    { "flushinp",       lc_flushinp     },

    /* getch */
    { "ungetch",        lc_ungetch      },

    /* outopts */
    { "nl",             lc_nl           },

    /* query terminfo database */
    { "tigetflag",	ti_getflag	},
    { "tigetnum",	ti_getnum	},
    { "tigetstr",	ti_getstr	},

    /* slk */
    ECF(slk_init)
    ECF(slk_set)
    ECF(slk_refresh)
    ECF(slk_noutrefresh)
    ECF(slk_label)
    ECF(slk_clear)
    ECF(slk_restore)
    ECF(slk_touch)
    ECF(slk_attron)
    ECF(slk_attroff)
    ECF(slk_attrset)

    /* terminator */
    {NULL, NULL}
};


/* Prototype to keep compiler happy. */
int luaopen_curses (lua_State *L);

int luaopen_curses (lua_State *L)
{
    /*
    ** create new metatable for window objects
    */
    luaL_newmetatable(L, WINDOWMETA);
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -2);               /* push metatable */
    lua_rawset(L, -3);                  /* metatable.__index = metatable */
    luaL_openlib(L, NULL, windowlib, 0);

    lua_pop(L, 1);                      /* remove metatable from stack */

    /*
    ** create new metatable for chstr objects
    */
    luaL_newmetatable(L, CHSTRMETA);
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -2);               /* push metatable */
    lua_rawset(L, -3);                  /* metatable.__index = metatable */
    luaL_openlib(L, NULL, chstrlib, 0);

    lua_pop(L, 1);                      /* remove metatable from stack */

    /*
    ** create global table with curses methods/variables/constants
    */
    luaL_register(L, "curses", curseslib);

    lua_pushstring(L, "initscr");
    lua_pushvalue(L, -2);
    lua_pushcclosure(L, lc_initscr, 1);
    lua_settable(L, -3);

    return 1;
}

/* Local Variables: */
/* c-basic-offset: 4 */
/* End:             */
