Name:		xmonad-log-applet
Version:	1.0.0
Release:	1%{?dist}
Summary:	A little applet that grabs logging output from Xmonad

Group:		User Interface/X
License:	BSD3
URL:		http://uhsure.com/xmonad-log-applet.html
Source0:	xmonad-log-applet.tar.gz
BuildRoot:	%(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

BuildRequires:	coreutils,gcc,sed,pkgconfig,glib2-devel,gnome-panel-devel
Requires:	glib2,gnome-panel

%description


%prep
%setup -q


%build
%configure
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,-)
%{_libdir}/gnome-applets/xmonad-log-applet
%{_libdir}/bonobo/servers/xmonad-log-applet.server
%{_datadir}/icons/hicolor/48x48/apps
%doc



%changelog
* Mon Aug 15 2010 Adam Wick <awick@uhsure.com> - 1.0.0-1
- Initial spec file
