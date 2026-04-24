#ifndef CMD_CD_H
#define CMD_CD_H

#include "cmd_spec.h"

extern cmd_spec_t cmd_cd_spec;

int  cd_run(int argc, char **argv);
void cd_print_usage(FILE *out);
void register_cd_command(void);

#endif /* CMD_CD_H */
