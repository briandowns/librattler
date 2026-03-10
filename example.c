/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Brian J. Downs
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdbool.h>
#include <stdio.h>

#include "rattler.h"

static void
remove_run(rattler_cmd *cmd, int argc, char **argv)
{
    bool force = rattler_flag_bool(cmd, "force");

    printf("Removing %d item(s)%s\n", argc, force ? " (forced)" : "");

    for (int i = 0; i < argc; i++) {
        printf("  - %s\n", argv[i]);
    }
}

static void
export_run(rattler_cmd *cmd, int argc, char **argv)
{
    RATTLER_UNUSED(argc);
    RATTLER_UNUSED(argv);

    bool json = rattler_flag_bool(cmd, "json");
    bool yaml = rattler_flag_bool(cmd, "yaml");

    printf("Exporting as %s\n", json ? "JSON" : yaml ? "YAML" : "default");
}

// login: --user and --pass required together
static void
login_run(rattler_cmd *cmd, int argc, char **argv)
{
    RATTLER_UNUSED(argc);
    RATTLER_UNUSED(argv);

    const char *user = rattler_flag_string(cmd, "user");

    printf("Logged in as: %s\n", *user ? user : "(anonymous)");
}

// process: --file or --stdin (one required)
static void
process_run(rattler_cmd *cmd, int argc, char **argv)
{
    RATTLER_UNUSED(argc);
    RATTLER_UNUSED(argv);

    const char *file = rattler_flag_string(cmd, "file");
    bool from_stdin  = rattler_flag_bool(cmd, "stdin");

    if (from_stdin) {
        printf("Processing from stdin\n");
    } else {
        printf("Processing file: %s\n", file);
    }
}

// output individually required 
static void
convert_run(rattler_cmd *cmd, int argc, char **argv)
{
    RATTLER_UNUSED(argc);
    RATTLER_UNUSED(argv);

    printf("Converting -> %s\n", rattler_flag_string(cmd, "output"));
}

int
main(int argc, char **argv)
{
    rattler_cmd *root = rattler_new_command(
        "example [command]", "rattler feature demo",
        "Demonstrates aliases, mutually-exclusive flags,\n"
        "required-together flags, one-required flags, and\n"
        "individually required flags.");
    rattler_set_version(root, "0.1.0");
    rattler_persistent_bool(root, "verbose", 'v', false, "verbose output");

    // remove with aliases
    rattler_cmd *rem = rattler_new_command(
        "remove [flags] [items...]", "Remove items",
        "Remove one or more items.\nAlso callable as: rm, del");
    rem->run = remove_run;
    rattler_add_alias(rem, "rm");
    rattler_add_alias(rem, "del");
    rattler_flags_bool(rem, "force", 'f', false, "skip confirmation");

    // export: mutually exclusive
    rattler_cmd *exp = rattler_new_command(
        "export [flags]", "Export data",
        "Export data in JSON or YAML format (not both).");
    exp->run = export_run;
    rattler_flags_bool(exp, "json", 'j', false, "export as JSON");
    rattler_flags_bool(exp, "yaml", 'y', false, "export as YAML");
    rattler_mark_flags_mutually_exclusive(exp, "json", "yaml", NULL);

    // login: required together
    rattler_cmd *login = rattler_new_command(
        "login [flags]", "Log in to the service",
        "Authenticate. --user and --pass must be supplied together.");
    login->run = login_run;
    rattler_flags_string(login, "user", 'u', "", "username");
    rattler_flags_string(login, "pass", 'p', "", "password");
    rattler_mark_flags_required_together(login, "user", "pass", NULL);

    // process: one required
    rattler_cmd *proc = rattler_new_command(
        "process [flags]", "Process input",
        "Process data from a file or stdin (one must be given).");
    proc->run = process_run;
    rattler_flags_string(proc, "file",  'f', "", "input file path");
    rattler_flags_bool(proc, "stdin", 's', false, "read from stdin");
    rattler_mark_flags_one_required(proc, "file", "stdin", NULL);

    // convert: individually required
    rattler_cmd *conv = rattler_new_command(
        "convert [flags]", "Convert a file",
        "Convert a file. --output is always required.");
    conv->run = convert_run;
    rattler_flags_string(conv, "output", 'o', "", "output file path");
    rattler_mark_required(conv, "output");

    rattler_add_command(root, rem);
    rattler_add_command(root, exp);
    rattler_add_command(root, login);
    rattler_add_command(root, proc);
    rattler_add_command(root, conv);

    if (rattler_execute(root, argc, argv) != 0) {
        fprintf(stderr, "error: failed to run\n");
        return 1;
    }

    rattler_free(root);

    return 0;
}
