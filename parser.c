/* parser.c
 *
 * See parser.h. See LICENSE for licensing information
 */

#include <stdlib.h>
#include <string.h>

#include "parser.h"

static void workspace_free(gpointer data)
{
    XlaWorkspace *ws = data;
    g_array_free(ws->windows, TRUE);
    g_free(ws);
}

static XlaWorkspace *workspace_new(char marker, long id)
{
    XlaWorkspace *ws = g_new0(XlaWorkspace, 1);
    ws->marker = marker;
    ws->id = id;
    ws->windows = g_array_new(FALSE, FALSE, sizeof(long));
    return ws;
}

XlaMessage *xla_message_parse(const char *msg, const char *markers)
{
    XlaMessage *message = g_new0(XlaMessage, 1);
    message->workspaces = g_ptr_array_new_with_free_func(workspace_free);
    GString *problems = g_string_new(NULL);

    char *m = (char *)msg;
    while (*m != 0) {
        if (*m == '|') {
            m++;
            break;
        }

        char *ws_start = m;
        const char *problem = NULL;
        XlaWorkspace *ws = NULL;

        char marker = *m;
        if (strchr(markers, marker) == NULL) {
            problem = "unknown workspace state marker";
            goto skip_workspace;
        }
        m++;

        char *id_start = m;
        long workspace_id = strtol(m, &m, 0);

        if (m == id_start || *m != ':' || workspace_id == 0) {
            problem = "workspace name is not a non-zero number";
            goto skip_workspace;
        }
        m++;

        ws = workspace_new(marker, workspace_id);

        while (*m != 0) {
            long xid = strtol(m, &m, 0);

            if (xid != 0)
                g_array_append_val(ws->windows, xid);

            if (*m == ';') {
                m++;
                break;
            }
            if (*m != ',') {
                if (*m != 0) {
                    problem = "malformed window list";
                    goto skip_workspace;
                }
                break;
            }
            m++;
        }
        g_ptr_array_add(message->workspaces, ws);
        continue;

    skip_workspace:
        // Describe the offending workspace, then continue after it. The
        // list ends with ';' before the title's '|'.
        if (ws != NULL)
            workspace_free(ws);
        m = ws_start;
        while (*m != 0 && *m != ';' && *m != '|')
            m++;
        g_string_append_printf(problems, "%s%s: \"%.*s\"",
                               problems->len > 0 ? "; " : "", problem,
                               (int)(m - ws_start), ws_start);
        if (*m == ';')
            m++;
    }

    message->title = g_strdup(m);
    message->problems = g_string_free(problems, FALSE);
    return message;
}

void xla_message_free(XlaMessage *message)
{
    if (message == NULL)
        return;
    g_ptr_array_free(message->workspaces, TRUE);
    g_free(message->title);
    g_free(message->problems);
    g_free(message);
}
