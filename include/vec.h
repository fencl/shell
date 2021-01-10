#pragma once
#ifndef	__vec_h
#define	__vec_h
#include <stddef.h>

/* simple very dynamic array implementation */

typedef struct vec {
	void *data;
	size_t count;
	size_t capacity;
} vec_t;

/* add element to vector, type has to be the consistent */
#define	vec_add(v, type, value) vec_add_impl((v), (void*)&value, sizeof (type));

/* trim vector capacity to fit the data, type has to be the consistent */
#define	vec_trim(v, type) vec_trim_impl((v), sizeof (type));

/* get element from vector, type has to be the consistent */
#define	vec_get(v, type, index) (((type *)(v).data)[index])

/*
 * each element in vector to use in for loop, iterator is pointer to element
 * ex.: for (vec_each(vector, int, i)) {
 *          sum += *i;
 *      }
 */
#define	vec_each(v, type, it) type *it = (type*)(v)->data; it != (type *)(v)->data + (v)->count; ++it

/* get vector content as pointer to data, type has to be the consistent */
#define	vec_data(v, type) ((type *)(v)->data)


void vec_add_impl(vec_t *v, void* data, size_t data_size);
void vec_free(vec_t *v);
vec_t vec_create();
void vec_trim_impl(vec_t *v, size_t data_size);

#endif
