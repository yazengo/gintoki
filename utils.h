#pragma once 

#include <uv.h>
#include <lua.h>

enum {
	LOG_DEBUG,
	LOG_INFO,
	LOG_WARN,
	LOG_ERROR,
};

#define log(fmt, args...) _log(LOG_DEBUG, __func__, __FILE__, __LINE__, fmt, ##args) 
#define info(fmt, args...) _log(LOG_INFO, __func__, __FILE__, __LINE__, fmt, ##args) 
#define warn(fmt, args...) _log(LOG_WARN, __func__, __FILE__, __LINE__, fmt, ##args) 
#define error(fmt, args...) _log(LOG_ERROR, __func__, __FILE__, __LINE__, fmt, ##args) 

void _log(int level, const char *, const char *, int, char *, ...);
void log_ban(const char *, const char *);
void log_set_level(int level);
void log_init();

float now();

void run_hello(int i);
void run_test_c(int i, lua_State *L, uv_loop_t *loop);
void run_test_lua(int i, lua_State *L, uv_loop_t *loop);

typedef void (*luv_cb_t)(lua_State *L, void *cb_p);
void pthread_call_luv_sync(lua_State *L, uv_loop_t *loop, luv_cb_t cb, void *cb_p);

