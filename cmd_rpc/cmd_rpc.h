#ifndef CMD_RPC_H
#define CMD_RPC_H

#include "cmd_spec.h"

extern cmd_spec_t cmd_rpc_spec;

int  rpc_run(int argc, char **argv);
void rpc_print_usage(FILE *out);
void register_rpc_command(void);

#endif /* CMD_RPC_H */
