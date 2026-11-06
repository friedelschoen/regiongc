#pragma once

#include <regiongc.h>
#include <stddef.h>


#define RGC_VECTOR_DEFINE(TYPE, NAME)                              \
	struct NAME {                                                  \
		struct rgc_vector base;                                    \
	};                                                             \
                                                                   \
	static inline void NAME##_init(                                \
	    struct NAME *v, rgc_allocate_t alloc, void *userdata) {    \
		rgc_vector_new(alloc, userdata, &v->base, sizeof(TYPE));   \
	}                                                              \
                                                                   \
	static inline size_t NAME##_size(struct NAME *v) {             \
		return rgc_vector_size(&v->base);                          \
	}                                                              \
                                                                   \
	static inline size_t NAME##_capacity(struct NAME *v) {         \
		return rgc_vector_capacity(&v->base);                      \
	}                                                              \
                                                                   \
	static inline TYPE *NAME##_append(struct NAME *v, bool init) { \
		return (TYPE *) rgc_vector_append(&v->base, init);         \
	}                                                              \
                                                                   \
	static inline TYPE *NAME##_get(struct NAME *v, size_t i) {     \
		return (TYPE *) rgc_vector_get(&v->base, i);               \
	}                                                              \
                                                                   \
	static inline void NAME##_remove(struct NAME *v) {             \
		rgc_vector_remove(&v->base);                               \
	}                                                              \
                                                                   \
	static inline void NAME##_clear(struct NAME *v) {              \
		rgc_vector_clear(&v->base);                                \
	}                                                              \
                                                                   \
	static inline void NAME##_free(struct NAME *v) {               \
		if (v == NULL)                                             \
			return;                                                \
		rgc_vector_free(&v->base);                                 \
	}

struct rgc_vector {
	rgc_allocate_t alloc;
	void *alloc_userdata;
	size_t esize;

	void *content;
	size_t size;
	size_t capacity;
};

void rgc_vector_new(rgc_allocate_t alloc, void *userdata, struct rgc_vector *v, size_t esize);

size_t rgc_vector_size(struct rgc_vector *v);
size_t rgc_vector_capacity(struct rgc_vector *v);

void *rgc_vector_get(struct rgc_vector *v, size_t i);
void *rgc_vector_append(struct rgc_vector *v, bool init);
void rgc_vector_remove(struct rgc_vector *v);

void rgc_vector_clear(struct rgc_vector *v);
void rgc_vector_free(struct rgc_vector *v);
