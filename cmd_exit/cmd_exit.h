#ifndef CMD_EXIT_H
#define CMD_EXIT_H

#include "cmd_spec.h"

extern cmd_spec_t cmd_exit_spec;

int  exit_run(int argc, char **argv);
void exit_print_usage(FILE *out);
void register_exit_command(void);

#endif /* CMD_EXIT_H */
