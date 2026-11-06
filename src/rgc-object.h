#pragma once

#include "rgc-defaults.h"
#include "rgc-macro.h"
#include "rgc-vector.h"

#include <regiongc.h>
#include <stdalign.h>

struct rgc_ref_edge {
	rgc_ref_t ref;
	bool weak;
};

RGC_VECTOR_DEFINE(struct rgc_ref_edge, rgc_ref_edge_vector)

typedef uint32_t rgc_collect_key_t;

struct rgc_object {
	struct rgc_layer *layer; /* logical layer it should belong to, NULL = free'ed */
	struct rgc_object *next; /* next inside CONTAINING layer */
	rgc_ref_t ref;           /* pointing to ref-tree; NULL = free'ed */
	size_t size;             /* allocated bytes */
	int pin;                 /* borrows of payload */

	rgc_finalizer_t finalize;
	void *finalize_userdata;

	struct rgc_ref_edge_vector refs;
	rgc_collect_key_t collect_key;

	max_align_t payload[];
};

CHECK_ALIGNMENT(struct rgc_object, RGC_ALIGNMENT);

void rgc_object_set_finalizer(struct rgc_object *obj, rgc_finalizer_t finalize, void *userdata);
void rgc_object_free(struct rgc_object *obj, rgc_ref_t ref);

void rgc_object_mark(struct rgc_object *obj, rgc_collect_key_t collect_key);
enum rgc_error rgc_object_move(struct rgc_object *obj, struct rgc_object *newobj);

void rgc_object_pin(struct rgc_object *obj, void **payload);
void rgc_object_unpin(struct rgc_object *obj);

enum rgc_error rgc_object_link(struct rgc_object *obj, rgc_ref_t r, bool weak);
enum rgc_error rgc_object_unlink(struct rgc_object *obj, rgc_ref_t r);
