#include "rgc-context.h"
#include "rgc-object.h"
#include "rgc-ref.h"

#include <string.h>

static enum rgc_error rgc_layer_gc_new(struct rgc_layer *l) {
	(void) l;
	return RGC_OK;
}

static enum rgc_error rgc_layer_gc_collect(struct rgc_layer *l, rgc_collect_key_t key) {
	for (struct rgc_object *obj = l->head; obj != NULL; obj = obj->next) {
		if (obj->pin)
			rgc_object_mark(obj, key);
	}

	/* Sweep unreachable objects. */
	struct rgc_object **p = &l->head;
	l->tail = NULL;

	while (*p) {
		struct rgc_object *obj = *p;

		if (obj->collect_key != key) {
			*p = obj->next;

			rgc_ref_clear(l->ctx, obj->ref);
			rgc_ref_edge_vector_free(&obj->refs);

			l->ctx->alloc(obj, sizeof(*obj) + obj->size, 0, l->ctx->alloc_userdata);
		} else {
			l->tail = obj;
			p = &obj->next;
		}
	}
	return RGC_OK;
}

enum rgc_error rgc_layer_gc_adopt(struct rgc_layer *target, struct rgc_object *obj) {
	struct rgc_object *newobj;
	enum rgc_error err = rgc_layer_alloc(target, obj->size, 0, &newobj);
	if (err != RGC_OK)
		return err;

	return rgc_object_move(obj, newobj);
}

static enum rgc_error rgc_layer_gc_finish(struct rgc_layer *l) {
	for (struct rgc_object *obj = l->head; obj != NULL; obj = obj->next) {
		rgc_ref_clear(l->ctx, obj->ref);
		rgc_ref_edge_vector_free(&obj->refs);

		l->ctx->alloc(obj, sizeof(*obj) + obj->size, 0, l->ctx->alloc_userdata);
	}

	return RGC_OK;
}


static void rgc_layer_gc_destroy(struct rgc_layer *l) {
	struct rgc_object *next;
	for (struct rgc_object *obj = l->head; obj != NULL; obj = next) {
		rgc_ref_clear(l->ctx, obj->ref);
		rgc_ref_edge_vector_free(&obj->refs);
		next = obj->next;

		l->ctx->alloc(obj, sizeof(*obj) + obj->size, 0, l->ctx->alloc_userdata);
	}
}

enum rgc_error rgc_layer_gc_alloc(struct rgc_layer *l, struct rgc_layer *owner, size_t size, bool init, struct rgc_object **objout) {
	struct rgc_object *obj = *objout = l->ctx->alloc(NULL, 0, sizeof(*obj) + size, l->ctx->alloc_userdata);
	if (obj == NULL)
		return RGC_ERR_NO_MEMORY;

	memset(obj, 0, sizeof(*obj));
	if (init)
		memset(obj->payload, 0, size);

	obj->layer = owner;
	obj->next = NULL;
	obj->size = size;
	rgc_ref_edge_vector_init(&obj->refs, l->ctx->alloc, l->ctx->alloc_userdata);

	if (l->tail)
		l->tail->next = obj;
	else
		l->head = obj;

	l->tail = obj;
	return RGC_OK;
}

const struct rgc_layer_api layer_api_gc = {
	.new = rgc_layer_gc_new,
	.collect = rgc_layer_gc_collect,
	.adopt = rgc_layer_gc_adopt,
	.finish = rgc_layer_gc_finish,
	.destroy = rgc_layer_gc_destroy,
	.alloc = rgc_layer_gc_alloc,
};
