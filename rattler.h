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

#ifndef __RATTLER_H
#define __RATTLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RATTLER_UNUSED(x) (void)x;

typedef struct rattler_flag rattler_flag;
typedef struct rattler_cmd rattler_cmd;
typedef struct rattler_flag_group rattler_flag_group; 

typedef enum {
    FLAG_BOOL,
    FLAG_STRING,
    FLAG_INT,
    FLAG_FLOAT
} flag_type;

typedef enum {
    FGROUP_MUTUALLY_EXCLUSIVE,
    FGROUP_REQUIRED_TOGETHER,
    FGROUP_ONE_REQUIRED
} flag_group_kind;

struct rattler_flag_group {
    flag_group_kind kind;
    char **names;
    int count;
    rattler_flag_group *next;
};

struct rattler_flag {
    char *name;
    char shorthand;
    char *usage;
    flag_type type;
    bool persistent;
    bool required;
    bool changed;
    char *value_str;
    char *defval_str;
    union {
        bool b;
        int i;
        double f;
    } value;
    union {
        bool b;
        int i;
        double f;
    } def_val;
    rattler_flag *next;
};

typedef void (*rattler_run_fn)(rattler_cmd *cmd, int argc, char **argv);

struct rattler_cmd {
    char *use;
    char *short_desc;
    char *long_desc;
    char *example;
    char *version;

    char **aliases;
    int num_aliases;

    bool hidden;
    char *deprecated;

    rattler_run_fn persistent_pre_run;
    rattler_run_fn pre_run;
    rattler_run_fn run;
    rattler_run_fn post_run;
    rattler_run_fn persistent_post_run;

    rattler_cmd *parent;
    rattler_cmd **children;
    int num_children;
    int cap_children;

    rattler_flag *flags;
    rattler_flag *persistent_flags;
    rattler_flag_group *flag_groups;

    bool silence_errors;
    bool silence_usage;
    bool disable_flag_parsing;
    int min_args;
    int max_args;

    bool help_flag_added;
    bool version_flag_added;
};

rattler_cmd *rattler_new_command(const char *use, const char *short_desc,
                                 const char *long_desc);
void rattler_free(rattler_cmd *root);
void rattler_add_command(rattler_cmd *parent, rattler_cmd *child);
int rattler_execute(rattler_cmd *root, int argc, char **argv);
int rattler_execute_c(rattler_cmd *root, int argc, char **argv);

void rattler_add_alias(rattler_cmd *cmd, const char *alias);

void rattler_flags_bool(rattler_cmd *cmd, const char *name, char sh, bool def,
                        const char *usage);
void rattler_flags_string(rattler_cmd *cmd, const char *name, char sh,
                          const char *def, const char *usage);
void rattler_flags_int(rattler_cmd *cmd, const char *name, char sh, int def,
                       const char *usage);
void rattler_flags_float(rattler_cmd *cmd, const char *name, char sh,
                         double def, const char *usage);

void rattler_persistent_bool(rattler_cmd *cmd, const char *name, char sh,
                             bool def, const char *usage);
void rattler_persistent_string(rattler_cmd *cmd, const char *name, char sh,
                               const char *def, const char *usage);
void rattler_persistent_int(rattler_cmd *cmd, const char *name, char sh,
                            int def, const char *usage);
void rattler_persistent_float(rattler_cmd *cmd, const char *name, char sh,
                              double def, const char *usage);

void rattler_mark_required(rattler_cmd *cmd, const char *flag_name);

void rattler_mark_flags_mutually_exclusive(rattler_cmd *cmd, ...);
void rattler_mark_flags_required_together(rattler_cmd *cmd, ...);
void rattler_mark_flags_one_required(rattler_cmd *cmd, ...);

// flag lookup
rattler_flag *rattler_lookup_flag(rattler_cmd *cmd, const char *name);
rattler_flag *rattler_lookup_flag_short(rattler_cmd *cmd, char shorthand);
bool rattler_flag_changed(rattler_cmd *cmd, const char *name);

// typed value accessors
const char *rattler_flag_string(rattler_cmd *cmd, const char *name);
int rattler_flag_int(rattler_cmd *cmd, const char *name);
double rattler_flag_float(rattler_cmd *cmd, const char *name);
bool rattler_flag_bool(rattler_cmd *cmd, const char *name);

// help / version
void rattler_print_usage(rattler_cmd *cmd);
void rattler_print_help(rattler_cmd *cmd);
void rattler_set_version(rattler_cmd *cmd, const char *version);
void rattler_set_args(rattler_cmd *cmd, int min, int max);

#ifdef __cplusplus
}
#endif
#endif /** end __RATTLER_H */
