#include "rgc-ref.h"

#include "rgc-context.h"
#include "rgc-object.h"

static size_t rgc_context_ref_slot(struct rgc_context *ctx) {
	for (size_t i = 1; i < rgc_ref_entry_vector_size(&ctx->refs); i++) {
		if (rgc_ref_entry_vector_get(&ctx->refs, i)->obj == NULL)
			return i;
	}

	if (rgc_ref_entry_vector_size(&ctx->refs) == 0) {
		/* special noref entry */
		if (!rgc_ref_entry_vector_append(&ctx->refs, 1))
			return SIZE_MAX;
	}

	if (!rgc_ref_entry_vector_append(&ctx->refs, 1))
		return SIZE_MAX;
	return rgc_ref_entry_vector_size(&ctx->refs) - 1;
}

enum rgc_error rgc_ref_new(struct rgc_layer *l, size_t size, rgc_ref_t *out) {
	size_t slot = rgc_context_ref_slot(l->ctx);
	if (slot == SIZE_MAX)
		return RGC_ERR_NO_MEMORY;

	struct rgc_ref_entry *entry = rgc_ref_entry_vector_get(&l->ctx->refs, slot);
	uint32_t gen = entry->gen;
	rgc_ref_t ref = slot | (gen << 24);

	enum rgc_error err = rgc_layer_alloc(l, size, 1, &entry->obj);
	if (err != RGC_OK)
		return err;
	entry->obj->ref = ref;
	*out = ref;
	return RGC_OK;
}

enum rgc_error rgc_ref_valid(struct rgc_context *ctx, rgc_ref_t r) {
	return rgc_ref_lookup(ctx, r, NULL);
}

enum rgc_error rgc_ref_lookup(struct rgc_context *ctx, rgc_ref_t r, struct rgc_object **objout) {
	size_t idx = RGC_REF_INDEX(r);
	uint8_t gen = RGC_REF_GEN(r);

	if (idx == 0 || idx >= rgc_ref_entry_vector_size(&ctx->refs))
		return RGC_ERR_NULL_REF;

	struct rgc_ref_entry *entry = rgc_ref_entry_vector_get(&ctx->refs, idx);
	if (entry->gen != gen)
		return RGC_ERR_INVAL_REF;

	if (objout)
		*objout = entry->obj;
	return RGC_OK;
}

enum rgc_error rgc_ref_resize(struct rgc_context *ctx, rgc_ref_t ref, size_t newsize) {
	struct rgc_object *obj;
	enum rgc_error err = rgc_ref_lookup(ctx, ref, &obj);
	if (err != RGC_OK)
		return err;

	if (obj->pin)
		return RGC_ERR_PINNED;

	if (newsize <= obj->size) {
		obj->size = newsize;
		return RGC_OK;
	}

	struct rgc_object *newobj;
	err = rgc_layer_alloc(obj->layer, newsize, 0, &newobj);
	if (err != RGC_OK)
		return err;

	return rgc_object_move(obj, newobj);
}

enum rgc_error rgc_ref_set_finalizer(struct rgc_context *ctx, rgc_ref_t ref, rgc_finalizer_t finalize, void *userdata) {
	struct rgc_object *obj;
	enum rgc_error err = rgc_ref_lookup(ctx, ref, &obj);
	if (err != RGC_OK)
		return err;

	rgc_object_set_finalizer(obj, finalize, userdata);
	return RGC_OK;
}

enum rgc_error rgc_ref_clear(struct rgc_context *ctx, rgc_ref_t ref) {
	struct rgc_ref_entry *entry = rgc_ref_entry_vector_get(&ctx->refs, RGC_REF_INDEX(ref));
	if (entry == NULL)
		return RGC_ERR_INVAL_REF;
	if (entry->obj == NULL)
		return RGC_ERR_NULL_REF;
	if (entry->gen != RGC_REF_GEN(ref))
		return RGC_ERR_INVAL_REF;

	rgc_object_free(entry->obj, ref);

	entry->obj = NULL;
	entry->gen++; /* new generation */
	return RGC_OK;
}

enum rgc_error rgc_ref_pin(struct rgc_context *ctx, rgc_ref_t ref, void **payload) {
	struct rgc_object *obj;
	enum rgc_error err = rgc_ref_lookup(ctx, ref, &obj);
	if (err != RGC_OK)
		return err;

	rgc_object_pin(obj, payload);
	return RGC_OK;
}

enum rgc_error rgc_ref_unpin(struct rgc_context *ctx, rgc_ref_t ref) {
	struct rgc_object *obj;
	enum rgc_error err = rgc_ref_lookup(ctx, ref, &obj);
	if (err != RGC_OK)
		return err;

	rgc_object_unpin(obj);
	return RGC_OK;
}

enum rgc_error rgc_ref_link(struct rgc_context *ctx, rgc_ref_t ref, rgc_ref_t other, bool weak) {
	struct rgc_object *obj;
	enum rgc_error err = rgc_ref_lookup(ctx, ref, &obj);
	if (err != RGC_OK)
		return err;

	return rgc_object_link(obj, other, weak);
}

enum rgc_error rgc_ref_unlink(struct rgc_context *ctx, rgc_ref_t ref, rgc_ref_t other) {
	struct rgc_object *obj;
	enum rgc_error err = rgc_ref_lookup(ctx, ref, &obj);
	if (err != RGC_OK)
		return err;

	return rgc_object_unlink(obj, other);
}
