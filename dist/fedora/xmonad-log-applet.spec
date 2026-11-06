Name:           xmonad-log-applet
Version:        2.90.0
Release:        1%{?dist}
Summary:        MATE panel applet showing xmonad's workspaces, window icons and title

License:        BSD-3-Clause
URL:            https://github.com/kalj/xmonad-log-applet
Source0:        %{url}/archive/%{version}/%{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  autoconf
BuildRequires:  automake
BuildRequires:  libtool
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gio-2.0)
BuildRequires:  pkgconfig(xcb)
BuildRequires:  pkgconfig(xcb-ewmh)
BuildRequires:  pkgconfig(libmatepanelapplet-4.0)

Requires:       mate-panel

%description
An applet for the MATE panel that shows xmonad's workspaces, the icons of
the windows on each workspace and the title of the focused window. Clicking
a window's icon activates that window. xmonad sends the information over
D-Bus; an example xmonad configuration is included in the documentation.

%prep
%autosetup

%build
autoreconf -fi
%configure --with-panel=mate
%make_build

%install
%make_install

%check
make check

%files
%license LICENSE
%doc README.md AUTHORS.md xmonad.hs
%{_libexecdir}/%{name}
%{_datadir}/mate-panel/applets/org.mate.panel.XmonadLogApplet.mate-panel-applet
%{_datadir}/dbus-1/services/org.mate.panel.applet.XmonadLogAppletFactory.service
%{_datadir}/glib-2.0/schemas/org.mate.panel.applet.xmonad-log.gschema.xml
%{_datadir}/icons/hicolor/48x48/apps/%{name}.png

%changelog
* Fri Nov 06 2026 Karl Ljungkvist <dev@kalj.se> - 2.90.0-1
- Update to 2.90.0 and package the MATE applet instead of the GNOME 2 one
- Modernise the spec file

* Sun Aug 15 2010 Adam Wick <awick@uhsure.com> - 1.0.0-1
- Initial spec file
