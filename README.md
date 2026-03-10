# libgithub

libratter brings the elegant command-tree model of Go's cobra framework to C.

Build rich, nested CLIs with flags, aliases, lifecycle hooks, and constraint validation — all with **zero external dependencies**, inspired by [spf13/cobra](https://github.com/spf13/cobra).

[![Build and Deploy](https://github.com/briandowns/libgithub/actions/workflows/main.yml/badge.svg)](https://github.com/briandowns/librattler/actions/workflows/main.yml/badge.svg)

## Features

| Feature 
|---|
| Nested sub-commands (unlimited depth) |
| Command aliases |
| Local and persistent flags (inherited by children) |
| Flag Types: `bool`, `string`, `int`, `float` |
| Syntax: `--flag=value` and `-f value` / `-fvalue` |
| Stacked short booleans `-vvv` |
| End-of-flags sentinel `--` |
| Individually required flags |
| Mutually exclusive flag groups |
| Required-together flag groups |
| One-required flag groups |
| Auto `--help` / `-h` per command |
| Auto `--version` / `-V` on root command |
| Typo suggestions |
| Lifecycle hooks |
| Hidden commands |
| Deprecated commands |
| Zero external dependencies |
| C23, compiles cleanly with `-Wall -Wextra` |

## Quick Start

```c
#include <stdbool.h>
#include <stdio.h>

#include "rattler.h"

static void
hello_run(rattler_cmd *cmd, int argc, char **argv)
{
    bool loud = rattler_flag_bool(cmd, "loud");
    printf(loud ? "HELLO, WORLD!\n" : "Hello, World!\n");
}

int
main(int argc, char **argv)
{
    rattler_cmd *root = rattler_new_command(
        "greet [command]",
        "A friendly greeter",
        "greet is a tiny demo of rattler_c.");
    rattler_set_version(root, "1.0.0");

    rattler_cmd *hello = rattler_new_command("hello", "Say hello", "");
    hello->run = hello_run;
    rattler_flags_bool(hello, "loud", 'l', false, "shout the greeting");

    rattler_add_command(root, hello);

    int rc = rattler_execute(root, argc, argv);
    rattler_free_tree(root);

    return rc;
}
```

## Example 

Build the example:

```sh
make example
```

## Build shared object

To build the shared object:

```sh
make install
```

## Testing

Run the tests

```c
make tests
```

## API

### One Required

At least **one** flag in the group must be set. Useful when multiple
input sources are valid but at least one is needed.

```c
rattler_flags_string(cmd, "file",  'f', "", "input file");
rattler_flags_bool  (cmd, "stdin", 's', false, "read from stdin");

rattler_mark_flags_one_required(cmd, "file", "stdin", NULL);
```

```
$ ./example process --file data.csv
Processing file: data.csv

$ ./example process --stdin
Processing from stdin

$ ./example process
rattler: at least one of the flags in the group [--file --stdin] is required
```

### Flags

#### Local Flags

Local flags are only available on the command they are registered on.

```c
rattler_flags_bool(cmd, "verbose", 'v', false, "enable verbose output");
rattler_flags_string(cmd, "output", 'o', "stdout", "output file");
rattler_flags_int(cmd, "count", 'n', 1, "repeat count");
rattler_flags_float(cmd, "rate", 'r', 1.0, "processing rate");
```

All flag syntaxes are supported:

```sh
--verbose              # bool toggle
--verbose=true         # explicit bool value
--output report.txt    # space-separated value
--output=report.txt    # equals-separated value
-o report.txt          # short form
-vn3                   # stacked: -v (bool) then -n 3
-- extra args          # everything after -- is positional
```

### Commands & Sub-commands

Commands are assembled into a tree with `rattler_add_command()`.
Dispatch is automatic — rattler_c finds the right command by walking the tree.

```c
rattler_cmd *root = rattler_new_command("example [command]", "My example", "");
rattler_cmd *server = rattler_new_command("server [command]", "Server ops", "");
rattler_cmd *start = rattler_new_command("start", "Start the server", "");
rattler_cmd *stop = rattler_new_command("stop",  "Stop the server",  "");

start->run = start_run;
stop->run  = stop_run;

rattler_add_command(server, start);   // example server start
rattler_add_command(server, stop);    // example server stop
rattler_add_command(root,   server);

rattler_execute(root, argc, argv);
rattler_free_tree(root);
```

```c
$ ./example server start
$ ./example server stop
$ ./example server --help
```

### Aliases

Give a command multiple names. All aliases dispatch to the same `run`
function and support the same flags.

```c
rattler_cmd *rem = rattler_new_command("remove [flags] [items...]",
                                  "Remove items", "");
rem->run = remove_run;
rattler_add_alias(rem, "rm");    // ./example rm  ...
rattler_add_alias(rem, "del");   // ./example del ...
rattler_flags_bool(rem, "force", 'f', false, "skip confirmation");

rattler_add_command(root, rem);
```

```sh
$ ./example remove file.txt
Removing 1 item(s)
  - file.txt

$ ./example rm file.txt
Removing 1 item(s)
  - file.txt

$ ./example del file.txt --force
Removing 1 item(s) (forced)
  - file.txt
```

Aliases in `--help` output:

```sh
Aliases:
  remove, rm, del
```

Typo suggestions also cover aliases:

```sh
$ ./example dek
rattler: unknown command "dek"

Did you mean this?
    del
```

### Persistent Flags

Persistent flags are defined on a parent command and **automatically
inherited** by every sub-command in the tree.

```c
// Defined once on root:
rattler_persistent_bool(root, "verbose", 'v', false, "enable verbose output");

// Available in every sub-command's run callback:
static void serve_run(rattler_cmd *cmd, int argc, char **argv) {
    if (rattler_flag_bool(cmd, "verbose"))   // inherited from root
        printf("[verbose] server starting\n");
}
```

```sh
$ ./example serve --verbose
[verbose] server starting
```

### Individually Required Flags

Mark a single flag as mandatory. rattler_c errors before calling `run` if
it is not provided.

```c
rattler_flags_string(conv, "output", 'o', "", "output file path");
rattler_mark_required(conv, "output");
```

```sh
$ ./example convert
rattler: required flag --output not set

Usage:
  convert [flags]

Flags:
  -o, --output string (REQUIRED)
        output file path (default "")
```

### Flag Group Constraints

#### Mutually Exclusive

At most **one** flag in the group may be set. Useful for format selectors,
log levels, or any either/or choice.

```c
rattler_flags_bool(cmd, "json",  'j', false, "output as JSON");
rattler_flags_bool(cmd, "yaml",  'y', false, "output as YAML");
rattler_flags_bool(cmd, "plain", 'p', false, "output as plain text");

// NULL-terminated varargs list
rattler_mark_flags_mutually_exclusive(cmd, "json", "yaml", "plain", NULL);
```

```
$ ./example export --json
Exporting as JSON

$ ./example export --json --yaml
error: if any flags in the group [--json --yaml --plain] are set
       none of the others can be; 2 were set
```

### Required Together

If **any** flag in the group is set, **all** must be set.
Perfect for credential pairs, TLS cert+key, host+port combos, etc.

```c
rattler_flags_string(cmd, "user", 'u', "", "username");
rattler_flags_string(cmd, "pass", 'p', "", "password");

rattler_mark_flags_required_together(cmd, "user", "pass", NULL);
```

```c
$ ./example login --user alice --pass s3cr3t
Logged in as: alice

$ ./example login --user alice
error: if any flags in the group [--user --pass] are set
       they must all be set; missing: --pass

$ ./example login
(anonymous — neither flag set, constraint satisfied)
```

### Lifecycle Hooks

rattler_c supports all five lifecycle hooks from Go rattler, called in this order:

```
PersistentPreRun   (outermost ancestor → innermost)
PreRun
Run                ← your main logic
PostRun
PersistentPostRun  (innermost → outermost ancestor)
```

```c
static void
root_persistent_pre(rattler_cmd *cmd, int argc, char **argv)
{
    // runs before EVERY command in the tree
    if (rattler_flag_bool(cmd, "verbose"))
        printf("[hook] persistent_pre_run\n");
}

static void
serve_pre(rattler_cmd *cmd, int argc, char **argv)
{
    printf("[hook] pre_run: validating config...\n");
}

static void
serve_run(rattler_cmd *cmd, int argc, char **argv)
{
    printf("Server running on port %d\n", rattler_flag_int(cmd, "port"));
}

static void
serve_post(rattler_cmd *cmd, int argc, char **argv)
{
    printf("[hook] post_run: cleanup complete\n");
}

// wire them up:
root->persistent_pre_run = root_persistent_pre;
serve->pre_run           = serve_pre;
serve->run               = serve_run;
serve->post_run          = serve_post;
```

```sh
$ ./example serve --verbose --port 9090
[hook] persistent_pre_run
[hook] pre_run: validating config...
Server running on port 9090
[hook] post_run: cleanup complete
```

### Argument Validation

Constrain the number of positional arguments accepted:

```c
rattler_set_args(cmd, min, max);   // max = -1 means unlimited
```

| Call | Meaning |
|---|---|
| `rattler_set_args(cmd, 0, 0)` | No positional args allowed |
| `rattler_set_args(cmd, 1, 1)` | Exactly 1 arg |
| `rattler_set_args(cmd, 2, 2)` | Exactly 2 args |
| `rattler_set_args(cmd, 1, -1)` | At least 1 arg (unlimited max) |
| `rattler_set_args(cmd, 0, 5)` | Up to 5 args |

```sh
$ ./example config set only-one-arg
rattler: need at least 2 arg(s), got 1
```

### Help & Version

#### Auto-generated Help

Every command automatically gets `--help` / `-h`:

```sh
$ ./example --help
Demonstrates aliases, mutually-exclusive flags,
required-together flags, one-required flags, and
individually required flags.

Examples:
  example remove a.txt b.txt
  example export --json
  example login --user alice --pass s3cr3t

Usage:
  example [command]

Available Commands:
  remove           Remove items
  export           Export data
  login            Log in to the service
  process          Process input
  convert          Convert a file

Global Flags:
  -v, --verbose
        verbose output (default false)

Use "exmaple [command] --help" for more information.
```

### Version

```c
rattler_set_version(root, "2.0.0");
```

```sh
$ ./example --version
example version 2.0.0
```

### Typo Suggestions (Levenshtein distance)

```sh
$ ./example convrt
rattler: unknown command "convrt"

Did you mean this?
    convert
```

### Typed Value Accessors

Read flag values inside your `run` callbacks using the typed accessors.
These are safe to call at any point while the command tree is alive.

```c
static void
my_run(rattler_cmd *cmd, int argc, char **argv)
{
    const char *output  = rattler_flag_string(cmd, "output");
    int         count   = rattler_flag_int   (cmd, "count");
    double      rate    = rattler_flag_float (cmd, "rate");
    bool        verbose = rattler_flag_bool  (cmd, "verbose");

    // Check if a flag was explicitly set by the user
    if (rattler_flag_changed(cmd, "count"))
        printf("User explicitly set --count to %d\n", count);
}
```

### Command Lifecycle

```c
// create a new command
rattler_cmd *rattler_new_command(const char *use, const char *short_desc,
                                const char *long_desc);

// add a child command
void rattler_add_command(rattler_cmd *parent, rattler_cmd *child);

// execute the command tree (pass main's argc/argv directly)
int rattler_execute(rattler_cmd *root, int argc, char **argv);

// free the entire command tree (single call)
void rattler_free_tree(rattler_cmd *root);

// set version string (enables --version / -V)
void rattler_set_version(rattler_cmd *cmd, const char *version);

// set positional argument constraints (max = -1 for unlimited)
void rattler_set_args(rattler_cmd *cmd, int min, int max);
```

### Aliases

```c
void rattler_add_alias(rattler_cmd *cmd, const char *alias);
```

### Flag Registration

```c
// local flags (this command only)
void rattler_flags_bool(rattler_cmd *cmd, const char *name, char shorthand,
                        bool default_val, const char *usage);

void rattler_flags_string(rattler_cmd *cmd, const char *name, char shorthand,
                          const char *default_val, const char *usage);

void rattler_flags_int(rattler_cmd *cmd, const char *name, char shorthand,
                       int default_val, const char *usage);

void rattler_flags_float (rattler_cmd *cmd, const char *name, char shorthand,
                          double default_val, const char *usage);

// persistent flags (inherited by all sub-commands)
void rattler_persistent_bool(rattler_cmd *cmd, const char *name, char shorthand,
                             bool default_val, const char *usage);

void rattler_persistent_string(rattler_cmd *cmd, const char *name,
                               char shorthand, const char *default_val,
                               const char *usage);

void rattler_persistent_int(rattler_cmd *cmd, const char *name, char shorthand,
                            int default_val, const char *usage);

void rattler_persistent_float (rattler_cmd *cmd, const char *name,
                               char shorthand, double default_val,
                               const char *usage);
```

### Flag Constraints

```c
// mark a single flag as always required
void rattler_mark_required(rattler_cmd *cmd, const char *flag_name);

// at most one flag in the group may be set (NULL-terminated varargs)
void rattler_mark_flags_mutually_exclusive(rattler_cmd *cmd, ...);

// if any flag is set, all must be set (NULL-terminated varargs)
void rattler_mark_flags_required_together(rattler_cmd *cmd, ...);

// at least one flag in the group must be set (NULL-terminated varargs)
void rattler_mark_flags_one_required(rattler_cmd *cmd, ...);
```

### Flag Lookup & Accessors

```c
rattlerFlag *rattler_lookup_flag(rattler_cmd *cmd, const char *name);
rattlerFlag *rattler_lookup_flag_short(rattler_cmd *cmd, char shorthand);
bool rattler_flag_changed(rattler_cmd *cmd, const char *name);

const char *rattler_flag_string(rattler_cmd *cmd, const char *name);
int rattler_flag_int(rattler_cmd *cmd, const char *name);
double rattler_flag_float(rattler_cmd *cmd, const char *name);
bool rattler_flag_bool(rattler_cmd *cmd, const char *name);
```

### rattler_cmd Fields (set directly)

```c
cmd->run                  = my_run_fn;            // main run function
cmd->pre_run              = my_pre_fn;            // runs before run
cmd->post_run             = my_post_fn;           // runs after run
cmd->persistent_pre_run   = my_ppr_fn;            // inherited pre-run
cmd->persistent_post_run  = my_ppor_fn;           // inherited post-run
cmd->example              = "  example foo\n"         // shown in --help
                            "  example bar\n";
cmd->hidden               = true;                 // hide from help listing
cmd->deprecated           = "use 'new' instead";  // deprecation warning
cmd->silence_usage        = true;                 // suppress usage on error
cmd->disable_flag_parsing = true;                 // pass all args raw
```

## Contributing

Please feel free to open a PR!

## Contact

Brian Downs [@bdowns328](http://twitter.com/bdowns328)

## License

BSD 2 Clause [License](/LICENSE).
