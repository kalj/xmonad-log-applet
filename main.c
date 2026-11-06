/* main.c
 *
 * Copyright (c) 2009 Adam Wick
 * Copyright (c) 2011-2012 Alexander Kojevnikov
 * Copyright (c) 2011 Dan Callaghan
 * Copyright (c) 2012 Ari Croock
 * Copyright (c) 2013 Ziga Ham
 *
 * See LICENSE for licensing information
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <xcb/xcb_ewmh.h>

#include "parser.h"

// GSettings schema with the appearance settings and their defaults. It is
// installed and used for all panels, which store per-applet settings with it.
#define XMONAD_LOG_APPLET_SCHEMA "org.xmonad.log-applet"

#ifdef PANEL_GNOME
#include <libgnome-panel/gp-applet.h>
#include <libgnome-panel/gp-module.h>
#endif
#ifdef PANEL_MATE
#include <mate-panel-applet.h>
#include <mate-panel-applet-gsettings.h>
#endif
#ifdef PANEL_XFCE4
#include <libxfce4panel/libxfce4panel.h>
#endif

// Misc
static void pixbuff_free(guchar *pixels, gpointer data) {
    free(pixels);
}

// XCB
// One X connection, shared by every applet instance in the process. It is
// set up by xcb_init and is only usable while xcb_ready is TRUE.
static xcb_connection_t *c = NULL;
static xcb_ewmh_connection_t ewmh;
static int screen_number;
static gboolean xcb_ready = FALSE;

// Connect to X and fetch the EWMH atoms, unless that has already been done.
// Returns TRUE if the connection is usable. On failure everything is cleaned
// up, so that a later call (e.g. from another applet instance) can retry.
static gboolean xcb_init(void) {
    xcb_intern_atom_cookie_t *atom_cookie;

    if (xcb_ready)
        return TRUE;

    c = xcb_connect(NULL, &screen_number);
    if (c == NULL || xcb_connection_has_error(c)) {
        g_warning("Can not connect to the X server; window icons will not be shown");
        goto fail;
    }

    atom_cookie = xcb_ewmh_init_atoms(c, &ewmh);
    if (atom_cookie == NULL) {
        g_warning("Can not request EWMH atoms; window icons will not be shown");
        goto fail;
    }

    // On failure this frees what xcb_ewmh_init_atoms allocated
    if (!xcb_ewmh_init_atoms_replies(&ewmh, atom_cookie, NULL)) {
        g_warning("Can not get EWMH atom replies; window icons will not be shown");
        goto fail;
    }

    xcb_ready = TRUE;
    return TRUE;

fail:
    if (c != NULL)
        xcb_disconnect(c);
    c = NULL;
    return FALSE;
}

// A generic icon for windows that have none (or when X is not available)
static GtkWidget *fallback_icon(void)
{
    static const char *names[] = { "application-x-executable", "image-missing" };
    GtkIconTheme *theme = gtk_icon_theme_get_default();

    for (guint i = 0; i < G_N_ELEMENTS(names); i++) {
        GdkPixbuf *pixbuf = gtk_icon_theme_load_icon(theme, names[i], 20,
                                                     GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
        if (pixbuf != NULL) {
            GtkWidget *widget = gtk_image_new_from_pixbuf(pixbuf);
            g_object_unref(G_OBJECT(pixbuf));
            return widget;
        }
    }
    return gtk_image_new_from_icon_name(names[0], GTK_ICON_SIZE_MENU);
}

// Get icon using XCB. Windows without an icon (which is perfectly normal)
// get a generic one, so that they still show up and can be clicked.
static GtkWidget* get_icon(long xid)
{
    xcb_window_t w = xid;
    xcb_get_property_cookie_t prop_cookie;
    xcb_ewmh_get_wm_icon_reply_t wm_icon;
    int i;

    if (!xcb_ready)
        return fallback_icon();

    prop_cookie = xcb_ewmh_get_wm_icon(&ewmh, w);
    if (!xcb_ewmh_get_wm_icon_reply(&ewmh, prop_cookie, &wm_icon, NULL))
        return fallback_icon();

    // Take the first icon (sometimes the icon is in multiple resolutions)
    xcb_ewmh_wm_icon_iterator_t iterator = xcb_ewmh_get_wm_icon_iterator(&wm_icon);

    if (iterator.data == NULL || iterator.width == 0 || iterator.height == 0) {
        // No usable icon
        xcb_ewmh_get_wm_icon_reply_wipe(&wm_icon);
        return fallback_icon();
    }

    // RGBA <-> BGRA
    char* data_source = (char*)iterator.data;
    guchar* data = malloc(iterator.width * iterator.height * 4);
    for (i = 0; i < iterator.width * iterator.height; i++) {
        data[4 * i + 0] = data_source[4 * i + 2];
        data[4 * i + 1] = data_source[4 * i + 1];
        data[4 * i + 2] = data_source[4 * i + 0];
        data[4 * i + 3] = data_source[4 * i + 3];
    }

    // Scale the icon
    GdkPixbuf *pixbuf_orig = gdk_pixbuf_new_from_data(data, GDK_COLORSPACE_RGB, TRUE, 8, iterator.width, iterator.height, iterator.width * 4, pixbuff_free, NULL);
    GdkPixbuf *pixbuf = gdk_pixbuf_scale_simple(pixbuf_orig, 20, 20, GDK_INTERP_BILINEAR);
    g_object_unref(G_OBJECT(pixbuf_orig)); // original data is not used anymore now

    // Create GtkImage from GdkPixbuf
    GtkWidget *widget = gtk_image_new_from_pixbuf(pixbuf);
    g_object_unref(G_OBJECT(pixbuf)); // GtkImage doesn't assume our reference

    // Cleanup
    xcb_ewmh_get_wm_icon_reply_wipe(&wm_icon);

    return widget;
}

// Change active window on a left click on its icon. Other buttons (and
// double clicks) are not handled, so e.g. a right click still reaches the
// panel and opens its menu.
static gboolean icon_click(GtkWidget *widget, GdkEventButton *event, gpointer xid) {
    if (event->type != GDK_BUTTON_PRESS || event->button != 1)
        return FALSE;
    if (!xcb_ready)
        return FALSE;

    xcb_ewmh_request_change_active_window(&ewmh, screen_number, (xcb_window_t)GPOINTER_TO_SIZE(xid), XCB_EWMH_CLIENT_SOURCE_TYPE_OTHER, XCB_CURRENT_TIME, XCB_NONE);
    xcb_flush(c);
    return TRUE;
}

// Workspace states as marked by xmonad.hs, the GSettings key holding each
// state's colour (whose default is in the schema), and its dialog label
static const struct {
    char marker;
    const char *key;
    const char *label;
} workspace_states[] = {
    { '*', "color-current", "_Current:" },
    { '+', "color-visible", "_Visible:" },
    { '-', "color-hidden",  "_Hidden:"  },
    { '!', "color-urgent",  "_Urgent:"  },
};

// All the state markers, for the parser
static const char *state_markers(void)
{
    static char markers[G_N_ELEMENTS(workspace_states) + 1];

    if (markers[0] == 0)
        for (guint i = 0; i < G_N_ELEMENTS(workspace_states); i++)
            markers[i] = workspace_states[i].marker;
    return markers;
}

// Index into workspace_states for a state marker, or -1 if it is unknown
static int workspace_state(char marker)
{
    for (guint i = 0; i < G_N_ELEMENTS(workspace_states); i++)
        if (workspace_states[i].marker == marker)
            return i;
    return -1;
}

// The appearance schema, looked up once. NULL if it is not installed.
static GSettingsSchema *appearance_schema(void)
{
    static GSettingsSchema *schema = NULL;
    static gboolean looked_up = FALSE;

    if (!looked_up) {
        GSettingsSchemaSource *source = g_settings_schema_source_get_default();
        if (source != NULL)
            schema = g_settings_schema_source_lookup(source, XMONAD_LOG_APPLET_SCHEMA, TRUE);
        if (schema == NULL)
            g_warning("GSettings schema %s not found, using the theme's look",
                      XMONAD_LOG_APPLET_SCHEMA);
        looked_up = TRUE;
    }
    return schema;
}

// The schema default for an appearance key, or NULL if the schema is missing
static GVariant *schema_default_value(const char *key)
{
    GSettingsSchema *schema = appearance_schema();
    if (schema == NULL)
        return NULL;

    GSettingsSchemaKey *schema_key = g_settings_schema_get_key(schema, key);
    GVariant *value = g_settings_schema_key_get_default_value(schema_key);
    g_settings_schema_key_unref(schema_key);
    return value;
}

// Parse a colour stored as a string ("red", "#3465a4", ...)
static gboolean parse_color_variant(GVariant *variant, GdkColor *color)
{
    return gdk_color_parse(g_variant_get_string(variant, NULL), color);
}

// The colour for a workspace state: the user's setting if there is a valid
// one, otherwise the schema default. Returns FALSE if neither is available,
// in which case the theme's text colour should be used.
static gboolean workspace_color(GtkWidget *container, int state, GdkColor *color)
{
    const char *key = workspace_states[state].key;
    gboolean found = FALSE;

    GSettings *settings = g_object_get_data(G_OBJECT(container), "settings");
    if (settings != NULL) {
        GVariant *value = g_settings_get_value(settings, key);
        found = parse_color_variant(value, color);
        g_variant_unref(value);
    }

    if (!found) {
        GVariant *value = schema_default_value(key);
        if (value != NULL) {
            found = parse_color_variant(value, color);
            g_variant_unref(value);
        }
    }
    return found;
}

// An appearance setting: the user's setting where there are settings,
// otherwise the schema default. NULL if neither is available.
static GVariant *appearance_value(GtkWidget *container, const char *key)
{
    GSettings *settings = g_object_get_data(G_OBJECT(container), "settings");
    if (settings != NULL)
        return g_settings_get_value(settings, key);
    return schema_default_value(key);
}

// A boolean appearance setting, FALSE if it is not available
static gboolean appearance_boolean(GtkWidget *container, const char *key)
{
    GVariant *value = appearance_value(container, key);
    if (value == NULL)
        return FALSE;
    gboolean result = g_variant_get_boolean(value);
    g_variant_unref(value);
    return result;
}

// An integer appearance setting, 0 if it is not available
static gint appearance_int(GtkWidget *container, const char *key)
{
    GVariant *value = appearance_value(container, key);
    if (value == NULL)
        return 0;
    gint result = g_variant_get_int32(value);
    g_variant_unref(value);
    return result;
}

// A label showing text as is (no markup, so window titles need no
// escaping), optionally in bold
static GtkWidget *text_label(const char *text, gboolean bold)
{
    GtkWidget *label = gtk_label_new(text);
    if (bold) {
        PangoAttrList *attrs = pango_attr_list_new();
        pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
        gtk_label_set_attributes(GTK_LABEL(label), attrs);
        pango_attr_list_unref(attrs);
    }
    return label;
}

// Show a message from xmonad
static void signal_handler(const char *msg, GtkWidget *container)
{
    // Remember the message, so it can be redrawn when the appearance changes
    if (msg != g_object_get_data(G_OBJECT(container), "last-msg")) {
        char *copy = g_strdup(msg);
        g_object_set_data_full(G_OBJECT(container), "last-msg", copy, g_free);
        msg = copy;
    }

    // Clear container
    GList *children, *iter;
    children = gtk_container_get_children(GTK_CONTAINER(container));
    for(iter = children; iter != NULL; iter = g_list_next(iter)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);

    gboolean bold_numbers = appearance_boolean(container, "bold-numbers");
    gboolean bold_title = appearance_boolean(container, "bold-title");
    gint title_max_width = appearance_int(container, "title-max-width");

    // Parse msg and repopulate container. A malformed workspace is left out
    // (and reported), rather than throwing away the whole update.
    XlaMessage *message = xla_message_parse(msg, state_markers());

    for (guint i = 0; i < message->workspaces->len; i++) {
        XlaWorkspace *ws = g_ptr_array_index(message->workspaces, i);

        GtkWidget *box = gtk_hbox_new(FALSE,2);
        gtk_container_add(GTK_CONTAINER(container), box);

        char str[24];
        snprintf(str, sizeof(str), "%ld:", ws->id);
        GtkWidget *label = text_label(str, bold_numbers);
        GdkColor color;
        if (workspace_color(container, workspace_state(ws->marker), &color))
            gtk_widget_modify_fg(GTK_WIDGET(label), GTK_STATE_NORMAL, &color);
        gtk_container_add(GTK_CONTAINER(box), label);

        for (guint j = 0; j < ws->windows->len; j++) {
            long xid = g_array_index(ws->windows, long, j);
            GtkWidget *icon = get_icon(xid);
            GtkWidget *eventbox = gtk_event_box_new();
            gtk_event_box_set_visible_window(GTK_EVENT_BOX(eventbox), FALSE);
            gtk_container_add(GTK_CONTAINER(eventbox), icon);
            gtk_container_add(GTK_CONTAINER(box), eventbox);
            g_signal_connect(G_OBJECT(eventbox), "button_press_event", G_CALLBACK(icon_click), GSIZE_TO_POINTER(xid));
        }
    }

    // Report problems, but only when they change, to avoid flooding the log
    // with the same complaint on every update
    const char *problems = message->problems;
    const char *last_problems = g_object_get_data(G_OBJECT(container), "last-problems");
    if (g_strcmp0(problems, last_problems ? last_problems : "") != 0) {
        if (problems[0] != 0)
            g_warning("Ignoring malformed workspaces in message from xmonad (%s). "
                      "See xmonad.hs for the expected format.", problems);
        g_object_set_data_full(G_OBJECT(container), "last-problems",
                               g_strdup(problems), g_free);
    }

    // Window title. A title longer than the limit is shortened with "...",
    // keeping the limit's width: an ellipsized label may otherwise shrink to
    // just the "...". A limited title doesn't expand into free space either.
    const char *title = message->title;
    GtkWidget *label = text_label(title, bold_title);
    if (title_max_width > 0) {
        if (g_utf8_strlen(title, -1) > title_max_width) {
            gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
            gtk_label_set_width_chars(GTK_LABEL(label), title_max_width);
            gtk_label_set_max_width_chars(GTK_LABEL(label), title_max_width);
        }
        gtk_box_pack_start(GTK_BOX(container), label, FALSE, FALSE, 0);
    } else {
        gtk_container_add(GTK_CONTAINER(container), label);
    }

    xla_message_free(message);

    gtk_widget_show_all(container);
}

// DBUS
// An "Update" signal from xmonad.hs, carrying the message as a string
static void on_update(GDBusConnection *connection, const gchar *sender,
                      const gchar *object_path, const gchar *interface_name,
                      const gchar *signal_name, GVariant *parameters,
                      gpointer container)
{
    if (!g_variant_is_of_type(parameters, G_VARIANT_TYPE("(s)"))) {
        g_warning("Ignoring an Update signal from xmonad with arguments of type %s",
                  g_variant_get_type_string(parameters));
        return;
    }

    const gchar *msg;
    g_variant_get(parameters, "(&s)", &msg);
    signal_handler(msg, GTK_WIDGET(container));
}

typedef struct {
    GDBusConnection *connection;
    guint id;
} Subscription;

// Stop listening when the container goes away, so that no message is
// shown in a destroyed container
static void unsubscribe(GtkWidget *container, gpointer data)
{
    Subscription *subscription = data;
    g_dbus_connection_signal_unsubscribe(subscription->connection, subscription->id);
    g_object_unref(subscription->connection);
    g_free(subscription);
}

// Show xmonad's messages in container. Only signals sent by the owner of the
// name org.xmonad.Log, which xmonad.hs takes, are accepted.
static void set_up_dbus_transfer(GtkWidget *container)
{
    GError *error = NULL;
    GDBusConnection *connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (connection == NULL) {
        g_printerr("Failed to open connection: %s\n", error->message);
        g_error_free(error);
        #if defined(PANEL_GNOME) || defined(PANEL_XFCE4)
        // A module can't exit: that would take down the process loading it
        return;
        #else
        exit(-1);
        #endif
    }

    Subscription *subscription = g_new(Subscription, 1);
    subscription->connection = connection;
    subscription->id = g_dbus_connection_signal_subscribe(
        connection, "org.xmonad.Log", "org.xmonad.Log", "Update", "/org/xmonad/Log",
        NULL, G_DBUS_SIGNAL_FLAGS_NONE, on_update, container, NULL);
    g_signal_connect(container, "destroy", G_CALLBACK(unsubscribe), subscription);
}

// Appearance settings and the preferences dialog. Each applet has its own
// settings: the applet widget holds its GSettings object as "settings", and
// the open dialog as "prefs-dialog".

// Apply the "expand" setting: let the applet fill the free space along the
// panel, or not. Not used on GNOME: gnome-panel grows an applet with
// GP_APPLET_FLAGS_EXPAND_MAJOR only up to its natural size, so the setting
// would have no visible effect there.
#ifndef PANEL_GNOME
static void apply_expand_setting(GSettings *settings, const gchar *key, gpointer applet)
{
    gboolean expand = g_settings_get_boolean(settings, "expand");
#ifdef PANEL_MATE
    MatePanelAppletFlags flags = mate_panel_applet_get_flags(MATE_PANEL_APPLET(applet));

    if (expand)
        flags |= MATE_PANEL_APPLET_EXPAND_MAJOR;
    else
        flags &= ~MATE_PANEL_APPLET_EXPAND_MAJOR;

    mate_panel_applet_set_flags(MATE_PANEL_APPLET(applet), flags);
#endif
#ifdef PANEL_XFCE4
    xfce_panel_plugin_set_expand(XFCE_PANEL_PLUGIN(applet), expand);
#endif
}
#endif

// Redraw the workspaces with the last message when a setting that affects
// them changes (all settings but "expand" do)
static void redraw_workspaces(GSettings *settings, const gchar *key, gpointer container)
{
    if (g_strcmp0(key, "expand") == 0)
        return;

    const char *msg = g_object_get_data(G_OBJECT(container), "last-msg");
    if (msg != NULL)
        signal_handler(msg, GTK_WIDGET(container));
}

// Colours are stored as strings ("red", "#3465a4", ...) and shown as GdkColor
static gboolean color_from_setting(GValue *value, GVariant *variant, gpointer user_data)
{
    GdkColor color;
    if (!gdk_color_parse(g_variant_get_string(variant, NULL), &color))
        return FALSE;   // GSettings then falls back to the schema default
    g_value_set_boxed(value, &color);
    return TRUE;
}

static GVariant *color_to_setting(const GValue *value, const GVariantType *type, gpointer user_data)
{
    const GdkColor *color = g_value_get_boxed(value);
    gchar *str = g_strdup_printf("#%02x%02x%02x",
                                 color->red >> 8, color->green >> 8, color->blue >> 8);
    GVariant *variant = g_variant_new_string(str);
    g_free(str);
    return variant;
}

// A "<label> [colour button]" row in the preferences dialog, bound to a key
static GtkWidget *color_row(GSettings *settings, GtkSizeGroup *labels,
                            const char *label_text, const char *key)
{
    GtkWidget *row = gtk_hbox_new(FALSE, 12);

    GtkWidget *label = gtk_label_new_with_mnemonic(label_text);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_size_group_add_widget(labels, label);
    gtk_box_pack_start(GTK_BOX(row), label, FALSE, FALSE, 0);

    GtkWidget *button = gtk_color_button_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), button);
    g_settings_bind_with_mapping(settings, key, button, "color", G_SETTINGS_BIND_DEFAULT,
                                 color_from_setting, color_to_setting, NULL, NULL);
    gtk_box_pack_start(GTK_BOX(row), button, FALSE, FALSE, 0);

    return row;
}

// "Reset Colours": put every workspace colour back to its schema default
static void reset_colors(GtkButton *button, gpointer settings)
{
    for (guint i = 0; i < G_N_ELEMENTS(workspace_states); i++)
        g_settings_reset(G_SETTINGS(settings), workspace_states[i].key);
}

// Only offer the reset when some colour differs from its default
static void update_reset_colors(GSettings *settings, const gchar *key, gpointer button)
{
    gboolean changed = FALSE;

    for (guint i = 0; i < G_N_ELEMENTS(workspace_states); i++) {
        GVariant *value = g_settings_get_value(settings, workspace_states[i].key);
        GVariant *default_value = g_settings_get_default_value(settings, workspace_states[i].key);
        changed |= !g_variant_equal(value, default_value);
        g_variant_unref(value);
        g_variant_unref(default_value);
    }
    gtk_widget_set_sensitive(GTK_WIDGET(button), changed);
}

// A titled section in the preferences dialog: a bold heading followed by
// an indented box, which is returned for the section's rows
static GtkWidget *dialog_section(GtkWidget *vbox, const char *title)
{
    GtkWidget *heading = gtk_label_new(NULL);
    gchar *markup = g_markup_printf_escaped("<b>%s</b>", title);
    gtk_label_set_markup(GTK_LABEL(heading), markup);
    g_free(markup);
    gtk_misc_set_alignment(GTK_MISC(heading), 0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), heading, FALSE, FALSE, 6);

    GtkWidget *indent = gtk_hbox_new(FALSE, 0);
    GtkWidget *rows = gtk_vbox_new(FALSE, 6);
    gtk_box_pack_start(GTK_BOX(indent), rows, FALSE, FALSE, 12);
    gtk_box_pack_start(GTK_BOX(vbox), indent, FALSE, FALSE, 0);
    return rows;
}

// A check button bound to a boolean key
static GtkWidget *check_row(GSettings *settings, const char *label_text,
                            const char *tooltip, const char *key)
{
    GtkWidget *check = gtk_check_button_new_with_mnemonic(label_text);
    gtk_widget_set_tooltip_text(check, tooltip);
    g_settings_bind(settings, key, check, "active", G_SETTINGS_BIND_DEFAULT);
    return check;
}

// "<label> [spin button] <unit>" bound to an integer key, with the range
// taken from the schema
static GtkWidget *spin_row(GSettings *settings, const char *label_text, const char *unit,
                           const char *tooltip, const char *key)
{
    gint min = 0, max = G_MAXINT;
    GSettingsSchemaKey *schema_key = g_settings_schema_get_key(appearance_schema(), key);
    GVariant *range = g_settings_schema_key_get_range(schema_key);
    const gchar *range_type;
    GVariant *limits;
    g_variant_get(range, "(&sv)", &range_type, &limits);
    if (strcmp(range_type, "range") == 0)
        g_variant_get(limits, "(ii)", &min, &max);
    g_variant_unref(limits);
    g_variant_unref(range);
    g_settings_schema_key_unref(schema_key);

    GtkWidget *row = gtk_hbox_new(FALSE, 6);
    gtk_widget_set_tooltip_text(row, tooltip);

    GtkWidget *label = gtk_label_new_with_mnemonic(label_text);
    gtk_box_pack_start(GTK_BOX(row), label, FALSE, FALSE, 0);

    GtkWidget *spin = gtk_spin_button_new_with_range(min, max, 1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), spin);
    g_settings_bind(settings, key, spin, "value", G_SETTINGS_BIND_DEFAULT);
    gtk_box_pack_start(GTK_BOX(row), spin, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(row), gtk_label_new(unit), FALSE, FALSE, 0);
    return row;
}

static void preferences_destroyed(GObject *applet, GtkWidget *dialog)
{
    g_object_set_data(applet, "prefs-dialog", NULL);
#ifdef PANEL_XFCE4
    xfce_panel_plugin_unblock_menu(XFCE_PANEL_PLUGIN(applet));
#endif
}

// Open the preferences dialog, or raise it if it is already open. Changes
// are written to GSettings, and so applied, as soon as they are made.
static void open_preferences(GtkWidget *applet)
{
    GtkWidget *dialog = g_object_get_data(G_OBJECT(applet), "prefs-dialog");
    if (dialog != NULL) {
        gtk_window_present(GTK_WINDOW(dialog));
        return;
    }

    GSettings *settings = g_object_get_data(G_OBJECT(applet), "settings");

    dialog = gtk_dialog_new_with_buttons(
        "Xmonad Log Applet Preferences", NULL, 0,
        "_Close", GTK_RESPONSE_CLOSE,
        NULL);
    gtk_window_set_screen(GTK_WINDOW(dialog), gtk_widget_get_screen(applet));
    gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);

    GtkWidget *vbox = gtk_vbox_new(FALSE, 6);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
    gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), vbox);

#ifndef PANEL_GNOME
    gtk_box_pack_start(GTK_BOX(vbox),
                       check_row(settings, "_Expand to fill panel",
                                 "Let the applet take up all free space along the panel",
                                 "expand"),
                       FALSE, FALSE, 0);
#endif

    // Workspace colours: one row per state, and a reset button
    GtkWidget *rows = dialog_section(vbox, "Workspace Colours");

    GtkSizeGroup *labels = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
    for (guint i = 0; i < G_N_ELEMENTS(workspace_states); i++)
        gtk_box_pack_start(GTK_BOX(rows),
                           color_row(settings, labels, workspace_states[i].label,
                                     workspace_states[i].key),
                           FALSE, FALSE, 0);
    g_object_unref(labels);

    GtkWidget *reset_row = gtk_hbox_new(FALSE, 0);
    GtkWidget *reset = gtk_button_new_with_mnemonic("_Reset Colours");
    gtk_widget_set_tooltip_text(reset, "Use the default colour for every workspace state");
    g_signal_connect(reset, "clicked", G_CALLBACK(reset_colors), settings);
    gtk_box_pack_start(GTK_BOX(reset_row), reset, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(rows), reset_row, FALSE, FALSE, 0);

    // Keep the button's sensitivity up to date while the dialog is open
    for (guint i = 0; i < G_N_ELEMENTS(workspace_states); i++) {
        gchar *signal = g_strconcat("changed::", workspace_states[i].key, NULL);
        g_signal_connect_object(settings, signal, G_CALLBACK(update_reset_colors), reset, 0);
        g_free(signal);
    }
    update_reset_colors(settings, NULL, reset);

    // Text style
    GtkWidget *text_rows = dialog_section(vbox, "Text");
    gtk_box_pack_start(GTK_BOX(text_rows),
                       check_row(settings, "_Bold workspace numbers",
                                 "Show the workspace numbers in bold", "bold-numbers"),
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(text_rows),
                       check_row(settings, "Bold window _title",
                                 "Show the title of the focused window in bold", "bold-title"),
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(text_rows),
                       spin_row(settings, "Maximum title _width:", "characters",
                                "Shorten longer window titles with \u2026. 0 means no limit.",
                                "title-max-width"),
                       FALSE, FALSE, 0);

    // Forget the dialog when it closes, and close it if the applet is removed
#ifdef PANEL_XFCE4
    // Xfce: grey out Properties while the dialog is open, as Xfce's own
    // plugins do
    xfce_panel_plugin_block_menu(XFCE_PANEL_PLUGIN(applet));
#endif
    g_object_set_data(G_OBJECT(applet), "prefs-dialog", dialog);
    g_signal_connect_object(dialog, "destroy", G_CALLBACK(preferences_destroyed),
                            applet, G_CONNECT_SWAPPED);
    g_signal_connect_object(applet, "destroy", G_CALLBACK(gtk_widget_destroy),
                            dialog, G_CONNECT_SWAPPED);

    gtk_widget_show_all(dialog);
}

// Keep settings with the applet, read them in signal_handler (via the
// container), redraw when they change and apply the "expand" setting (not
// on GNOME)
static void set_up_settings(GtkWidget *applet, GtkWidget *container, GSettings *settings)
{
    // Owned by the applet from here on
    g_object_set_data_full(G_OBJECT(applet), "settings", settings, g_object_unref);

    g_object_set_data(G_OBJECT(container), "settings", settings);
    g_signal_connect_object(settings, "changed", G_CALLBACK(redraw_workspaces), container, 0);

#ifndef PANEL_GNOME
    g_signal_connect(settings, "changed::expand", G_CALLBACK(apply_expand_setting), applet);
    apply_expand_setting(settings, "expand", applet);
#endif
}

#ifdef PANEL_MATE
// MATE: the settings are per applet instance, at a path the panel chooses,
// and the dialog is opened from a Preferences item in the applet's menu
static void show_preferences(GtkAction *action, gpointer applet)
{
    open_preferences(GTK_WIDGET(applet));
}

static const char menu_xml[] =
    "<menuitem name=\"Preferences\" action=\"Preferences\" />";

static const GtkActionEntry menu_actions[] = {
    { "Preferences", "document-properties", "_Preferences", NULL,
      "Configure the appearance of the applet", G_CALLBACK(show_preferences) },
};

static void set_up_settings_and_menu(MatePanelApplet *applet, GtkWidget *container)
{
    GSettings *settings = mate_panel_applet_settings_new(applet, XMONAD_LOG_APPLET_SCHEMA);
    set_up_settings(GTK_WIDGET(applet), container, settings);

    GtkActionGroup *actions = gtk_action_group_new("XmonadLogAppletActions");
    gtk_action_group_add_actions(actions, menu_actions, G_N_ELEMENTS(menu_actions), applet);
    mate_panel_applet_setup_menu(applet, menu_xml, actions);
    g_object_unref(actions);
}
#endif

#ifdef PANEL_XFCE4
// Xfce: the settings are per plugin instance, at a path made from the
// plugin's id, and the dialog is opened from the plugin menu's Properties
static void configure_plugin(XfcePanelPlugin *plugin, gpointer data)
{
    open_preferences(GTK_WIDGET(plugin));
}

// The plugin was removed from the panel for good: forget its settings
static void forget_settings(XfcePanelPlugin *plugin, gpointer data)
{
    GSettings *settings = g_object_get_data(G_OBJECT(plugin), "settings");
    gchar **keys = g_settings_schema_list_keys(appearance_schema());
    for (gchar **key = keys; *key != NULL; key++)
        g_settings_reset(settings, *key);
    g_strfreev(keys);
}

static void set_up_settings_and_menu(XfcePanelPlugin *plugin, GtkWidget *container)
{
    GSettingsSchema *schema = appearance_schema();
    if (schema == NULL) {
        // No settings to store or show: use the defaults
        xfce_panel_plugin_set_expand(plugin, TRUE);
        return;
    }

    gchar *path = g_strdup_printf("/org/xmonad/log-applet/xfce4-panel/plugin-%d/",
                                  xfce_panel_plugin_get_unique_id(plugin));
    GSettings *settings = g_settings_new_full(schema, NULL, path);
    g_free(path);
    set_up_settings(GTK_WIDGET(plugin), container, settings);

    xfce_panel_plugin_menu_show_configure(plugin);
    g_signal_connect(plugin, "configure-plugin", G_CALLBACK(configure_plugin), NULL);
    g_signal_connect(plugin, "removed", G_CALLBACK(forget_settings), NULL);
}
#endif

#ifdef PANEL_GNOME
// GNOME: the settings are per applet instance, at a path the panel chooses
// (and resets when the applet is removed), and the dialog is opened from a
// Preferences item in the applet's menu
#define APPLET_ID "xmonad-log-applet"

static void preferences_activated(GSimpleAction *action, GVariant *parameter, gpointer applet)
{
    open_preferences(GTK_WIDGET(applet));
}

static const GActionEntry menu_actions[] = {
    { .name = "preferences", .activate = preferences_activated },
    { NULL }
};

// gnome-panel looks for the menu "<applet id>-menu", whose actions are
// "<applet id>.<action>"
static const char menu_xml[] =
    "<interface>"
    "  <menu id=\"" APPLET_ID "-menu\">"
    "    <section>"
    "      <item>"
    "        <attribute name=\"label\">_Preferences</attribute>"
    "        <attribute name=\"action\">" APPLET_ID ".preferences</attribute>"
    "      </item>"
    "    </section>"
    "  </menu>"
    "</interface>";

static void set_up_settings_and_menu(GpApplet *applet, GtkWidget *container)
{
    // Without the schema there are no settings to store or show: keep the
    // defaults (gp_applet_settings_new would abort)
    if (appearance_schema() == NULL)
        return;

    GSettings *settings = gp_applet_settings_new(applet, XMONAD_LOG_APPLET_SCHEMA);
    set_up_settings(GTK_WIDGET(applet), container, settings);

    gp_applet_setup_menu(applet, menu_xml, menu_actions);

    // Like gnome-panel's own applets, offer no preferences while the panel
    // is locked down
    GAction *action = gp_applet_menu_lookup_action(applet, "preferences");
    g_object_bind_property(applet, "locked-down", action, "enabled",
                           G_BINDING_DEFAULT | G_BINDING_INVERT_BOOLEAN | G_BINDING_SYNC_CREATE);
}
#endif

#ifdef PANEL_GNOME
static gboolean xmonad_log_applet_fill(GpApplet *applet)
#endif
#ifdef PANEL_MATE
static gboolean xmonad_log_applet_fill(MatePanelApplet *applet)
#endif
#ifdef PANEL_XFCE4
static void xmonad_log_applet_fill(XfcePanelPlugin *plugin)
#endif
{
#ifdef PANEL_GNOME
    gp_applet_set_flags(
        applet,
        GP_APPLET_FLAGS_EXPAND_MAJOR |
        GP_APPLET_FLAGS_EXPAND_MINOR |
        GP_APPLET_FLAGS_HAS_HANDLE);
#endif
#ifdef PANEL_MATE
    // EXPAND_MAJOR is controlled by the "expand" setting
    mate_panel_applet_set_flags(
        applet,
        MATE_PANEL_APPLET_EXPAND_MINOR |
        MATE_PANEL_APPLET_HAS_HANDLE);

    mate_panel_applet_set_background_widget(applet, GTK_WIDGET(applet));
#endif

    GtkWidget *box = gtk_hbox_new(FALSE, 8);
#ifdef PANEL_MATE
    set_up_settings_and_menu(applet, box);
#endif
#ifdef PANEL_XFCE4
    set_up_settings_and_menu(plugin, box);
#endif
#ifdef PANEL_GNOME
    set_up_settings_and_menu(applet, box);
#endif
    // Without X the applet still works, just with generic icons that can't
    // be clicked; xcb_init has already explained why
    xcb_init();
    set_up_dbus_transfer(box);

#ifndef PANEL_XFCE4
    gtk_container_add(GTK_CONTAINER(applet), box);
    gtk_widget_show_all(GTK_WIDGET(applet));

    return TRUE;
#else
    gtk_container_add(GTK_CONTAINER(plugin), box);
#endif
}

#ifdef PANEL_MATE
static gboolean xmonad_log_applet_factory(
    MatePanelApplet *applet, const gchar *iid, gpointer data)
{
    gboolean retval = FALSE;

    if(!strcmp(iid, "XmonadLogApplet"))
        retval = xmonad_log_applet_fill(applet);

    if(retval == FALSE) {
        printf("Wrong applet!\n");
        exit(-1);
    }

    return retval;
}
#endif
#ifdef PANEL_XFCE4
static void xmonad_log_applet_construct(XfcePanelPlugin *plugin)
{
    xmonad_log_applet_fill(plugin);
    gtk_widget_show_all(GTK_WIDGET(plugin));
}
#endif

#ifdef PANEL_GNOME
// gnome-panel (3.38 and later) loads applets from modules, in its own
// process. The applet is a GpApplet subclass, set up when constructed.
#define XMONAD_LOG_TYPE_APPLET (xmonad_log_applet_get_type())
G_DECLARE_FINAL_TYPE(XmonadLogApplet, xmonad_log_applet, XMONAD_LOG, APPLET, GpApplet)

struct _XmonadLogApplet {
    GpApplet parent;
};

G_DEFINE_TYPE(XmonadLogApplet, xmonad_log_applet, GP_TYPE_APPLET)

static void xmonad_log_applet_constructed(GObject *object)
{
    G_OBJECT_CLASS(xmonad_log_applet_parent_class)->constructed(object);
    xmonad_log_applet_fill(GP_APPLET(object));
}

static void xmonad_log_applet_class_init(XmonadLogAppletClass *klass)
{
    G_OBJECT_CLASS(klass)->constructed = xmonad_log_applet_constructed;
}

static void xmonad_log_applet_init(XmonadLogApplet *self)
{
}

static GpAppletInfo *get_applet_info(const gchar *id)
{
    if (g_strcmp0(id, APPLET_ID) != 0)
        return NULL;
    return gp_applet_info_new(xmonad_log_applet_get_type, "Xmonad Log Applet",
                              "Monitor your Xmonad log", "xmonad-log-applet");
}

// Panels set up with the old libpanel-applet version know the applet by
// its factory and applet names
static const gchar *get_applet_id_from_iid(const gchar *iid)
{
    if (g_strcmp0(iid, "XmonadLogAppletFactory::XmonadLogApplet") == 0)
        return APPLET_ID;
    return NULL;
}

void gp_module_load(GpModule *module)
{
    gp_module_set_abi_version(module, GP_MODULE_ABI_VERSION);
    gp_module_set_id(module, "org.xmonad.xmonad-log-applet");
    gp_module_set_version(module, PACKAGE_VERSION);
    gp_module_set_applet_ids(module, APPLET_ID, NULL);
    gp_module_set_get_applet_info(module, get_applet_info);
    gp_module_set_compatibility(module, get_applet_id_from_iid);
}
#endif
#ifdef PANEL_MATE
MATE_PANEL_APPLET_OUT_PROCESS_FACTORY(
    "XmonadLogAppletFactory",
    PANEL_TYPE_APPLET,
    "XmonadLogApplet",
    xmonad_log_applet_factory,
    NULL);
#endif
#ifdef PANEL_XFCE4
XFCE_PANEL_PLUGIN_REGISTER(xmonad_log_applet_construct);
#endif
