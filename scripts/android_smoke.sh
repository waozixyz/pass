#!/usr/bin/env bash
set -euo pipefail

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
apk="$root_dir/build/pass-android-debug.apk"
package_name="${PASS_ANDROID_PACKAGE:-xyz.waozi.pass}"

if [ ! -s "$apk" ]; then
  echo "android smoke: missing APK artifact: $apk" >&2
  exit 1
fi
if ! command -v adb >/dev/null 2>&1; then
  echo "android smoke: skipped, adb is not installed" >&2
  exit 0
fi

device="${PASS_ANDROID_DEVICE:-}"
if [ -z "$device" ]; then
  device=$(adb devices | awk 'NR > 1 && $1 ~ /^emulator-/ && $2 == "device" { print $1; exit }')
fi
if [ -z "$device" ]; then
  echo "android smoke: skipped, no private emulator; set PASS_ANDROID_DEVICE explicitly for a device" >&2
  exit 0
fi

adb -s "$device" install -r "$apk" >/dev/null
adb -s "$device" shell monkey -p "$package_name" 1 >/dev/null
sleep 2
if ! adb -s "$device" shell pidof "$package_name" >/dev/null; then
  echo "android smoke: $package_name did not stay running on $device" >&2
  exit 1
fi

echo "android smoke: launched $package_name on $device"
