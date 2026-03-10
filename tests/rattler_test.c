#include <time.h>
#include <stdlib.h>
#include "crosscheck.h"

#include "../rattler.h"

static rattler_cmd *g_root = NULL;
static int g_run_count = 0;
static int g_pre_count = 0;
static int g_post_count = 0;
static int g_ppr_count = 0;
static int g_ppor_count = 0;
static int g_last_argc = -1;
static char **g_last_argv = NULL;

void
cc_setup()
{
    g_root       = NULL;
    g_run_count  = 0;
    g_pre_count  = 0;
    g_post_count = 0;
    g_ppr_count  = 0;
    g_ppor_count = 0;
    g_last_argc  = -1;
    g_last_argv  = NULL;
}

void
cc_tear_down()
{
    if (g_root) {
        rattler_free(g_root);
        g_root = NULL;
    }
}

#define MAX_FAKE_ARGS 32
static char test_buf[512];
static char *test_argv[MAX_FAKE_ARGS];
static int test_argc = 0;

static void
build_argv(const char *cmd_line)
{
    strncpy(test_buf, cmd_line, sizeof test_buf - 1);

    test_buf[sizeof test_buf - 1] = '\0';
    test_argc = 0;

    char *tok = strtok(test_buf, " ");

    while (tok && test_argc < MAX_FAKE_ARGS) {
        test_argv[test_argc++] = tok;
        tok = strtok(NULL, " ");
    }
}

cc_result_t
test_new_command_not_null(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "short", "long");
    CC_ASSERT_NOT_NULL(cmd);
    rattler_free(cmd);

    CC_SUCCESS;
}

cc_result_t
test_new_command_use_string(void)
{
    rattler_cmd *cmd = rattler_new_command("mytest_app [command]", "short", "long");

    CC_ASSERT_STRING_EQUAL(cmd->use, "mytest_app [command]");
    rattler_free(cmd);

    CC_SUCCESS;
}

cc_result_t
test_new_command_short_desc(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "A short description", "long");

    CC_ASSERT_STRING_EQUAL(cmd->short_desc, "A short description");
    rattler_free(cmd);

    CC_SUCCESS;
}

cc_result_t
test_new_command_long_desc(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "short", "A longer description");

    CC_ASSERT_STRING_EQUAL(cmd->long_desc, "A longer description");
    rattler_free(cmd);

    CC_SUCCESS;
}

cc_result_t
test_new_command_defaults(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "s", "l");

    CC_ASSERT_INT_EQUAL(cmd->max_args, -1);
    CC_ASSERT_INT_EQUAL(cmd->min_args, 0);
    CC_ASSERT_INT_EQUAL(cmd->num_children, 0);
    CC_ASSERT_NULL(cmd->run);
    CC_ASSERT_NULL(cmd->parent);

    rattler_free(cmd);

    CC_SUCCESS;
}

cc_result_t
test_add_command_increments_children(void)
{
    rattler_cmd *root  = rattler_new_command("test_app", "", "");
    rattler_cmd *child = rattler_new_command("sub", "", "");

    rattler_add_command(root, child);
    CC_ASSERT_INT_EQUAL(root->num_children, 1);
    rattler_free(root);

    CC_SUCCESS;
}

cc_result_t
test_add_command_sets_parent(void)
{
    rattler_cmd *root  = rattler_new_command("test_app", "", "");
    rattler_cmd *child = rattler_new_command("sub", "", "");

    rattler_add_command(root, child);
    CC_ASSERT_NOT_NULL(child->parent);
    CC_ASSERT_STRING_EQUAL(child->parent->use, "test_app");
    rattler_free(root);

    CC_SUCCESS;
}

cc_result_t
test_add_multiple_children(void)
{
    rattler_cmd *root = rattler_new_command("test_app", "", "");

    rattler_add_command(root, rattler_new_command("a", "", ""));
    rattler_add_command(root, rattler_new_command("b", "", ""));
    rattler_add_command(root, rattler_new_command("c", "", ""));
    CC_ASSERT_INT_EQUAL(root->num_children, 3);
    rattler_free(root);

    CC_SUCCESS;
}

static void
stub_run(rattler_cmd *c, int a, char **v)
{
    RATTLER_UNUSED(c);

    g_run_count++;
    g_last_argc = a;
    g_last_argv = v;
}

static void
stub_pre(rattler_cmd *c, int a, char **v)
{
    RATTLER_UNUSED(c);
    RATTLER_UNUSED(a);
    RATTLER_UNUSED(v);

    g_pre_count++;
}

static void
stub_post(rattler_cmd *c, int a, char **v)
{
    RATTLER_UNUSED(c);
    RATTLER_UNUSED(a);
    RATTLER_UNUSED(v);

    g_post_count++;
}

static void
stub_ppr(rattler_cmd *c, int a, char **v)
{
    RATTLER_UNUSED(c);
    RATTLER_UNUSED(a);
    RATTLER_UNUSED(v);

    g_ppr_count++;
}
static void
stub_ppor(rattler_cmd *c, int a, char **v)
{
    RATTLER_UNUSED(c);
    RATTLER_UNUSED(a);
    RATTLER_UNUSED(v);

    g_ppor_count++;
}

cc_result_t
test_add_command_deep_nesting(void)
{
    rattler_cmd *root = rattler_new_command("test_app", "", "");
    rattler_cmd *lvl1 = rattler_new_command("server", "", "");
    rattler_cmd *lvl2 = rattler_new_command("start", "", "");

    lvl2->run = stub_run;
    rattler_add_command(lvl1, lvl2);
    rattler_add_command(root, lvl1);
    CC_ASSERT_INT_EQUAL(root->num_children, 1);
    CC_ASSERT_INT_EQUAL(lvl1->num_children, 1);
    CC_ASSERT_STRING_EQUAL(lvl2->parent->use, "server");
    build_argv("test_app server start");

    int rc = rattler_execute(root, test_argc, test_argv);
    CC_ASSERT_INT_EQUAL(rc, 0);
    CC_ASSERT_INT_EQUAL(g_run_count, 1);
    rattler_free(root);

    CC_SUCCESS;
}

// rattler_execute basic dispatch

cc_result_t
test_execute_root_run(void)
{
    rattler_cmd *root = rattler_new_command("test_app", "", "");

    root->run = stub_run;
    build_argv("test_app");

    int rc = rattler_execute(root, test_argc, test_argv);
    CC_ASSERT_INT_EQUAL(rc, 0);
    CC_ASSERT_INT_EQUAL(g_run_count, 1);
    rattler_free(root);

    CC_SUCCESS;
}

cc_result_t
test_execute_subcommand_dispatch(void)
{
    rattler_cmd *root = rattler_new_command("test_app", "", "");
    rattler_cmd *sub  = rattler_new_command("serve", "", "");

    sub->run = stub_run;
    rattler_add_command(root, sub);
    build_argv("test_app serve");

    int rc = rattler_execute(root, test_argc, test_argv);
    CC_ASSERT_INT_EQUAL(rc, 0);
    CC_ASSERT_INT_EQUAL(g_run_count, 1);
    rattler_free(root);

    CC_SUCCESS;
}

cc_result_t
test_execute_unknown_subcommand_returns_error(void)
{
    rattler_cmd *root = rattler_new_command("test_app", "", "");
    rattler_cmd *sub  = rattler_new_command("serve", "", "");

    sub->run = stub_run;
    root->silence_usage = true;
    rattler_add_command(root, sub);
    build_argv("test_app doesnotexist");

    int rc = rattler_execute(root, test_argc, test_argv);
    CC_ASSERT_INT_NOT_EQUAL(rc, 0);
    CC_ASSERT_INT_EQUAL(g_run_count, 0);
    rattler_free(root);

    CC_SUCCESS;
}

cc_result_t
test_execute_no_run_fn_returns_zero(void)
{
    rattler_cmd *root = rattler_new_command("test_app", "short", "long");
    root->silence_usage = true;
    build_argv("test_app");
    int rc = rattler_execute(root, test_argc, test_argv);
    CC_ASSERT_INT_EQUAL(rc, 0);
    rattler_free(root);
    CC_SUCCESS;
}

// flag registration & lookup

cc_result_t
test_flag_bool_registered(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");
    rattler_flags_bool(cmd, "verbose", 'v', false, "enable verbose");
    rattler_flag *f = rattler_lookup_flag(cmd, "verbose");
    CC_ASSERT_NOT_NULL(f);
    CC_ASSERT_STRING_EQUAL(f->name, "verbose");
    rattler_free(cmd);
    CC_SUCCESS;
}

cc_result_t
test_flag_string_registered(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");
    rattler_flags_string(cmd, "output", 'o', "stdout", "output file");
    rattler_flag *f = rattler_lookup_flag(cmd, "output");
    CC_ASSERT_NOT_NULL(f);
    CC_ASSERT_STRING_EQUAL(f->name, "output");
    rattler_free(cmd);
    CC_SUCCESS;
}

cc_result_t
test_flag_int_registered(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");
    rattler_flags_int(cmd, "port", 'p', 8080, "port number");
    rattler_flag *f = rattler_lookup_flag(cmd, "port");
    CC_ASSERT_NOT_NULL(f);
    CC_ASSERT_INT_EQUAL(f->value.i, 8080);
    rattler_free(cmd);
    CC_SUCCESS;
}

cc_result_t
test_flag_float_registered(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");
    rattler_flags_float(cmd, "rate", 'r', 1.5, "rate");
    rattler_flag *f = rattler_lookup_flag(cmd, "rate");
    CC_ASSERT_NOT_NULL(f);
    CC_ASSERT_DOUBLE_EQUAL(f->value.f, 1.5);
    rattler_free(cmd);
    CC_SUCCESS;
}

cc_result_t
test_flag_unknown_returns_null(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    rattler_flag *f = rattler_lookup_flag(cmd, "nonexistent");
    CC_ASSERT_NULL(f);
    rattler_free(cmd);

    CC_SUCCESS;
}

cc_result_t
test_flag_shorthand_lookup(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    rattler_flags_bool(cmd, "verbose", 'v', false, "verbose");
    rattler_flag *f = rattler_lookup_flag_short(cmd, 'v');
    CC_ASSERT_NOT_NULL(f);
    CC_ASSERT_STRING_EQUAL(f->name, "verbose");
    rattler_free(cmd);

    CC_SUCCESS;
}

cc_result_t
test_flag_shorthand_unknown_returns_null(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");
    rattler_flag *f = rattler_lookup_flag_short(cmd, 'z');
    CC_ASSERT_NULL(f);
    rattler_free(cmd);

    CC_SUCCESS;
}

// flag parsing

cc_result_t
test_parse_long_bool_flag(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    cmd->run = stub_run;
    rattler_flags_bool(cmd, "verbose", 'v', false, "verbose");
    build_argv("test_app --verbose");

    rattler_execute(cmd, test_argc, test_argv);
    CC_ASSERT_TRUE(rattler_flag_bool(cmd, "verbose"));
    rattler_free(cmd);

    CC_SUCCESS;
}

cc_result_t
test_parse_short_bool_flag(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    cmd->run = stub_run;
    rattler_flags_bool(cmd, "verbose", 'v', false, "verbose");
    build_argv("test_app -v");

    rattler_execute(cmd, test_argc, test_argv);
    CC_ASSERT_TRUE(rattler_flag_bool(cmd, "verbose"));
    rattler_free(cmd);

    CC_SUCCESS;
}

cc_result_t
test_parse_long_string_flag(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    cmd->run = stub_run;
    rattler_flags_string(cmd, "output", 'o', "", "output");
    build_argv("test_app --output report.txt");

    rattler_execute(cmd, test_argc, test_argv);
    CC_ASSERT_STRING_EQUAL(rattler_flag_string(cmd, "output"), "report.txt");
    rattler_free(cmd);

    CC_SUCCESS;
}

cc_result_t
test_parse_equals_syntax(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    cmd->run = stub_run;
    rattler_flags_int(cmd, "port", 'p', 80, "port");
    build_argv("test_app --port=9090");

    rattler_execute(cmd, test_argc, test_argv);
    CC_ASSERT_INT_EQUAL(rattler_flag_int(cmd, "port"), 9090);
    rattler_free(cmd);

    CC_SUCCESS;
}

cc_result_t
test_parse_short_int_flag(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    cmd->run = stub_run;
    rattler_flags_int(cmd, "port", 'p', 80, "port");
    build_argv("test_app -p 4433");

    rattler_execute(cmd, test_argc, test_argv);
    CC_ASSERT_INT_EQUAL(rattler_flag_int(cmd, "port"), 4433);
    rattler_free(cmd);

    CC_SUCCESS;
}

cc_result_t
test_parse_float_flag(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    cmd->run = stub_run;
    rattler_flags_float(cmd, "rate", 'r', 1.0, "rate");
    build_argv("test_app --rate=2.5");

    rattler_execute(cmd, test_argc, test_argv);
    CC_ASSERT_DOUBLE_EQUAL(rattler_flag_float(cmd, "rate"), 2.5);
    rattler_free(cmd);

    CC_SUCCESS;
}

cc_result_t
test_parse_unknown_flag_returns_error(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    cmd->run = stub_run;
    cmd->silence_usage = true;
    build_argv("test_app --doesnotexist");

    int rc = rattler_execute(cmd, test_argc, test_argv);
    CC_ASSERT_INT_NOT_EQUAL(rc, 0);
    rattler_free(cmd);

    CC_SUCCESS;
}

cc_result_t
test_parse_flag_default_unchanged(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    cmd->run = stub_run;
    rattler_flags_int(cmd, "port", 'p', 8080, "port");
    build_argv("test_app");

    rattler_execute(cmd, test_argc, test_argv);
    CC_ASSERT_INT_EQUAL(rattler_flag_int(cmd, "port"), 8080);
    CC_ASSERT_FALSE(rattler_flag_changed(cmd, "port"));
    rattler_free(cmd);

    CC_SUCCESS;
}

cc_result_t
test_flag_changed_after_set(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    cmd->run = stub_run;
    rattler_flags_int(cmd, "port", 'p', 8080, "port");
    build_argv("test_app --port 9090");

    rattler_execute(cmd, test_argc, test_argv);
    CC_ASSERT_TRUE(rattler_flag_changed(cmd, "port"));
    rattler_free(cmd);

    CC_SUCCESS;
}

// persistent flags

cc_result_t
test_persistent_flag_inherited_by_child(void)
{
    rattler_cmd *root = rattler_new_command("test_app", "", "");
    rattler_cmd *sub = rattler_new_command("serve", "", "");

    sub->run = stub_run;
    rattler_persistent_bool(root, "verbose", 'v', false, "verbose");
    rattler_add_command(root, sub);
    rattler_flag *f = rattler_lookup_flag(sub, "verbose");
    CC_ASSERT_NOT_NULL(f);
    rattler_free(root);

    CC_SUCCESS;
}

cc_result_t
test_persistent_flag_parsed_on_child(void)
{
    rattler_cmd *root = rattler_new_command("test_app", "", "");
    rattler_cmd *sub  = rattler_new_command("serve", "", "");

    sub->run = stub_run;
    rattler_persistent_bool(root, "verbose", 'v', false, "verbose");
    rattler_add_command(root, sub);
    build_argv("test_app serve --verbose");

    rattler_execute(root, test_argc, test_argv);
    CC_ASSERT_TRUE(rattler_flag_bool(sub, "verbose"));
    rattler_free(root);

    CC_SUCCESS;
}

cc_result_t
test_persistent_string_flag_value(void)
{
    rattler_cmd *root = rattler_new_command("test_app", "", "");
    rattler_cmd *sub = rattler_new_command("run", "", "");

    sub->run = stub_run;
    rattler_persistent_string(root, "config", 'c', "default.cfg", "config file");
    rattler_add_command(root, sub);
    build_argv("test_app run --config custom.cfg");

    rattler_execute(root, test_argc, test_argv);
    CC_ASSERT_STRING_EQUAL(rattler_flag_string(sub, "config"), "custom.cfg");
    rattler_free(root);

    CC_SUCCESS;
}

// required flags

cc_result_t
test_required_flag_missing_returns_error(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    cmd->run = stub_run;
    cmd->silence_usage = true;
    rattler_flags_string(cmd, "output", 'o', "", "output");
    rattler_mark_required(cmd, "output");
    build_argv("test_app");

    int rc = rattler_execute(cmd, test_argc, test_argv);
    CC_ASSERT_INT_NOT_EQUAL(rc, 0);
    CC_ASSERT_INT_EQUAL(g_run_count, 0);
    rattler_free(cmd);

    CC_SUCCESS;
}

cc_result_t
test_required_flag_present_succeeds(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    cmd->run = stub_run;
    rattler_flags_string(cmd, "output", 'o', "", "output");
    rattler_mark_required(cmd, "output");
    build_argv("test_app --output out.txt");

    int rc = rattler_execute(cmd, test_argc, test_argv);
    CC_ASSERT_INT_EQUAL(rc, 0);
    CC_ASSERT_INT_EQUAL(g_run_count, 1);
    rattler_free(cmd);

    CC_SUCCESS;
}

// mutually exclusive flags

cc_result_t
test_mutually_exclusive_both_set_returns_error(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    cmd->run = stub_run;
    cmd->silence_usage = true;
    rattler_flags_bool(cmd, "json", 'j', false, "json");
    rattler_flags_bool(cmd, "yaml", 'y', false, "yaml");
    rattler_mark_flags_mutually_exclusive(cmd, "json", "yaml", NULL);
    build_argv("test_app --json --yaml");

    int rc = rattler_execute(cmd, test_argc, test_argv);
    CC_ASSERT_INT_NOT_EQUAL(rc, 0);
    CC_ASSERT_INT_EQUAL(g_run_count, 0);
    rattler_free(cmd);

    CC_SUCCESS;
}

cc_result_t
test_mutually_exclusive_one_set_succeeds(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    cmd->run = stub_run;
    rattler_flags_bool(cmd, "json", 'j', false, "json");
    rattler_flags_bool(cmd, "yaml", 'y', false, "yaml");
    rattler_mark_flags_mutually_exclusive(cmd, "json", "yaml", NULL);
    build_argv("test_app --json");

    int rc = rattler_execute(cmd, test_argc, test_argv);
    CC_ASSERT_INT_EQUAL(rc, 0);
    CC_ASSERT_INT_EQUAL(g_run_count, 1);
    rattler_free(cmd);

    CC_SUCCESS;
}

cc_result_t
test_mutually_exclusive_none_set_succeeds(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    cmd->run = stub_run;
    rattler_flags_bool(cmd, "json", 'j', false, "json");
    rattler_flags_bool(cmd, "yaml", 'y', false, "yaml");
    rattler_mark_flags_mutually_exclusive(cmd, "json", "yaml", NULL);
    build_argv("test_app");

    int rc = rattler_execute(cmd, test_argc, test_argv);
    CC_ASSERT_INT_EQUAL(rc, 0);
    CC_ASSERT_INT_EQUAL(g_run_count, 1);
    rattler_free(cmd);

    CC_SUCCESS;
}

// required together flags

cc_result_t
test_required_together_both_set_succeeds(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    cmd->run = stub_run;
    rattler_flags_string(cmd, "user", 'u', "", "user");
    rattler_flags_string(cmd, "pass", 'p', "", "pass");
    rattler_mark_flags_required_together(cmd, "user", "pass", NULL);
    build_argv("test_app --user alice --pass s3cr3t");

    int rc = rattler_execute(cmd, test_argc, test_argv);
    CC_ASSERT_INT_EQUAL(rc, 0);
    CC_ASSERT_INT_EQUAL(g_run_count, 1);
    rattler_free(cmd);

    CC_SUCCESS;
}

cc_result_t
test_required_together_only_one_set_returns_error(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    cmd->run = stub_run;
    cmd->silence_usage = true;
    rattler_flags_string(cmd, "user", 'u', "", "user");
    rattler_flags_string(cmd, "pass", 'p', "", "pass");
    rattler_mark_flags_required_together(cmd, "user", "pass", NULL);
    build_argv("test_app --user alice");

    int rc = rattler_execute(cmd, test_argc, test_argv);
    CC_ASSERT_INT_NOT_EQUAL(rc, 0);
    CC_ASSERT_INT_EQUAL(g_run_count, 0);
    rattler_free(cmd);

    CC_SUCCESS;
}

cc_result_t
test_required_together_neither_set_succeeds(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    cmd->run = stub_run;
    rattler_flags_string(cmd, "user", 'u', "", "user");
    rattler_flags_string(cmd, "pass", 'p', "", "pass");
    rattler_mark_flags_required_together(cmd, "user", "pass", NULL);
    build_argv("test_app");

    int rc = rattler_execute(cmd, test_argc, test_argv);
    CC_ASSERT_INT_EQUAL(rc, 0);
    CC_ASSERT_INT_EQUAL(g_run_count, 1);
    rattler_free(cmd);

    CC_SUCCESS;
}

// one required flags

cc_result_t
test_one_required_first_set_succeeds(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    cmd->run = stub_run;
    rattler_flags_string(cmd, "file",  'f', "", "file");
    rattler_flags_bool  (cmd, "stdin", 's', false, "stdin");
    rattler_mark_flags_one_required(cmd, "file", "stdin", NULL);
    build_argv("test_app --file data.csv");

    int rc = rattler_execute(cmd, test_argc, test_argv);
    CC_ASSERT_INT_EQUAL(rc, 0);
    CC_ASSERT_INT_EQUAL(g_run_count, 1);
    rattler_free(cmd);

    CC_SUCCESS;
}

cc_result_t
test_one_required_second_set_succeeds(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    cmd->run = stub_run;
    rattler_flags_string(cmd, "file",  'f', "", "file");
    rattler_flags_bool  (cmd, "stdin", 's', false, "stdin");
    rattler_mark_flags_one_required(cmd, "file", "stdin", NULL);
    build_argv("test_app --stdin");

    int rc = rattler_execute(cmd, test_argc, test_argv);
    CC_ASSERT_INT_EQUAL(rc, 0);
    CC_ASSERT_INT_EQUAL(g_run_count, 1);
    rattler_free(cmd);

    CC_SUCCESS;
}

cc_result_t
test_one_required_none_set_returns_error(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    cmd->run = stub_run;
    cmd->silence_usage = true;
    rattler_flags_string(cmd, "file",  'f', "", "file");
    rattler_flags_bool  (cmd, "stdin", 's', false, "stdin");
    rattler_mark_flags_one_required(cmd, "file", "stdin", NULL);
    build_argv("test_app");

    int rc = rattler_execute(cmd, test_argc, test_argv);
    CC_ASSERT_INT_NOT_EQUAL(rc, 0);
    CC_ASSERT_INT_EQUAL(g_run_count, 0);
    rattler_free(cmd);

    CC_SUCCESS;
}

// aliases

cc_result_t
test_alias_dispatches_correctly(void)
{
    rattler_cmd *root = rattler_new_command("test_app", "", "");
    rattler_cmd *rem = rattler_new_command("remove", "", "");

    rem->run = stub_run;
    rattler_add_alias(rem, "rm");
    rattler_add_command(root, rem);
    build_argv("test_app rm");

    int rc = rattler_execute(root, test_argc, test_argv);
    CC_ASSERT_INT_EQUAL(rc, 0);
    CC_ASSERT_INT_EQUAL(g_run_count, 1);
    rattler_free(root);

    CC_SUCCESS;
}

cc_result_t
test_alias_with_flags(void)
{
    rattler_cmd *root = rattler_new_command("test_app", "", "");
    rattler_cmd *rem  = rattler_new_command("remove", "", "");

    rem->run = stub_run;
    rattler_add_alias(rem, "rm");
    rattler_flags_bool(rem, "force", 'f', false, "force");
    rattler_add_command(root, rem);
    build_argv("test_app rm --force");

    int rc = rattler_execute(root, test_argc, test_argv);
    CC_ASSERT_INT_EQUAL(rc, 0);
    CC_ASSERT_TRUE(rattler_flag_bool(rem, "force"));
    rattler_free(root);

    CC_SUCCESS;
}

cc_result_t
test_multiple_aliases(void)
{
    rattler_cmd *root = rattler_new_command("test_app", "", "");
    rattler_cmd *rem  = rattler_new_command("remove", "", "");

    rem->run = stub_run;
    rattler_add_alias(rem, "rm");
    rattler_add_alias(rem, "del");
    rattler_add_command(root, rem);
    build_argv("test_app del");

    int rc = rattler_execute(root, test_argc, test_argv);
    CC_ASSERT_INT_EQUAL(rc, 0);
    CC_ASSERT_INT_EQUAL(g_run_count, 1);
    CC_ASSERT_INT_EQUAL(rem->num_aliases, 2);
    rattler_free(root);

    CC_SUCCESS;
}

cc_result_t
test_alias_count_correct(void)
{
    rattler_cmd *cmd = rattler_new_command("remove", "", "");

    rattler_add_alias(cmd, "rm");
    rattler_add_alias(cmd, "del");
    rattler_add_alias(cmd, "erase");
    CC_ASSERT_INT_EQUAL(cmd->num_aliases, 3);
    rattler_free(cmd);

    CC_SUCCESS;
}

// lifecycle hooks

cc_result_t
test_pre_run_called_before_run(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    cmd->pre_run = stub_pre;
    cmd->run = stub_run;
    build_argv("test_app");

    rattler_execute(cmd, test_argc, test_argv);
    CC_ASSERT_INT_EQUAL(g_pre_count, 1);
    CC_ASSERT_INT_EQUAL(g_run_count, 1);
    rattler_free(cmd);

    CC_SUCCESS;
}

cc_result_t
test_post_run_called_after_run(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    cmd->run = stub_run;
    cmd->post_run = stub_post;
    build_argv("test_app");

    rattler_execute(cmd, test_argc, test_argv);
    CC_ASSERT_INT_EQUAL(g_run_count, 1);
    CC_ASSERT_INT_EQUAL(g_post_count, 1);
    rattler_free(cmd);

    CC_SUCCESS;
}

cc_result_t
test_persistent_pre_run_fires_on_child(void)
{
    rattler_cmd *root = rattler_new_command("test_app", "", "");
    rattler_cmd *sub = rattler_new_command("serve", "", "");

    sub->run = stub_run;
    root->persistent_pre_run = stub_ppr;
    rattler_add_command(root, sub);
    build_argv("test_app serve");

    rattler_execute(root, test_argc, test_argv);
    CC_ASSERT_INT_EQUAL(g_ppr_count, 1);
    CC_ASSERT_INT_EQUAL(g_run_count, 1);
    rattler_free(root);

    CC_SUCCESS;
}

cc_result_t
test_persistent_post_run_fires_on_child(void)
{
    rattler_cmd *root = rattler_new_command("test_app", "", "");
    rattler_cmd *sub = rattler_new_command("serve", "", "");

    sub->run = stub_run;
    root->persistent_post_run = stub_ppor;
    rattler_add_command(root, sub);
    build_argv("test_app serve");

    rattler_execute(root, test_argc, test_argv);
    CC_ASSERT_INT_EQUAL(g_ppor_count, 1);
    CC_ASSERT_INT_EQUAL(g_run_count, 1);
    rattler_free(root);

    CC_SUCCESS;
}

cc_result_t
test_all_hooks_fire_in_order(void)
{
    rattler_cmd *root = rattler_new_command("test_app", "", "");
    rattler_cmd *sub = rattler_new_command("sub", "", "");

    sub->run = stub_run;
    sub->pre_run = stub_pre;
    sub->post_run = stub_post;
    root->persistent_pre_run = stub_ppr;
    root->persistent_post_run = stub_ppor;
    rattler_add_command(root, sub);
    build_argv("test_app sub");

    rattler_execute(root, test_argc, test_argv);
    CC_ASSERT_INT_EQUAL(g_ppr_count, 1);
    CC_ASSERT_INT_EQUAL(g_pre_count, 1);
    CC_ASSERT_INT_EQUAL(g_run_count, 1);
    CC_ASSERT_INT_EQUAL(g_post_count, 1);
    CC_ASSERT_INT_EQUAL(g_ppor_count, 1);
    rattler_free(root);

    CC_SUCCESS;
}

// argument validation

cc_result_t
test_min_args_not_met_returns_error(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    cmd->run = stub_run;
    rattler_set_args(cmd, 2, -1);
    build_argv("test_app only-one");

    int rc = rattler_execute(cmd, test_argc, test_argv);
    CC_ASSERT_INT_NOT_EQUAL(rc, 0);
    CC_ASSERT_INT_EQUAL(g_run_count, 0);
    rattler_free(cmd);

    CC_SUCCESS;
}

cc_result_t
test_max_args_exceeded_returns_error(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    cmd->run = stub_run;
    rattler_set_args(cmd, 0, 1);
    build_argv("test_app one two");

    int rc = rattler_execute(cmd, test_argc, test_argv);
    CC_ASSERT_INT_NOT_EQUAL(rc, 0);
    CC_ASSERT_INT_EQUAL(g_run_count, 0);
    rattler_free(cmd);

    CC_SUCCESS;
}

cc_result_t
test_exact_args_succeeds(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    cmd->run = stub_run;
    rattler_set_args(cmd, 2, 2);
    build_argv("test_app foo bar");

    int rc = rattler_execute(cmd, test_argc, test_argv);
    CC_ASSERT_INT_EQUAL(rc, 0);
    CC_ASSERT_INT_EQUAL(g_run_count, 1);
    CC_ASSERT_INT_EQUAL(g_last_argc, 2);
    rattler_free(cmd);

    CC_SUCCESS;
}

cc_result_t
test_unlimited_args_succeeds(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    cmd->run = stub_run;
    rattler_set_args(cmd, 0, -1);
    build_argv("test_app a b c d e");

    int rc = rattler_execute(cmd, test_argc, test_argv);
    CC_ASSERT_INT_EQUAL(rc, 0);
    CC_ASSERT_INT_EQUAL(g_last_argc, 5);
    rattler_free(cmd);

    CC_SUCCESS;
}

// version flag

cc_result_t
test_set_version_stored(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    rattler_set_version(cmd, "1.2.3");
    CC_ASSERT_STRING_EQUAL(cmd->version, "1.2.3");
    rattler_free(cmd);

    CC_SUCCESS;
}

cc_result_t
test_version_flag_returns_zero(void)
{
    rattler_cmd *cmd = rattler_new_command("test_app", "", "");

    rattler_set_version(cmd, "2.0.0");
    build_argv("test_app --version");

    int rc = rattler_execute(cmd, test_argc, test_argv);
    CC_ASSERT_INT_EQUAL(rc, 0);
    rattler_free(cmd);

    CC_SUCCESS;
}

int
main(void)
{
    srand(time(NULL));

    CC_INIT;

    CC_RUN(test_new_command_not_null);
    CC_RUN(test_new_command_use_string);
    CC_RUN(test_new_command_short_desc);
    CC_RUN(test_new_command_long_desc);
    CC_RUN(test_new_command_defaults);
    CC_RUN(test_add_command_increments_children);
    CC_RUN(test_add_command_sets_parent);
    CC_RUN(test_add_multiple_children);
    CC_RUN(test_add_command_deep_nesting);
    CC_RUN(test_execute_root_run);
    CC_RUN(test_execute_subcommand_dispatch);
    CC_RUN(test_execute_unknown_subcommand_returns_error);
    CC_RUN(test_execute_no_run_fn_returns_zero);
    CC_RUN(test_flag_bool_registered);
    CC_RUN(test_flag_string_registered);
    CC_RUN(test_flag_int_registered);
    CC_RUN(test_flag_float_registered);
    CC_RUN(test_flag_unknown_returns_null);
    CC_RUN(test_flag_shorthand_lookup);
    CC_RUN(test_flag_shorthand_unknown_returns_null);
    CC_RUN(test_parse_long_bool_flag);
    CC_RUN(test_parse_short_bool_flag);
    CC_RUN(test_parse_long_string_flag);
    CC_RUN(test_parse_equals_syntax);
    CC_RUN(test_parse_short_int_flag);
    CC_RUN(test_parse_float_flag);
    CC_RUN(test_parse_unknown_flag_returns_error);
    CC_RUN(test_parse_flag_default_unchanged);
    CC_RUN(test_flag_changed_after_set);
    CC_RUN(test_persistent_flag_inherited_by_child);
    CC_RUN(test_persistent_flag_parsed_on_child);
    CC_RUN(test_persistent_string_flag_value);
    CC_RUN(test_required_flag_missing_returns_error);
    CC_RUN(test_required_flag_present_succeeds);
    CC_RUN(test_mutually_exclusive_both_set_returns_error);
    CC_RUN(test_mutually_exclusive_one_set_succeeds);
    CC_RUN(test_mutually_exclusive_none_set_succeeds);
    CC_RUN(test_required_together_both_set_succeeds);
    CC_RUN(test_required_together_only_one_set_returns_error);
    CC_RUN(test_required_together_neither_set_succeeds);
    CC_RUN(test_one_required_first_set_succeeds);
    CC_RUN(test_one_required_second_set_succeeds);
    CC_RUN(test_one_required_none_set_returns_error);
    CC_RUN(test_alias_dispatches_correctly);
    CC_RUN(test_alias_with_flags);
    CC_RUN(test_multiple_aliases);
    CC_RUN(test_alias_count_correct);
    CC_RUN(test_pre_run_called_before_run);
    CC_RUN(test_post_run_called_after_run);
    CC_RUN(test_persistent_pre_run_fires_on_child);
    CC_RUN(test_persistent_post_run_fires_on_child);
    CC_RUN(test_all_hooks_fire_in_order);
    CC_RUN(test_min_args_not_met_returns_error);
    CC_RUN(test_max_args_exceeded_returns_error);
    CC_RUN(test_exact_args_succeeds);
    CC_RUN(test_unlimited_args_succeeds);
    CC_RUN(test_set_version_stored);
    CC_RUN(test_version_flag_returns_zero);

    CC_COMPLETE;
}
