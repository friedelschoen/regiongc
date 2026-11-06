#pragma once
#include "rgc-defaults.h"
#include "rgc-macro.h"
#include "rgc-object.h"

#include <regiongc.h>
#include <stdalign.h>
#include <stddef.h>


struct rgc_layer_api {
	enum rgc_error (*new)(struct rgc_layer *l);
	enum rgc_error (*finish)(struct rgc_layer *l);
	enum rgc_error (*collect)(struct rgc_layer *l, rgc_collect_key_t key);
	enum rgc_error (*adopt)(struct rgc_layer *l, struct rgc_object *obj);
	enum rgc_error (*alloc)(struct rgc_layer *l, struct rgc_layer *owner, size_t size, bool init, struct rgc_object **obj);
	void (*destroy)(struct rgc_layer *l);
};

struct rgc_layer {
	struct rgc_context *ctx;
	struct rgc_segment *seg;
	struct rgc_layer *parent; /* NULL = root-layer */
	int flags;

	const struct rgc_layer_api *api;
	size_t seg_mark;
	bool seg_owner; /* this layer is the owner of segment */

	size_t final_depth;

	int depth; /* 0 = root-layer */
	struct rgc_layer *child;
	struct rgc_layer *sibling;

	struct rgc_object *head;
	struct rgc_object *tail;
} __attribute((aligned(RGC_ALIGNMENT)));

CHECK_ALIGNMENT(struct rgc_layer, RGC_ALIGNMENT);

extern const struct rgc_layer_api layer_api_region;
extern const struct rgc_layer_api layer_api_gc;

enum rgc_error rgc_layer_alloc(struct rgc_layer *l, size_t size, bool init, struct rgc_object **obj);
enum rgc_error rgc_layer_adopt(struct rgc_layer *l, struct rgc_object *obj);
void rgc_layer_destroy(struct rgc_layer *l);
