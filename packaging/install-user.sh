#!/bin/sh
# Per-user install (no root): binary + launcher in ~/.local/bin, menu entry in
# ~/.local/share/applications, server address in ~/.config/sdviewer/server.
# Usage: install-user.sh <path-to-sdviewer> <server-ip>
set -eu

if [ $# -ne 2 ]; then
    echo "usage: $0 <path-to-sdviewer> <server-ip>" >&2
    exit 2
fi
src_bin=$1
server=$2
here=$(cd "$(dirname "$0")" && pwd)
bindir="$HOME/.local/bin"
appdir="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
confdir="${XDG_CONFIG_HOME:-$HOME/.config}/sdviewer"

mkdir -p "$bindir" "$appdir" "$confdir"
install -m 0755 "$src_bin" "$bindir/sdviewer"
install -m 0755 "$here/sdviewer-launch" "$bindir/sdviewer-launch"
sed "s|@BINDIR@|$bindir|g" "$here/sdviewer.desktop.in" >"$appdir/sdviewer.desktop"
chmod 0644 "$appdir/sdviewer.desktop"
printf '%s\n' "$server" >"$confdir/server"

command -v update-desktop-database >/dev/null && update-desktop-database "$appdir" 2>/dev/null || true
echo "installed: $bindir/sdviewer, menu entry 'SpaceDesk Linux', server $server"
