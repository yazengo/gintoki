#pragma once 

#include <uv.h>
#include <lua.h>

enum {
	LOG_DEBUG,
	LOG_INFO,
	LOG_WARN,
	LOG_ERROR,
};

#define debug(fmt, args...) _log(LOG_DEBUG, __func__, __FILE__, __LINE__, fmt, ##args) 
#define info(fmt, args...) _log(LOG_INFO, __func__, __FILE__, __LINE__, fmt, ##args) 
#define warn(fmt, args...) _log(LOG_WARN, __func__, __FILE__, __LINE__, fmt, ##args) 
#define error(fmt, args...) _log(LOG_ERROR, __func__, __FILE__, __LINE__, fmt, ##args) 

void _log(int level, const char *, const char *, int, char *, ...);
void log_ban(const char *, const char *);
void log_set_level(int level);
void log_init();

float now();

typedef void (*luv_cb_t)(lua_State *L, void *cb_p);
void pthread_call_luv_sync(lua_State *L, uv_loop_t *loop, luv_cb_t cb, void *cb_p);

void lua_dofile_or_die(lua_State *L, char *fname);
void lua_call_or_die(lua_State *L, int nargs, int nresults);

void *zalloc(int len);

void utils_init(lua_State *L, uv_loop_t *loop);
void utils_preinit();

void lua_set_global_callback_and_pushname(lua_State *L, const char *pref, void *p);
void lua_set_global_callback(lua_State *L, const char *name, void *p);
void lua_do_global_callback(lua_State *L, const char *name, void *p, int nargs, int setnil);

