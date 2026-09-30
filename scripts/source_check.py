#!/usr/bin/env python3
"""Reject handwritten application implementations outside Ziran."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
tracked = subprocess.check_output(["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"], cwd=root).decode().split("\0")
bad = []
for name in tracked:
    if not name or not (root / name).is_file():
        continue
    path = Path(name)
    if path.suffix in {".c", ".h", ".cc", ".cpp", ".cxx", ".hpp", ".kry", ".java", ".kt", ".go", ".rs", ".swift", ".m", ".mm", ".zig"}:
        bad.append(name)
    if name.startswith("web/site/app/") and path.suffix == ".js":
        bad.append(name)
if bad:
    raise SystemExit("Non-Ziran app implementation:\n" + "\n".join(bad))
manifest = (root / "droid/app/src/main/AndroidManifest.xml").read_text()
assert 'android:hasCode="false"' in manifest
assert 'android:name="android.app.NativeActivity"' in manifest
assert "android.permission.INTERNET" not in manifest
print("All maintained Pass application implementations are Ziran")
