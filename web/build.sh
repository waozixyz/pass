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
cp -R "$root_dir/web/site/assets" "$out_dir/assets"
cp -R "$root_dir/web/site/images" "$out_dir/images"
cp "$root_dir/web/site/app/index.html" "$out_dir/app/index.html"
cp "$root_dir/web/site/CNAME" "$out_dir/CNAME"
cp "$root_dir/web/site/robots.txt" "$out_dir/robots.txt"
cp "$root_dir/web/site/sitemap.xml" "$out_dir/sitemap.xml"
cp "$root_dir/web/site/_redirects" "$out_dir/_redirects"
cp "$root_dir/web/site/_headers" "$out_dir/_headers"
cp "$root_dir/web/site/manifest.webmanifest" "$out_dir/manifest.webmanifest"
cp -R "$root_dir/web/site/icons" "$out_dir/icons"
cp -R "$root_dir/web/site/app/icons" "$out_dir/app/icons"
"${MAKE:-make}" -C "$root_dir" web-canvas
cp "$root_dir/build/web-app/index.html" "$out_dir/app/index.html"
cp "$root_dir/build/web-app/sw.js" "$out_dir/app/sw.js"
cp "$root_dir/build/web-app/index.js" "$out_dir/app/index.js"
cp "$root_dir/build/web-app/index.wasm" "$out_dir/app/index.wasm"
cp "$root_dir/assets/fonts/emoji-OFL.txt" "$out_dir/app/emoji-OFL.txt"

test -s "$out_dir/app/index.wasm"
printf 'built site at %s\n' "$out_dir"
python3 - "$out_dir" <<'META'
import hashlib, json, sys
from pathlib import Path
root = Path(sys.argv[1])
manifest = json.loads((root / "manifest.webmanifest").read_text())
manifest.update(start_url=".", scope=".")
for icon in manifest.get("icons", []):
    icon["src"] = icon["src"].lstrip("/")
(root / "app/manifest.webmanifest").write_text(json.dumps(manifest, indent=2) + "\n")
app = root / "app"
wasm = (app / "index.wasm").read_bytes()
wasm_name = "index-" + hashlib.sha256(wasm).hexdigest()[:16] + ".wasm"
(app / wasm_name).write_bytes(wasm)
loader = (app / "index.js").read_text()
assert loader.count('"index.wasm"') == 1, "Expected one Wasm URL in the generated loader"
loader = loader.replace('"index.wasm"', json.dumps(wasm_name))
js_name = "index-" + hashlib.sha256(loader.encode()).hexdigest()[:16] + ".js"
(app / js_name).write_text(loader)
(app / "index.js").write_text(loader)
html = (app / "index.html").read_text()
assert html.count('src="index.js"') == 1
(app / "index.html").write_text(html.replace('src="index.js"', 'src="' + js_name + '"'))
assets = ["./", js_name, wasm_name, "sw.js", "icons/icon-192.png", "icons/icon-512.png", "manifest.webmanifest", "assets.json"]
(app / "assets.json").write_text(json.dumps(assets) + "\n")
with (root / "_headers").open("a") as headers:
    for name in [js_name, wasm_name]:
        headers.write("\n/app/" + name + "\n  Cache-Control: public, max-age=31536000, immutable\n")
META
