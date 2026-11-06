#pragma once

#include "rgc-layer.h"
#include "rgc-object.h"

#include <regiongc.h>

struct rgc_ref_entry {
	uint8_t gen;
	struct rgc_object *obj;
};

RGC_VECTOR_DEFINE(struct rgc_ref_entry, rgc_ref_entry_vector)


struct rgc_context {
	rgc_allocate_t alloc;
	void *alloc_userdata;
	int flags;

	struct rgc_layer root;
	struct rgc_ref_entry_vector refs;

	rgc_collect_key_t collect_key;
};
