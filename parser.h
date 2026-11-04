/* parser.h
 *
 * Parsing of the messages xmonad.hs sends to the applet. Kept free of GTK,
 * X and D-Bus so that it can be unit tested on its own.
 *
 * See LICENSE for licensing information
 */

#ifndef XMONAD_LOG_APPLET_PARSER_H
#define XMONAD_LOG_APPLET_PARSER_H

#include <glib.h>

// A message is a list of workspaces, then '|' and the window title:
//
//     <marker><number>:<xid>,<xid>,...;<marker><number>:...;|<title>
//
// where <marker> is one of the workspace state markers (see main.c) and the
// window ids may be empty. Example: "*1:1234,5678;-2:;|Terminal"

typedef struct {
    char marker;        // workspace state marker, e.g. '*'
    long id;            // workspace number, never 0
    GArray *windows;    // window ids (long), in the order sent
} XlaWorkspace;

typedef struct {
    GPtrArray *workspaces;  // XlaWorkspace *, the well-formed workspaces
    char *title;            // the window title, "" if there is none
    char *problems;         // "" if the message was well-formed, otherwise
                            // a description of each workspace left out
} XlaMessage;

// Parse a message. Workspaces whose marker isn't in markers, whose number
// isn't a non-zero number, or whose window list is malformed are left out
// and described in problems; the rest of the message is still parsed.
//
// Never returns NULL. The result is newly allocated and owned by the caller,
// who must free it with xla_message_free. It holds its own copies of
// everything, so msg and markers may be freed while it is still in use.
XlaMessage *xla_message_parse(const char *msg, const char *markers);

// Free a message from xla_message_parse, including its workspaces, their
// window arrays, the title and the problems. Does nothing if message is
// NULL.
void xla_message_free(XlaMessage *message);

#endif
