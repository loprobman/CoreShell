#ifndef CMD_CAT_H
#define CMD_CAT_H

#include "cmd_spec.h"

extern cmd_spec_t cmd_cat_spec;

int  cat_run(int argc, char **argv);
void cat_print_usage(FILE *out);
void register_cat_command(void);

#endif /* CMD_CAT_H */
