AC_INIT([Xmonad Log Applet], [3.0.0])
AC_CONFIG_SRCDIR([main.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign subdir-objects])

AM_MAINTAINER_MODE

LT_PREREQ([2.2.6])
LT_INIT([dlopen disable-static])
LT_LIB_M

AC_PROG_CC
AC_PROG_SED
AC_PROG_INSTALL
AC_HEADER_STDC

PKG_PROG_PKG_CONFIG

PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.40.0)
PKG_CHECK_MODULES(XCB, xcb xcb-ewmh)

PKG_CHECK_MODULES(GIO, gio-2.0 >= 2.40.0)

# Where the D-Bus service file for the applet factory goes. Ask D-Bus if its
# development files are installed, otherwise use the standard location.
PKG_CHECK_VAR([SESSION_BUS_SERVICES_DIR], [dbus-1], [session_bus_services_dir], [],
              [SESSION_BUS_SERVICES_DIR='${datadir}/dbus-1/services'])
AC_SUBST([SESSION_BUS_SERVICES_DIR])

AC_ARG_WITH(
    [panel],
    [AS_HELP_STRING([--with-panel], [target mate (default), xfce4 or gnome panel])],
    [panel=$withval],
    [panel=mate]
)
AS_IF(
    [test "x$panel" = xgnome -o "x$panel" = xgnomeflashback],
        dnl gnome-panel 3.38 and later (as used by GNOME Flashback) load
        dnl applets from modules in its modules directory
        [PKG_CHECK_MODULES(LIBPANEL, libgnome-panel >= 3.38.0)]
        PLUGIN_DIR=`$PKG_CONFIG --variable=modulesdir libgnome-panel`
        panel=gnome
        [AC_DEFINE(PANEL_GNOME, 1, [panel type])]
        ,
    [test "x$panel" = xgnome2 -o "x$panel" = xgnome3],
        [AC_MSG_ERROR([--with-panel=$panel is no longer supported: the libpanel-applet library it needs was removed in gnome-panel 3.38. Use --with-panel=gnome for gnome-panel 3.38 or later.])]
        ,
    [test "x$panel" = xmate],
        [PKG_CHECK_MODULES(LIBPANEL, libmatepanelapplet-4.0 >= 1.6.0)]
        LIBPANEL_APPLET_DIR=`$PKG_CONFIG --variable=prefix libmatepanelapplet-4.0`/share/mate-panel/applets
        PLUGIN_DIR=`$PKG_CONFIG --variable=prefix libmatepanelapplet-4.0`/libexec
        [AC_DEFINE(PANEL_MATE, 1, [panel type])]
        ,
    [test "x$panel" = xxfce4],
        dnl xfce4-panel 4.14 and later load plugins as modules; the panel
        dnl looks for them, and their .desktop files, under its own prefix
        [PKG_CHECK_MODULES(LIBPANEL, libxfce4panel-2.0 >= 4.14.0)]
        LIBPANEL_APPLET_DIR=`$PKG_CONFIG --variable=prefix libxfce4panel-2.0`/share/xfce4/panel/plugins
        PLUGIN_DIR=`$PKG_CONFIG --variable=libdir libxfce4panel-2.0`/xfce4/panel/plugins
        [AC_DEFINE(PANEL_XFCE4, 1, [panel type])]
        ,
    [AC_MSG_ERROR([Unknown panel type, use gnome, mate or xfce4])]
)
AC_SUBST([LIBPANEL_APPLET_DIR])
AC_SUBST([PLUGIN_DIR])

AM_CONDITIONAL([PANEL_GNOME], [test "x$panel" = xgnome])
AM_CONDITIONAL([PANEL_MATE], [test "x$panel" = xmate])
AM_CONDITIONAL([PANEL_XFCE4], [test "x$panel" = xxfce4])

# GSettings schema with the appearance settings and their defaults
GLIB_GSETTINGS

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
