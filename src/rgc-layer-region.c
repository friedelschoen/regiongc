#include "rgc-context.h"
#include "rgc-layer.h"
#include "rgc-ref.h"
#include "rgc-segment.h"

#include <string.h>

static enum rgc_error rgc_layer_region_new(struct rgc_layer *l) {
	(void) l;
	return RGC_OK;
}

static enum rgc_error rgc_layer_region_collect(struct rgc_layer *l, rgc_collect_key_t key) {
	/* Region objects are conservatively live. */
	for (struct rgc_object *obj = l->head; obj; obj = obj->next)
		rgc_object_mark(obj, key);

	return RGC_OK;
}

enum rgc_error rgc_layer_region_adopt(struct rgc_layer *target, struct rgc_object *obj) {
	struct rgc_object *newobj;
	enum rgc_error err = rgc_layer_alloc(target, obj->size, 0, &newobj);
	if (err != RGC_OK)
		return err;

	return rgc_object_move(obj, newobj);
}

static enum rgc_error rgc_layer_region_finish(struct rgc_layer *l) {
	struct rgc_segment *seg = l->seg;
	struct rgc_layer *parent = l->parent;

	struct rgc_object *inherit_head = NULL;
	struct rgc_object *inherit_tail = NULL;

	/* ->next chain is reused for foreign objects. */
	for (struct rgc_object *obj = l->head; obj != NULL; obj = obj->next) {
		if (obj->layer == NULL)
			/* freed object */
			continue;

		if (obj->layer != l) {
			if (!inherit_head) {
				inherit_head = obj;
				inherit_tail = obj;
			} else {
				inherit_tail->next = obj;
				inherit_tail = obj;
			}
			continue;
		}

		rgc_ref_clear(l->ctx, obj->ref);
	}

	if (inherit_tail)
		inherit_tail->next = NULL;

	rgc_segment_reset(seg, l->seg_mark);
	/* hereafter l will probably be overwritten, which makes l meaningless */
	l = NULL;

	if (inherit_head && parent) {
		for (struct rgc_object *obj = inherit_head; obj != NULL; obj = obj->next) {
			rgc_layer_adopt(parent, obj);
		}
	}
	return RGC_OK;
}


static void rgc_layer_region_destroy(struct rgc_layer *l) {
	struct rgc_segment *seg = l->seg;

	/* ->next chain is reused for foreign objects. */
	for (struct rgc_object *obj = l->head; obj != NULL; obj = obj->next) {
		if (obj->layer == NULL)
			/* freed object */
			continue;

		rgc_ref_clear(l->ctx, obj->ref);
	}

	rgc_segment_reset(seg, l->seg_mark);
	/* hereafter l will probably be overwritten, which makes l meaningless */
}

static enum rgc_error rgc_layer_region_alloc(struct rgc_layer *l, struct rgc_layer *owner, size_t size, bool init, struct rgc_object **outobj) {
	struct rgc_object *obj = *outobj = rgc_segment_alloc(l->seg, sizeof(*obj) + size, init, NULL);
	if (!obj)
		return RGC_ERR_NO_MEMORY;

	if (!init)
		memset(obj, 0, sizeof(*obj));

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

const struct rgc_layer_api layer_api_region = {
	.new = rgc_layer_region_new,
	.collect = rgc_layer_region_collect,
	.adopt = rgc_layer_region_adopt,
	.alloc = rgc_layer_region_alloc,
	.finish = rgc_layer_region_finish,
	.destroy = rgc_layer_region_destroy,
};
