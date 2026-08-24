#!/bin/sh
set -eu

release_version=$(sed -n 's/^## \[\([0-9][0-9.]*\)\].*/\1/p' CHANGELOG.md | head -n 1)
release_date=$(sed -n 's/^## \[[0-9][0-9.]*\] - \([0-9-]*\)$/\1/p' CHANGELOG.md | head -n 1)
if [ -z "$release_version" ]; then
	echo "No numeric release entry found in CHANGELOG.md" >&2
	exit 1
fi
if [ -z "$release_date" ]; then
	echo "No release date found for $release_version in CHANGELOG.md" >&2
	exit 1
fi

printf '%s\n' "$release_version" > VERSION

# Android versionName and versionCode: bump the code only when preparing a
# different release, so rerunning this script for the same version is
# idempotent.
gradle_file=droid/app/build.gradle
current_name=$(sed -n 's/.*versionName "\([^"]*\)".*/\1/p' "$gradle_file" | head -n 1)
current_code=$(sed -n 's/^[[:space:]]*versionCode \([0-9][0-9]*\).*/\1/p' "$gradle_file" | head -n 1)
if [ -z "$current_code" ]; then
	echo "Could not read versionCode from $gradle_file" >&2
	exit 1
fi
if [ "$current_name" = "$release_version" ]; then
	release_code=$current_code
else
	release_code=$((current_code + 1))
fi
sed -E -i "s/versionName \"[^\"]+\"/versionName \"$release_version\"/" "$gradle_file"
sed -E -i "s/^([[:space:]]*)versionCode [0-9]+/\1versionCode $release_code/" "$gradle_file"
metainfo=packaging/linux/xyz.waozi.pass.appdata.xml
if ! grep -F "<release version=\"$release_version\"" "$metainfo" >/dev/null; then
	sed -E -i "s#<releases>#<releases><release version=\"$release_version\" date=\"$release_date\"/>#" "$metainfo"
fi
grep -Fx "$release_version" VERSION >/dev/null
grep -F "versionName \"$release_version\"" "$gradle_file" >/dev/null
grep -E "^[[:space:]]*versionCode $release_code$" "$gradle_file" >/dev/null
grep -F "<release version=\"$release_version\" date=\"$release_date\"" "$metainfo" >/dev/null

# Generate the Fastlane changelog for the Android versionCode being built.
# Fastlane file names are versionCode values, not changelog positions, and
# store listings cap changelogs at 500 characters.
changelog_dir=fastlane/metadata/android/en-US/changelogs
mkdir -p "$changelog_dir"
changelog=$(awk -v ver="$release_version" '
	BEGIN { in_section = 0; buf = "" }
	/^## \[/ {
		if (in_section) exit
		if (index($0, "[" ver "]") > 0) {
			in_section = 1
			next
		}
	}
	in_section {
		if ($0 == "") next
		if (/^- /) {
			if (buf != "") print buf
			buf = $0
			next
		}
		if (buf != "") {
			line = $0
			sub(/^[ \t]+/, "", line)
			buf = buf " " line
		}
	}
	END { if (buf != "") print buf }
' CHANGELOG.md)
if [ -z "$changelog" ]; then
	echo "No changelog bullets found for $release_version in CHANGELOG.md" >&2
	exit 1
fi
printf '%s\n' "$changelog" | head -c 500 > "$changelog_dir/$release_code.txt"
