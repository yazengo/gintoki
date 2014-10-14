
typedef struct {
	uint32_t type;
	uint32_t uuid;
	char *name;

	void *data;
	uint32_t datalen;

	int fd;
	int mode;
	void *buf;
	int len, filled;
} msg_t;

enum {
	M_FILL,
	M_FD,
	M_FAKE,
};

#define MT_VERSION 0xdeadbeef

enum {
	MT_DISCOVERY = 0x1000,
	MT_DATA      = 0x1001,
};

static int msg_read(msg_t *m, void *buf, int len) {
	int r;

	switch (m->mode) {
	case M_FILL:
		if (m->filled + len > m->len)
			return 1;
		memcpy(buf, m->buf + m->filled, len);
		m->filled += len;
		return 0;

	case M_FD:
		r = read(m->fd, buf, len);
		m->filled += len;
		return !(r == len);

	case M_FAKE:
		m->filled += len;
		return 0;
	}
	return 1;
}

static int msg_read_u32(msg_t *m, uint32_t *v) {
	return msg_read(m, v, 4);
}

static int msg_read_buf32(msg_t *m, void *buf, uint32_t *len) {
	if (msg_read_u32(m, len))
		return 1;
	if (msg_read(m, buf, *len))
		return 1;
	return 0;
}

static int msg_read_str32(msg_t *m, char **str) {
	uint32_t len;
	if (msg_read_u32(m, &len))
		return 1;

	char *s = (char *)zalloc(len+1);
	msg_read(m, s, len);

	*str = s;
	return 0;
}

static int msg_write(msg_t *m, void *buf, int len) {
	int r;

	switch (m->mode) {
	case M_FILL:
		if (m->filled + len > m->len)
			return 1;
		memcpy(m->buf + m->filled, buf, len);
		m->filled += len;
		return 0;

	case M_FD:
		r = write(m->fd, buf, len);
		m->filled += len;
		return !(r == len);

	case M_FAKE:
		m->filled += len;
		return 0;
	}
	return 1;
}

static int msg_write_u32(msg_t *m, uint32_t v) {
	return msg_write(m, &v, sizeof(v));
}

static int msg_write_buf32(msg_t *m, void *buf, uint32_t len) {
	if (msg_write_u32(m, len))
		return 1;
	if (msg_write(m, buf, len))
		return 1;
	return 0;
}

static int msg_write_str32(msg_t *m, char *str) {
	if (str == NULL)
		str = "";
	int len = strlen(str);
	return msg_write_buf32(m, str, len);
}

static int msg_fill(msg_t *m) {
	m->filled = 0;
	if (msg_write_u32(m, MT_VERSION)) 
		return 1;
	if (msg_write_u32(m, m->type)) 
		return 1;
	if (msg_write_u32(m, m->uuid)) 
		return 1;
	if (msg_write_str32(m, m->name)) 
		return 1;
	return 0;
}

static int msg_allocfill(msg_t *m) {
	m->mode = M_FAKE;
	msg_fill(m);

	m->buf = zalloc(m->filled);
	m->len = m->filled;
	m->mode = M_FILL;
	return msg_fill(m);
}

static int msg_send(msg_t *m, int fd) {
	m->mode = M_FD;
	m->fd = fd;
	return msg_fill(m);
}

static int msg_parse(msg_t *m) {
	m->filled = 0;

	uint32_t v;
	if (msg_read_u32(m, &v) || v != MT_VERSION) 
		return 1;

	if (msg_read_u32(m, &m->type))
		return 1;
	if (msg_read_u32(m, &m->uuid))
		return 1;
	if (msg_read_str32(m, &m->name))
		return 1;

	if (m->type == MT_DATA) {
		if (msg_read_buf32(m, &m->data, &m->datalen))
			return 1;
	}

	return 0;
}

static msg_t *msg_recv(int fd) {
	msg_t *m = (msg_t *)zalloc(sizeof(msg_t));
	m->mode = M_FD;
	m->fd = fd;

	if (msg_parse(m))
		return m;

	free(m);
	return NULL;
}

static void msg_free(msg_t *m) {
	if (m->name)
		free(m->name);
	free(m);
}

