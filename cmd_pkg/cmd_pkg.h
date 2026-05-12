#ifndef CMD_PKG_H
#define CMD_PKG_H

#include "cmd_spec.h"

extern cmd_spec_t cmd_pkg_spec;

int pkg_cmd_run(int argc, char **argv);
void pkg_cmd_print_usage(FILE *out);
void register_pkg_command(void);

#endif /* CMD_PKG_H */
