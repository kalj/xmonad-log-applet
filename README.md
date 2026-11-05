# xmonad-log-applet

## This Fork

This fork changes what the applet shows. Instead of displaying text formatted
by xmonad, it draws each workspace number followed by the icons of its windows,
then the title of the focused window. Click an icon to activate that window.

It builds on two earlier versions, both of which appear to be inactive:

* [alexkay/xmonad-log-applet](https://github.com/alexkay/xmonad-log-applet), the
  original, last changed in 2013 (version 2.1.0).
* [EricGebhart/xmonad-log-applet](https://github.com/EricGebhart/xmonad-log-applet),
  which added the three pull requests still open on the original: the
  [D-Bus type fix](https://github.com/alexkay/xmonad-log-applet/pull/16),
  [libmatepanelapplet 4.0](https://github.com/alexkay/xmonad-log-applet/pull/15)
  and [newer libpanel-applet](https://github.com/alexkay/xmonad-log-applet/pull/17)
  support.

This fork includes all of that and adds:

* **Window icons and colours**, from [hamax's fork](https://github.com/hamax/xmonad-log-applet):
  icons, click-to-activate, and separate colours for current, visible, hidden
  and urgent workspaces.
* **Preferences dialog (MATE only):** right-click the applet to set expand to
  fill panel, workspace colours, bold text and a maximum title width. Settings
  are stored per applet with GSettings.
* **Current panels:** a module for gnome-panel 3.38 or later (GNOME
  Flashback), and a plugin for Xfce panel 4.14 or later. Panel layouts from
  the older GNOME applet still find the applet.
* **Debian packaging** for the MATE applet (see `debian/`).

**This fork is not a drop-in replacement.** The applet expects its own message
format rather than Pango markup, so a config written for the other versions
shows little more than the window title. Use the [`xmonad.hs`](xmonad.hs) in
this repository as your starting point. Workspace names must be non-zero
numbers; other workspaces are left out, with a warning in the panel's log.

**Version 2.90 is a preview of 3.0.**

Please report problems on [this fork's issue tracker](https://github.com/kalj/xmonad-log-applet/issues).

--------------------

xmonad-log-applet is for Xmonad users who find the GNOME, MATE, or Xfce panel
useful. The applet will show your workspaces, the icons of their windows and
the title of the selected window, using the information sent from your
xmonad.hs.

xmonad-log-applet was originally written by Adam Wick, you can get more details
on the [old website][0].

## Installation

There are two pieces of setup you'll need to get this working, assuming you are
already running Xmonad within [GNOME][1], MATE or [Xfce][2]:

* An xmonad.hs setup file that will send properly-formatted information to
  D-Bus. You can use [this xmonad.hs][3] as an example. It is set up for
  xmonad in a MATE session, and its comments explain how to make MATE use
  xmonad as its window manager and how to adapt it to other desktops. It needs
  xmonad and xmonad-contrib 0.17 or later, and the Haskell [dbus][5] package.

* The panel applet. Pull the latest version from the git repository: `git clone
  https://github.com/kalj/xmonad-log-applet.git`

The applet supports the MATE panel, gnome-panel 3.38 or later (as used by GNOME
Flashback) and Xfce panel 4.14 or later. Pass `--with-panel=X` to `./configure`
when compiling, where X is one of mate, gnome or xfce4 (the default). The GNOME
2 and older GNOME 3 panels are no longer supported.

If you are compiling from a git clone, you should run `./autogen.sh` instead of
`./configure`.

`make check` runs the unit tests for the parsing of xmonad's messages.

## FAQ

*   Which Haskell D-Bus package should I use?

    The recommended package is [dbus][5].

*   How to add applets to the GNOME panel?

    Hold the Alt key when right-clicking the panel. Alt and right-click on the
    applet gives the menu for moving or removing it.

## Contact

If you have any questions, comments or bug reports, please open an issue on
[GitHub](https://github.com/kalj/xmonad-log-applet/issues).

[0]: http://uhsure.com/xmonad-log-applet.html
[1]: http://www.haskell.org/haskellwiki/Xmonad/Using_xmonad_in_Gnome
[2]: http://www.haskell.org/haskellwiki/Xmonad/Using_xmonad_in_XFCE
[3]: xmonad.hs
[5]: http://hackage.haskell.org/package/dbus
[6]: http://versia.com/2011/09/xmonad-log-applet-gnome-xfce/
