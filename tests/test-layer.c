#include "allocator.h"

#include <check.h>
#include <regiongc.h>
#include <stdalign.h>
#include <stddef.h>

/* ---------------------------------------------------------
 * Common helpers
 * --------------------------------------------------------- */

static struct rgc_context *new_context(
    struct alloc_state *alloc,
    int flags) {
	struct rgc_context *ctx = NULL;

	ck_assert_int_eq(
	    rgc_context_new(allocate, alloc, flags, &ctx),
	    RGC_OK);
	ck_assert_ptr_nonnull(ctx);

	return ctx;
}

static struct rgc_layer *new_layer(
    struct rgc_layer *parent,
    int flags) {
	struct rgc_layer *layer = NULL;

	ck_assert_int_eq(
	    rgc_layer_new(parent, flags, &layer),
	    RGC_OK);
	ck_assert_ptr_nonnull(layer);

	return layer;
}

static rgc_ref_t new_ref(struct rgc_layer *layer, size_t size) {
	rgc_ref_t ref = RGC_NOREF;

	ck_assert_int_eq(rgc_ref_new(layer, size, &ref), RGC_OK);
	ck_assert_uint_ne(ref, RGC_NOREF);

	return ref;
}

/* ---------------------------------------------------------
 * Layer: general lifecycle
 * --------------------------------------------------------- */

START_TEST(test_alloc_free) {
	struct alloc_state alloc = {};
	struct rgc_context *ctx = new_context(&alloc, 0);

	struct rgc_layer *layer =
	    new_layer(rgc_context_layer(ctx), 0);

	ck_assert_int_eq(rgc_layer_finish(layer), RGC_OK);

	rgc_context_destroy(ctx);
	ck_assert_int_eq(alloc.current, 0);
}
END_TEST

START_TEST(test_nested) {
	struct alloc_state alloc = {};
	struct rgc_context *ctx = new_context(&alloc, 0);

	struct rgc_layer *a =
	    new_layer(rgc_context_layer(ctx), 0);
	struct rgc_layer *b = new_layer(a, 0);
	struct rgc_layer *c = new_layer(b, 0);

	ck_assert_int_eq(rgc_layer_finish(c), RGC_OK);
	ck_assert_int_eq(rgc_layer_finish(b), RGC_OK);
	ck_assert_int_eq(rgc_layer_finish(a), RGC_OK);

	rgc_context_destroy(ctx);
	ck_assert_int_eq(alloc.current, 0);
}
END_TEST

START_TEST(test_not_top) {
	struct alloc_state alloc = {};
	struct rgc_context *ctx = new_context(&alloc, 0);

	struct rgc_layer *parent =
	    new_layer(rgc_context_layer(ctx), 0);
	struct rgc_layer *child = new_layer(parent, 0);

	ck_assert_int_eq(
	    rgc_layer_finish(parent),
	    RGC_ERR_NOT_TOP);

	/* Failed finish must leave both layers usable. */
	ck_assert_int_eq(rgc_layer_finish(child), RGC_OK);
	ck_assert_int_eq(rgc_layer_finish(parent), RGC_OK);

	rgc_context_destroy(ctx);
	ck_assert_int_eq(alloc.current, 0);
}
END_TEST

START_TEST(test_cascade) {
	struct alloc_state alloc = {};
	struct rgc_context *ctx = new_context(&alloc, RGC_CASCADE);

	struct rgc_layer *a =
	    new_layer(rgc_context_layer(ctx), 0);
	struct rgc_layer *b = new_layer(a, 0);
	struct rgc_layer *c = new_layer(b, 0);

	rgc_ref_t ref = new_ref(c, 32);

	ck_assert_int_eq(rgc_layer_finish(a), RGC_OK);
	ck_assert_int_ne(rgc_ref_valid(ctx, ref), RGC_OK);

	rgc_context_destroy(ctx);
	ck_assert_int_eq(alloc.current, 0);
}
END_TEST

START_TEST(test_siblings) {
	struct alloc_state alloc = {};
	struct rgc_context *ctx = new_context(&alloc, 0);

	struct rgc_layer *parent =
	    new_layer(rgc_context_layer(ctx), 0);

	struct rgc_layer *a = new_layer(parent, 0);
	struct rgc_layer *b = new_layer(parent, 0);
	struct rgc_layer *c = new_layer(parent, 0);

	rgc_ref_t ref_b = new_ref(b, 16);
	rgc_ref_t ref_c = new_ref(c, 16);

	ck_assert_int_eq(rgc_layer_finish(a), RGC_OK);

	ck_assert_int_eq(rgc_ref_valid(ctx, ref_b), RGC_OK);
	ck_assert_int_eq(rgc_ref_valid(ctx, ref_c), RGC_OK);

	ck_assert_int_eq(rgc_layer_finish(b), RGC_OK);
	ck_assert_int_eq(rgc_ref_valid(ctx, ref_c), RGC_OK);

	ck_assert_int_eq(rgc_layer_finish(c), RGC_OK);
	ck_assert_int_eq(rgc_layer_finish(parent), RGC_OK);

	rgc_context_destroy(ctx);
	ck_assert_int_eq(alloc.current, 0);
}
END_TEST

START_TEST(test_final) {
	struct alloc_state alloc = {};
	struct rgc_context *ctx = new_context(&alloc, 0);

	struct rgc_layer *layer =
	    new_layer(rgc_context_layer(ctx), RGC_LAYER_FINAL);

	struct rgc_layer *nested = new_layer(layer, 0);

	ck_assert_ptr_eq(layer, nested);

	rgc_ref_t ref = new_ref(layer, 16);

	/* Nested finish only decreases the nesting counter. */
	ck_assert_int_eq(rgc_layer_finish(nested), RGC_OK);
	ck_assert_int_eq(rgc_ref_valid(ctx, ref), RGC_OK);

	/* The second finish actually destroys the layer. */
	ck_assert_int_eq(rgc_layer_finish(layer), RGC_OK);
	ck_assert_int_ne(rgc_ref_valid(ctx, ref), RGC_OK);

	rgc_context_destroy(ctx);
	ck_assert_int_eq(alloc.current, 0);
}
END_TEST

/* ---------------------------------------------------------
 * Region: allocation and lifetime
 * --------------------------------------------------------- */

START_TEST(test_region_alloc) {
	struct alloc_state alloc = {};
	struct rgc_context *ctx = new_context(&alloc, 0);

	struct rgc_layer *region =
	    new_layer(rgc_context_layer(ctx), 0);

	rgc_ref_t ref = new_ref(region, sizeof(unsigned));

	unsigned *value = NULL;

	ck_assert_int_eq(
	    rgc_ref_pin(ctx, ref, (void **) &value),
	    RGC_OK);

	ck_assert_ptr_nonnull(value);
	*value = 1234;

	ck_assert_int_eq(rgc_ref_unpin(ctx, ref), RGC_OK);

	value = NULL;
	ck_assert_int_eq(
	    rgc_ref_pin(ctx, ref, (void **) &value),
	    RGC_OK);

	ck_assert_uint_eq(*value, 1234);

	ck_assert_int_eq(rgc_ref_unpin(ctx, ref), RGC_OK);
	ck_assert_int_eq(rgc_layer_finish(region), RGC_OK);

	rgc_context_destroy(ctx);
	ck_assert_int_eq(alloc.current, 0);
}
END_TEST

START_TEST(test_region_invalidate) {
	struct alloc_state alloc = {};
	struct rgc_context *ctx = new_context(&alloc, 0);

	struct rgc_layer *region =
	    new_layer(rgc_context_layer(ctx), 0);

	rgc_ref_t ref = new_ref(region, 32);

	ck_assert_int_eq(rgc_ref_valid(ctx, ref), RGC_OK);
	ck_assert_int_eq(rgc_layer_finish(region), RGC_OK);

	ck_assert_int_ne(rgc_ref_valid(ctx, ref), RGC_OK);

	rgc_context_destroy(ctx);
	ck_assert_int_eq(alloc.current, 0);
}
END_TEST

START_TEST(test_region_pin) {
	struct alloc_state alloc = {};
	struct rgc_context *ctx = new_context(&alloc, 0);

	struct rgc_layer *region =
	    new_layer(rgc_context_layer(ctx), 0);

	rgc_ref_t ref = new_ref(region, sizeof(unsigned));

	unsigned *value = NULL;
	ck_assert_int_eq(
	    rgc_ref_pin(ctx, ref, (void **) &value),
	    RGC_OK);

	*value = 42;

	ck_assert_int_eq(
	    rgc_layer_finish(region),
	    RGC_ERR_PINNED);

	/* Rejected finish must not invalidate borrowed memory. */
	ck_assert_uint_eq(*value, 42);
	ck_assert_int_eq(rgc_ref_valid(ctx, ref), RGC_OK);

	ck_assert_int_eq(rgc_ref_unpin(ctx, ref), RGC_OK);
	ck_assert_int_eq(rgc_layer_finish(region), RGC_OK);

	rgc_context_destroy(ctx);
	ck_assert_int_eq(alloc.current, 0);
}
END_TEST

START_TEST(test_region_survivor) {
	struct alloc_state alloc = {};
	struct rgc_context *ctx = new_context(&alloc, 0);

	struct rgc_layer *parent =
	    new_layer(rgc_context_layer(ctx), 0);

	rgc_ref_t parent_ref = new_ref(parent, sizeof(unsigned));

	unsigned *value = NULL;
	ck_assert_int_eq(
	    rgc_ref_pin(ctx, parent_ref, (void **) &value),
	    RGC_OK);
	*value = 9876;
	ck_assert_int_eq(rgc_ref_unpin(ctx, parent_ref), RGC_OK);

	struct rgc_layer *child = new_layer(parent, 0);
	rgc_ref_t child_ref = new_ref(child, sizeof(unsigned));

	ck_assert_int_eq(rgc_layer_finish(child), RGC_OK);

	ck_assert_int_eq(rgc_ref_valid(ctx, parent_ref), RGC_OK);
	ck_assert_int_ne(rgc_ref_valid(ctx, child_ref), RGC_OK);

	ck_assert_int_eq(
	    rgc_ref_pin(ctx, parent_ref, (void **) &value),
	    RGC_OK);
	ck_assert_uint_eq(*value, 9876);
	ck_assert_int_eq(rgc_ref_unpin(ctx, parent_ref), RGC_OK);

	ck_assert_int_eq(rgc_layer_finish(parent), RGC_OK);

	rgc_context_destroy(ctx);
	ck_assert_int_eq(alloc.current, 0);
}
END_TEST

/* ---------------------------------------------------------
 * GC: tracing and reachability
 * --------------------------------------------------------- */

START_TEST(test_gc_unreachable) {
	struct alloc_state alloc = {};
	struct rgc_context *ctx = new_context(&alloc, 0);

	struct rgc_layer *gc =
	    new_layer(rgc_context_layer(ctx), RGC_LAYER_GC);

	rgc_ref_t ref = new_ref(gc, 32);

	ck_assert_int_eq(rgc_layer_collect(gc), RGC_OK);
	ck_assert_int_ne(rgc_ref_valid(ctx, ref), RGC_OK);

	rgc_context_destroy(ctx);
	ck_assert_int_eq(alloc.current, 0);
}
END_TEST

START_TEST(test_gc_pinned) {
	struct alloc_state alloc = {};
	struct rgc_context *ctx = new_context(&alloc, 0);

	struct rgc_layer *gc =
	    new_layer(rgc_context_layer(ctx), RGC_LAYER_GC);

	rgc_ref_t ref = new_ref(gc, sizeof(unsigned));

	unsigned *value = NULL;

	ck_assert_int_eq(
	    rgc_ref_pin(ctx, ref, (void **) &value),
	    RGC_OK);

	*value = 42;

	ck_assert_int_eq(rgc_layer_collect(gc), RGC_OK);
	ck_assert_int_eq(rgc_ref_valid(ctx, ref), RGC_OK);
	ck_assert_uint_eq(*value, 42);

	ck_assert_int_eq(rgc_ref_unpin(ctx, ref), RGC_OK);

	ck_assert_int_eq(rgc_layer_collect(gc), RGC_OK);
	ck_assert_int_ne(rgc_ref_valid(ctx, ref), RGC_OK);

	rgc_context_destroy(ctx);
	ck_assert_int_eq(alloc.current, 0);
}
END_TEST

START_TEST(test_gc_strong) {
	struct alloc_state alloc = {};
	struct rgc_context *ctx = new_context(&alloc, 0);

	struct rgc_layer *gc =
	    new_layer(rgc_context_layer(ctx), RGC_LAYER_GC);

	rgc_ref_t target = new_ref(gc, 32);

	struct rgc_layer *region = new_layer(gc, 0);
	rgc_ref_t owner = new_ref(region, 16);

	ck_assert_int_eq(
	    rgc_ref_link(ctx, owner, target, false),
	    RGC_OK);

	ck_assert_int_eq(rgc_layer_collect(gc), RGC_OK);
	ck_assert_int_eq(rgc_ref_valid(ctx, target), RGC_OK);

	/* Removing the strong edge makes the target collectible. */
	ck_assert_int_eq(
	    rgc_ref_unlink(ctx, owner, target),
	    RGC_OK);

	ck_assert_int_eq(rgc_layer_collect(gc), RGC_OK);
	ck_assert_int_ne(rgc_ref_valid(ctx, target), RGC_OK);

	rgc_context_destroy(ctx);
	ck_assert_int_eq(alloc.current, 0);
}
END_TEST

START_TEST(test_gc_weak) {
	struct alloc_state alloc = {};
	struct rgc_context *ctx = new_context(&alloc, 0);

	struct rgc_layer *gc =
	    new_layer(rgc_context_layer(ctx), RGC_LAYER_GC);

	rgc_ref_t target = new_ref(gc, 32);

	struct rgc_layer *region = new_layer(gc, 0);
	rgc_ref_t owner = new_ref(region, 16);

	ck_assert_int_eq(
	    rgc_ref_link(ctx, owner, target, true),
	    RGC_OK);

	ck_assert_int_eq(rgc_layer_collect(gc), RGC_OK);
	ck_assert_int_ne(rgc_ref_valid(ctx, target), RGC_OK);

	/* The owner itself remains alive. */
	ck_assert_int_eq(rgc_ref_valid(ctx, owner), RGC_OK);

	rgc_context_destroy(ctx);
	ck_assert_int_eq(alloc.current, 0);
}
END_TEST

START_TEST(test_gc_cycle) {
	struct alloc_state alloc = {};
	struct rgc_context *ctx = new_context(&alloc, 0);

	struct rgc_layer *gc =
	    new_layer(rgc_context_layer(ctx), RGC_LAYER_GC);

	rgc_ref_t a = new_ref(gc, 16);
	rgc_ref_t b = new_ref(gc, 16);

	ck_assert_int_eq(rgc_ref_link(ctx, a, b, false), RGC_OK);
	ck_assert_int_eq(rgc_ref_link(ctx, b, a, false), RGC_OK);

	/* An unreachable cycle must not keep itself alive. */
	ck_assert_int_eq(rgc_layer_collect(gc), RGC_OK);

	ck_assert_int_ne(rgc_ref_valid(ctx, a), RGC_OK);
	ck_assert_int_ne(rgc_ref_valid(ctx, b), RGC_OK);

	rgc_context_destroy(ctx);
	ck_assert_int_eq(alloc.current, 0);
}
END_TEST


START_TEST(test_region_lifetime) {
	struct alloc_state alloc = {};
	struct rgc_context *ctx = new_context(&alloc, 0);

	struct rgc_layer *parent =
	    new_layer(rgc_context_layer(ctx), 0);

	rgc_ref_t a = new_ref(parent, 16);

	struct rgc_layer *child = new_layer(parent, 0);
	rgc_ref_t b = new_ref(child, 16);

	/* Child -> parent is allowed. */
	ck_assert_int_eq(rgc_ref_link(ctx, b, a, false), RGC_OK);

	/* Parent -> child is forbidden. */
	ck_assert_int_eq(
	    rgc_ref_link(ctx, a, b, false),
	    RGC_ERR_LIFETIME);

	ck_assert_int_eq(rgc_layer_finish(child), RGC_OK);
	ck_assert_int_eq(rgc_layer_finish(parent), RGC_OK);

	rgc_context_destroy(ctx);
	ck_assert_int_eq(alloc.current, 0);
}
END_TEST

START_TEST(test_gc_branches) {
	struct alloc_state alloc = {};
	struct rgc_context *ctx = new_context(&alloc, 0);

	struct rgc_layer *gc =
	    new_layer(rgc_context_layer(ctx), RGC_LAYER_GC);

	rgc_ref_t a = new_ref(gc, 16);
	rgc_ref_t b = new_ref(gc, 16);

	struct rgc_layer *left = new_layer(gc, 0);
	struct rgc_layer *right = new_layer(gc, 0);

	rgc_ref_t left_root = new_ref(left, 16);
	rgc_ref_t right_root = new_ref(right, 16);

	ck_assert_int_eq(rgc_ref_link(ctx, left_root, a, false), RGC_OK);
	ck_assert_int_eq(rgc_ref_link(ctx, right_root, b, false), RGC_OK);

	/* Both sibling branches must contribute roots. */
	ck_assert_int_eq(rgc_layer_collect(gc), RGC_OK);

	ck_assert_int_eq(rgc_ref_valid(ctx, a), RGC_OK);
	ck_assert_int_eq(rgc_ref_valid(ctx, b), RGC_OK);

	ck_assert_int_eq(rgc_ref_unlink(ctx, left_root, a), RGC_OK);

	ck_assert_int_eq(rgc_layer_collect(gc), RGC_OK);

	ck_assert_int_ne(rgc_ref_valid(ctx, a), RGC_OK);
	ck_assert_int_eq(rgc_ref_valid(ctx, b), RGC_OK);

	rgc_context_destroy(ctx);
	ck_assert_int_eq(alloc.current, 0);
}
END_TEST

START_TEST(test_cascade_pinned) {
	struct alloc_state alloc = {};
	struct rgc_context *ctx = new_context(&alloc, RGC_CASCADE);

	struct rgc_layer *parent =
	    new_layer(rgc_context_layer(ctx), 0);

	struct rgc_layer *child = new_layer(parent, 0);

	rgc_ref_t ref = new_ref(child, sizeof(unsigned));

	unsigned *value = NULL;
	ck_assert_int_eq(
	    rgc_ref_pin(ctx, ref, (void **) &value),
	    RGC_OK);

	*value = 123;

	/* Destruction must stop at the pinned descendant. */
	ck_assert_int_eq(
	    rgc_layer_finish(parent),
	    RGC_ERR_PINNED);

	ck_assert_uint_eq(*value, 123);
	ck_assert_int_eq(rgc_ref_valid(ctx, ref), RGC_OK);

	ck_assert_int_eq(rgc_ref_unpin(ctx, ref), RGC_OK);
	ck_assert_int_eq(rgc_layer_finish(parent), RGC_OK);

	rgc_context_destroy(ctx);
	ck_assert_int_eq(alloc.current, 0);
}
END_TEST


/* ---------------------------------------------------------
 * Context: creation and destruction
 * --------------------------------------------------------- */

START_TEST(test_context_empty) {
	struct alloc_state alloc = {};
	struct rgc_context *ctx = NULL;

	ck_assert_int_eq(
	    rgc_context_new(allocate, &alloc, 0, &ctx),
	    RGC_OK);

	ck_assert_ptr_nonnull(ctx);

	rgc_context_destroy(ctx);
	ck_assert_int_eq(alloc.current, 0);
}
END_TEST

START_TEST(test_context_regions) {
	struct alloc_state alloc = {};
	struct rgc_context *ctx = new_context(&alloc, 0);

	struct rgc_layer *a =
	    new_layer(rgc_context_layer(ctx), 0);
	struct rgc_layer *b = new_layer(a, 0);
	struct rgc_layer *c = new_layer(b, 0);

	new_ref(a, 32);
	new_ref(b, 64);
	new_ref(c, 128);

	rgc_context_destroy(ctx);
	ck_assert_int_eq(alloc.current, 0);
}
END_TEST

START_TEST(test_context_branches) {
	struct alloc_state alloc = {};
	struct rgc_context *ctx = new_context(&alloc, 0);

	struct rgc_layer *root = rgc_context_layer(ctx);

	struct rgc_layer *a = new_layer(root, 0);
	struct rgc_layer *b = new_layer(root, 0);
	struct rgc_layer *c = new_layer(root, 0);

	new_ref(a, 16);
	new_ref(b, 32);
	new_ref(c, 64);

	rgc_context_destroy(ctx);
	ck_assert_int_eq(alloc.current, 0);
}
END_TEST

START_TEST(test_context_gc) {
	struct alloc_state alloc = {};
	struct rgc_context *ctx = new_context(&alloc, 0);

	struct rgc_layer *gc =
	    new_layer(rgc_context_layer(ctx), RGC_LAYER_GC);

	rgc_ref_t a = new_ref(gc, 32);
	rgc_ref_t b = new_ref(gc, 64);

	ck_assert_int_eq(rgc_ref_link(ctx, a, b, false), RGC_OK);

	/* Destruction must reclaim objects regardless of reachability. */
	rgc_context_destroy(ctx);
	ck_assert_int_eq(alloc.current, 0);
}
END_TEST

START_TEST(test_context_mixed) {
	struct alloc_state alloc = {};
	struct rgc_context *ctx = new_context(&alloc, 0);

	struct rgc_layer *gc =
	    new_layer(rgc_context_layer(ctx), RGC_LAYER_GC);

	struct rgc_layer *a = new_layer(gc, 0);
	struct rgc_layer *b = new_layer(gc, 0);

	rgc_ref_t target = new_ref(gc, 128);
	rgc_ref_t owner_a = new_ref(a, 16);
	rgc_ref_t owner_b = new_ref(b, 16);

	ck_assert_int_eq(
	    rgc_ref_link(ctx, owner_a, target, false),
	    RGC_OK);
	ck_assert_int_eq(
	    rgc_ref_link(ctx, owner_b, target, true),
	    RGC_OK);

	rgc_context_destroy(ctx);
	ck_assert_int_eq(alloc.current, 0);
}
END_TEST

static void count_finalizer(
    struct rgc_context *ctx,
    rgc_ref_t ref,
    void *userdata) {
	(void) ctx;
	(void) ref;

	unsigned *count = userdata;
	++*count;
}


START_TEST(test_context_finalizers) {
	struct alloc_state alloc = {};
	struct rgc_context *ctx = new_context(&alloc, 0);

	struct rgc_layer *gc =
	    new_layer(rgc_context_layer(ctx), RGC_LAYER_GC);

	rgc_ref_t a = new_ref(gc, 16);
	rgc_ref_t b = new_ref(gc, 16);
	rgc_ref_t c = new_ref(gc, 16);

	unsigned finalized = 0;

	ck_assert_int_eq(
	    rgc_ref_set_finalizer(
	        ctx, a, count_finalizer, &finalized),
	    RGC_OK);

	ck_assert_int_eq(
	    rgc_ref_set_finalizer(
	        ctx, b, count_finalizer, &finalized),
	    RGC_OK);

	ck_assert_int_eq(
	    rgc_ref_set_finalizer(
	        ctx, c, count_finalizer, &finalized),
	    RGC_OK);

	rgc_context_destroy(ctx);
	ck_assert_uint_eq(finalized, 3);
	ck_assert_int_eq(alloc.current, 0);
}
END_TEST


/* ---------------------------------------------------------
 * Suite registration
 * --------------------------------------------------------- */

int main(void) {
	Suite *suite = suite_create("regiongc");

	TCase *layer = tcase_create("layer");
	tcase_add_test(layer, test_alloc_free);
	tcase_add_test(layer, test_nested);
	tcase_add_test(layer, test_not_top);
	tcase_add_test(layer, test_cascade);
	tcase_add_test(layer, test_cascade_pinned);
	tcase_add_test(layer, test_siblings);
	tcase_add_test(layer, test_final);
	suite_add_tcase(suite, layer);

	TCase *region = tcase_create("region");
	tcase_add_test(region, test_region_alloc);
	tcase_add_test(region, test_region_invalidate);
	tcase_add_test(region, test_region_pin);
	tcase_add_test(region, test_region_survivor);
	tcase_add_test(region, test_region_lifetime);
	suite_add_tcase(suite, region);

	TCase *gc = tcase_create("gc");
	tcase_add_test(gc, test_gc_unreachable);
	tcase_add_test(gc, test_gc_pinned);
	tcase_add_test(gc, test_gc_strong);
	tcase_add_test(gc, test_gc_weak);
	tcase_add_test(gc, test_gc_cycle);
	tcase_add_test(gc, test_gc_branches);
	suite_add_tcase(suite, gc);

	TCase *context = tcase_create("context");
	tcase_add_test(context, test_context_empty);
	tcase_add_test(context, test_context_regions);
	tcase_add_test(context, test_context_branches);
	tcase_add_test(context, test_context_gc);
	tcase_add_test(context, test_context_mixed);
	tcase_add_test(context, test_context_finalizers);
	suite_add_tcase(suite, context);

	SRunner *runner = srunner_create(suite);
	//	srunner_set_fork_status(runner, CK_NOFORK);
	srunner_run_all(runner, CK_VERBOSE);

	int failed = srunner_ntests_failed(runner);
	srunner_free(runner);

	return failed == 0 ? 0 : 1;
}
