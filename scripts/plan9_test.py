#!/usr/bin/env python3
"""Build and exercise Pass with native 8c/libdraw in an owned headless q9 VM."""
from pathlib import Path
import atexit
import json
import os
import shlex
import shutil
import signal
import socket
import subprocess
import time
from PIL import Image

root = Path(__file__).resolve().parents[1]
projects = root.parent.parent
candidates = [Path(os.environ["TAIJI_ROOT"])] if "TAIJI_ROOT" in os.environ else [projects / "taijiosnet/taiji", projects / "taiji"]
taiji = next((path.resolve() for path in candidates if (path / "q9").is_file()), None)
if taiji is None:
    raise SystemExit("Set TAIJI_ROOT to the existing Plan 9 tree with its q9 launcher")
lock = taiji / "boot/q9/run.lock"
try:
    lock.mkdir()
except FileExistsError:
    raise SystemExit("The q9 VM is already in use; leave that session untouched")
owner = str(os.getpid()) + "\n"
(lock / "pid").write_text(owner)

def release_lock():
    if (lock / "pid").exists() and (lock / "pid").read_text() == owner:
        (lock / "pid").unlink()
        lock.rmdir()

atexit.register(release_lock)
output = root / "build/plan9-test"
output.mkdir(parents=True, exist_ok=True)
stage = taiji / "usr/glenda/tmp/pass-plan9-test"
stage.mkdir(parents=True, exist_ok=True)
guest = f"/usr/glenda/tmp/{stage.name}"

def copy_changed(source, destination):
    if not Path(destination).exists() or Path(source).read_bytes() != Path(destination).read_bytes():
        shutil.copy2(source, destination)
    return destination

# Retain native objects between runs, while replacing every changed generated
# source/header. This directory contains disposable build artifacts only.
shutil.copytree(root / "build/plan9", stage / "build/plan9", dirs_exist_ok=True, copy_function=copy_changed)
for target in ("gui", "cli", "core"):
    current = root / "build/plan9" / target
    for stale in (stage / "build/plan9" / target).iterdir():
        if stale.suffix in (".c", ".h") and not (current / stale.name).exists():
            stale.unlink()
            stale.with_suffix(".8").unlink(missing_ok=True)
copy_changed(root / "mkfile", stage / "mkfile")
if (stage / "data").exists():
    shutil.rmtree(stage / "data")
(stage / "data").mkdir(mode=0o700)
for name, value in (("auto_copy", "1"), ("clear_after_seconds", "0")):
    setting = stage / "data" / f".kryon_pass_{name}.txt"
    setting.write_text(value)
    setting.chmod(0o600)
(stage / "control").mkdir(exist_ok=True)
for name in ("ready", "command.rc", "command-next", "command-done", "snarf.txt", "cli.txt", "app.pid", "app.winid"):
    (stage / name).unlink(missing_ok=True)
qmp_path = output / "qmp.sock"
qmp_path.unlink(missing_ok=True)
wrapper = output / "qemu"
wrapper.write_text('#!/bin/sh\nexec qemu-system-x86_64 "$@" -display none -qmp ' +
                   shlex.quote(f"unix:{qmp_path},server=on,wait=off") + "\n")
wrapper.chmod(0o755)
(stage / "rio.rc").write_text(f"""#!/bin/rc
mount $wsys {guest}/control 'new -r 0 0 100 100 -hide'
window -r '40 40 1000 740' /bin/rc -c 'rfork s; echo $pid >{guest}/app.pid; cat /dev/winid >{guest}/app.winid; exec {guest}/pass-gui >{guest}/app.trace >[2]{guest}/app.log' &
echo ready > {guest}/ready
while(){{
    if(test -e {guest}/command.rc){{
        . {guest}/command.rc
        rm -f {guest}/command.rc
        echo done > {guest}/command-done
    }}
    sleep 1
}}
""")
command = f"""
echo pass-plan9-build-start
cd {guest}
mk
if(test -x pass-gui && test -x pass){{
    PASS_DATA={guest}/data
    PASS_ASSETS={guest}/build/plan9/assets
    LESSPASS_MASTER_PASSWORD='test master' ./pass example.com alice > cli.txt
    echo pass-plan9-build-ok
    rio -i {guest}/rio.rc
}}
if not
    echo pass-plan9-build-failed
"""
environment = dict(os.environ)
environment.pop("DISPLAY", None)
environment.pop("WAYLAND_DISPLAY", None)
environment.update(QEMU=str(wrapper), Q9_QEMU_NICE="", Q9_BOOT_TIMEOUT="240")
log_path = output / "guest.log"
log = log_path.open("w")
vm = subprocess.Popen([str(taiji / "q9"), "--raw", "run", command],
                      cwd=taiji, env=environment, stdout=log, stderr=subprocess.STDOUT,
                      start_new_session=True)
connection = None
stream = None

def wait_for(predicate, label, timeout=600):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return
        if vm.poll() is not None:
            raise AssertionError(f"VM exited while waiting for {label}: {log_path.read_text()[-5000:]}")
        if "pass-plan9-build-failed" in log_path.read_text():
            raise AssertionError(log_path.read_text()[-6000:])
        time.sleep(0.25)
    raise AssertionError(f"Timed out waiting for {label}: {log_path.read_text()[-5000:]}")

def qmp(name, arguments=None):
    connection.sendall((json.dumps({"execute": name, "arguments": arguments or {}}) + "\n").encode())
    while True:
        response = json.loads(stream.readline())
        if "error" in response:
            raise AssertionError(response)
        if "return" in response:
            return response["return"]

def monitor(command):
    result = qmp("human-monitor-command", {"command-line": command})
    assert "unknown command" not in result.lower(), result
    return result

def screenshot(name):
    target = output / f"{name}.ppm"
    qmp("screendump", {"filename": str(target)})
    png = target.with_suffix(".png")
    Image.open(target).save(png)
    return png

def click(x, y):
    # Inject through the guest's mouse device, then let rio deliver the native
    # /dev/mouse packets. This avoids PS/2 acceleration and host mouse state.
    guest_command("echo -n 'm-2048 -1536 0 0' > '#m/mousein'\n"
                  f"echo -n 'm{x} {y} 1 0' > '#m/mousein'\n"
                  "sleep 1\necho -n 'm0 0 0 0' > '#m/mousein'")
    time.sleep(0.2)

def key(value):
    monitor(f"sendkey {value}")
    time.sleep(0.12)

def type_text(value):
    names = {" ": "spc", ".": "dot", "-": "minus", "@": "shift-2"}
    for char in value:
        key(names.get(char, f"shift-{char.lower()}" if char.isupper() else char))

def guest_command(text):
    done = stage / "command-done"
    done.unlink(missing_ok=True)
    temporary = stage / "command-next"
    temporary.write_text(text + "\n")
    temporary.rename(stage / "command.rc")
    wait_for(done.exists, "guest command", 30)

def read_clipboard():
    guest_command(f"cat {guest}/control/snarf > {guest}/snarf.txt")
    return (stage / "snarf.txt").read_text()

def restart(mini=False):
    pid = (stage / "app.pid").read_text().strip()
    assert pid.isdecimal(), f"Invalid owned application PID: {pid!r}"
    guest_command(f"echo kill > /proc/{pid}/notepg\nsleep 1")
    (stage / "app.pid").unlink()
    (stage / "app.winid").unlink()
    rectangle = "40 40 520 560" if mini else "40 40 1000 740"
    argument = " --mini" if mini else ""
    guest_command(f"window -r '{rectangle}' /bin/rc -c 'rfork s; echo $pid >{guest}/app.pid; cat /dev/winid >{guest}/app.winid; exec {guest}/pass-gui{argument} >{guest}/app.trace >[2]{guest}/app.log' &")
    wait_for((stage / "app.winid").exists, "restarted application", 30)
    time.sleep(2)

def wheel_down(steps):
    guest_command("echo -n 'm-2048 -1536 0 0' > '#m/mousein'\n"
                  "echo -n 'm134 350 0 0' > '#m/mousein'\n"
                  "for(i in " + " ".join(str(i) for i in range(steps)) + "){\n"
                  "echo -n 'm0 0 16 0' > '#m/mousein'\nsleep 1\n"
                  "echo -n 'm0 0 0 0' > '#m/mousein'\n}")
    time.sleep(1)

try:
    wait_for(qmp_path.exists, "QMP")
    connection = socket.socket(socket.AF_UNIX)
    connection.connect(str(qmp_path))
    connection.settimeout(30)
    stream = connection.makefile("rb")
    json.loads(stream.readline())
    qmp("qmp_capabilities")
    wait_for((stage / "ready").exists, "native compiler and rio")
    assert (stage / "cli.txt").read_text().strip() == "dEeEDu7/b27L#r<&"
    time.sleep(2)
    screenshot("initial")
    print("Plan 9: native CLI golden password and libdraw GUI launched", flush=True)
    # Rio content is inset four pixels within the requested rectangle.
    click(134, 99); type_text("example.com")
    click(134, 174); type_text("alice")
    click(134, 249); type_text("test master")
    screenshot("typed")
    # Master fingerprint inserts a row, placing Generate here.
    click(164, 564)
    expected = "dEeEDu7/b27L#r<&"
    # PBKDF2 runs at its real iteration count; CPU emulation can take minutes.
    wait_for(lambda: read_clipboard() == expected, "native GUI derivation and auto-copy", 360)
    time.sleep(1)
    screenshot("generated")
    guest_command(f"echo -n '' > {guest}/control/snarf")
    # Generate scrolls to the end; the Copy button is then around y=490.
    click(164, 490)
    actual = read_clipboard()
    assert actual == expected, f"Native generation/clipboard mismatch: {actual!r}"
    click(520, 700)
    click(134, 99); type_text("Plan9 test")
    click(164, 150)
    profiles = stage / "data/profiles.tsv"
    saved = "Plan9 test\texample.com\talice\t16\t1\t1\t1\t1\t1\t\n"
    wait_for(lambda: profiles.exists() and profiles.read_text() == saved, "saved profile", 30)
    assert profiles.stat().st_mode & 0o777 == 0o600
    assert "test master" not in profiles.read_text()
    screenshot("profiles")
    print("Plan 9: native typing, password generation, real snarf clipboard and private profile storage pass", flush=True)
    click(820, 700)
    click(73, 74)  # Disable auto-copy and persist the setting through the UI.
    for unused in range(5):
        click(943, 122)
    click(164, 497)
    clear_setting = stage / "data/.kryon_pass_clear_after_seconds.txt"
    wait_for(lambda: clear_setting.exists() and clear_setting.read_text() == "5", "saved clipboard settings", 30)
    assert (stage / "data/.kryon_pass_auto_copy.txt").read_text() == "0"
    click(164, 700)
    click(164, 490)
    assert read_clipboard() == expected
    wait_for(lambda: read_clipboard() == "", "clipboard expiration", 15)
    click(164, 490)
    assert read_clipboard() == expected
    guest_command(f"echo -n 'external clipboard' > {guest}/control/snarf")
    time.sleep(6)
    assert read_clipboard() == "external clipboard", "Expiration erased another application's clipboard"
    click(820, 700)
    wheel_down(14)
    click(164, 360); click(164, 360)  # System -> Light -> Dark.
    click(164, 558)
    dark_setting = stage / "data/.kryon_pass_theme_mode.txt"
    wait_for(lambda: dark_setting.exists() and dark_setting.read_text() == "2", "saved dark theme", 30)
    screenshot("settings")
    print("Plan 9: settings, dark theme, mouse wheel and clipboard expiration pass", flush=True)
    restart()
    screenshot("restarted")
    assert max(Image.open(output / "restarted.png").getpixel((55, 50))) < 100, "Saved dark theme was not restored"
    click(520, 700)
    click(820, 260)
    wait_for(lambda: profiles.read_text() == "", "deletion of reloaded profile", 30)
    guest_command(f"echo -n 'café' > {guest}/control/snarf")
    click(134, 99); key("ctrl-v")
    click(164, 150)
    unicode_profile = "café\t\t\t16\t1\t1\t1\t1\t1\t\n"
    wait_for(lambda: profiles.read_text() == unicode_profile, "Unicode clipboard paste and profile storage", 30)
    click(820, 260)
    wait_for(lambda: profiles.read_text() == "", "Unicode profile deletion", 30)
    winid = (stage / "app.winid").read_text().strip()
    assert winid.isdecimal()
    guest_command(f"echo 'resize -r 40 40 860 640' > {guest}/control/wsys/{winid}/wctl")
    time.sleep(2)
    screenshot("resized")
    assert Image.open(output / "resized.png").getpixel((900, 700)) == (119, 119, 119), "Owned window did not resize"
    restart(mini=True)
    screenshot("mini")
    click(478, 74)
    time.sleep(2)
    screenshot("expanded")
    assert max(Image.open(output / "expanded.png").getpixel((950, 600))) < 100, "Mini did not expand into the full app"
    assert all(path.stat().st_mode & 0o777 == 0o600 for path in (stage / "data").iterdir())
    print("Plan 9: restart, profile reload/deletion, Unicode paste, native resize and mini expansion pass", flush=True)
finally:
    if connection is not None:
        try:
            screenshot("final")
            qmp("quit")
        except (OSError, ValueError, AssertionError):
            pass
        connection.close()
    if vm.poll() is None:
        os.killpg(vm.pid, signal.SIGTERM)
    try:
        vm.wait(timeout=10)
    except subprocess.TimeoutExpired:
        os.killpg(vm.pid, signal.SIGKILL)
        vm.wait()
    log.close()
    # Keep generated test artifacts and data for inspection; no source checkout.
    (output / "stage.txt").write_text(str(stage) + "\n")
