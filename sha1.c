
#include <assert.h>
#include <stdint.h>
#include <ctype.h>
#include <stdio.h>

#include <uv.h>
#include <lua.h>

#include "utils.h"

typedef struct _SHA1_Digest {
	uint8_t digest[20];
} SHA1_Digest;

static SHA1_Digest SHA1_Digest_fromStr(const char* src);
static void SHA1_Digest_toStr(const SHA1_Digest* digest, char* dst);
static SHA1_Digest SHA1_get(const void* msg, uint64_t bytes);

#define ALIGN(x,a) (((x)+(a)-1ULL)&~((a)-1ULL))

static uint32_t rotl32(uint32_t x, int b) {
	return (x << b) | (x >> (32-b));
}

static uint32_t get32(const void *p) {
	const uint8_t *x = (const uint8_t *)p;
	return (x[0] << 24) | (x[1] << 16) | (x[2] << 8) | x[3];
}

static uint32_t f(int t, uint32_t b, uint32_t c, uint32_t d) {
	if (t < 20)
		return (b & c) | ((~b) & d);
	if (t < 40)
		return b ^ c ^ d;
	if (t < 60)
		return (b & c) | (b & d) | (c & d);
	return b ^ c ^ d;
}

static void processBlock (const uint32_t* block, uint32_t* h) {
	static const uint32_t k[4] = {
		0x5A827999,
		0x6ED9EBA1,
		0x8F1BBCDC,
		0xCA62C1D6
	};

	uint32_t w[80];
	uint32_t a = h[0];
	uint32_t b = h[1];
	uint32_t c = h[2];
	uint32_t d = h[3];
	uint32_t e = h[4];
	int t;

	for (t = 0; t < 16; t++)
		w[t] = get32(block++);

	for (t = 0; t < 80; t++) {
		int s = t & 0xf;
		uint32_t temp;
		if (t >= 16)
			w[s] = rotl32(w[(s + 13) & 0xf] ^ w[(s + 8) & 0xf] ^ w[(s + 2) & 0xf] ^ w[s], 1);

		temp = rotl32(a, 5) + f(t, b,c,d) + e + w[s] + k[t/20];

		e = d; d = c; c = rotl32(b, 30); b = a; a = temp;
	}

	h[0] += a;
	h[1] += b;
	h[2] += c;
	h[3] += d;
	h[4] += e;
}

static int padMsg(const void* msg, uint64_t bytes, uint8_t* lastBlock)
{
	int msgBytesInLast   = (int)(bytes & 63);
	int extraBlocks      = (msgBytesInLast + 9) > 64 ? 2 : 1;
	int numZeroBytes     = extraBlocks * 64 - 9 - msgBytesInLast;

	// fill remaining msg bytes
	const uint8_t* msgLast = (uint8_t*)msg + (bytes & ~63);
	while (msgBytesInLast--)
		*lastBlock++ = *msgLast++;

	// separator
	*lastBlock++ = 0x80;

	while (numZeroBytes--)
		*lastBlock++ = 0;

	// original length in bits (!), switch endianness
	bytes *= 8;
	*lastBlock++ = (uint8_t)(bytes >> 56 & 0xff);
	*lastBlock++ = (uint8_t)(bytes >> 48 & 0xff);
	*lastBlock++ = (uint8_t)(bytes >> 40 & 0xff);
	*lastBlock++ = (uint8_t)(bytes >> 32 & 0xff);
	*lastBlock++ = (uint8_t)(bytes >> 24 & 0xff);
	*lastBlock++ = (uint8_t)(bytes >> 16 & 0xff);
	*lastBlock++ = (uint8_t)(bytes >> 8  & 0xff);
	*lastBlock++ = (uint8_t)(bytes >> 0  & 0xff);

	return extraBlocks;
}

static SHA1_Digest SHA1_get(const void *msg, uint64_t bytes) {
	SHA1_Digest digest;

	uint32_t h[5] = {
		0x67452301,
		0xefcdab89,
		0x98badcfe,
		0x10325476,
		0xc3d2e1f0
	};

	uint64_t totalBlocks = ALIGN(bytes + 9, 64) / 64; // including padding
	const uint32_t* block = (const uint32_t*)msg;
	uint64_t b;

	// we could assume that msg is always required to
	// hold padding, but let's not
	uint8_t lastBlocks[128];  // either one or two blocks
	int numLast = padMsg(msg, bytes, lastBlocks);

	for (b = 0; b < totalBlocks-numLast; b++) {
		processBlock(block, h);
		block += 16;
	}

	// process last block
	for (b = 0; b < numLast; b++)
		processBlock((const uint32_t*)(lastBlocks + b*64), h);

	*((uint32_t*)&digest.digest[0])  = get32(&h[0]);
	*((uint32_t*)&digest.digest[4])  = get32(&h[1]);
	*((uint32_t*)&digest.digest[8])  = get32(&h[2]);
	*((uint32_t*)&digest.digest[12]) = get32(&h[3]);
	*((uint32_t*)&digest.digest[16]) = get32(&h[4]);

	return digest;
}

void sha1_digest(void *in, int inlen, void *out) {
	SHA1_Digest d = SHA1_get(in, inlen);
	memcpy(out, d.digest, 20);
}

static void SHA1_Digest_toStr (const SHA1_Digest* digest, char* dst) {
	int i;
	assert(digest && dst); // dst must be at least 41 bytes (terminator)
	for (i = 0; i < 20; i++)
	{
		int c0 = digest->digest[i] >> 4;
		int c1 = digest->digest[i] & 0xf;

		assert(0 <= c0 && c0 <= 0xf);
		assert(0 <= c1 && c1 <= 0xf);
		c0 = c0 <= 9 ? '0' + c0 : 'a' + c0 - 0xa;
		c1 = c1 <= 9 ? '0' + c1 : 'a' + c1 - 0xa;

		*dst++ = (char)c0;
		*dst++ = (char)c1;
	}
	*dst = '\0';
}

static int lua_sha1_encode(lua_State *L) {
	char *in = (char *)lua_tostring(L, 1);
	if (in == NULL)
		panic("input string must set");
	int inlen = strlen(in);

	SHA1_Digest d = SHA1_get(in, inlen);
	char out[256];
	SHA1_Digest_toStr(&d, out);

	lua_pushstring(L, out);
	return 1;
}

void luv_sha1_init(lua_State *L, uv_loop_t *loop) {
	lua_pushuserptr(L, loop);
	lua_pushcclosure(L, lua_sha1_encode, 1);
	lua_setglobal(L, "sha1_encode");
}

#ifdef T

static SHA1_Digest SHA1_Digest_fromStr(const char* src) {
	// \todo why does msvc require this initializer to not issue C4701 ?
	SHA1_Digest d = { "" }; 
	int i;

	assert(src); // also, src must be at least 40 bytes
	for (i = 0; i < 20; i++)
	{
		// \todo just use atoi or something
		int c0 = tolower(*src++);
		int c1 = tolower(*src++);

		c0 = '0' <= c0 && c0 <= '9' ? c0 - '0' : ('a' <= c0 && c0 <= 'f' ? 0xa + c0 - 'a' : -1);
		c1 = '0' <= c1 && c1 <= '9' ? c1 - '0' : ('a' <= c1 && c1 <= 'f' ? 0xa + c1 - 'a' : -1);
		d.digest[i] = (uint8_t)((c0 << 4) | c1);
	}

	return d;
}

int main() {
	char *s = "2tJ4U8RPJi4+pQiOlE44xg==" "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	SHA1_Digest d = SHA1_get(s, strlen(s));
	char buf[1024] = {};
	SHA1_Digest_toStr(&d, buf);
	printf("%s\n", buf);
}

#endif

#undef ALIGN

