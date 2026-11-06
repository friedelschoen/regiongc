#include "allocator.h"
#include "rgc-defaults.h"
#include "rgc-segment.h"

#include <check.h>
#include <regiongc.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdlib.h>


START_TEST(test_alloc_free) {
	struct alloc_state alloc = {};
	struct rgc_segment *seg = rgc_segment_new(allocate, &alloc);
	ck_assert(seg);

	ck_assert_ptr_nonnull(rgc_segment_alloc(seg, rgc_object_size_max, 0, NULL));

	rgc_segment_free(seg);
	ck_assert_int_eq(alloc.current, 0);
}
END_TEST

START_TEST(test_alloc_free_cascade) {
	struct alloc_state alloc = {};
	struct rgc_segment *seg = rgc_segment_new(allocate, &alloc);
	ck_assert(seg);

	for (int i = 0; i < 10; i++)
		ck_assert_ptr_nonnull(rgc_segment_alloc(seg, rgc_object_size_max, 0, NULL));

	ck_assert_int_eq(rgc_segment_successors(seg), 9);

	rgc_segment_free(seg);
	ck_assert_int_eq(alloc.current, 0);
}
END_TEST

START_TEST(test_alloc_free_child) {
	struct alloc_state alloc = {};
	struct rgc_segment *seg = rgc_segment_new(allocate, &alloc);
	ck_assert(seg);

	ck_assert_ptr_nonnull(rgc_segment_alloc(seg, rgc_object_size_max, 0, NULL));
	struct rgc_segment *seg2;
	ck_assert_ptr_nonnull(rgc_segment_alloc(seg, rgc_object_size_max, 0, &seg2));
	ck_assert_ptr_ne(seg, seg2);

	ck_assert_int_eq(rgc_segment_successors(seg), 1);
	rgc_segment_free(seg2);

	ck_assert_int_eq(rgc_segment_successors(seg), 0);
	rgc_segment_free(seg);

	ck_assert_int_eq(alloc.current, 0);
}
END_TEST

START_TEST(test_alloc_align) {
	struct alloc_state alloc = {};
	struct rgc_segment *seg = rgc_segment_new(allocate, &alloc);
	ck_assert(seg);

	for (size_t size = 1; size <= 128; size++) {
		void *ptr = rgc_segment_alloc(seg, size, 0, NULL);

		ck_assert(ptr);
		ck_assert_int_eq((uintptr_t) ptr % alignof(max_align_t), 0);
	}

	rgc_segment_free(seg);
}
END_TEST

START_TEST(test_alloc_limit) {
	struct alloc_state alloc = {
		.limit = RGC_SEGMENT_SIZE
	};
	struct rgc_segment *seg = rgc_segment_new(allocate, &alloc);
	ck_assert(seg);

	ck_assert_ptr_nonnull(rgc_segment_alloc(seg, rgc_object_size_max, 0, NULL));
	ck_assert_ptr_null(rgc_segment_alloc(seg, rgc_object_size_max, 0, NULL));

	rgc_segment_free(seg);
}
END_TEST

START_TEST(test_alloc_object_size) {
	struct alloc_state alloc = {};
	struct rgc_segment *seg = rgc_segment_new(allocate, &alloc);
	ck_assert(seg);

	ck_assert_ptr_null(rgc_segment_alloc(seg, RGC_SEGMENT_SIZE, 0, NULL));
	ck_assert_ptr_nonnull(rgc_segment_alloc(seg, rgc_object_size_max, 0, NULL));

	rgc_segment_free(seg);
}
END_TEST

START_TEST(test_alloc_next_segment) {
	struct alloc_state alloc = {};
	struct rgc_segment *seg = rgc_segment_new(allocate, &alloc);
	ck_assert(seg);

	struct rgc_segment *outseg;
	ck_assert_ptr_nonnull(rgc_segment_alloc(seg, rgc_object_size_max, 0, &outseg));
	ck_assert_ptr_nonnull(rgc_segment_alloc(seg, rgc_object_size_max, 0, &outseg));
	ck_assert_ptr_ne(seg, outseg);

	rgc_segment_free(seg);
}
END_TEST

START_TEST(test_reset_zero) {
	struct alloc_state alloc = {};
	struct rgc_segment *seg = rgc_segment_new(allocate, &alloc);
	ck_assert(seg);

	ck_assert_int_eq(rgc_segment_top(seg), 0);
	ck_assert_ptr_nonnull(rgc_segment_alloc(seg, 1, 0, NULL));
	ck_assert_int_ne(rgc_segment_top(seg), 0);
	rgc_segment_reset(seg, 0);
	ck_assert_int_eq(rgc_segment_top(seg), 0);

	rgc_segment_free(seg);
}
END_TEST


START_TEST(test_reset_cleanup) {
	struct alloc_state alloc = {};
	struct rgc_segment *seg = rgc_segment_new(allocate, &alloc);
	ck_assert(seg);

	ck_assert_int_eq(rgc_segment_top(seg), 0);
	ck_assert_ptr_nonnull(rgc_segment_alloc(seg, rgc_object_size_max, 0, NULL));
	ck_assert_ptr_nonnull(rgc_segment_alloc(seg, rgc_object_size_max, 0, NULL));
	ck_assert_int_ne(rgc_segment_top(seg), 0);
	ck_assert_int_eq(rgc_segment_successors(seg), 1);

	rgc_segment_reset(seg, 0);
	ck_assert_int_eq(rgc_segment_top(seg), 0);
	ck_assert_int_eq(rgc_segment_successors(seg), 1);

	rgc_segment_cleanup(seg);
	ck_assert_int_eq(rgc_segment_top(seg), 0);
	ck_assert_int_eq(rgc_segment_successors(seg), 0);

	rgc_segment_free(seg);
}
END_TEST

int main(void) {
	Suite *suite = suite_create("segment");

	TCase *alloc = tcase_create("alloc");
	tcase_add_test(alloc, test_alloc_free);
	tcase_add_test(alloc, test_alloc_free_cascade);
	tcase_add_test(alloc, test_alloc_free_child);
	tcase_add_test(alloc, test_alloc_align);
	tcase_add_test(alloc, test_alloc_limit);
	tcase_add_test(alloc, test_alloc_object_size);
	tcase_add_test(alloc, test_alloc_next_segment);
	suite_add_tcase(suite, alloc);

	TCase *reset = tcase_create("reset");
	tcase_add_test(alloc, test_reset_zero);
	tcase_add_test(alloc, test_reset_cleanup);
	suite_add_tcase(suite, reset);

	SRunner *runner = srunner_create(suite);
	srunner_run_all(runner, CK_VERBOSE);

	int failed = srunner_ntests_failed(runner);

	srunner_free(runner);

	return failed == 0 ? 0 : 1;
}
