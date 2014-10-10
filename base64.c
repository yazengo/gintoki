
#include <uv.h>
#include <lua.h>
#include <stdlib.h>

#include "utils.h"

static const char b64_alphabet[] = 
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789+/";

static inline void a3_to_a4(unsigned char *a4, unsigned char *a3) {
	a4[0] = (a3[0] & 0xfc) >> 2;
	a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xf0) >> 4);
	a4[2] = ((a3[1] & 0x0f) << 2) + ((a3[2] & 0xc0) >> 6);
	a4[3] = (a3[2] & 0x3f);
}

static inline void a4_to_a3(unsigned char *a3, unsigned char *a4) {
	a3[0] = (a4[0] << 2) + ((a4[1] & 0x30) >> 4);
	a3[1] = ((a4[1] & 0xf) << 4) + ((a4[2] & 0x3c) >> 2);
	a3[2] = ((a4[2] & 0x3) << 6) + a4[3];
}

static inline unsigned char b64_lookup(char c) {
	if (c >='A' && c <='Z') return c - 'A';
	if (c >='a' && c <='z') return c - 71;
	if (c >='0' && c <='9') return c + 4;
	if (c == '+') return 62;
	if (c == '/') return 63;
	return -1;
}

static int base64_encode(char *output, char *input, int inputLen) {
	int i = 0, j = 0;
	int encLen = 0;
	unsigned char a3[3];
	unsigned char a4[4];

	while (inputLen--) {
		a3[i++] = *(input++);
		if (i == 3) {
			a3_to_a4(a4, a3);

			for (i = 0; i < 4; i++) {
				output[encLen++] = b64_alphabet[a4[i]];
			}

			i = 0;
		}
	}

	if (i) {
		for (j = i; j < 3; j++) {
			a3[j] = '\0';
		}

		a3_to_a4(a4, a3);

		for (j = 0; j < i + 1; j++) {
			output[encLen++] = b64_alphabet[a4[j]];
		}

		while ((i++ < 3)) {
			output[encLen++] = '=';
		}
	}

	output[encLen] = '\0';
	return encLen;
}

static int base64_decode(char *output, char *input, int inputLen) {
	int i = 0, j = 0;
	int decLen = 0;
	unsigned char a3[3];
	unsigned char a4[4];


	while (inputLen--) {
		if (*input == '=') {
			break;
		}

		a4[i++] = *(input++);
		if (i == 4) {
			for (i = 0; i <4; i++) {
				a4[i] = b64_lookup(a4[i]);
			}

			a4_to_a3(a3,a4);

			for (i = 0; i < 3; i++) {
				output[decLen++] = a3[i];
			}
			i = 0;
		}
	}

	if (i) {
		for (j = i; j < 4; j++) {
			a4[j] = '\0';
		}

		for (j = 0; j <4; j++) {
			a4[j] = b64_lookup(a4[j]);
		}

		a4_to_a3(a3,a4);

		for (j = 0; j < i - 1; j++) {
			output[decLen++] = a3[j];
		}
	}
	output[decLen] = '\0';
	return decLen;
}

static int base64_enc_len(int plainLen) {
	int n = plainLen;
	return (n + 2 - ((n + 2) % 3)) / 3 * 4;
}

static int base64_dec_len(char *input, int inputLen) {
	int i = 0;
	int numEq = 0;

	for (i = inputLen - 1; input[i] == '='; i--) {
		numEq++;
	}
	return ((6 * inputLen) / 8) - numEq;
}

static int lua_base64_encode(lua_State *L) {
	char *in = (char *)lua_tostring(L, 1);
	if (in == NULL)
		panic("in is nil");

	int inlen = strlen(in);
	int outlen = base64_enc_len(inlen);
	char *out = (char *)zalloc(outlen+1);

	base64_encode(out, in, inlen);
	lua_pushstring(L, out);
	free(out);

	return 1;
}

static int lua_base64_decode(lua_State *L) {
	char *in = (char *)lua_tostring(L, 1);
	if (in == NULL)
		panic("in is nil");

	int inlen = strlen(in);
	int outlen = base64_dec_len(in, inlen);
	char *out = (char *)zalloc(outlen+1);

	base64_decode(out, in, inlen);
	lua_pushstring(L, out);
	free(out);

	return 1;
}

void luv_base64_init(lua_State *L, uv_loop_t *loop) {
	lua_pushcfunction(L, lua_base64_encode);
	lua_setglobal(L, "base64_encode");

	lua_pushcfunction(L, lua_base64_decode);
	lua_setglobal(L, "base64_decode");
}

