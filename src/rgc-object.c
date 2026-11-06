#include "rgc-object.h"

#include "rgc-context.h"
#include "rgc-layer.h"
#include "rgc-ref.h"

#include <regiongc.h>
#include <stdbool.h>
#include <string.h>


void rgc_object_set_finalizer(struct rgc_object *obj, rgc_finalizer_t finalize, void *userdata) {
	obj->finalize = finalize;
	obj->finalize_userdata = userdata;
}

enum rgc_error rgc_object_move(struct rgc_object *src, struct rgc_object *dst) {
	if (src->pin)
		return RGC_ERR_PINNED;

	struct rgc_context *ctx = src->layer->ctx;

	/* Overflow */
	if (dst->size < src->size)
		return RGC_ERR_NO_MEMORY;

	/* payload */
	memmove(dst->payload, src->payload, src->size);

	/* metadata */
	dst->ref = src->ref;
	dst->refs = src->refs;
	dst->finalize = src->finalize;
	dst->finalize_userdata = src->finalize_userdata;
	dst->collect_key = src->collect_key;

	/* update reference entry */
	rgc_ref_entry_vector_get(&ctx->refs, RGC_REF_INDEX(src->ref))->obj = dst;

	/* reset previous object */
	src->layer = NULL;
	src->ref = RGC_NOREF;
	src->refs = (struct rgc_ref_edge_vector){ 0 }; /* don't free! vector is copied. */
	src->finalize = NULL;
	src->finalize_userdata = NULL;

	return RGC_OK;
}

void rgc_object_mark(struct rgc_object *obj, rgc_collect_key_t key) {
	if (obj->collect_key == key)
		return;

	obj->collect_key = key;

	for (size_t i = 0; i < rgc_ref_edge_vector_size(&obj->refs); i++) {
		struct rgc_ref_edge *edge = rgc_ref_edge_vector_get(&obj->refs, i);
		if (!edge || edge->weak)
			continue;

		struct rgc_object *child;
		enum rgc_error err = rgc_ref_lookup(obj->layer->ctx, edge->ref, &child);
		if (err != RGC_OK)
			continue;

		if (child)
			rgc_object_mark(child, key);
	}
}

void rgc_object_pin(struct rgc_object *obj, void **payload) {
	obj->pin++;
	*payload = obj->payload;
}

void rgc_object_unpin(struct rgc_object *obj) {
	obj->pin--;
}

bool rgc_object_can_reference(struct rgc_object *obj, struct rgc_object *robj) {
	/* quick check */
	if (obj->layer->depth < robj->layer->depth)
		return false;

	for (struct rgc_layer *l = obj->layer; l != NULL; l = l->parent)
		if (l == robj->layer)
			return true;

	return false;
}

enum rgc_error rgc_object_link(struct rgc_object *obj, rgc_ref_t r, bool weak) {
	struct rgc_object *robj;
	enum rgc_error err = rgc_ref_lookup(obj->layer->ctx, r, &robj);
	if (err != RGC_OK)
		return err;

	if (!rgc_object_can_reference(obj, robj))
		return RGC_ERR_LIFETIME;

	struct rgc_ref_edge *edge = rgc_ref_edge_vector_append(&obj->refs, 0);
	if (!edge)
		return RGC_ERR_NO_MEMORY;
	edge->ref = r;
	edge->weak = weak;
	return RGC_OK;
}

enum rgc_error rgc_object_unlink(struct rgc_object *obj, rgc_ref_t r) {
	size_t count = rgc_ref_edge_vector_size(&obj->refs);

	for (size_t i = 0; i < count; i++) {
		struct rgc_ref_edge *edge =
		    rgc_ref_edge_vector_get(&obj->refs, i);

		if (edge->ref != r)
			continue;

		if (i != count - 1) {
			struct rgc_ref_edge *last =
			    rgc_ref_edge_vector_get(&obj->refs, count - 1);

			*edge = *last;
		}

		rgc_ref_edge_vector_remove(&obj->refs);
		return RGC_OK;
	}

	return RGC_ERR_NOT_FOUND;
}

void rgc_object_free(struct rgc_object *obj, rgc_ref_t ref) {
	if (obj->finalize)
		obj->finalize(obj->layer->ctx, ref, obj->finalize_userdata);

	rgc_ref_edge_vector_free(&obj->refs);
}
