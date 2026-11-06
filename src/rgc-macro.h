#pragma once

#define MIN(a, b)            ((a) < (b) ? (a) : (b))
#define MAX(a, b)            ((a) > (b) ? (a) : (b))
#define ALIGN_DOWN(n, align) ((n) & ~((size_t) (align) - 1))
#define ALIGN_UP(n, align)   (((n) + (align) - 1) & ~((align) - 1))
#define STRINGIFY(s)         #s

#define CHECK_ALIGNMENT(t, a) \
	_Static_assert(alignof(t) == a, STRINGIFY(t) " must be aligned to " STRINGIFY(a))
