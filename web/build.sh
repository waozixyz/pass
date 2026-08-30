#!/bin/sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
out_dir="$root_dir/build/site"

rm -rf "$out_dir"
mkdir -p "$out_dir/app"
# Versioned asset names (AppImage, .deb) are templated into index.html from
# VERSION so the site always links the current release.
version=$(sed -n '1p' "$root_dir/VERSION")
if [ -z "$version" ]; then
	echo "Could not read the version from VERSION" >&2
	exit 1
fi
sed "s/\${version}/$version/g" "$root_dir/web/site/index.html" > "$out_dir/index.html"
cp "$root_dir/web/site/styles.css" "$out_dir/styles.css"
cp "$root_dir/web/site/app/index.html" "$out_dir/app/index.html"
cp "$root_dir/web/site/app/app.js" "$out_dir/app/app.js"
cp "$root_dir/web/site/CNAME" "$out_dir/CNAME"
cp "$root_dir/web/site/robots.txt" "$out_dir/robots.txt"
cp "$root_dir/web/site/sitemap.xml" "$out_dir/sitemap.xml"
cp "$root_dir/web/site/_redirects" "$out_dir/_redirects"
cp "$root_dir/web/site/manifest.webmanifest" "$out_dir/manifest.webmanifest"
cp -R "$root_dir/web/site/icons" "$out_dir/icons"
cp -R "$root_dir/web/site/app/icons" "$out_dir/app/icons"
cp "$root_dir/web/site/app/sw.js" "$out_dir/app/sw.js"
"${MAKE:-make}" -C "$root_dir" web-canvas
cp "$root_dir/build/web-app/index.html" "$out_dir/app/index.html"
cp "$root_dir/build/web-app/app.js" "$out_dir/app/app.js"
cp "$root_dir/build/web-app/index.js" "$out_dir/app/index.js"
cp "$root_dir/build/web-app/index.wasm" "$out_dir/app/index.wasm"

test -s "$out_dir/app/index.wasm"
printf 'built site at %s\n' "$out_dir"
