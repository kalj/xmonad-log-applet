This spec file builds an RPM of the MATE applet from a release tag, like the
`xmonad-log-applet-mate` package from the Debian packaging in `debian/`.

To build it, download the release tarball and build the package:

    spectool -g -R xmonad-log-applet.spec
    rpmbuild -ba xmonad-log-applet.spec

(`spectool` comes with `rpmdevtools`; install the build dependencies first
with `sudo dnf builddep xmonad-log-applet.spec`.) To build from a git
checkout instead, create the tarball the spec expects:

    git archive --prefix=xmonad-log-applet-2.90.0/ -o ~/rpmbuild/SOURCES/xmonad-log-applet-2.90.0.tar.gz HEAD

Update `Version` and add a `%changelog` entry for each release.
