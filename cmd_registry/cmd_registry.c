#include <string.h>
#include "cmd_registry.h"
#include "argtable3.h"
#include "cmd_help.h"
#include "cmd_exit.h"
#include "cmd_cd.h"
#include "cmd_pwd.h"
#include "cmd_echo.h"
#include "cmd_ls.h"
#include "cmd_stat.h"
#include "cmd_cat.h"
#include "cmd_head.h"
#include "cmd_tail.h"
#include "cmd_cp.h"
#include "cmd_mv.h"
#include "cmd_rm.h"
#include "cmd_mkdir.h"
#include "cmd_rmdir.h"
#include "cmd_touch.h"
#include "cmd_pkg.h"

/* ── JSON help output ──────────────────────────────────────────────────── */

/* Write a JSON string literal to out, or JSON null if s is NULL */
void cmd_json_str(FILE *out, const char *s)
{
    if (s == NULL)
    {
        fputs("null", out);
        return;
    }
    fputc('"', out);
    for (; *s; s++)
    {
        unsigned char c = (unsigned char)*s;
        if      (c == '"')  { fputs("\\\"", out); }
        else if (c == '\\') { fputs("\\\\", out); }
        else if (c == '\n') { fputs("\\n",  out); }
        else if (c == '\r') { fputs("\\r",  out); }
        else if (c == '\t') { fputs("\\t",  out); }
        else if (c < 0x20)  { fprintf(out, "\\u%04x", c); }
        else                { fputc(c, out); }
    }
    fputc('"', out);
}

/*
 * cmd_print_help_json - Print command help as a JSON object.
 *
 * Emits a JSON object to 'out' with the command's name, summary, long_help,
 * and an options array derived from the argtable3 argument definitions.
 *
 * Input:
 *   out       - Output stream.
 *   name      - Command name.
 *   summary   - One-line description.
 *   long_help - Full description (may be NULL).
 *   argtable  - NULL-terminated argtable3 array for the command.
 */
void cmd_print_help_json(FILE *out, const char *name, const char *summary,
                         const char *long_help, void **argtable)
{
    fprintf(out, "{\n");
    fprintf(out, "  \"name\": ");      cmd_json_str(out, name);      fprintf(out, ",\n");
    fprintf(out, "  \"summary\": ");   cmd_json_str(out, summary);   fprintf(out, ",\n");
    fprintf(out, "  \"long_help\": "); cmd_json_str(out, long_help); fprintf(out, ",\n");
    fprintf(out, "  \"options\": [\n");

    int first = 1;
    for (int i = 0; argtable[i] != NULL; i++)
    {
        arg_hdr_t *hdr = (arg_hdr_t *)argtable[i];
        if (hdr->flag & ARG_TERMINATOR) break;

        if (!first) fprintf(out, ",\n");
        first = 0;

        fprintf(out, "    {");
        fprintf(out, "\"short\": ");
        if (hdr->shortopts && hdr->shortopts[0])
            cmd_json_str(out, hdr->shortopts);
        else
            fputs("null", out);

        fprintf(out, ", \"long\": ");
        if (hdr->longopts && hdr->longopts[0])
            cmd_json_str(out, hdr->longopts);
        else
            fputs("null", out);

        fprintf(out, ", \"datatype\": ");
        cmd_json_str(out, hdr->datatype);

        fprintf(out, ", \"description\": ");
        cmd_json_str(out, hdr->glossary);

        fprintf(out, ", \"required\": %s", hdr->mincount > 0 ? "true" : "false");
        fprintf(out, "}");
    }

    fprintf(out, "\n  ]\n}\n");
}

/* ───────────────────────────────────────────────────────────────────────── */

#define MAX_COMMANDS 64

static const cmd_spec_t *s_registry[MAX_COMMANDS];
static int               s_registry_count = 0;

/*
 * register_command - Register a command specification in the global registry.
 *
 * Adds the given command specification pointer to the static command registry array,
 * if there is space available. This allows the shell to look up and invoke commands
 * by name at runtime.
 *
 * Input:
 *   spec - Pointer to a cmd_spec_t structure describing the command to register.
 *
 * Output:
 *   None (void). The command is added to the registry for later lookup and execution.
 */
void register_command(const cmd_spec_t *spec)
{
    if (s_registry_count < MAX_COMMANDS)
    {
        s_registry[s_registry_count++] = spec;
    }
}

/*
 * find_command - Look up a command specification by name.
 *
 * Searches the global command registry for a command whose name matches the given string.
 * Returns a pointer to the corresponding cmd_spec_t structure if found, or NULL if not found.
 *
 * Input:
 *   name - The name of the command to search for (null-terminated string).
 *
 * Output:
 *   Returns a pointer to the matching cmd_spec_t if found, or NULL if no match exists.
 */
const cmd_spec_t *find_command(const char *name)
{
    for (int i = 0; i < s_registry_count; i++)
    {
        if (strcmp(s_registry[i]->name, name) == 0)
        {
            return s_registry[i];
        }
    }
    return NULL;
}

/*
 * for_each_command - Apply a callback to every registered command.
 *
 * Iterates over all command specifications currently registered in the global registry,
 * invoking the provided callback function for each one. The callback receives a pointer
 * to the command specification and a user-provided data pointer.
 *
 * Input:
 *   cb       - Function pointer to a callback that takes a const cmd_spec_t* and a void* userdata.
 *   userdata - Pointer to user data that will be passed to the callback for each command.
 *
 * Output:
 *   None (void). The callback is invoked for each registered command.
 */
void for_each_command(void (*cb)(const cmd_spec_t *spec, void *userdata),
                      void *userdata)
{
    for (int i = 0; i < s_registry_count; i++)
    {
        cb(s_registry[i], userdata);
    }
}

/*
 * register_all_builtin_commands - Register all built-in shell commands.
 *
 * Calls the registration function for each built-in command, adding their specifications
 * to the global command registry. This ensures that all standard commands are available
 * for lookup and execution by the shell at runtime.
 *
 * Input:
 *   None.
 *
 * Output:
 *   None (void). All built-in commands are registered in the global registry.
 */
void register_all_builtin_commands(void)
{
    register_help_command();
    register_exit_command();
    register_cd_command();
    register_pwd_command();
    register_echo_command();
    register_ls_command();
    register_stat_command();
    register_cat_command();
    register_head_command();
    register_tail_command();
    register_cp_command();
    register_mv_command();
    register_rm_command();
    register_mkdir_command();
    register_rmdir_command();
    register_touch_command();
    register_pkg_command();
}
