
#define PORT_DISCOVERY   33123
#define PORT_NOTIFY      33125
#define PORT_DATA        33124

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
	MT_DISCOVERY  = 0x1000,
	MT_DATA       = 0x1001,
};

static int copy_all(int fd, ssize_t (*cb)(int,void*,size_t), void *buf, int len) {
	if (len == 0)
		return 0;

	int r;
	int got = 0;

	while (len) {
		r = cb(fd, buf, len);
		if (r <= 0)
			return -EPIPE;
		len -= r;
		buf += r;
		got += r;
	}

	return got;
}

static int msg_read(msg_t *m, void *buf, int len) {
	int r;

	switch (m->mode) {
	case M_FILL:
		if (m->filled + len > m->len)
			return -ENOENT;
		memcpy(buf, m->buf + m->filled, len);
		m->filled += len;
		return 0;

	case M_FD:
		r = copy_all(m->fd, read, buf, len);
		if (r < 0)
			return r;
		m->filled += len;
		return 0;

	case M_FAKE:
		m->filled += len;
		return 0;
	}
	return -EINVAL;
}

static int msg_read_u32(msg_t *m, uint32_t *v) {
	return msg_read(m, v, 4);
}

static int msg_read_buf32(msg_t *m, void *buf, uint32_t *len) {
	int r;

	r = msg_read_u32(m, len);
	if (r) 
		return r;

	r = msg_read(m, buf, *len);
	if (r)
		return r;

	return 0;
}

static int msg_read_allocbuf32(msg_t *m, void **_buf, uint32_t *_len) {
	int r;

	uint32_t len;
	r = msg_read_u32(m, &len);
	if (r)
		return r;

	void *buf = zalloc(len);
	r = msg_read(m, buf, len);
	if (r) {
		free(buf);
		return r;
	}

	*_buf = buf;
	*_len = len;
	return 0;
}

static int msg_read_str32(msg_t *m, char **str) {
	int r;

	uint32_t len;
	r = msg_read_u32(m, &len);
	if (r)
		return r;

	char *s = (char *)zalloc(len+1);
	r = msg_read(m, s, len);
	if (r) {
		free(s);
		return r;
	}

	*str = s;
	return 0;
}

static int msg_write(msg_t *m, void *buf, int len) {
	int r;

	switch (m->mode) {
	case M_FILL:
		if (m->filled + len > m->len)
			return -ENOENT;
		memcpy(m->buf + m->filled, buf, len);
		m->filled += len;
		return 0;

	case M_FD:
		r = copy_all(m->fd, (ssize_t (*)(int, void *, size_t))write, buf, len);
		if (r < 0)
			return r;
		m->filled += len;
		return 0;

	case M_FAKE:
		m->filled += len;
		return 0;
	}
	return -EINVAL;
}

static int msg_write_u32(msg_t *m, uint32_t v) {
	return msg_write(m, &v, sizeof(v));
}

static int msg_write_buf32(msg_t *m, void *buf, uint32_t len) {
	int r;

	r = msg_write_u32(m, len);
	if (r)
		return r;

	r = msg_write(m, buf, len);
	if (r)
		return r;

	return 0;
}

static int msg_write_str32(msg_t *m, char *str) {
	if (str == NULL)
		str = "";
	int len = strlen(str);
	return msg_write_buf32(m, str, len);
}

static int msg_fill(msg_t *m) {
	int r;

	m->filled = 0;
	r = msg_write_u32(m, MT_VERSION);
	if (r)
		return r;

	r = msg_write_u32(m, m->type);
	if (r)
		return r;

	r = msg_write_u32(m, m->uuid);
	if (r)
		return r;

	r = msg_write_str32(m, m->name);
	if (r)
		return r;

	if (m->type == MT_DATA) {
		r = msg_write_buf32(m, m->data, m->datalen);
		if (r)
			return r;
	}

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
	int r;

	m->filled = 0;

	uint32_t v;
	r = msg_read_u32(m, &v);
	if (r || v != MT_VERSION) 
		return r;

	r = msg_read_u32(m, &m->type);
	if (r)
		return r;

	r = msg_read_u32(m, &m->uuid);
	if (r)
		return r;

	r = msg_read_str32(m, &m->name);
	if (r)
		return r;

	if (m->type == MT_DATA) {
		r = msg_read_allocbuf32(m, &m->data, &m->datalen);
		if (r)
			return r;
	}

	return 0;
}

static int msg_recv(int fd, msg_t *m) {
	m->mode = M_FD;
	m->fd = fd;

	int r = msg_parse(m);
	if (r)
		return r;

	return 0;
}

static void msg_free(msg_t *m) {
	if (m->name)
		free(m->name);
	if (m->data)
		free(m->data);
}

