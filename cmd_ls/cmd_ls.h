#ifndef CMD_LS_H
#define CMD_LS_H

#include "cmd_spec.h"

extern cmd_spec_t cmd_ls_spec;

int  ls_run(int argc, char **argv);
void ls_print_usage(FILE *out);
void register_ls_command(void);

#endif /* CMD_LS_H */
