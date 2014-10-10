
#pragma once

typedef struct strparser_match_s {
	int i;
	char *s[16];
	void (*done)(struct strparser_match_s *);
} strparser_match_t;

#define STRPARSE_MATCH_NR 4

typedef struct {
	char buf[128];
	int len;
	int stat;
	strparser_match_t match[STRPARSE_MATCH_NR];
} strparser_t;

