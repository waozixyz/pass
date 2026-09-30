#!/usr/bin/env python3
"""Reject packaged legacy app classes; Android generates resource constants."""
import re
import struct
import sys
from zipfile import ZipFile


def dex_classes(data):
    assert data.startswith(b"dex\n"), "Invalid DEX header"
    word = lambda offset: struct.unpack_from("<I", data, offset)[0]
    strings, types = word(60), word(68)
    count, definitions = word(96), word(100)
    for index in range(count):
        descriptor = word(types + word(definitions + index * 32) * 4)
        offset = word(strings + descriptor * 4)
        # Skip the ULEB128 string length, then read its ASCII descriptor.
        while data[offset] & 128:
            offset += 1
        offset += 1
        yield data[offset:data.index(0, offset)].decode("ascii")


def check_apk(path):
    with ZipFile(path) as archive:
        names = archive.namelist()
        for name in names:
            if name.endswith(".dex"):
                classes = list(dex_classes(archive.read(name)))
                assert all(re.fullmatch(r"Lxyz/waozi/pass/R(?:\$[^;]+)?;", item) for item in classes), f"{path}: packaged application bytecode: {classes}"
        for font in ["ui.ttf", "emoji.ttf"]:
            assert "assets/fonts/" + font in names, f"{path}: missing {font}"
        assert "assets/fingerprint.png" in names, f"{path}: missing fingerprint images"
        assert any(name.endswith("/libmain.so") for name in names), f"{path}: no NativeActivity payload"
    print(f"{path}: native Ziran app; only generated resource classes")


if __name__ == "__main__":
    for path in sys.argv[1:]:
        check_apk(path)
