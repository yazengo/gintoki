
#include <ctype.h>
#include <string.h>
#include "strparser.h"

enum {
	SPACING,
	READING,
};

static void on_match(strparser_match_t *m) {
	m->done(m);
}

static void on_token(strparser_t *p) {
	int i;

	for (i = 0; i < STRPARSE_MATCH_NR; i++) {
		strparser_match_t *m = &p->match[i];

		if (m->s[m->i+1] && !strcmp(p->buf, m->s[m->i+1])) {
			m->i++;
			if (m->s[m->i+1] == NULL) {
				on_match(m);
				m->i = 0;
			}
		} else
			m->i = 0;
	}
}

void strparser_parse(strparser_t *p, void *buf, int len) {
	char *s = (char *)buf;

	while (len--) {
		if (isspace(*s)) {
			if (p->stat == READING) {
				p->buf[p->len] = 0;
				on_token(p);
				p->len = 0;
				p->stat = SPACING;
			}
		} else {
			if (p->stat == SPACING)
				p->stat = READING;
			if (p->len < sizeof(p->buf)-1)
				p->buf[p->len++] = *s;
		}
		s++;
	}
}


