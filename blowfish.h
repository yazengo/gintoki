
#pragma once

/*
blowfish.h:  Header file for blowfish.c

Copyright (C) 1997 by Paul Kocher

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.
You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


See blowfish.c for more information about this file.
*/

#include <lua.h>
#include <uv.h>
#include <stdint.h>

#include "utils.h"
  
typedef struct {
  uint32_t P[16 + 2];
  uint32_t S[4][256];
} BLOWFISH_CTX;

void Blowfish_Init(BLOWFISH_CTX *ctx, uint8_t *key, int keyLen);
void Blowfish_Encrypt(BLOWFISH_CTX *ctx, uint32_t *xl, uint32_t *xr);
void Blowfish_Decrypt(BLOWFISH_CTX *ctx, uint32_t *xl, uint32_t *xr);

typedef struct {
	BLOWFISH_CTX ctx;
} blowfish_t;

void blowfish_init(blowfish_t *b, void *key, int keylen);

void blowfish_encode(blowfish_t *b, void *in, int len);
void blowfish_decode(blowfish_t *b, void *in, int len);

void blowfish_encode_hex(blowfish_t *b, void *in, int inlen, char *out);
void blowfish_decode_hex(blowfish_t *b, char *in, int inlen, void *out);

void luv_blowfish_init(lua_State *L, uv_loop_t *loop);

