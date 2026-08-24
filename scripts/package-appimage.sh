#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
version=$(sed -n '1p' "$root/VERSION")
arch=${APPIMAGE_ARCH:-$(uname -m)}
linuxdeploy=${LINUXDEPLOY:-linuxdeploy}
appdir="$root/build/package/appimage/pass.AppDir"
out="$root/build/release/pass-${version}-${arch}.AppImage"

test -n "$version"
rm -rf "$appdir"
mkdir -p "$appdir/usr/bin" "$appdir/usr/share/applications" "$appdir/usr/share/metainfo" "$appdir/usr/share/icons/hicolor/512x512/apps" "$(dirname "$out")"
cp "$root/build/pass-gui" "$appdir/usr/bin/pass-gui"
cp "$root/packaging/linux/xyz.waozi.pass.desktop" "$appdir/usr/share/applications/xyz.waozi.pass.desktop"
cp "$root/packaging/linux/xyz.waozi.pass.appdata.xml" "$appdir/usr/share/metainfo/xyz.waozi.pass.appdata.xml"
cp "$root/assets/app/icon.png" "$appdir/usr/share/icons/hicolor/512x512/apps/pass.png"
rm -f "$root"/pass-*.AppImage
ARCH="$arch" "$linuxdeploy" --appdir "$appdir" --desktop-file "$root/packaging/linux/xyz.waozi.pass.desktop" --icon-file "$root/assets/app/icon.png" --output appimage
image=$(find "$root" -maxdepth 1 -type f -name '*.AppImage' -print | head -n 1)
test -n "$image"
mv "$image" "$out"
chmod 0755 "$out"
printf 'built %s\n' "$out"
