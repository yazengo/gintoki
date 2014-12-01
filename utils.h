#pragma once 

#include <uv.h>
#include <lua.h>

#include "queue.h"
#include "luv.h"

#include "immediate.h"

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
void setloglevel(int level);
void log_init();

float now();

#define lua_dofile_or_die(L, fname) lua_dofile_or_die_at(__func__, __FILE__, __LINE__, L, fname)
#define lua_dostring_or_die(L, str) lua_dostring_or_die_at(__func__, __FILE__, __LINE__, L, str)
#define lua_call_or_die(L, nargs, nresults) lua_call_or_die_at(__func__, __FILE__, __LINE__, L, nargs, nresults)
void lua_dofile_or_die_at(const char *at_func, const char *at_file, int at_lineno, lua_State *L, char *fname);
void lua_dostring_or_die_at(const char *at_func, const char *at_file, int at_lineno, lua_State *L, const char *str);
void lua_call_or_die_at(const char *at_func, const char *at_file, int at_lineno, lua_State *L, int nargs, int nresults);

void *zalloc(int len);
void *memdup(void *buf, int len);

void print_trackback();

void luv_utils_init(lua_State *L, uv_loop_t *loop);
void utils_preinit();
void utils_onexit(void (*cb)());

void lua_pushuserptr(lua_State *L, void *p);
void lua_pushuserdata(lua_State *L, void *p, int len);
void *lua_touserptr(lua_State *L, int index);
void lua_setuserptr(lua_State *L, int index, void *p);

#define lua_dumpstack(L) _lua_dumpstack_at(__func__, __FILE__, __LINE__, L)
void _lua_dumpstack_at(const char *at_func, const char *at_file, int at_lineno, lua_State *L);

char *strndup(const char *s, size_t n);

struct fs_req_s;
typedef void (*fs_req_cb)(struct fs_req_s *req);

typedef struct fs_req_s {
	uv_work_t w;
	int fd;
	char *path;
	fs_req_cb done;
	void *data;
} fs_req_t;

