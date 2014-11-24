
#include "utils.h"
#include "mem.h"
#include "prof.h"

typedef struct {
	queue_t q;
} obj_t;

static prof_t pf_get = {"objpool.get"};
static prof_t pf_put = {"objpool.put"};
static prof_t pf_new = {"objpool.new"};

prof_t *pf_objpool[] = {
	&pf_get, 
	&pf_put, 
	&pf_new, 
	NULL,
};

void *objpool_get(objpool_t *p) {
	prof_inc(&pf_get);

	if (p->objs.prev == NULL) {
		queue_init(&p->objs);
		info("init");
	}

	obj_t *o;
	if (p->freed > 0) {
		o = queue_data(queue_head(&p->objs), obj_t, q);
		queue_remove(&o->q);
		p->freed--;
	} else {
		prof_inc(&pf_new);
		o = (obj_t *)zalloc(p->size + sizeof(obj_t));
	}

	p->inuse++;
	return (void *)o + sizeof(obj_t);
}

void objpool_put(objpool_t *p, void *_o) {
	prof_inc(&pf_put);

	obj_t *o = (obj_t *)(_o - sizeof(obj_t));
	queue_insert_head(&p->objs, &o->q);
	p->freed++;
	p->inuse--;
}

