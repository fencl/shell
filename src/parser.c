#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <err.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <readline/readline.h>
#include <readline/history.h>

#include <parser.h>

#define	READER_BUFFER_SIZE 1024

/* Test if c is whitespace (excluding newline) */
static bool is_whitespace(char c) {
	switch (c) {
		case '\t':
		case '\r':
		case '\v':
		case '\f':
		case '\0':
		case ' ':
			return (true);
		default:
			return (false);
	}
}

/* Test if c can be part of identifier */
static bool is_identifier(char c) {
	switch (c) {
		case ';':
		case '\n':
		case '#':
		case '|':
		case '>':
		case '<':
		case '\t':
		case '\r':
		case '\v':
		case '\f':
		case '\0':
		case ' ':
			return (false);
		default:
			return (true);
	}
}

/*
 * returns whether reader can supply next character
 * updates buffer if neccesarry
 */
static bool reader_can_supply(reader_t *r) {
	if (r->buffer_ptr >= r->buffer_end) {
		/* only file reader with valid file can update buffer */
		if (r->type == reader_type_file && r->file) {
			ssize_t len = read(r->file, r->buffer_start, READER_BUFFER_SIZE);
			if (len == 0) {
				return (false);
			} else {
				r->buffer_end = r->buffer_start + len;
				r->buffer_ptr = r->buffer_start;
				return (true);
			}
		} else {
			return (false);
		}
	} else {
		return (true);
	}
}

/* supply next character from the source */
void reader_supply(reader_t *r) {
	if (reader_can_supply(r)) {
		r->line = r->line_ahead;
		if (r->c == '\n')
			r->line_ahead += 1;
		r->c = *r->buffer_ptr++;
	} else {
		r->c = '\0';
		r->end = true;
	}
}

/* initalize common reader */
static void reader_init(reader_t *r, reader_type_t type, size_t len) {
	r->line_ahead = r->line = 1;
	r->buffer_end = r->buffer_start + len;
	r->buffer_ptr = r->buffer_start;
	r->c = '\0';
	r->end = false;
	r->type = type;
	reader_supply(r);
}

/* create file reader */
bool reader_create(const char *file, reader_t *r) {
	r->file = open(file, O_RDONLY);
	if (r->file != -1) {
		r->buffer_start = malloc(READER_BUFFER_SIZE);
		if (!r->buffer_start)
			err(1, "malloc");
		reader_init(r, reader_type_file, 0);
		return (true);
	} else {
		return (false);
	}
}

/* crate static buffer reader */
bool reader_create_static(char *input, size_t length, reader_t *r) {
	r->buffer_start = input;
	reader_init(r, reader_type_buffer, length);
	return (true);
}

/* destroy reader and free resources */
void reader_destroy(reader_t *r) {
	if (r->type == reader_type_file) {
		if (r->buffer_start != NULL)
			free(r->buffer_start);
		if (r->file != -1)
			close(r->file);
	}
}

/* helper function for choosing single/double character token (ex.: > or >>) */
static token_t choose_token(reader_t *r, token_type_t deftype, ...) {
	reader_supply(r);
	va_list args;
	va_start(args, deftype);
	for (;;) {
		char c = (char)va_arg(args, int);
		if (c != '\0') {
			token_type_t type = va_arg(args, token_type_t);
			if (r->c == c) {
				va_end(args);
				reader_supply(r);
				return ((token_t) { NULL, type });
			}
		} else {
			break;
		}
	}
	va_end(args);
	return ((token_t) { NULL, deftype });
}

/* helper function for creating single token */
static token_t make_token(reader_t *r, token_type_t type) {
	reader_supply(r);
	return ((token_t) { NULL, type });
}

/* skip any whitespace characters and comments */
static void ignore_whitespace_and_comments(reader_t *r) {
	while (!r->end) {
		while (!r->end && is_whitespace(r->c))
			reader_supply(r);
		if (!r->end && r->c == '#') {
			while (!r->end && r->c != '\n')
				reader_supply(r);
			continue;
		}
		break;
	}
}

/*
 * read next token
 * if result toknen.is identifier and ignore is false,
 * buffer will be allocated with the token string, needs to be freed
 */
token_t read_token(reader_t *r, bool ignore) {
	ignore_whitespace_and_comments(r);
	if (r->end)
		return ((token_t) { NULL, token_type_end });

	switch (r->c) {
		case '\n':
			return (make_token(r, token_type_newline));

		case '|':
			return (make_token(r, token_type_pipe));

		case ';':
			return (choose_token(r, token_type_semicolon,
				';', token_type_double_semicolon,
			0));

		case '>':
			return (choose_token(r, token_type_out_file,
				'>', token_type_append_file,
			0));

		case '<':
			return (make_token(r, token_type_in_file));

		/* identifier */
		default: {
			vec_t token = vec_create();
			while (!r->end && is_identifier(r->c)) {
				if (!ignore)
					vec_add(&token, char, r->c);
				reader_supply(r);
			}
			if (!ignore) {
				char zero_term = '\0';
				vec_add(&token, char, zero_term);
				vec_trim(&token, char);
			}
			return ((token_t) { ignore ? NULL : vec_data(&token, char), token_type_identifier });
		}
	}
}

/* skip all tokens up to line ending */
void skip_rest_line(reader_t *input) {
	token_t tok;
	do {
		tok = read_token(input, true);
	} while (tok.tok != token_type_newline && tok.tok != token_type_end);
}

/*
 * try to load file descriptor from the redirection
 * previous descriptor will be closed
 */
static bool load_file_input_redirection(reader_t *input, int *input_desc) {
	token_t tok = read_token(input, false);
	if (tok.tok != token_type_identifier) {
		fprintf(stderr, "shell: syntax error (line %d): Unexpected token\n", input->line);
		return (false);
	} else {
		if (*input_desc != -1)
			close(*input_desc);
		*input_desc = open(tok.buffer, O_RDWR);
		return (true);
	}
}

/*
 * try to load file descriptor from the redirection
 * previous descriptor will be closed
 */
static bool load_file_output_redirection(reader_t *input, int *output_desc, bool append) {
	token_t tok = read_token(input, false);
	if (tok.tok != token_type_identifier) {
		fprintf(stderr, "shell: syntax error (line %d): Unexpected token\n", input->line);
		return (false);
	} else {
		if (*output_desc != -1)
			close(*output_desc);
		int flags = O_RDWR | O_CREAT;
		if (append)
			flags |= O_APPEND;
		*output_desc = open(tok.buffer, flags, 0666);
		return (true);
	}
}

/* closes command with NULL if not empty */
static bool close_single_command(vec_t *args, bool *empty) {
	if (*empty == false) {
		char *null_term = NULL;
		vec_add(args, char *, null_term);
	}
	return (true);
}

/*
 * load command/pipe segment into args buffer
 * if command is not empty, appends NULL after args
 * redirections will be loaded into input_desc, output_desc
 * returns true if succeeded, false on error
 */
bool load_single_command(reader_t *input, vec_t *args, int *input_desc, int *output_desc, token_type_t *terminator, bool *empty) {
	*input_desc = -1;
	*output_desc = -1;
	*empty = true;
	for (;;) {
		token_t tok = read_token(input, false);
		switch (tok.tok) {
			case token_type_semicolon:
			case token_type_newline:
			case token_type_pipe:
			case token_type_end:
				*terminator = tok.tok;
				return (close_single_command(args, empty));

			case token_type_out_file:
				if (!load_file_output_redirection(input, output_desc, false))
					return (false);

				break;

			case token_type_append_file:
				if (!load_file_output_redirection(input, output_desc, true))
					return (false);
				break;

			case token_type_in_file:
				if (!load_file_input_redirection(input, input_desc))
					return (false);
				break;

			case token_type_double_semicolon:
				fprintf(stderr, "shell: syntax error (line %d): Unexpected token\n", input->line);
				return (false);

			default:
				if (!tok.buffer)
					return (close_single_command(args, empty));
				*empty = false;
				vec_add(args, char *, tok.buffer);
				break;
		}
	}
}
