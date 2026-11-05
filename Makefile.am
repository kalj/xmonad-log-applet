plugindir = $(PLUGIN_DIR)

# The applet is a module loaded by the panel for GNOME and Xfce, and a
# program the panel starts for MATE
if PANEL_MATE
plugin_PROGRAMS = xmonad-log-applet
PLUGIN_DIR_ = $(PLUGIN_DIR)
PLUGIN_FILE = xmonad-log-applet
else
xmonad_log_appletdir = $(PLUGIN_DIR)
xmonad_log_applet_LTLIBRARIES = libxmonad-log-applet.la
endif

libxmonad_log_applet_la_SOURCES = main.c parser.c parser.h
libxmonad_log_applet_la_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(GIO_CFLAGS) \
	$(LIBPANEL_CFLAGS) \
	$(XCB_CFLAGS)

libxmonad_log_applet_la_LIBADD = \
	$(GLIB_LIBS) \
	$(GIO_LIBS) \
	$(LIBPANEL_LIBS) \
	$(XCB_LIBS)

libxmonad_log_applet_la_LDFLAGS = \
	-module -avoid-version \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(XCB_LDFLAGS)

xmonad_log_applet_SOURCES = main.c parser.c parser.h
xmonad_log_applet_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(GIO_CFLAGS) \
	$(LIBPANEL_CFLAGS) \
	$(XCB_CFLAGS)

xmonad_log_applet_LDADD = \
	$(GLIB_LIBS) \
	$(GIO_LIBS) \
	$(LIBPANEL_LIBS) \
	$(XCB_LIBS)

# Unit tests, run by "make check". They only need GLib.
check_PROGRAMS = tests/test-parser
tests_test_parser_SOURCES = tests/test-parser.c parser.c parser.h
tests_test_parser_CPPFLAGS = $(GLIB_CFLAGS)
tests_test_parser_LDADD = $(GLIB_LIBS)
TESTS = $(check_PROGRAMS)

appletdir = $(LIBPANEL_APPLET_DIR)
if PANEL_MATE
applet_files = org.mate.panel.XmonadLogApplet.mate-panel-applet
$(applet_files): $(applet_files:.mate-panel-applet=.mate-panel-applet.in)
	$(SED) -e "s|\@PLUGIN_DIR\@|$(PLUGIN_DIR_)|" \
		   -e "s|\@PLUGIN_FILE\@|$(PLUGIN_FILE)|" \
		   -e "s|\@IN_PROC\@|$(IN_PROC_STR)|" $< > $@
endif
if PANEL_XFCE4
applet_files = xmonad-log-applet.desktop
$(applet_files): $(applet_files:.desktop=.desktop.in)
	cp $< $@
endif

applet_DATA = $(applet_files)

if PANEL_MATE
servicedir = $(SESSION_BUS_SERVICES_DIR)
service_in_files = org.mate.panel.applet.XmonadLogAppletFactory.service.in
service_DATA = $(service_in_files:.service.in=.service)
org.mate.panel.applet.XmonadLogAppletFactory.service: $(service_in_files)
	$(SED) -e "s|\@PLUGIN_DIR\@|$(PLUGIN_DIR)|" $< > $@
endif

# Installed for all panels: the defaults are read from it everywhere, even
# though only MATE stores settings with it so far
gsettings_SCHEMAS = org.mate.panel.applet.xmonad-log.gschema.xml
@GSETTINGS_RULES@

DISTCLEANFILES = \
	$(applet_DATA) \
	$(service_DATA)

EXTRA_DIST = \
	AUTHORS.md \
	LICENSE \
	README.md \
	xmonad.hs \
	xmonad-log-applet48.png \
	org.mate.panel.XmonadLogApplet.mate-panel-applet.in \
	org.mate.panel.applet.XmonadLogAppletFactory.service.in \
	org.mate.panel.applet.xmonad-log.gschema.xml \
	xmonad-log-applet.desktop.in

gtk_update_icon_cache = gtk-update-icon-cache -f -t

update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache) $(datadir)/icons/hicolor; \
	else \
		echo "*** Icon cache not updated.  After (un)install, run this:"; \
		echo "*** $(gtk_update_icon_cache) $(datadir)/icons/hicolor"; \
	fi

install-icons:
	mkdir -p $(DESTDIR)$(datadir)/icons/hicolor/48x48/apps; \
	$(INSTALL_DATA) $(srcdir)/xmonad-log-applet48.png \
	$(DESTDIR)$(datadir)/icons/hicolor/48x48/apps/xmonad-log-applet.png;

uninstall-icons:
	rm -f $(DESTDIR)$(datadir)/icons/hicolor/48x48/apps/xmonad-log-applet.png;

install-data-local: install-icons update-icon-cache

uninstall-local: uninstall-icons update-icon-cache
