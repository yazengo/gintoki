#pragma once 

#include <uv.h>
#include <lua.h>

enum {
	LOG_DEBUG = 0,
	LOG_INFO = 1,
	LOG_WARN = 2,
	LOG_ERROR = 3,
	LOG_PANIC = 4,
};

#define debug(fmt, args...) _log(LOG_DEBUG, __func__, __FILE__, __LINE__, fmt, ##args) 
#define info(fmt, args...) _log(LOG_INFO, __func__, __FILE__, __LINE__, fmt, ##args) 
#define warn(fmt, args...) _log(LOG_WARN, __func__, __FILE__, __LINE__, fmt, ##args) 
#define error(fmt, args...) _log(LOG_ERROR, __func__, __FILE__, __LINE__, fmt, ##args) 
#define panic(fmt, args...) _log(LOG_PANIC, __func__, __FILE__, __LINE__, fmt, ##args) 

void _log(int level, const char *at_func, const char *at_file, int at_lineno, char *fmt, ...);
void log_ban(const char *, const char *);
void log_set_level(int level);
void log_init();

float now();

void pthread_call_luv_sync_v2(lua_State *L, uv_loop_t *loop, lua_CFunction on_start, lua_CFunction on_done, void *data);

#define lua_dofile_or_die(L, fname) lua_dofile_or_die_at(__func__, __FILE__, __LINE__, L, fname)
#define lua_dostring_or_die(L, str) lua_dostring_or_die_at(__func__, __FILE__, __LINE__, L, str)
#define lua_call_or_die(L, nargs, nresults) lua_call_or_die_at(__func__, __FILE__, __LINE__, L, nargs, nresults)
void lua_dofile_or_die_at(const char *at_func, const char *at_file, int at_lineno, lua_State *L, char *fname);
void lua_dostring_or_die_at(const char *at_func, const char *at_file, int at_lineno, lua_State *L, const char *str);
void lua_call_or_die_at(const char *at_func, const char *at_file, int at_lineno, lua_State *L, int nargs, int nresults);

void *zalloc(int len);

void print_trackback();

void utils_init(lua_State *L, uv_loop_t *loop);
void utils_preinit();

void lua_set_global_callback_and_pushname(lua_State *L, const char *pref, void *p);
void lua_set_global_callback(lua_State *L, const char *name, void *p);
void lua_do_global_callback(lua_State *L, const char *name, void *p, int nargs, int setnil);

void lua_pushuserptr(lua_State *L, void *p);
void *lua_touserptr(lua_State *L, int index);
void lua_setuserptr(lua_State *L, int index, void *p);

void lua_set_global_ptr(lua_State *L, const char *pref, void *p);
void lua_get_global_ptr(lua_State *L, const char *pref, void *p);

