#include "rgc-vector.h"

#include <regiongc.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_CAPACITY 16

void rgc_vector_new(rgc_allocate_t alloc, void *alloc_userdata, struct rgc_vector *v, size_t esize) {
	memset(v, 0, sizeof(*v));
	v->alloc = alloc;
	v->alloc_userdata = alloc_userdata;
	v->esize = esize;
}

size_t rgc_vector_size(struct rgc_vector *v) {
	return v->size;
}

size_t rgc_vector_capacity(struct rgc_vector *v) {
	return v->capacity;
}

void *rgc_vector_append(struct rgc_vector *v, bool init) {
	if (v->size >= v->capacity) {
		size_t newcap = v->capacity ? v->capacity * 2 : DEFAULT_CAPACITY;
		void *newptr = v->alloc(v->content, v->capacity * v->esize, newcap * v->esize, v->alloc_userdata);
		if (newptr == NULL)
			return NULL;
		v->content = newptr;
		v->capacity = newcap;
	}

	void *ptr = rgc_vector_get(v, v->size++);
	if (init)
		memset(ptr, 0, v->esize);
	return ptr;
}

void *rgc_vector_get(struct rgc_vector *v, size_t i) {
	if (i >= v->size)
		return NULL;
	return (unsigned char *) v->content + (i * v->esize);
}

void rgc_vector_remove(struct rgc_vector *v) {
	v->size--;
}

void rgc_vector_clear(struct rgc_vector *v) {
	v->size = 0;
}

void rgc_vector_free(struct rgc_vector *v) {
	if (v == NULL)
		return;

	if (v->content)
		v->alloc(v->content, v->capacity * v->esize, 0, v->alloc_userdata);

	v->content = NULL;
	v->capacity = 0;
	v->size = 0;
}
