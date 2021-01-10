#include <vec.h>
#include <stdio.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>

/*
 * create vector
 * vector with no data does not allocate heap space
 */
vec_t vec_create() {
	vec_t v;
	v.data = NULL;
	v.capacity = 0;
	v.count = 0;
	return (v);
}

/* add new element from data */
void vec_add_impl(vec_t *v, void *data, size_t data_size) {
	if (!v->data) {
		v->data = malloc(data_size);
		if (!v->data)
			err(1, "malloc");
		v->capacity = 1;
	} else {
		if (v->count >= v->capacity) {
			while (v->count >= v->capacity)
				v->capacity *= 2;
			v->data = realloc(v->data, data_size * v->capacity);
			if (!v->data)
				err(1, "realloc");
		}
	}

	memcpy(((char *)v->data) + data_size * v->count, data, data_size);
	v->count += 1;
}

/* free vector if needed */
void vec_free(vec_t *v) {
	if (v->data) {
		free(v->data);
		v->data = NULL;
	}
}

/* trim vector allocated size to fit the data */
void vec_trim_impl(vec_t *v, size_t data_size) {
	v->capacity = v->count;
	if (v->capacity == 0) {
		if (v->data)
			free(v->data);
		v->data = NULL;
	} else {
		if (v->data != NULL)
			v->data = realloc(v->data, data_size * v->capacity);
		if (!v->data)
			err(1, "realloc");
	}
}
