#ifndef CMD_HELP_H
#define CMD_HELP_H

#include "cmd_spec.h"

extern cmd_spec_t cmd_help_spec;

int help_run(int argc, char **argv);
void help_print_usage(FILE *out);
void register_help_command(void);

#endif /* CMD_HELP_H */
