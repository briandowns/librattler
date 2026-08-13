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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rattler.h"

static char*
_strdup(const char *s)
{
    if (s == NULL) {
        return NULL;
    }

    char *d = malloc(strlen(s) + 1);
    if (d == NULL) {
        return NULL;
    }

    return strcpy(d, s);
}

static char*
cmd_name(const rattler_cmd *cmd)
{
    if (cmd->use == NULL) {
        return _strdup("(unknown)");
    }

    char *c = _strdup(cmd->use);
    if (c == NULL) {
        return NULL;
    }
    char *sp = strchr(c, ' ');
    if (sp != NULL) {
        *sp = '\0';
    }

    return c;
}

static void
build_path(const rattler_cmd *cmd, char *buf, size_t sz)
{
    if (cmd == NULL) {
        return;
    }

    if (cmd->parent) {
        build_path(cmd->parent, buf, sz);
        strncat(buf, " ", sz-strlen(buf)-1);
    }

    char *n = cmd_name(cmd);
    strncat(buf, n, sz-strlen(buf)-1);

    free(n);
}

static rattler_flag*
new_flag(const char *name, char sh, const char *usage,
         flag_type type, bool persistent)
{
    rattler_flag *flag = calloc(1, sizeof *flag);
    if (flag == NULL) {
        return NULL;
    }

    flag->name = _strdup(name);
    if (flag->name == NULL) {
        free(flag);
        return NULL;
    }

    flag->shorthand = sh;
    flag->usage = _strdup(usage);
    if (flag->usage == NULL) {
        free(flag->name);
        free(flag);
        return NULL;
    }

    flag->type = type;
    flag->persistent = persistent;

    return flag;
}

static void
append_flag(rattler_flag **list, rattler_flag *flag)
{
    if (*list == NULL) {
        *list = flag;
        return;
    }

    rattler_flag *c = *list;

    while (c->next) {
        c = c->next;
    }
    c->next = flag;
}

static void
free_flag_list(rattler_flag *flag)
{
    while (flag != NULL) {
        rattler_flag *nx = flag->next;

        free(flag->name);
        free(flag->usage);

        if (flag->type == FLAG_STRING) {
            free(flag->value_str);
            free(flag->defval_str);
        }

        free(flag);
        flag = nx;
    }
}

static void
free_flag_groups(rattler_flag_group *groups)
{
    while (groups) {
        rattler_flag_group *nx = groups->next;

        for (int i = 0; i < groups->count; i++) {
            free(groups->names[i]);
        }

        free(groups->names);
        free(groups);

        groups = nx;
    }
}

static rattler_flag_group*
make_flag_group(flag_group_kind kind, va_list ap)
{
    rattler_flag_group *group = calloc(1, sizeof *group);
    if (group == NULL) {
        return NULL;
    }

    group->kind = kind;
    int cap = 8;

    group->names = malloc((size_t)cap * sizeof(char*));
    if (group->names == NULL) {
        free(group);
        return NULL;
    }

    const char *name;
    while ((name = va_arg(ap, const char*)) != NULL) {
        if (group->count >= cap) {
            cap *= 2;

            char **names = realloc(group->names, (size_t)cap * sizeof(char*));
            if (names == NULL) {
                free(group);
                return NULL;
            }
            group->names = names;
        }

        char *n = _strdup(name);
        if (n == NULL) {
            return NULL;
        }
        group->names[group->count++] = n;
    }

    return group;
}

static void
append_flag_group(rattler_cmd *cmd, rattler_flag_group *group)
{
    if (cmd->flag_groups == NULL) {
        cmd->flag_groups = group;
        return;
    }

    rattler_flag_group *f_group = cmd->flag_groups;

    while (f_group->next) {
        f_group = f_group->next;
    }
    f_group->next = group;
}

rattler_cmd*
rattler_new_command(const char *use, const char *short_desc,
                    const char *long_desc)
{
    if (use == NULL || *use == '\0') {
        return NULL;
    }
    if (short_desc == NULL) {
        short_desc = "";
    }
    if (long_desc == NULL) {
        long_desc = "";
    }

    rattler_cmd *cmd = calloc(1, sizeof *cmd);
    if (cmd == NULL) {
        return NULL;
    }

    char *u = _strdup(use);
    if (u == NULL) {
        free(cmd);
        return NULL;
    }
    cmd->use = u;

    char *s = _strdup(short_desc);
    if (s == NULL) {
        free(cmd->use);
        free(cmd);

        return NULL;
    }
    cmd->short_desc = s;

    char *l = _strdup(long_desc);
    if (l == NULL) {
        free(cmd->use);
        free(cmd->short_desc);
        free(cmd);

        return NULL;
    }
    cmd->long_desc = l;
    cmd->max_args = -1;

    return cmd;
}

void
rattler_free(rattler_cmd *root)
{
    if (root == NULL) {
        return;
    }

    for (int i = 0; i < root->num_children; i++) {
        rattler_free(root->children[i]);
    }

    free(root->use);
    free(root->short_desc);
    free(root->long_desc);
    free(root->version);
    free(root->deprecated);

    for (int i = 0; i < root->num_aliases; i++) {
        free(root->aliases[i]);
    }

    free(root->aliases);
    free_flag_list(root->flags);
    free_flag_list(root->persistent_flags);
    free_flag_groups(root->flag_groups);
    free(root->children);
    free(root);
}

void
rattler_add_command(rattler_cmd *parent, rattler_cmd *child)
{
    child->parent = parent;

    if (parent->num_children >= parent->cap_children) {
        parent->cap_children = parent->cap_children ? parent->cap_children*2:4;
        rattler_cmd **children = realloc(parent->children,
            (size_t)parent->cap_children * sizeof(*children));

        if (children == NULL) {
            return;
        }
        parent->children = children;
    }
    parent->children[parent->num_children++] = child;
}

void
rattler_set_version(rattler_cmd *cmd, const char *version)
{
    if (version == NULL) {
        return;
    }

    char *ver = _strdup(version);
    if (ver == NULL) {
        return;
    }

    free(cmd->version);
    cmd->version = ver;
}

void
rattler_set_args(rattler_cmd *cmd, int min, int max)
{
    cmd->min_args = min;
    cmd->max_args = max;
}

void
rattler_add_alias(rattler_cmd *cmd, const char *alias)
{
    char **new_aliases = realloc(cmd->aliases,
        (size_t)(cmd->num_aliases + 1) * sizeof(char *));
    if (new_aliases == NULL) {
        return;
    }
    cmd->aliases = new_aliases;

    char *a = _strdup(alias);
    if (a == NULL) {
        return;
    }
    cmd->aliases[cmd->num_aliases++] = a;
}

void
rattler_flags_bool(rattler_cmd *cmd, const char *name, char sh, bool def,
                   const char *usage)
{
    rattler_flag *flag = new_flag(name, sh, usage, FLAG_BOOL, false);
    if (flag == NULL) {
        return;
    }
    flag->value.b = def;
    flag->def_val.b = def;

    append_flag(&cmd->flags, flag);
}

void
rattler_flags_string(rattler_cmd *cmd, const char *name, char sh,
                     const char *def, const char *usage)
{
    rattler_flag *flag = new_flag(name, sh, usage, FLAG_STRING, false);
    if (flag == NULL) {
        return;
    }

    char *d1 = _strdup(def);
    if (d1 == NULL) {
        free(flag);
        return;
    }
    flag->value_str = d1;

    char *d2 = _strdup(def);
    if (d2 == NULL) {
        free(flag->value_str);
        free(flag);
        return;
    }
    flag->defval_str = d2;

    append_flag(&cmd->flags, flag);
}

void
rattler_flags_int(rattler_cmd *cmd, const char *name, char sh, int def,
                  const char *usage)
{
    rattler_flag *flag = new_flag(name, sh, usage, FLAG_INT, false);
    if (flag == NULL) {
        return;
    }

    flag->value.i = def;
    flag->def_val.i = def;

    append_flag(&cmd->flags, flag);
}

void
rattler_flags_float(rattler_cmd *cmd, const char *name, char sh, double def,
                    const char *usage)
{
    rattler_flag *flag = new_flag(name, sh, usage, FLAG_FLOAT, false);
    if (flag == NULL) {
        return;
    }

    flag->value.f = def;
    flag->def_val.f = def;

    append_flag(&cmd->flags, flag);
}

void
rattler_persistent_bool(rattler_cmd *cmd, const char *name, char sh, bool def,
                        const char *usage)
{
    rattler_flag *flag = new_flag(name, sh, usage, FLAG_BOOL, true);
    if (flag == NULL) {
        return;
    }

    flag->value.b = def;
    flag->def_val.b = def;

    append_flag(&cmd->persistent_flags, flag);
}

void
rattler_persistent_string(rattler_cmd *cmd, const char *name, char sh,
                          const char *def, const char *usage)
{
    rattler_flag *flag = new_flag(name, sh, usage, FLAG_STRING, true);
    if (flag == NULL) {
        return;
    }

    char *d1 = _strdup(def);
    if (d1 == NULL) {
        free(flag);
        return;
    }
    flag->value_str = d1;

    char *d2 = _strdup(def);
    if (d2 == NULL) {
        free(flag->value_str);
        free(flag);
        return;
    }
    flag->defval_str = d2;

    append_flag(&cmd->persistent_flags, flag);
}

void
rattler_persistent_int(rattler_cmd *cmd, const char *name, char sh, int def,
                       const char *usage)
{
    rattler_flag *flag = new_flag(name, sh, usage, FLAG_INT, true);
    if (flag == NULL) {
        return;
    }

    flag->value.i = def;
    flag->def_val.i = def;

    append_flag(&cmd->persistent_flags, flag);
}

void
rattler_persistent_float(rattler_cmd *cmd, const char *name, char sh,
                         double def, const char *usage)
{
    rattler_flag *flag = new_flag(name, sh, usage, FLAG_FLOAT, true);
    if (flag == NULL) {
        return;
    }

    flag->value.f = def;
    flag->def_val.f = def;

    append_flag(&cmd->persistent_flags, flag);
}

void
rattler_mark_required(rattler_cmd *cmd, const char *name)
{
    rattler_flag *flag = rattler_lookup_flag(cmd, name);
    if (flag != NULL) {
        flag->required = true;
    } else {
        fprintf(stderr, "error: rattler_mark_required: unknown flag --%s\n",
            name);
    }
}

void
rattler_mark_flags_mutually_exclusive(rattler_cmd *cmd, ...)
{
    va_list ap;
    va_start(ap, cmd);
    append_flag_group(cmd, make_flag_group(FGROUP_MUTUALLY_EXCLUSIVE, ap));
    va_end(ap);
}

void
rattler_mark_flags_required_together(rattler_cmd *cmd, ...)
{
    va_list ap;
    va_start(ap, cmd);
    append_flag_group(cmd, make_flag_group(FGROUP_REQUIRED_TOGETHER, ap));
    va_end(ap);
}

void
rattler_mark_flags_one_required(rattler_cmd *cmd, ...)
{
    va_list ap;
    va_start(ap, cmd);
    append_flag_group(cmd, make_flag_group(FGROUP_ONE_REQUIRED, ap));
    va_end(ap);
}

rattler_flag*
rattler_lookup_flag(rattler_cmd *cmd, const char *name)
{
    for (rattler_flag *f = cmd->flags; f; f = f->next) {
        if (strcmp(f->name, name) == 0) {
            return f;
        }
    }

    for (rattler_flag *f = cmd->persistent_flags; f; f = f->next) {
        if (strcmp(f->name, name) == 0) {
            return f;
        }
    }

    for (rattler_cmd *c = cmd->parent; c; c = c->parent) {
        for (rattler_flag *f = c->persistent_flags; f; f = f->next) {
            if (strcmp(f->name, name) == 0) {
                return f;
            }
        }
    }

    return NULL;
}

rattler_flag*
rattler_lookup_flag_short(rattler_cmd *cmd, char sh)
{
    for (rattler_flag *flag = cmd->flags; flag; flag = flag->next) {
        if (flag->shorthand == sh) {
            return flag;
        }
    }

    for (rattler_flag *flag = cmd->persistent_flags; flag; flag = flag->next) {
        if (flag->shorthand == sh) {
            return flag;
        }
    }

    for (rattler_cmd *c = cmd->parent; c; c = c->parent) {
        for (rattler_flag *flag = c->persistent_flags; flag; flag = flag->next) {
            if (flag->shorthand == sh) {
                return flag;
            }
        }
    }

    return NULL;
}

bool
rattler_flag_changed(rattler_cmd *cmd, const char *name)
{
    rattler_flag *flag = rattler_lookup_flag(cmd, name);
    return flag && flag->changed;
}

const char*
rattler_flag_string(rattler_cmd *cmd, const char *name)
{
    rattler_flag *flag = rattler_lookup_flag(cmd, name);
    return (flag && flag->type == FLAG_STRING) ?
        (flag->value_str ? flag->value_str : "") : "";
}

int
rattler_flag_int(rattler_cmd *cmd, const char *name)
{
    rattler_flag *flag = rattler_lookup_flag(cmd, name);
    return (flag && flag->type == FLAG_INT) ? flag->value.i : 0;
}

double
rattler_flag_float(rattler_cmd *cmd, const char *name)
{
    rattler_flag *flag = rattler_lookup_flag(cmd, name);
    return (flag && flag->type == FLAG_FLOAT) ? flag->value.f : 0.0;
}

bool
rattler_flag_bool(rattler_cmd *cmd, const char *name)
{
    rattler_flag *flag = rattler_lookup_flag(cmd, name);
    return (flag && flag->type == FLAG_BOOL) ? flag->value.b : false;
}

static void
print_flags_section(const char *title, rattler_flag *list)
{
    if (title == NULL || *title == '\0') {
        return;
    }

    if (list == NULL) {
        return;
    }

    printf("\n%s:\n", title);

    for (rattler_flag *flag = list; flag; flag = flag->next) {
        char sb[8];

        if (flag->shorthand) {
            snprintf(sb, sizeof sb, "-%c,", flag->shorthand);
        } else {
            snprintf(sb, sizeof sb, "   ");
        }

        const char *req = flag->required ? " (REQUIRED)" : "";
        switch (flag->type) {
        case FLAG_BOOL:
            printf("  %s --%s%s\n\t\t%s (default %s)\n",
                sb, flag->name, req, flag->usage, flag->def_val.b ? "true" : "false");
                break;
        case FLAG_STRING:
            printf("  %s --%s string%s\n\t\t%s (default \"%s\")\n",
                sb, flag->name, req, flag->usage, flag->defval_str ? flag->defval_str : "");
                break;
        case FLAG_INT:
            printf("  %s --%s int%s\n\t\t%s (default %d)\n",
                sb, flag->name, req, flag->usage, flag->def_val.i);
                break;
        case FLAG_FLOAT:
            printf("  %s --%s float%s\n\t\t%s (default %g)\n",
                sb, flag->name, req, flag->usage, flag->def_val.f);
                break;
        }
    }
}

void
rattler_print_usage(rattler_cmd *cmd) {
    char path[512] = {0};

    build_path(cmd, path, sizeof path);
    printf("Usage:\n  %s\n", cmd->use ? cmd->use : path);

    if (cmd->num_aliases > 0) {
        char *n = cmd_name(cmd);

        printf("\nAliases:\n  %s", n); free(n);

        for (int i = 0; i < cmd->num_aliases; i++) {
            printf(", %s", cmd->aliases[i]);
        }
        printf("\n");
    }

    bool any = false;

    for (int i = 0; i < cmd->num_children; i++) {
        if (!cmd->children[i]->hidden) {
            any = true;
            break;
        }
    }

    if (any) {
        printf("\nAvailable Commands:\n");

        for (int i = 0; i < cmd->num_children; i++) {
            rattler_cmd *ch = cmd->children[i];

            if (ch->hidden) {
                continue;
            }

            char *n = cmd_name(ch);
            if (ch->deprecated != NULL) {
                printf("  %-16s %s (DEPRECATED: %s)\n", n,
                    ch->short_desc ? ch->short_desc : "", ch->deprecated);
            } else {
                printf("  %-16s %s\n", n, ch->short_desc ? ch->short_desc : "");
            }
            free(n);
        }
    }

    print_flags_section("Flags", cmd->flags);
    print_flags_section("Global Flags", cmd->persistent_flags);

    for (rattler_flag_group *g = cmd->flag_groups; g; g = g->next) {
        printf("\n");

        switch (g->kind) {
        case FGROUP_MUTUALLY_EXCLUSIVE:
            printf("  [mutually exclusive]: ");
            break;
        case FGROUP_REQUIRED_TOGETHER:
            printf("  [required together]:  ");
            break;
        case FGROUP_ONE_REQUIRED:
            printf("  [one required]:       ");
            break;
        }
        for (int i = 0; i < g->count; i++) {
            printf("%s--%s", i ? ", " : "", g->names[i]);
        }
        printf("\n");
    }

    printf("\nUse \"%s [command] --help\" for more information.\n", path);
}

void
rattler_print_help(rattler_cmd *cmd)
{
    if (cmd->deprecated) {
        fprintf(stderr, "Command \"%s\" is deprecated: %s\n",
            cmd->use ? cmd->use : "", cmd->deprecated);
    }

    if (cmd->long_desc && *cmd->long_desc) {
        printf("%s\n", cmd->long_desc);
    } else if (cmd->short_desc && *cmd->short_desc) {
        printf("%s\n", cmd->short_desc);
    }

    if (cmd->example && *cmd->example) {
        printf("\nExamples:\n%s\n", cmd->example);
    }

    rattler_print_usage(cmd);
}

static int
levenshtein(const char *a, const char *b)
{
    int la = (int)strlen(a);
    int lb = (int)strlen(b);

    int *row = calloc((size_t)(lb+1), sizeof(int));
    if (row == NULL) {
        return la + lb + 1;;
    }

    for (int j = 0; j <= lb; j++) {
        row[j] = j;
    }

    for (int i = 1; i <= la; i++) {
        int prev = i;

        for (int j = 1; j <= lb; j++) {
            int cost = (a[i-1] == b[j-1]) ? 0 : 1;
            int cur = row[j-1] + cost;

            if (row[j] + 1 < cur) {
                cur = row[j] + 1;
            }
            if (prev + 1 < cur) {
                cur = prev + 1;
            }
            row[j-1] = prev; prev = cur;
        }
        row[lb] = prev;
    }
    int r = row[lb];
    free(row);
    
    return r;
}

static void
suggest_command(rattler_cmd *parent, const char *typo)
{
    int best = 3;
    char *bname = NULL;

    for (int i = 0; i<parent->num_children; i++) {
        if (parent->children[i]->hidden) {
            continue;
        }

        char *n = cmd_name(parent->children[i]);
        int d = levenshtein(typo, n);
        if (d < best) {
            free(bname);
            best = d;
            bname = n;
        } else {
            free(n);
        }

        for (int j = 0; j < parent->children[i]->num_aliases; j++) {
            d = levenshtein(typo, parent->children[i]->aliases[j]);

            if (d < best) {
                free(bname);
                best = d;

                char *alias = _strdup(parent->children[i]->aliases[j]);
                if (alias == NULL) {
                    return;
                }
                bname = alias;
            }
        }
    }

    if (bname != NULL) {
        fprintf(stderr, "\nDid you mean this?\n\t%s\n", bname);
        free(bname);
    }
}

static int
set_flag_value(rattler_flag *flag, const char *val)
{
    char *end;
    long lval;
    double dval;

    switch (flag->type) {
    case FLAG_BOOL:
        if (val == NULL || strcmp(val, "true") == 0 || strcmp(val, "1") == 0
                || strcmp(val,"yes") == 0) {
            flag->value.b = true;
        } else if (strcmp(val, "false") == 0 || strcmp(val, "0") == 0
                || strcmp(val, "no") == 0) {
            flag->value.b = false;
        } else {
            fprintf(stderr,"error: invalid bool '%s'\n", val);
            return -1;
        }

        break;
    case FLAG_STRING:
        free(flag->value_str);

        char *s = _strdup(val);
        if (s == NULL) {
            return -1;
        }
        flag->value_str = s;

        break;
    case FLAG_INT:
        lval = strtol(val, &end, 10);
        if (*end != '\0') {
            fprintf(stderr, "error: invalid int '%s'\n", val);
            return -1;
        }
        flag->value.i = (int)lval;

        break;
    case FLAG_FLOAT:
        dval = strtod(val, &end);
        if (*end != '\0') {
            fprintf(stderr, "error: invalid float '%s'\n", val);
            return -1;
        }
        flag->value.f = dval;

        break;
    }

    flag->changed = true;

    return 0;
}

static int
parse_flags(rattler_cmd *cmd, int argc, char **argv,
            char **pos, int pos_cap)
{
    int pc = 0;

    for (int i = 0; i < argc;) {
        char *arg = argv[i];
        if (strcmp(arg, "--") == 0) {
            i++;
            while (i < argc && pc < pos_cap) {
                pos[pc++] = argv[i++];
            }
            break;
        }

        if (strncmp(arg, "--", 2) == 0) {
            char *name = arg + 2;
            char namebuf[256];
            const char *val = NULL;
            char *eq = strchr(name, '=');

            if (eq) {
                size_t len = (size_t)(eq-name);
                if (len >= sizeof namebuf) {
                    len = sizeof namebuf - 1;
                }

                strncpy(namebuf, name, len);
                namebuf[len] = '\0';
                name = namebuf;
                val = eq + 1;
            }

            rattler_flag *flag = rattler_lookup_flag(cmd, name);
            if (flag == NULL) {
                fprintf(stderr,"error: unknown flag: --%s\n", name);
                return -1;
            }

            if (flag->type == FLAG_BOOL) {
                if (set_flag_value(flag, val ? val : "true") < 0) {
                    return -1;
                }
            } else {
                if (!val) {
                    if (i+1 >= argc) {
                        fprintf(stderr,"error: --%s needs a value\n", name);
                        return -1;
                    }
                    val=argv[++i];
                }
                if (set_flag_value(flag, val) < 0) {
                    return -1;
                }
            }
            i++;
            continue;
        }
        if (arg[0] == '-' && arg[1] && arg[1] != '-') {
            int j = 1;

            while (arg[j]) {
                char sh = arg[j];
                rattler_flag *flag = rattler_lookup_flag_short(cmd, sh);
                if (flag == NULL) {
                    fprintf(stderr,"error: unknown flag: -%c\n", sh);
                    return -1;
                }

                if (flag->type == FLAG_BOOL) {
                    set_flag_value(flag, "true");
                    j++;
                } else {
                    const char *val = arg[j + 1] ? &arg[j + 1] :
                        (i + 1 < argc ? argv[++i]: NULL);
                    if (val == NULL) {
                        fprintf(stderr,"error: -%c needs a value\n", sh);
                        return -1;
                    }
                    if (set_flag_value(flag, val) < 0) {
                        return -1;
                    }
                    break;
                }
            }
            i++;
            continue;
        }
        if (pc<pos_cap) {
            pos[pc++]=arg;
        }
        i++;
    }

    return pc;
}

static int
validate_flag_groups(rattler_cmd *cmd)
{
    int errors = 0;

    for (rattler_flag_group *group = cmd->flag_groups; group;
        group = group->next) {
        int set_count = 0;

        for (int i = 0; i < group->count; i++) {
            rattler_flag *f = rattler_lookup_flag(cmd, group->names[i]);
            if (f && f->changed) {
                set_count++;
            }
        }

        switch (group->kind) {
        case FGROUP_MUTUALLY_EXCLUSIVE:
            if (set_count > 1) {
                fprintf(stderr,"error: if any flags in the group [");

                for (int i = 0; i < group->count; i++) {
                    fprintf(stderr,"%s--%s", i ? " " : "", group->names[i]);
                }
                fprintf(stderr,"] are set none of the others can be; %d were set\n",
                    set_count);
                errors++;
            }
            break;
        case FGROUP_REQUIRED_TOGETHER:
            if (set_count > 0 && set_count < group->count) {
                fprintf(stderr,"error: if any flags in the group [");

                for (int i = 0; i < group->count; i++) {
                    fprintf(stderr,"%s--%s", i ? " " : "", group->names[i]);
                }
                fprintf(stderr,"] are set they must all be set; missing:");

                for (int i = 0; i < group->count; i++) {
                    rattler_flag *f = rattler_lookup_flag(cmd, group->names[i]);

                    if (f && !f->changed) {
                        fprintf(stderr," --%s", group->names[i]);
                    }
                }
                fprintf(stderr,"\n");
                errors++;
            }
            break;
        case FGROUP_ONE_REQUIRED:
            if (set_count==0) {
                fprintf(stderr,"error: at least one of the flags in the group [");

                for (int i = 0; i < group->count; i++) {
                    fprintf(stderr,"%s--%s", i ? " " : "", group->names[i]);
                }
                fprintf(stderr,"] is required\n");
                errors++;
            }
            break;
        }
    }

    return errors;
}

static int
cmd_command(rattler_cmd *cmd, int argc, char **argv)
{
    if (cmd->deprecated) {
        fprintf(stderr,"Command \"%s\" is deprecated: %s\n",
            cmd->use ? cmd->use : "", cmd->deprecated);
    }

    if (!cmd->help_flag_added) {
        rattler_flags_bool(cmd, "help", 'h', false, "help for this command");
        cmd->help_flag_added = true;
    }
    if (cmd->version && !cmd->version_flag_added) {
        rattler_flags_bool(cmd, "version", 'V', false,
            "version for this command");
        cmd->version_flag_added = true;
    }

     // scan argv for the first non-flag token and check if it matches a child
     // command name or alias.  If so, hand off the REST of argv (everything
     // after that token) directly to the child – preserving all flags that
     // follow the sub-command name.
    if (cmd->num_children > 0) {
        for (int i = 0; i < argc; i++) {
            // skip flag tokens so "myapp --verbose serve" still works
            if (argv[i][0] == '-') {
                // skip value of non-bool flags: --port 8080
                if (strncmp(argv[i], "--", 2) == 0 && !strchr(argv[i],'=')) {
                    // peek: if next token exists and isn't a flag, skip it too
                    if (i+1 < argc && argv[i+1][0] != '-') {
                        i++;
                    }
                }
                continue;
            }

            // argv[i] is a candidate sub-command token
            const char *tok = argv[i];

            for (int ci = 0; ci < cmd->num_children; ci++) {
                rattler_cmd *ch = cmd->children[ci];

                char *n = cmd_name(ch);
                bool match = strcmp(n, tok) == 0;
                free(n);

                if (!match) {
                    for (int ai = 0; ai < ch->num_aliases; ai++) {
                        if (strcmp(ch->aliases[ai], tok) == 0) {
                            match = true;
                            break;
                        }
                    }
                }
                if (match) {
                    // pass everything after the sub-command token
                    return cmd_command(ch, argc-i-1, argv+i+1);
                }
            }

            // first non-flag token didn't match any child, fall through
            break;
        }
    }

    // parse flags for this command
    char *pos[256];
    int pc = 0;

    if (!cmd->disable_flag_parsing) {
        pc = parse_flags(cmd, argc, argv, pos, 256);

        if (pc < 0) {
            if (!cmd->silence_usage) {
                rattler_print_usage(cmd);
            }
            return 1;
        }
    } else {
        for (int i = 0;i < argc && i < 256;i++) {
            pos[pc++] = argv[i];
        }
    }

    // help
    rattler_flag *hf = rattler_lookup_flag(cmd, "help");
    if (hf && hf->value.b) {
        rattler_print_help(cmd);
        return 0;
    }

    // version
    if (cmd->version) {
        rattler_flag *vf = rattler_lookup_flag(cmd, "version");

        if (vf && vf->value.b) {
            char *n = cmd_name(cmd);
            printf("%s version %s\n", n, cmd->version);
            free(n);

            return 0;
        }
    }

    // unknown positional that looks like a sub-command
    if (pc > 0 && cmd->num_children > 0) {
        fprintf(stderr,"error: unknown command \"%s\"\n",pos[0]);
 
        suggest_command(cmd,pos[0]);
        if (!cmd->silence_usage) {
            rattler_print_usage(cmd);
        }

        return 1;
    }

    // individual required flags
    for (rattler_flag *f = cmd->flags; f; f = f->next) {
        if (f->required && !f->changed) {
            fprintf(stderr,"error: required flag --%s not set\n",f->name);

            if (!cmd->silence_usage) {
                rattler_print_usage(cmd);
            }

            return 1;
        }
    }

    // flag group validation
    if (validate_flag_groups(cmd)>0) {
        if (!cmd->silence_usage) {
            rattler_print_usage(cmd);
        }

        return 1;
    }

    // arg count
    if (cmd->min_args > 0 && pc < cmd->min_args) {
        fprintf(stderr,"error: need at least %d arg(s), got %d\n",
            cmd->min_args,pc);
        return 1;
    }
    if (cmd->max_args >= 0 && pc > cmd->max_args) {
        fprintf(stderr,"error: need at most %d arg(s), got %d\n",
            cmd->max_args,pc);
        return 1;
    }

    if (!cmd->cmd) {
        rattler_print_help(cmd);
        return 0;
    }

    // lifecycle hooks
    {
        rattler_cmd *ch[64];
        int d = 0;

        for (rattler_cmd *c = cmd; c && d < 64; c = c->parent) {
            if (c->persistent_pre_cmd) {
                ch[d++] = c;
            }
        }
        for (int i = d-1; i >= 0; i--) {
            ch[i]->persistent_pre_cmd(cmd, pc, pos);
        }
    }

    if (cmd->pre_cmd) {
        cmd->pre_cmd(cmd, pc, pos);
    }
    if (cmd->cmd) {
        cmd->cmd(cmd, pc, pos);
    }
    if (cmd->post_cmd) {
        cmd->post_cmd(cmd, pc, pos);
    }

    {
        rattler_cmd *ch[64]; int d = 0;

        for (rattler_cmd *c = cmd; c && d < 64; c = c->parent) {
            if (c->persistent_post_cmd) {
                ch[d++] = c;
            }
        }
        for (int i = 0; i < d; i++) {
            ch[i]->persistent_post_cmd(cmd, pc, pos);
        }
    }

    return 0;
}

int
rattler_execute(rattler_cmd *root, int argc, char **argv)
{
    return cmd_command(root, argc - 1, argv + 1);
}

int
rattler_execute_c(rattler_cmd *root, int argc, char **argv)
{
    return cmd_command(root, argc, argv);
}
