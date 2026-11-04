/* test-parser.c
 *
 * Unit tests for the message parser. See LICENSE for licensing information
 */

#include <glib.h>

#include "../parser.h"

#define MARKERS "*+-!"

// The parsed message in a compact form, e.g. "*1[12,34] -2[] |Title"
static char *describe(const XlaMessage *message)
{
    GString *s = g_string_new(NULL);

    for (guint i = 0; i < message->workspaces->len; i++) {
        const XlaWorkspace *ws = g_ptr_array_index(message->workspaces, i);
        g_string_append_printf(s, "%c%ld[", ws->marker, ws->id);
        for (guint j = 0; j < ws->windows->len; j++)
            g_string_append_printf(s, "%s%ld", j > 0 ? "," : "",
                                   g_array_index(ws->windows, long, j));
        g_string_append(s, "] ");
    }
    g_string_append_printf(s, "|%s", message->title);
    return g_string_free(s, FALSE);
}

// Parse msg and check the result against the expected description and
// problems ("" for none)
static void check(const char *msg, const char *expected, const char *expected_problems)
{
    XlaMessage *message = xla_message_parse(msg, MARKERS);
    char *description = describe(message);

    g_assert_cmpstr(description, ==, expected);
    g_assert_cmpstr(message->problems, ==, expected_problems);

    g_free(description);
    xla_message_free(message);
}

// Well-formed messages

static void test_typical(void)
{
    check("*1:12345,678;-2:;+3:999;|Title",
          "*1[12345,678] -2[] +3[999] |Title", "");
}

static void test_all_markers(void)
{
    check("*1:1;+2:2;-3:3;!4:4;|t", "*1[1] +2[2] -3[3] !4[4] |t", "");
}

static void test_empty_message(void)
{
    check("", "|", "");
}

static void test_title_only(void)
{
    check("|Just a title", "|Just a title", "");
}

static void test_no_title(void)
{
    // As sent when no window is focused
    check("*1:;|", "*1[] |", "");
    // No '|' at all
    check("*1:5;-2:6;", "*1[5] -2[6] |", "");
}

static void test_title_with_separators(void)
{
    // Everything after the first '|' is the title, as is
    check("*1:5;|a|b;c:d,e", "*1[5] |a|b;c:d,e", "");
}

static void test_utf8_title(void)
{
    check("*1:5;|Ünïcödé — タイトル", "*1[5] |Ünïcödé — タイトル", "");
}

static void test_last_workspace_without_separator(void)
{
    // The list may end without ';' when there is no title part
    check("*1:5,6", "*1[5,6] |", "");
}

static void test_window_ids(void)
{
    // Large X ids, as decimal (what xmonad.hs sends) or hex
    check("*1:4294967295,0x1a00003;|t", "*1[4294967295,27262979] |t", "");
    // A zero id is not a window
    check("*1:0,7;|t", "*1[7] |t", "");
}

static void test_multi_digit_workspace(void)
{
    check("*12:1;-345:;|t", "*12[1] -345[] |t", "");
}

// Malformed workspaces are left out and reported; the rest is kept

static void test_unknown_marker(void)
{
    check("*1:5;?2:6;-3:;|t", "*1[5] -3[] |t",
          "unknown workspace state marker: \"?2:6\"");
}

static void test_non_numeric_name(void)
{
    check("*1:5;-web:6,7;-2:;|t", "*1[5] -2[] |t",
          "workspace name is not a non-zero number: \"-web:6,7\"");
}

static void test_zero_workspace(void)
{
    check("*0:5;-2:;|t", "-2[] |t",
          "workspace name is not a non-zero number: \"*0:5\"");
}

static void test_missing_colon(void)
{
    check("*1;-2:;|t", "-2[] |t",
          "workspace name is not a non-zero number: \"*1\"");
}

static void test_name_with_trailing_text(void)
{
    check("*1a:5;|t", "|t",
          "workspace name is not a non-zero number: \"*1a:5\"");
}

static void test_malformed_window_list(void)
{
    // The whole workspace is left out, including the windows before the error
    check("*1:5;-2:6,7x,8;-3:;|t", "*1[5] -3[] |t",
          "malformed window list: \"-2:6,7x,8\"");
}

static void test_missing_separator_before_title(void)
{
    check("*1:5;-2:6|t", "*1[5] |t", "malformed window list: \"-2:6\"");
}

static void test_several_problems(void)
{
    check("*1:5;?4:1;-2:1x;-5:7|Title", "*1[5] |Title",
          "unknown workspace state marker: \"?4:1\"; "
          "malformed window list: \"-2:1x\"; "
          "malformed window list: \"-5:7\"");
}

static void test_old_style_message(void)
{
    // A Pango markup message, as sent by configs written for other versions
    // of the applet: nothing usable, but no crash, and the problem is named
    check("<span>1</span> 2 3", "|",
          "unknown workspace state marker: \"<span>1</span> 2 3\"");
}

static void test_custom_markers(void)
{
    XlaMessage *message = xla_message_parse("*1:;#2:;|t", "#");
    char *description = describe(message);
    g_assert_cmpstr(description, ==, "#2[] |t");
    g_assert_cmpstr(message->problems, ==, "unknown workspace state marker: \"*1:\"");
    g_free(description);
    xla_message_free(message);
}

static void test_free_null(void)
{
    xla_message_free(NULL);
}

int main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/parser/typical", test_typical);
    g_test_add_func("/parser/all-markers", test_all_markers);
    g_test_add_func("/parser/empty-message", test_empty_message);
    g_test_add_func("/parser/title-only", test_title_only);
    g_test_add_func("/parser/no-title", test_no_title);
    g_test_add_func("/parser/title-with-separators", test_title_with_separators);
    g_test_add_func("/parser/utf8-title", test_utf8_title);
    g_test_add_func("/parser/last-workspace-without-separator", test_last_workspace_without_separator);
    g_test_add_func("/parser/window-ids", test_window_ids);
    g_test_add_func("/parser/multi-digit-workspace", test_multi_digit_workspace);

    g_test_add_func("/parser/malformed/unknown-marker", test_unknown_marker);
    g_test_add_func("/parser/malformed/non-numeric-name", test_non_numeric_name);
    g_test_add_func("/parser/malformed/zero-workspace", test_zero_workspace);
    g_test_add_func("/parser/malformed/missing-colon", test_missing_colon);
    g_test_add_func("/parser/malformed/name-with-trailing-text", test_name_with_trailing_text);
    g_test_add_func("/parser/malformed/window-list", test_malformed_window_list);
    g_test_add_func("/parser/malformed/missing-separator-before-title", test_missing_separator_before_title);
    g_test_add_func("/parser/malformed/several-problems", test_several_problems);
    g_test_add_func("/parser/malformed/old-style-message", test_old_style_message);

    g_test_add_func("/parser/custom-markers", test_custom_markers);
    g_test_add_func("/parser/free-null", test_free_null);

    return g_test_run();
}
