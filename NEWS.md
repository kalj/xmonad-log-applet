# News

## 3.0.0

The first release of this fork, which changes what the applet shows: each
workspace number followed by the icons of its windows, then the title of the
focused window. See the README for where the fork comes from.

### Changes since 2.90.0

2.90.0 was a preview of this release, with only the MATE applet.

* **Xfce and GNOME again:** a plugin for Xfce panel 4.14 or later and a
  module for gnome-panel 3.38 or later (GNOME Flashback). Panel layouts from
  the old GNOME applet still find it.
* **Preferences on all panels:** the preferences dialog now works on Xfce
  (Properties in the plugin's menu) and GNOME (Preferences in the applet's
  menu) too. "Expand to fill panel" is MATE and Xfce only.
* **Settings schema renamed** to `org.xmonad.log-applet`. Settings saved by
  2.90.0 are kept.
* **`./configure` builds the MATE applet by default.** It used to build the
  Xfce plugin. Pass `--with-panel=xfce4` or `--with-panel=gnome` for the
  other panels.
* **Debian packages** for all three panels: `xmonad-log-applet-mate`, `-xfce`
  and `-gnome`, plus `xmonad-log-applet-common`. They replace the
  `xmonad-log-applet` package; install them with `apt` to upgrade.
* The Fedora spec builds the MATE applet again. The outdated FreeBSD port
  is removed.

### Changes since 2.1.0

**Not a drop-in replacement:** the applet expects its own message format
rather than Pango markup. Start from the `xmonad.hs` in this repository.

* **Workspaces and window icons:** each workspace number is followed by the
  icons of its windows, then the focused window's title. A left click on an
  icon activates that window. Windows without an icon get a generic one.
* **Colours** for current, visible, hidden and urgent workspaces.
* **Preferences dialog:** expand to fill panel, the workspace colours, bold
  text and a maximum title width, stored per applet with GSettings.
* **Panels:** the MATE panel, Xfce panel 4.14 or later and gnome-panel 3.38
  or later. The GNOME 2 and older GNOME 3 panels are no longer supported.
* **Sample `xmonad.hs`:** based on `mateConfig`, marks urgent workspaces and
  formats its messages with `dynamicLogWithPP`. It needs xmonad and
  xmonad-contrib 0.17 or later.
* **Robustness:** a failed X connection no longer crashes the applet, and a
  malformed workspace in a message is skipped and logged instead of blanking
  the applet.
* **Under the hood:** GDBus instead of the deprecated dbus-glib, no
  intltool, unit tests for the message parser (`make check`) and CI builds.
* **Packaging:** Debian packages and a Fedora spec.

### Known limitations

* On vertical panels the applet is still laid out horizontally.
* Not tested in a full GNOME Flashback session, and urgent workspaces were
  only tested on MATE.
