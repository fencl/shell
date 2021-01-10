#include <cd.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <err.h>
#include <string.h>

#include <shell.h>

/* get OLDPWD */
char *get_old_path(void) {
	return (getenv("OLDPWD"));
}

/* get PWD */
char *get_new_path(void) {
	return (getenv("PWD"));
}

/* write PWD into OLDPWD, current path into PWD */
static inline void push_new_path() {
	char current_path[PATH_MAX];
	if (getcwd(current_path, PATH_MAX) == NULL)
		err(1, "getcwd");
	char *pwd = getenv("PWD");
	if (pwd)
		setenv("OLDPWD", pwd, 1);
	setenv("PWD", current_path, 1);
}

/* change working directory and push new path */
static void change_dir(reader_t *r, const char *dir) {
	if (chdir(dir) < 0) {
		warn("cd (line %d)", r->line);
		set_return_value(1);
	} else {
		push_new_path();
	}
}

/* execute internal cd command */
void cmd_cd(reader_t *r, char **args, int desc_out) {
	if (args[1] == NULL || strcmp(args[1], "~") == 0) {
		/* cd; cd ~ */
		char *home = getenv("HOME");
		if (home) {
			change_dir(r, home);
		} else {
			fprintf(stderr, "shell: cd (line %d): HOME not set\n", r->line);
			set_return_value(1);
		}
	} else if (strcmp(args[1], "-") == 0) {
		/* cd - */
		char *old = get_old_path();
		if (old) {
			change_dir(r, old);
			char *new_path = get_new_path();
			ignore_result(write(desc_out, new_path, strlen(new_path)), ssize_t);
			ignore_result(write(desc_out, "\n", 1), ssize_t);
		} else {
			fprintf(stderr, "shell: cd (line %d): OLDPWD not set\n", r->line);
			set_return_value(1);
		}
	} else if (args[2] != NULL) {
		fprintf(stderr, "shell: cd (line %d): too many arguments\n", r->line);
		set_return_value(1);
	} else {
		/* cd <path> */
		change_dir(r, args[1]);
	}
}
