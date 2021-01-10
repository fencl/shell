#pragma once
#ifndef	__cd_h
#define	__cd_h
#include <parser.h>

void cmd_cd(reader_t *r, char **args, int desc_out);
char *get_old_path(void);
char *get_new_path(void);

#endif
