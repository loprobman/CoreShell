#ifndef CMD_STAT_H
#define CMD_STAT_H

#include "cmd_spec.h"

extern cmd_spec_t cmd_stat_spec;

int  stat_run(int argc, char **argv);
void stat_print_usage(FILE *out);
void register_stat_command(void);

#endif /* CMD_STAT_H */
