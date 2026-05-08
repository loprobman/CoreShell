#ifndef CMD_REGISTRY_H
#define CMD_REGISTRY_H

#include "cmd_spec.h"

/* Register a command into the global registry */
void register_command(const cmd_spec_t *spec);

/* Look up a command by name; returns NULL if not found */
const cmd_spec_t *find_command(const char *name);

/* Iterate every registered command; cb is called once per entry */
void for_each_command(void (*cb)(const cmd_spec_t *spec, void *userdata),
                      void *userdata);

/* Register all CoreShell built-in commands */
void register_all_builtin_commands(void);

/* Write a JSON string literal (with escaping) to out, or JSON null */
void cmd_json_str(FILE *out, const char *s);

/* Print command help as a JSON object; options are derived from argtable */
void cmd_print_help_json(FILE *out, const char *name, const char *summary,
                         const char *long_help, void **argtable);

#endif /* CMD_REGISTRY_H */
