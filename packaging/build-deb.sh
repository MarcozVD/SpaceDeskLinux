#!/bin/sh
# Build a Debian package from an already built sdviewer binary.
# Usage: build-deb.sh <path-to-sdviewer> <output-dir>
# Needs dpkg-dev (dpkg-deb, dpkg-shlibdeps); runs inside the debian:bookworm build container.
set -eu

if [ $# -ne 2 ]; then
    echo "usage: $0 <path-to-sdviewer> <output-dir>" >&2
    exit 2
fi
bin=$1
out=$2
here=$(cd "$(dirname "$0")" && pwd)
version=$(sed -n 's/^project(sdviewer VERSION \([0-9.]*\) .*/\1/p' "$here/../viewer/CMakeLists.txt")
arch=$(dpkg --print-architecture)
pkg=sdviewer
work=$(mktemp -d)
root="$work/root"

install -D -m 0755 "$bin" "$root/usr/bin/sdviewer"
install -D -m 0755 "$here/sdviewer-launch" "$root/usr/bin/sdviewer-launch"
mkdir -p "$root/usr/share/applications"
sed "s|@BINDIR@|/usr/bin|g" "$here/sdviewer.desktop.in" >"$root/usr/share/applications/sdviewer.desktop"
chmod 0644 "$root/usr/share/applications/sdviewer.desktop"
install -D -m 0644 "$here/../README.md" "$root/usr/share/doc/$pkg/README.md"
install -D -m 0644 "$here/../CHANGELOG.md" "$root/usr/share/doc/$pkg/CHANGELOG.md"
strip --strip-unneeded "$root/usr/bin/sdviewer"

# shared library dependencies, computed the Debian way
mkdir -p "$work/debian"
printf 'Source: %s\n\nPackage: %s\nArchitecture: any\n' "$pkg" "$pkg" >"$work/debian/control"
depends=$(cd "$work" && dpkg-shlibdeps -O "root/usr/bin/sdviewer" | sed -n 's/^shlibs:Depends=//p')

mkdir -p "$root/DEBIAN"
cat >"$root/DEBIAN/control" <<EOF
Package: $pkg
Version: $version
Architecture: $arch
Maintainer: MarcozVD <mvaleradaza@gmail.com>
Installed-Size: $(du -sk "$root/usr" | cut -f1)
Depends: $depends
Recommends: intel-media-va-driver | i965-va-driver | mesa-va-drivers, libnotify-bin
Section: net
Priority: optional
Homepage: https://github.com/MarcozVD/SpaceDeskLinux
Description: Linux viewer for spacedesk (use this computer as an extra monitor)
 Connects to a Windows PC running the spacedesk server and shows one of its
 displays, with H.264 (VA-API) and JPEG video, audio, keyboard, mouse and touch.
 Not affiliated with datronicsoft.
EOF

mkdir -p "$out"
dpkg-deb --build --root-owner-group "$root" "$out/${pkg}_${version}_${arch}.deb" >/dev/null
rm -rf "$work"
echo "built: $out/${pkg}_${version}_${arch}.deb"
