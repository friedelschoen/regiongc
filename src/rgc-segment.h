#pragma once
#include <regiongc.h>

/** Creates a new segmented stack */
struct rgc_segment *rgc_segment_new(rgc_allocate_t alloc, void *userdata);

/** Returns the current offset */
size_t rgc_segment_top(struct rgc_segment *seg);

/** Allocates a new object on the segment, returns offset of that object. */
void *rgc_segment_alloc(struct rgc_segment *seg, size_t size, bool init, struct rgc_segment **outseg);

/** Resets the segment top to mark, and empties all segments after */
void rgc_segment_reset(struct rgc_segment *seg, size_t mark);

/** Removes all remaining empty pages */
void rgc_segment_cleanup(struct rgc_segment *seg);

/** Unlinks and frees this segment en successors */
void rgc_segment_free(struct rgc_segment *seg);

/* more or less helper functions: */

size_t rgc_segment_successors(struct rgc_segment *seg);
size_t rgc_segment_predecessors(struct rgc_segment *seg);
size_t rgc_segment_count(struct rgc_segment *seg);
