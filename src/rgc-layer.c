#include "rgc-layer.h"

#include "rgc-context.h"
#include "rgc-object.h"
#include "rgc-segment.h"

#include <regiongc.h>
#include <string.h>


enum rgc_error rgc_layer_new(struct rgc_layer *parent, int flags, struct rgc_layer **outl) {
	*outl = NULL;

	if (parent->flags & RGC_LAYER_FINAL) {
		parent->final_depth++;
		*outl = parent;
		return RGC_OK;
	}

	int branched = parent->child != NULL;
	struct rgc_segment *seg = parent->seg;

	if (branched) {
		seg = rgc_segment_new(parent->ctx->alloc, parent->ctx->alloc_userdata);
		if (!seg)
			return RGC_ERR_NO_MEMORY;
	}

	size_t mark = rgc_segment_top(seg);
	struct rgc_layer *l = rgc_segment_alloc(seg, sizeof(*l), 1, &seg);
	if (!l) {
		if (branched)
			rgc_segment_free(seg);

		return RGC_ERR_NO_MEMORY;
	}

	l->parent = parent;
	l->ctx = parent->ctx;
	l->flags = flags;
	l->seg = seg;
	l->seg_owner = branched;
	l->seg_mark = mark;
	l->depth = parent->depth + 1;

	l->api = flags & RGC_LAYER_GC ? &layer_api_gc : &layer_api_region;
	enum rgc_error err = l->api->new(l);
	if (err != RGC_OK) {
		if (branched)
			rgc_segment_free(seg);
		return err;
	}

	if (!branched) {
		parent->child = l;
	} else {
		struct rgc_layer *last = parent->child;

		while (last->sibling)
			last = last->sibling;

		last->sibling = l;
	}

	*outl = l;
	return RGC_OK;
}

enum rgc_error rgc_layer_adopt(struct rgc_layer *l, struct rgc_object *obj) {
	return l->api->adopt(l, obj);
}

static void rgc_layer_unlink(struct rgc_layer *l) {
	struct rgc_layer **p = &l->parent->child;

	while (*p && *p != l)
		p = &(*p)->sibling;

	if (*p)
		*p = l->sibling;
}

enum rgc_error rgc_layer_finish(struct rgc_layer *l) {
	if (l->flags & RGC_LAYER_FINAL) {
		if (l->final_depth > 0) {
			l->final_depth--;
			return RGC_OK;
		}
	}

	if (l->child && !(l->ctx->flags & RGC_CASCADE))
		return RGC_ERR_NOT_TOP;


	/* check if pinned */
	for (struct rgc_object *obj = l->head; obj != NULL; obj = obj->next) {
		if (obj->pin > 0)
			return RGC_ERR_PINNED;
	}

	/* Finish descendants */
	while (l->child) {
		enum rgc_error err = rgc_layer_finish(l->child);
		if (err != RGC_OK)
			return err;
	}

	struct rgc_segment *seg = l->seg;
	struct rgc_layer *parent = l->parent;
	bool seg_owner = l->seg_owner;

	rgc_layer_unlink(l);

	/* Finish may release memory containing l. */
	enum rgc_error err = l->api->finish(l);
	if (err != RGC_OK)
		return err;
	l = NULL;

	if (seg_owner)
		rgc_segment_free(seg);
	else
		rgc_segment_cleanup(seg);

	if (parent)
		rgc_layer_collect(parent);

	return RGC_OK;
}


void rgc_layer_destroy(struct rgc_layer *l) {
	if (l->sibling) {
		rgc_layer_destroy(l->sibling);
		l->sibling = NULL;
	}
	if (l->child) {
		rgc_layer_destroy(l->child);
		l->child = NULL;
	}

	struct rgc_segment *seg = l->seg;
	bool seg_owner = l->seg_owner;

	l->api->destroy(l);

	if (seg_owner)
		rgc_segment_free(seg);
}

enum rgc_error rgc_layer_alloc(struct rgc_layer *l, size_t size, bool init, struct rgc_object **obj) {
	/* the containing layer might be a foreign */
	struct rgc_layer *top = l;
	while (top->child)
		top = top->child;

	return top->api->alloc(top, l, size, init, obj);
}

static enum rgc_error rgc_layer_collect_key(struct rgc_layer *l, rgc_collect_key_t key) {
	for (struct rgc_layer *c = l->child; c != NULL; c = c->sibling) {
		enum rgc_error err = rgc_layer_collect_key(c, key);
		if (err != RGC_OK)
			return err;
	}

	return l->api->collect(l, key);
}

enum rgc_error rgc_layer_collect(struct rgc_layer *l) {
	return rgc_layer_collect_key(l, ++l->ctx->collect_key);
}
