#pragma once
#ifndef	__parser_h
#define	__parser_h
#include <vec.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum reader_type {
	reader_type_file,
	reader_type_buffer
} reader_type_t;

typedef struct reader {
	int file;
	char *buffer_start;
	char *buffer_end;
	char *buffer_ptr;
	char c;
	bool end;
	reader_type_t type;
	unsigned int line, line_ahead;
} reader_t;

typedef enum token_type {
	token_type_end,
	token_type_identifier,
	token_type_semicolon,
	token_type_newline,
	token_type_double_semicolon,
	token_type_pipe,
	token_type_out_file,
	token_type_in_file,
	token_type_append_file,
} token_type_t;

typedef struct token {
	char *buffer;
	token_type_t tok;
} token_t;

bool reader_create(const char *file, reader_t *r);
bool reader_create_static(char *input, size_t length, reader_t *r);
void reader_destroy(reader_t *r);
bool reader_valid(reader_t *r);
bool reader_valid_static(reader_t *r);
token_t read_token(reader_t *src, bool ignore);
void skip_rest_line(reader_t *input);
bool load_single_command(reader_t *input, vec_t *args, int *input_desc, int *output_desc, token_type_t *terminator, bool *empty);
void reader_supply(reader_t *input);

#endif
