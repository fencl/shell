#pragma once
#ifndef	__shell_h
#define	__shell_h
#include <stdbool.h>

void set_return_value(int value);
#define	ignore_result(expr, tp) { tp __ig = (expr); (void)__ig; }

#define	invalid_descriptor (-1)
static inline bool descriptor_valid(int desc) {
	return (desc != invalid_descriptor);
}

#endif
