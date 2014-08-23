#pragma once

void json_decode_from_buf(char *str, size_t json_len);
int luaopen_cjson_safe(lua_State *l);

