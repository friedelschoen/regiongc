#include "allocator.h"

#include <stdlib.h>

void *allocate(void *ptr, size_t oldsize, size_t newsize, void *userdata) {
	struct alloc_state *state = userdata;
	state->last_oldsize = oldsize;
	state->last_newsize = newsize;

	if (newsize == 0) {
		state->current -= oldsize;
		free(ptr);
		return NULL;
	}

	if (state->limit > 0 && newsize - oldsize > state->limit - state->current) {
		return NULL;
	}

	state->current += newsize - oldsize;
	return realloc(ptr, newsize);
}
