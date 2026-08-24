#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
version=$(sed -n '1p' "$root/VERSION")
arch=${DEB_ARCH:-$(dpkg --print-architecture)}
stage="$root/build/package/deb/pass"
out="$root/build/release"

test -n "$version"
rm -rf "$stage"
mkdir -p "$stage/DEBIAN" "$stage/usr/bin" "$stage/usr/share/applications" "$stage/usr/share/metainfo" "$stage/usr/share/icons/hicolor/512x512/apps" "$stage/usr/share/doc/pass" "$out"
cp "$root/build/pass-gui" "$stage/usr/bin/pass-gui"
cp "$root/packaging/linux/xyz.waozi.pass.desktop" "$stage/usr/share/applications/xyz.waozi.pass.desktop"
cp "$root/packaging/linux/xyz.waozi.pass.appdata.xml" "$stage/usr/share/metainfo/xyz.waozi.pass.appdata.xml"
cp "$root/assets/app/icon.png" "$stage/usr/share/icons/hicolor/512x512/apps/pass.png"
cp "$root/LICENSE" "$root/README.md" "$stage/usr/share/doc/pass/"
find "$stage" -type d -exec chmod 0755 {} \;
chmod 0755 "$stage/usr/bin/pass-gui"
find "$stage/usr/share" -type f -exec chmod 0644 {} \;
installed=$(du -sk "$stage" | awk '{print $1}')
cat > "$stage/DEBIAN/control" <<EOF
Package: pass
Version: $version
Architecture: $arch
Maintainer: Waozi <waozi@proton.me>
Installed-Size: $installed
Depends: libc6, libsdl2-2.0-0, libgl1, libgtk-3-0
Section: utils
Priority: optional
Homepage: https://pass.waozi.xyz/
Description: deterministic local password generator
 Generate repeatable passwords locally without a password database.
EOF
dpkg-deb --root-owner-group --build "$stage" "$out/pass_${version}_${arch}.deb"
