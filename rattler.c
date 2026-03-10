#include <stdarg.h>

#include "rattler.h"

static char*
xstrdup(const char *s)
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
        return xstrdup("(unknown)");
    }

    char *c = xstrdup(cmd->use);
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
    rattler_flag *f = calloc(1, sizeof *f);
    if (f == NULL) {
        
        return NULL;
    }

    f->name = xstrdup(name);
    if (f->name == NULL) {
        free(f);
        return NULL;
    }

    f->shorthand = sh;
    f->usage = xstrdup(usage);
    if (f->usage == NULL) {
        free(f->name);
        free(f);
        return NULL;
    }

    f->type = type;
    f->persistent = persistent;

    return f;
}

static void
append_flag(rattler_flag **list, rattler_flag *f)
{
    if (!*list) {
        *list = f;
        return;
    }

    rattler_flag *c = *list;

    while (c->next) {
        c = c->next;
    }
    c->next = f;
}

static void
free_flag_list(rattler_flag *f)
{
    while (f != NULL) {
        rattler_flag *nx = f->next;

        free(f->name);
        free(f->usage);

        if (f->type == FLAG_STRING) {
            free(f->value_str);
            free(f->defval_str);
        }

        free(f); f = nx;
    }
}

static void
free_flag_groups(rattler_flag_group *g)
{
    while (g) {
        rattler_flag_group *nx = g->next;

        for (int i = 0; i < g->count; i++) {
            free(g->names[i]);
        }

        free(g->names);
        free(g);

        g = nx;
    }
}

static rattler_flag_group*
make_flag_group(flag_group_kind kind, va_list ap)
{
    rattler_flag_group *g = calloc(1, sizeof *g);
    if (g == NULL) {
        
        exit(1);
    }

    g->kind = kind;
    int cap = 8;

    g->names = malloc((size_t)cap * sizeof(char *));
    if (g->names == NULL) {
        return NULL;
    }

    const char *name;
    while ((name = va_arg(ap, const char *)) != NULL) {
        if (g->count >= cap) {
            cap *= 2;

            g->names = realloc(g->names, (size_t)cap * sizeof(char *));
            if (!g->names) {
                return NULL;
            }
        }

        char *n = xstrdup(name);
        if (n == NULL) {
            return NULL;
        }
        g->names[g->count++] = n;
    }

    return g;
}

static void
append_flag_group(rattler_cmd *cmd, rattler_flag_group *g)
{
    if (cmd->flag_groups == NULL) {
        cmd->flag_groups = g;
        return;
    }

    rattler_flag_group *c = cmd->flag_groups;

    while (c->next) {
        c = c->next;
    }
    c->next = g;
}

rattler_cmd*
rattler_new_command(const char *use, const char *sd, const char *ld)
{
    rattler_cmd *cmd = calloc(1, sizeof *cmd);
    if (cmd == NULL) {
        
        exit(1);
    }

    char *u = xstrdup(use);
    if (u == NULL) {
        free(cmd);
        return NULL;
    }
    cmd->use = u;

    char *s = xstrdup(sd);
    if (s == NULL) {
        free(cmd->use);
        free(cmd);
        return NULL;
    }
    cmd->short_desc = s;

    char *l = xstrdup(ld);
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
        parent->cap_children = parent->cap_children ? parent->cap_children*2 : 4;
        parent->children = realloc(parent->children,
            (size_t)parent->cap_children * sizeof(rattler_cmd*));

        if (parent->children == NULL) {
            return;
        }
    }
    parent->children[parent->num_children++] = child;
}

void
rattler_set_version(rattler_cmd *cmd, const char *v)
{
    free(cmd->version);
    char *ver = xstrdup(v);
    if (ver == NULL) {
        return;
    }
    cmd->version = ver;
}

void
rattler_set_args(rattler_cmd *cmd, int mn, int mx)
{
    cmd->min_args = mn;
    cmd->max_args = mx;
}

void
rattler_add_alias(rattler_cmd *cmd, const char *alias)
{
    cmd->aliases = realloc(cmd->aliases, (size_t)(cmd->num_aliases + 1) * sizeof(char*));
    if (cmd->aliases == NULL) {
        return;
    }

    char *a = xstrdup(alias);
    if (a == NULL) {
        return;
    }
    cmd->aliases[cmd->num_aliases++] = a;
}

void
rattler_flags_bool(rattler_cmd *cmd, const char *name, char sh, bool def,
                   const char *usage)
{
    rattler_flag *f = new_flag(name, sh, usage, FLAG_BOOL, false);
    f->value.b = def;
    f->def_val.b = def;

    append_flag(&cmd->flags, f);
}

void
rattler_flags_string(rattler_cmd *cmd, const char *name, char sh,
                     const char *def, const char *usage)
{
    rattler_flag *f = new_flag(name, sh, usage, FLAG_STRING, false);
    char *d1 = xstrdup(def);
    if (d1 == NULL) {
        return;
    }
    f->value_str = d1;

    char *d2 = xstrdup(def);
    if (d2 == NULL) {
        free(f->value_str);
        return;
    }
    f->defval_str = d2;

    append_flag(&cmd->flags, f);
}

void
rattler_flags_int(rattler_cmd *cmd, const char *name, char sh, int def,
                  const char *usage)
{
    rattler_flag *f = new_flag(name, sh, usage, FLAG_INT, false);
    f->value.i = def;
    f->def_val.i = def;

    append_flag(&cmd->flags, f);
}

void
rattler_flags_float(rattler_cmd *cmd, const char *name, char sh, double def,
                    const char *usage)
{
    rattler_flag *f = new_flag(name, sh, usage, FLAG_FLOAT, false);
    f->value.f = def;
    f->def_val.f = def;
    
    append_flag(&cmd->flags, f);
}

void
rattler_persistent_bool(rattler_cmd *cmd, const char *name, char sh, bool def,
                        const char *usage)
{
    rattler_flag *f = new_flag(name, sh, usage, FLAG_BOOL, true);
    f->value.b = def;
    f->def_val.b = def;

    append_flag(&cmd->persistent_flags, f);
}

void
rattler_persistent_string(rattler_cmd *cmd, const char *name, char sh,
                          const char *def, const char *usage)
{
    rattler_flag *f = new_flag(name, sh, usage, FLAG_STRING, true);

    char *d1 = xstrdup(def);
    if (d1 == NULL) {
        return;
    }
    f->value_str = d1;

    char *d2 = xstrdup(def);
    if (d2 == NULL) {
        free(f->value_str);
        return;
    }
    f->defval_str = d2;

    append_flag(&cmd->persistent_flags, f);
}

void
rattler_persistent_int(rattler_cmd *cmd, const char *name, char sh, int def,
                       const char *usage)
{
    rattler_flag *f = new_flag(name, sh, usage, FLAG_INT, true);

    f->value.i = def;
    f->def_val.i = def;

    append_flag(&cmd->persistent_flags, f);
}
void
rattler_persistent_float(rattler_cmd *cmd, const char *name, char sh,
                         double def, const char *usage)
{
    rattler_flag *f = new_flag(name, sh, usage, FLAG_FLOAT, true);

    f->value.f = def;
    f->def_val.f = def;

    append_flag(&cmd->persistent_flags, f);
}

void
rattler_mark_required(rattler_cmd *cmd, const char *name)
{
    rattler_flag *f = rattler_lookup_flag(cmd, name);
    if (f != NULL) {
        f->required = true;
    } else {
        fprintf(stderr, "rattler: rattler_mark_required: unknown flag --%s\n",
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
    for (rattler_flag *f = cmd->flags; f; f = f->next) {
        if (f->shorthand == sh) {
            return f;
        }
    }
    for (rattler_flag *f = cmd->persistent_flags; f; f = f->next) {
        if (f->shorthand == sh) {
            return f;
        }
    }
    for (rattler_cmd *c = cmd->parent; c; c = c->parent) {
        for (rattler_flag *f = c->persistent_flags; f; f = f->next) {
            if (f->shorthand == sh) {
                return f;
            }
        }
    }

    return NULL;
}

bool
rattler_flag_changed(rattler_cmd *cmd, const char *name)
{
    rattler_flag *f = rattler_lookup_flag(cmd, name);
    return f && f->changed;
}

const char*
rattler_flag_string(rattler_cmd *cmd, const char *name)
{
    rattler_flag *f = rattler_lookup_flag(cmd, name);
    return (f && f->type == FLAG_STRING) ?
        (f->value_str ? f->value_str : "") : "";
}

int
rattler_flag_int(rattler_cmd *cmd, const char *name)
{
    rattler_flag *f = rattler_lookup_flag(cmd, name);
    return (f && f->type == FLAG_INT) ? f->value.i : 0;
}

double
rattler_flag_float(rattler_cmd *cmd, const char *name)
{
    rattler_flag *f = rattler_lookup_flag(cmd, name);
    return (f && f->type == FLAG_FLOAT) ? f->value.f : 0.0;
}

bool
rattler_flag_bool(rattler_cmd *cmd, const char *name)
{
    rattler_flag *f = rattler_lookup_flag(cmd, name);
    return (f && f->type == FLAG_BOOL) ? f->value.b : false;
}

static void
print_flags_section(const char *title, rattler_flag *list)
{
    if (list == NULL) {
        return;
    }

    printf("\n%s:\n", title);

    for (rattler_flag *f = list; f; f = f->next) {
        char sb[8];

        if (f->shorthand) {
            snprintf(sb, sizeof sb, "-%c,", f->shorthand);
        } else {
            snprintf(sb, sizeof sb, "   ");
        }

        const char *req = f->required ? " (REQUIRED)" : "";
        switch (f->type) {
        case FLAG_BOOL:
            printf("  %s --%s%s\n\t\t%s (default %s)\n",
                sb, f->name, req, f->usage, f->def_val.b ? "true" : "false");
                break;
        case FLAG_STRING:
            printf("  %s --%s string%s\n\t\t%s (default \"%s\")\n",
                sb, f->name, req, f->usage, f->defval_str ? f->defval_str : "");
                break;
        case FLAG_INT:
            printf("  %s --%s int%s\n\t\t%s (default %d)\n",
                sb, f->name, req, f->usage, f->def_val.i);
                break;
        case FLAG_FLOAT:
            printf("  %s --%s float%s\n\t\t%s (default %g)\n",
                sb, f->name, req, f->usage, f->def_val.f);
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

                char *alias = xstrdup(parent->children[i]->aliases[j]);
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
set_flag_value(rattler_flag *f, const char *val)
{
    f->changed = true;

    switch (f->type) {
    case FLAG_BOOL:
        if (!val||strcmp(val,"true") == 0 || strcmp(val,"1") ==0|| strcmp(val,"yes") == 0) {
            f->value.b = true;
        } else if (strcmp(val,"false") == 0 || strcmp(val,"0") == 0 || strcmp(val,"no") == 0) {
            f->value.b = false;
        } else {
            fprintf(stderr,"rattler: invalid bool '%s'\n",val); return -1;
        }
        break;
    case FLAG_STRING:
        free(f->value_str);
        char *s = xstrdup(val);
        if (s == NULL) {
            return -1;
        }
        f->value_str = s;
        break;
    case FLAG_INT:
        f->value.i = atoi(val);
        break;
    case FLAG_FLOAT:
        f->value.f = atof(val);
        break;
    }

    return 0;
}

static int
parse_flags(rattler_cmd *cmd, int argc, char **argv,
            char **pos, int pos_cap)
{
    int pc=0, i=0;
    while (i < argc) {
        char *arg = argv[i];
        if (strcmp(arg,"--")==0) {
            i++;
            while (i<argc && pc<pos_cap) pos[pc++]=argv[i++];
            break;
        }
        if (strncmp(arg,"--",2)==0) {
            char *name=arg+2, namebuf[256]; const char *val=NULL;
            char *eq=strchr(name,'=');
            if (eq) {
                size_t len=(size_t)(eq-name);
                if (len>=sizeof namebuf) len=sizeof namebuf-1;
                strncpy(namebuf,name,len); namebuf[len]='\0';
                name=namebuf; val=eq+1;
            }
            rattler_flag *f=rattler_lookup_flag(cmd,name);
            if (!f) { fprintf(stderr,"rattler: unknown flag: --%s\n",name); return -1; }
            if (f->type==FLAG_BOOL) { set_flag_value(f,val?val:"true"); }
            else {
                if (!val) {
                    if (i+1>=argc) { fprintf(stderr,"rattler: --%s needs a value\n",name); return -1; }
                    val=argv[++i];
                }
                if (set_flag_value(f,val)<0) return -1;
            }
            i++; continue;
        }
        if (arg[0]=='-' && arg[1] && arg[1]!='-') {
            int j=1;
            while (arg[j]) {
                char sh=arg[j];
                rattler_flag *f=rattler_lookup_flag_short(cmd,sh);
                if (!f) { fprintf(stderr,"rattler: unknown flag: -%c\n",sh); return -1; }
                if (f->type==FLAG_BOOL) { set_flag_value(f,"true"); j++; }
                else {
                    const char *val=arg[j+1]?&arg[j+1]:(i+1<argc?argv[++i]:NULL);
                    if (!val) { fprintf(stderr,"rattler: -%c needs a value\n",sh); return -1; }
                    if (set_flag_value(f,val)<0) return -1;
                    break;
                }
            }
            i++; continue;
        }
        if (pc<pos_cap) pos[pc++]=arg;
        i++;
    }
    return pc;
}

static int
validate_flag_groups(rattler_cmd *cmd)
{
    int errors = 0;

    for (rattler_flag_group *g=cmd->flag_groups; g; g=g->next) {
        int set_count = 0;

        for (int i=0; i<g->count; i++) {
            rattler_flag *f=rattler_lookup_flag(cmd,g->names[i]);
            if (f && f->changed) {
                set_count++;
            }
        }

        switch (g->kind) {
        case FGROUP_MUTUALLY_EXCLUSIVE:
            if (set_count > 1) {
                fprintf(stderr,"error: if any flags in the group [");

                for (int i = 0; i < g->count; i++) {
                    fprintf(stderr,"%s--%s", i?" ":"",g->names[i]);
                }
                fprintf(stderr,"] are set none of the others can be; %d were set\n",set_count);
                errors++;
            }
            break;
        case FGROUP_REQUIRED_TOGETHER:
            if (set_count > 0 && set_count < g->count) {
                fprintf(stderr,"error: if any flags in the group [");

                for (int i = 0; i < g->count; i++) {
                    fprintf(stderr,"%s--%s", i?" ":"",g->names[i]);
                }
                fprintf(stderr,"] are set they must all be set; missing:");

                for (int i = 0; i < g->count; i++) {
                    rattler_flag *f = rattler_lookup_flag(cmd,g->names[i]);

                    if (f && !f->changed) {
                        fprintf(stderr," --%s",g->names[i]);
                    }
                }
                fprintf(stderr,"\n"); errors++;
            }
            break;
        case FGROUP_ONE_REQUIRED:
            if (set_count==0) {
                fprintf(stderr,"error: at least one of the flags in the group [");

                for (int i = 0; i < g->count; i++) {
                    fprintf(stderr,"%s--%s", i?" ":"",g->names[i]);
                }
                fprintf(stderr,"] is required\n"); errors++;
            }
            break;
        }
    }
    return errors;
}

static int
run_command(rattler_cmd *cmd, int argc, char **argv)
{
    if (cmd->deprecated) {
        fprintf(stderr,"Command \"%s\" is deprecated: %s\n",
            cmd->use?cmd->use:"", cmd->deprecated);
    }

    if (!cmd->help_flag_added) {
        rattler_flags_bool(cmd, "help", 'h', false, "help for this command");
        cmd->help_flag_added = true;
    }
    if (cmd->version && !cmd->version_flag_added) {
        rattler_flags_bool(cmd, "version", 'V', false, "version for this command");
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
                if (strncmp(argv[i],"--",2)==0 && !strchr(argv[i],'=')) {
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
                        if (strcmp(ch->aliases[ai], tok) == 0) { match=true; break; }
                    }
                }
                if (match) {
                    // pass everything AFTER the sub-command token
                    return run_command(ch, argc-i-1, argv+i+1);
                }
            }

            // first non-flag token didn't match any child, fall through
            break;
        }
    }

    // parse flags for THIS command
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
            pos[pc++]=argv[i];
        }
    }

    // help
    rattler_flag *hf=rattler_lookup_flag(cmd,"help");
    if (hf && hf->value.b) {
        rattler_print_help(cmd);
        return 0;
    }

    // version
    if (cmd->version) {
        rattler_flag *vf=rattler_lookup_flag(cmd,"version");
        if (vf && vf->value.b) {
            char *n=cmd_name(cmd);
            printf("%s version %s\n",n,cmd->version);
            free(n);

            return 0;
        }
    }

    // unknown positional that looks like a sub-command
    if (pc > 0 && cmd->num_children > 0) {
        fprintf(stderr,"rattler: unknown command \"%s\"\n",pos[0]);
        suggest_command(cmd,pos[0]);
        if (!cmd->silence_usage) {
            rattler_print_usage(cmd);
        }

        return 1;
    }

    // individual required flags
    for (rattler_flag *f=cmd->flags; f; f=f->next) {
        if (f->required && !f->changed) {
            fprintf(stderr,"rattler: required flag --%s not set\n",f->name);

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
    if (cmd->min_args > 0 && pc<cmd->min_args) {
        fprintf(stderr,"rattler: need at least %d arg(s), got %d\n",cmd->min_args,pc);
        return 1;
    }
    if (cmd->max_args>=0 && pc>cmd->max_args) {
        fprintf(stderr,"rattler: need at most %d arg(s), got %d\n",cmd->max_args,pc);
        return 1;
    }

    if (!cmd->run) {
        rattler_print_help(cmd);
        return 0;
    }

    // lifecycle hooks
    {
        rattler_cmd *ch[64]; int d = 0;

        for (rattler_cmd *c = cmd; c && d < 64; c = c->parent) {
            if (c->persistent_pre_run) {
                ch[d++]=c;
            }
        }
        for (int i = d-1; i >= 0; i--) {
            ch[i]->persistent_pre_run(cmd, pc, pos);
        }
    }

    if (cmd->pre_run) {
        cmd->pre_run(cmd, pc, pos);
    }
    if (cmd->run) {
        cmd->run(cmd, pc, pos);
    }
    if (cmd->post_run) {
        cmd->post_run(cmd, pc, pos);
    }

    {
        rattler_cmd *ch[64]; int d = 0;

        for (rattler_cmd *c = cmd; c && d < 64; c = c->parent) {
            if (c->persistent_post_run) {
                ch[d++]=c;
            }
        }
        for (int i = 0; i < d; i++) {
            ch[i]->persistent_post_run(cmd, pc, pos);
        }
    }

    return 0;
}

int
rattler_execute(rattler_cmd *root, int argc, char **argv)
{
    return run_command(root, argc-1, argv+1);
}

int
rattler_execute_c(rattler_cmd *root, int argc, char **argv)
{
    return run_command(root, argc, argv);
}
