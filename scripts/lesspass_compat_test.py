#!/usr/bin/env python3
import argparse
import hashlib
import subprocess
import sys


CHARACTER_SUBSETS = {
    "lowercase": "abcdefghijklmnopqrstuvwxyz",
    "uppercase": "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
    "digits": "0123456789",
    "symbols": "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~",
}


def consume_entropy(password, entropy, characters, max_length):
    while len(password) < max_length:
        entropy, remainder = divmod(entropy, len(characters))
        password += characters[remainder]
    return password, entropy


def available_characters(rule, exclude):
    characters = "".join(c for c in CHARACTER_SUBSETS[rule] if c not in exclude)
    if not characters:
        raise ValueError(f"{rule} has no characters after exclusions")
    return characters


def lesspass_password(profile, master):
    rules = [rule for rule in ("lowercase", "uppercase", "digits", "symbols")
             if profile[rule]]
    salt = profile["site"] + profile["login"] + format(profile["counter"], "x")
    entropy = int.from_bytes(
        hashlib.pbkdf2_hmac("sha256", master.encode("utf-8"),
                            salt.encode("utf-8"), 100000, 32),
        "big",
    )
    alphabet = "".join(available_characters(rule, profile.get("exclude", ""))
                       for rule in rules)
    password, entropy = consume_entropy("", entropy, alphabet,
                                        profile["length"] - len(rules))
    required = ""
    for rule in rules:
        value, entropy = consume_entropy(
            "", entropy, available_characters(rule, profile.get("exclude", "")), 1)
        required += value
    for char in required:
        entropy, position = divmod(entropy, len(password))
        password = password[:position] + char + password[position:]
    return password


def pass_cli(cli, profile, master):
    cmd = [
        cli,
        "--length", str(profile["length"]),
        "--counter", str(profile["counter"]),
    ]
    for rule in ("lowercase", "uppercase", "digits", "symbols"):
        if not profile[rule]:
            cmd.append(f"--no-{rule}")
    if profile.get("exclude"):
        cmd.extend(["--exclude", profile["exclude"]])
    cmd.extend([profile["site"], profile["login"], master])
    return subprocess.check_output(cmd, text=True).rstrip("\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cli", default="./pass")
    args = parser.parse_args()

    cases = [
        ("reviewer default", {
            "site": "lesspass.com",
            "login": "contact@lesspass.com",
            "length": 16,
            "counter": 1,
            "lowercase": True,
            "uppercase": True,
            "digits": True,
            "symbols": True,
        }, "password"),
        ("hex counter 10", {
            "site": "site",
            "login": "login",
            "length": 16,
            "counter": 10,
            "lowercase": True,
            "uppercase": True,
            "digits": True,
            "symbols": True,
        }, "test"),
        ("hex counter 16", {
            "site": "site",
            "login": "login",
            "length": 16,
            "counter": 16,
            "lowercase": True,
            "uppercase": True,
            "digits": True,
            "symbols": True,
        }, "test"),
        ("unicode and two classes", {
            "site": "δοκιμή.example",
            "login": "ユーザー",
            "length": 12,
            "counter": 7,
            "lowercase": True,
            "uppercase": False,
            "digits": True,
            "symbols": False,
        }, "pässword"),
        ("excluded characters", {
            "site": "example.com",
            "login": "alice",
            "length": 24,
            "counter": 1,
            "lowercase": True,
            "uppercase": True,
            "digits": True,
            "symbols": True,
            "exclude": "0Ool1I!|",
        }, "correct horse battery staple"),
    ]

    for name, profile, master in cases:
        expected = lesspass_password(profile, master)
        actual = pass_cli(args.cli, profile, master)
        if actual != expected:
            print(f"FAIL {name}: got {actual!r}, want {expected!r}")
            return 1
        print(f"ok   {name}: {actual}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
