#!/usr/bin/env python3
"""Drive only a private emulator; never choose a connected phone implicitly."""
from pathlib import Path
import os
import subprocess
import time
from android_apk_check import check_apk

root = Path(__file__).resolve().parents[1]
output = root / "build/android-input"
output.mkdir(parents=True, exist_ok=True)
serial = os.environ.get("PASS_ANDROID_EMULATOR")
if not serial:
    devices = subprocess.check_output(["adb", "devices"], text=True).splitlines()
    serial = next((line.split()[0] for line in devices if line.startswith("emulator-") and line.split()[-1] == "device"), None)
if not serial or not serial.startswith("emulator-"):
    raise SystemExit("Android input tests require a private emulator")
package = "xyz.waozi.pass"

def adb(*arguments, check=True):
    return subprocess.run(["adb", "-s", serial, *arguments], check=check, capture_output=True)

def shell(*arguments, check=True):
    return adb("shell", *arguments, check=check).stdout.decode()

def shot(name):
    (output / f"{name}.png").write_bytes(adb("exec-out", "screencap", "-p").stdout)

def alive():
    if not shell("pidof", package, check=False).strip():
        details = adb("logcat", "-d", "-b", "crash").stdout.decode()
        raise AssertionError("Pass stopped in the private emulator:\n" + details[-6000:])
    activities = shell("dumpsys", "activity", "activities")
    resumed = next((line for line in activities.splitlines() if "mResumedActivity:" in line), "")
    assert package in resumed, f"Pass activity closed or lost foreground: {resumed}"

def tap(x, y):
    shell("input", "tap", str(x), str(y)); time.sleep(0.25)

def type_text(text):
    shell("input", "text", text.replace(" ", "%s")); time.sleep(0.3)

apk = root / "build/pass-android-emulator.apk"
check_apk(apk)
try:
    adb("install", "-r", str(apk))
    # This is the test emulator and only this package's fixture data is cleared.
    shell("pm", "clear", package)
    shell("am", "start", "-n", package + "/android.app.NativeActivity")
    time.sleep(2); alive(); shot("launch")
    size = shell("wm", "size").strip().split()[-1]
    width, height = map(int, size.split("x"))
    zoom = width / 390
    tap(int(90 * zoom), int(55 * zoom)); type_text("example.com")
    assert "mInputShown=true" in shell("dumpsys", "input_method"), "Soft keyboard did not open"
    tap(int(90 * zoom), int(130 * zoom)); type_text("alice")
    tap(int(90 * zoom), int(205 * zoom)); type_text("test master")
    shot("keyboard")
    shell("input", "keyevent", "4"); time.sleep(0.4); alive()
    tap(width // 2, height - int(35 * zoom))
    tap(int(90 * zoom), int(55 * zoom)); type_text("Android test")
    shell("input", "keyevent", "4"); time.sleep(0.3)
    tap(int(100 * zoom), int(105 * zoom))
    expected = "Android test\texample.com\talice\t16\t1\t1\t1\t1\t1\t\n"
    saved = shell("run-as", package, "cat", "files/profiles.tsv")
    assert saved == expected, f"Profile input/persistence mismatch: {saved!r}"
    assert "test master" not in saved
    shell("am", "force-stop", package)
    shell("am", "start", "-n", package + "/android.app.NativeActivity")
    time.sleep(1); alive()
    assert shell("run-as", package, "cat", "files/profiles.tsv") == expected
    shot("persisted")
    print("Ziran Android: NativeActivity launch, soft keyboard, masked master, profiles and restart pass")
except BaseException:
    shot("failure")
    raise
