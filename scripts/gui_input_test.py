#!/usr/bin/env python3
"""Exercise the real desktop app and clipboard on the harness's private Xvfb."""
from pathlib import Path
import os
import subprocess
import tempfile
import time

assert os.environ.get("PASS_TEST_PRIVATE_DISPLAY") == "1", "Run scripts/gui_input_test.sh"
root = Path(__file__).resolve().parents[1]
output = root / "build/gui-input"
output.mkdir(parents=True, exist_ok=True)
fixture = Path(tempfile.mkdtemp(prefix="profile-", dir=output))
environment = dict(os.environ)
private_display = environment.pop("DISPLAY")
environment.pop("WAYLAND_DISPLAY", None)
environment["DISPLAY"] = private_display
app = None
window = None

def command(*args):
    return subprocess.check_output(args, env=environment, text=True, timeout=10)

def start():
    global app, window
    app = subprocess.Popen([str(root / "build/pass-gui")], cwd=fixture, env=environment)
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        assert app.poll() is None, "Pass exited before showing its window"
        result = subprocess.run(["xdotool", "search", "--onlyvisible", "--pid", str(app.pid)],
                                env=environment, capture_output=True, text=True)
        if result.returncode == 0:
            window = result.stdout.splitlines()[0]
            command("xdotool", "windowfocus", "--sync", window)
            time.sleep(0.3)
            return
        time.sleep(0.1)
    raise AssertionError("Pass did not create a window")

def stop():
    if app and app.poll() is None:
        app.terminate()
        try:
            app.wait(timeout=5)
        except subprocess.TimeoutExpired:
            app.kill()
            app.wait()

def click(x, y):
    command("xdotool", "mousemove", "--window", window, str(x), str(y), "click", "1")
    time.sleep(0.2)

def type_text(value):
    command("xdotool", "type", "--window", window, "--delay", "30", value)
    time.sleep(0.2)

try:
    start()
    click(90, 55); type_text("example.com")
    click(90, 130); type_text("alice")
    click(90, 205); type_text("test master")
    command("xdotool", "mousemove", "--window", window, "120", "400",
            "click", "--repeat", "20", "--delay", "40", "5")
    time.sleep(0.3)
    click(120, 225)  # Generate at the end of the scrolled form.
    click(120, 380)  # Copy the generated password.
    # Independently derived by scripts/lesspass_compat_test.py.
    expected = "dEeEDu7/b27L#r<&"
    actual = command("xclip", "-selection", "clipboard", "-out")
    assert actual == expected, f"Desktop generation/copy mismatch: {actual!r}"
    click(480, 565)
    click(90, 55); type_text("Desktop test")
    click(120, 105)
    saved = "Desktop test\texample.com\talice\t16\t1\t1\t1\t1\t1\t\n"
    profiles = fixture / "profiles.tsv"
    assert profiles.read_text() == saved
    assert "test master" not in profiles.read_text()
    stop(); start()
    click(480, 565)
    click(700, 216)  # Delete a profile loaded from disk after restart.
    assert profiles.read_text() == "", "Reloaded profile could not be deleted"
    print("Ziran desktop: typing, expected password, clipboard, profiles, restart and deletion pass")
finally:
    stop()
