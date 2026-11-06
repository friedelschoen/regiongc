#include "rgc-context.h"

#include "rgc-layer.h"
#include "rgc-segment.h"

#include <regiongc.h>
#include <stdlib.h>
#include <string.h>


static enum rgc_error rgc_layer_static_new(struct rgc_layer *l) {
	(void) l;
	/* never called */
	return RGC_OK;
}

static enum rgc_error rgc_layer_static_finish(struct rgc_layer *l) {
	(void) l;
	/* do nothing */
	return RGC_OK;
}

static void rgc_layer_static_destroy(struct rgc_layer *l) {
	(void) l;
	/* do nothing */
}

static enum rgc_error rgc_layer_static_collect(struct rgc_layer *l, rgc_collect_key_t key) {
	(void) l, (void) key;
	/* do nothing */
	return RGC_OK;
}

static enum rgc_error rgc_layer_static_adopt(struct rgc_layer *l, struct rgc_object *obj) {
	(void) l, (void) obj;
	/* error! */
	return RGC_ERR_RO_LAYER;
}

static enum rgc_error rgc_layer_static_alloc(struct rgc_layer *l, struct rgc_layer *owner, size_t size, bool init, struct rgc_object **obj) {
	(void) l, (void) owner, (void) size, (void) init, (void) obj;
	/* error! */
	return RGC_ERR_RO_LAYER;
}

static const struct rgc_layer_api layer_api_static = {
	.new = rgc_layer_static_new,
	.finish = rgc_layer_static_finish,
	.destroy = rgc_layer_static_destroy,
	.collect = rgc_layer_static_collect,
	.adopt = rgc_layer_static_adopt,
	.alloc = rgc_layer_static_alloc,
};

static void *rgc_default_alloc(void *ptr, size_t oldsize, size_t newsize, void *userdata) {
	(void) oldsize;
	(void) userdata;

	if (newsize == 0) {
		free(ptr);
		return NULL;
	}

	return realloc(ptr, newsize);
}

enum rgc_error rgc_context_new(rgc_allocate_t alloc, void *alloc_userdata, int flags, struct rgc_context **ctxout) {
	if (alloc == NULL)
		alloc = rgc_default_alloc;

	struct rgc_segment *root_seg = rgc_segment_new(alloc, alloc_userdata);
	if (!root_seg)
		return RGC_ERR_NO_MEMORY;

	struct rgc_context *ctx = *ctxout = rgc_segment_alloc(root_seg, sizeof(*ctx), 1, NULL);
	if (ctx == NULL)
		return RGC_ERR_NO_MEMORY;

	ctx->alloc = alloc;
	ctx->alloc_userdata = alloc_userdata;
	ctx->flags = flags;

	rgc_ref_entry_vector_init(&ctx->refs, alloc, alloc_userdata);

	ctx->root.api = &layer_api_static;
	ctx->root.ctx = ctx;
	ctx->root.seg = root_seg;

	return RGC_OK;
}

struct rgc_layer *rgc_context_layer(struct rgc_context *ctx) {
	return &ctx->root;
}

void rgc_context_destroy(struct rgc_context *ctx) {
	struct rgc_segment *seg = ctx->root.seg;

	rgc_layer_destroy(&ctx->root);
	rgc_ref_entry_vector_free(&ctx->refs);
	rgc_segment_free(seg);
}
