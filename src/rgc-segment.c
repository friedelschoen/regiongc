#include "rgc-segment.h"

#include "regiongc.h"
#include "rgc-defaults.h"
#include "rgc-macro.h"

#include <stdalign.h>
#include <stdint.h>
#include <string.h>


#define RGC_SEGMENT_CAPACITY \
	(RGC_SEGMENT_SIZE - sizeof(struct rgc_segment))

struct rgc_segment {
	rgc_allocate_t alloc;
	void *alloc_userdata;

	struct rgc_segment *prev;
	struct rgc_segment *next;

	size_t off;  /* offset from its root */
	size_t size; /* used bytes */

	max_align_t payload[];
};

CHECK_ALIGNMENT(struct rgc_segment, RGC_ALIGNMENT);


size_t rgc_object_size_max = RGC_SEGMENT_SIZE - sizeof(struct rgc_segment);

struct rgc_segment *rgc_segment_new(rgc_allocate_t alloc, void *alloc_userdata) {
	struct rgc_segment *seg = alloc(NULL, 0, RGC_SEGMENT_SIZE, alloc_userdata);
	if (!seg)
		return NULL;

	memset(seg, 0, sizeof(*seg));
	seg->alloc = alloc;
	seg->alloc_userdata = alloc_userdata;
	return seg;
}

static struct rgc_segment *rgc_segment_active(struct rgc_segment *seg) {
	while (seg->next && seg->next->size != 0)
		seg = seg->next;

	return seg;
}

void *rgc_segment_alloc(struct rgc_segment *seg, size_t size, bool init, struct rgc_segment **outseg) {
	if (!seg)
		return NULL;

	if (size > SIZE_MAX - (RGC_ALIGNMENT - 1))
		return NULL;

	size = ALIGN_UP(size, RGC_ALIGNMENT);

	if (size == 0 || size > RGC_SEGMENT_CAPACITY)
		return NULL;

	seg = rgc_segment_active(seg);

	if (size > RGC_SEGMENT_CAPACITY - seg->size) {
		struct rgc_segment *next = seg->next;

		if (!next) {
			next = rgc_segment_new(seg->alloc, seg->alloc_userdata);
			if (!next)
				return NULL;

			seg->next = next;
			next->prev = seg;
		}

		next->off = seg->off + seg->size;
		seg = next;
	}

	size_t off = seg->size;
	seg->size += size;

	uint8_t *payload = (void *) seg->payload;
	payload += off;
	if (init)
		memset(payload, 0, size);

	if (outseg)
		*outseg = seg;

	return payload;
}

size_t rgc_segment_top(struct rgc_segment *seg) {
	if (!seg)
		return SIZE_MAX;

	seg = rgc_segment_active(seg);
	return seg->off + seg->size;
}

void rgc_segment_reset(struct rgc_segment *seg, size_t mark) {
	if (!seg || mark > rgc_segment_top(seg))
		return;

	while (seg->next && seg->next->off < mark)
		seg = seg->next;

	seg->size = mark - seg->off;

	size_t end = mark;

	for (seg = seg->next; seg; seg = seg->next) {
		seg->size = 0;
		seg->off = end;
	}
}

void rgc_segment_cleanup(struct rgc_segment *seg) {
	struct rgc_segment *last_page = seg;

	while (seg) {
		if (seg->size > 0)
			last_page = seg;

		seg = seg->next;
	}

	seg = last_page->next;
	last_page->next = NULL;
	while (seg) {
		struct rgc_segment *next = seg->next;

		rgc_segment_free(seg);
		seg = next;
	}
}

void rgc_segment_free(struct rgc_segment *seg) {
	if (seg->prev)
		seg->prev->next = NULL;

	if (seg->next) {
		rgc_segment_free(seg->next);
		seg->next = NULL;
	}
	seg->alloc(seg, RGC_SEGMENT_SIZE, 0, seg->alloc_userdata);
}

size_t rgc_segment_successors(struct rgc_segment *seg) {
	size_t count = 0;
	for (struct rgc_segment *s = seg->next; s; s = s->next)
		count++;
	return count;
}

size_t rgc_segment_predecessors(struct rgc_segment *seg) {
	size_t count = 0;
	for (struct rgc_segment *s = seg->prev; s; s = s->prev)
		count++;
	return count;
}

size_t rgc_segment_count(struct rgc_segment *seg) {
	return 1 + rgc_segment_successors(seg) + rgc_segment_predecessors(seg);
}
