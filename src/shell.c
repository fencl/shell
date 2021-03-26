#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <libgen.h>
#include <limits.h>
#include <signal.h>
#include <setjmp.h>

#include <shell.h>
#include <vec.h>
#include <parser.h>
#include <cd.h>

int return_value = 0;
bool do_exit = false;

/* set shell internal return value */
void set_return_value(int value) {
	return_value = value;
}

/* signal handler */
sigjmp_buf sigint_jump;
void signal_handler(int sig) {
	switch (sig) {
		case SIGINT:
			printf("\n");
			set_return_value(128 + sig);
			siglongjmp(sigint_jump, 1);
			break;
		default:
			break;
	}
}

/* wait for pid and maybe kill it */
static void wait_for_pid(pid_t pid, bool last, bool *sigint_pipe) {
	if (pid > 0) {
		int stat;
		if (!*sigint_pipe && sigsetjmp(sigint_jump, 1) == 0) {
			while (waitpid(pid, &stat, 0) > 0) { }
			if (last)
				set_return_value(WEXITSTATUS(stat));
		} else {
			kill(pid, SIGINT);
			*sigint_pipe = true;
		}
	}
}

/* run command with specified pipe in/out */
static pid_t run(reader_t *r, char **args, int pipe_in, int pipe_out) {
	char *command = args[0];
	if (strcmp(command, "cd") == 0) {
		int out_desc = descriptor_valid(pipe_out) ? pipe_out : STDOUT_FILENO;
		cmd_cd(r, args, out_desc);
		return (0);
	} else if (strcmp(command, "exit") == 0) {
		do_exit = true;
		return (0);
	} else {
		pid_t pid;
		if ((pid = fork()) < 0) {
			err(1, "fork");
		} else if (pid == 0) {
			if (descriptor_valid(pipe_in)) {
				dup2(pipe_in, STDIN_FILENO);
				close(pipe_in);
			}
			if (descriptor_valid(pipe_out)) {
				dup2(pipe_out, STDOUT_FILENO);
				close(pipe_out);
			}
			args[0] = basename(args[0]);
			if (execvp(command, args) < 0)
				err(127, "%s (line %d)", command, r->line);
		} else {
			if (descriptor_valid(pipe_in))
				close(pipe_in);
			if (descriptor_valid(pipe_out))
				close(pipe_out);
		}
		return (pid);
	}
}

/* if new descriptor is valid, try close original and swap */
static void try_swap_descriptors(int *orig, int *new) {
	if (descriptor_valid(*new)) {
		if (descriptor_valid(*orig)) {
			close(*orig);
		}
		*orig = *new;
	}
}

/* create pipe if command terminated with pipe */
static bool pipe_if_pipe(token_type_t terminator, int pipe_desc[2]) {
	if (terminator == token_type_pipe) {
		if (pipe(pipe_desc) == -1)
			err(1, "pipe");
		return (true);
	} else {
		return (false);
	}
}

/* recursive pipe processor, processes single command up to ; \n or eof */
static void pipe_recur(vec_t *args, reader_t *input, int pipe_in, bool *sigint_pipe) {
	size_t cursor = args->count;
	token_type_t terminator;
	bool empty;
	int pipe_desc[2] = { invalid_descriptor, invalid_descriptor };
	int input_desc = invalid_descriptor;
	int output_desc = invalid_descriptor;

	if (load_single_command(input, args, &input_desc, &output_desc, &terminator, &empty)) {
		if (!empty) {

			/* maybe create pipe, create process, maybe recurse pipe, wait process */
			bool do_pipe = pipe_if_pipe(terminator, pipe_desc);
			try_swap_descriptors(&pipe_in, &input_desc);
			try_swap_descriptors(&pipe_desc[1], &output_desc);
			pid_t pid = run(input, vec_data(args, char *) + cursor, pipe_in, pipe_desc[1]);
			if (do_pipe)
				pipe_recur(args, input, pipe_desc[0], sigint_pipe);
			wait_for_pid(pid, !do_pipe, sigint_pipe);

		} else if (descriptor_valid(pipe_in)) {

			fprintf(stderr, "shell: syntax error (line %d): Pipe expected command\n", input->line);
			close(pipe_in);
			set_return_value(1);
			skip_rest_line(input);

		}
	} else {
		set_return_value(1);
		skip_rest_line(input);
	}

	if (descriptor_valid(input_desc))
		close(input_desc);
	if (descriptor_valid(output_desc))
		close(output_desc);
}

/* execute all commands from input */
static void execute(reader_t *input) {
	while (!input->end && !do_exit) {
		vec_t args = vec_create();
		bool sigint_pipe = false;
		pipe_recur(&args, input, invalid_descriptor, &sigint_pipe);
		for (vec_each(&args, char *, arg))
			free(*arg);
		vec_free(&args);
	}
}

/* execute all commands from file */
static void execute_file(char *input_file) {
	reader_t input;
	if (reader_create(input_file, &input)) {
		execute(&input);
	} else {
		err(1, "%s (line %d)", input_file, input.line);
	}
	reader_destroy(&input);
}

/* execute all commands from string */
static void execute_string(char *string) {
	reader_t input;
	if (reader_create_static(string, strlen(string), &input))
		execute(&input);
	reader_destroy(&input);
}

/* execute commands from stdin */
static void interactive(void) {
	char *buffer = (char *)malloc(PATH_MAX + 2);
	if (!buffer)
		err(1, "malloc");
	while (!do_exit) {
		while (sigsetjmp(sigint_jump, 1) != 0) { }
		sprintf(buffer, "%s$ ", get_new_path());
		char *line = readline(buffer);
		if (line != NULL) {
			execute_string(line);
			add_history(line);
			free(line);
		} else {
			break;
		}
	}
	free(buffer);
}

int main(int argc, char **args) {
	signal(SIGINT, signal_handler);
	if (argc <= 1) {
		interactive();
	} else {
		if (strcmp(args[1], "-c") == 0) {
			if (argc <= 2) {
				fprintf(stderr, "shell: Expected argument after -c\n");
				set_return_value(1);
			} else {
				execute_string(args[2]);
			}
		} else {
			execute_file(args[1]);
		}
	}
	return (return_value);
}
