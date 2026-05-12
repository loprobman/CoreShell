#include <stdio.h>
#include "cmd_pkg.h"
#include "pkg/pkg.h"
#include "cmd_registry.h"

int pkg_cmd_run(int argc, char **argv)
{
    return pkg_run(argc, argv);
}

void pkg_cmd_print_usage(FILE *out)
{
    pkg_print_usage(out);
}

cmd_spec_t cmd_pkg_spec = {
    .name        = "pkg",
    .summary     = "manage CoreShell packages",
    .long_help   = "Build, install, list, remove, compile, and upgrade packages for CoreShell.",
    .run         = pkg_cmd_run,
    .print_usage = pkg_cmd_print_usage,
};

void register_pkg_command(void)
{
    register_command(&cmd_pkg_spec);
}
