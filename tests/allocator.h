#pragma once

#include <stddef.h>


struct alloc_state {
	size_t limit;
	size_t current;

	size_t last_oldsize;
	size_t last_newsize;
};

void *allocate(void *ptr, size_t oldsize, size_t newsize, void *userdata);
