# pass

Website and browser app: [pass.waozi.xyz](https://pass.waozi.xyz/)

`pass` is a stateless password generator: the same site, login, master
password, and settings produce the same result without a password database.
It is a new, independently written implementation of the LessPass generation
algorithm. Compatibility does not imply endorsement by or affiliation with the
LessPass project.

## Install

Prebuilt CLI and desktop downloads are available from
[pass.waozi.xyz](https://pass.waozi.xyz/#downloads). To build from a checkout:

```sh
git submodule update --init --recursive
make cli
```

## Use

```text
pass [OPTIONS] SITE LOGIN [MASTER_PASSWORD]
```

The default password has 16 characters and includes lowercase letters,
uppercase letters, digits, and symbols. The default counter is 1.

Passing a master password as an argument can expose it to process inspection
and shell history. Omitting it is safer: `pass` first checks
`LESSPASS_MASTER_PASSWORD`, then reads one line from standard input. Use
`--prompt` to bypass the environment and read from standard input. For example:

```sh
pass --prompt example.com alice
pass --length 24 --counter 2 example.com alice
pass --no-symbols --exclude '0O1Il' example.com alice
```

Clipboard copying, profile storage, settings, and Android fingerprint unlock
live in the Kry app runtime. The C CLI stays stateless and prints the generated
password.

Run `pass --help` for all flags.

## Desktop app

The desktop, Android, and browser apps are written in Kry and compiled with
Kryon's `k2c` compiler to C. They share `native/pass_core.c` for password
generation and `native/pass_runtime.c` for app storage, clipboard, and platform
hooks.

```sh
git submodule update --init --recursive
make gui
./build/pass-gui
```

## Android

The Android app runs on Android 5.0 (API 21) and newer. Install the signed
APK (`pass-android.apk`) from [pass.waozi.xyz](https://pass.waozi.xyz/#downloads)
or the [releases page](https://github.com/waozixyz/pass/releases/latest). It
uses the same generation algorithm as the desktop app, computes everything
locally, and declares no Internet permission at all. The only permissions it
requests are for the optional fingerprint unlock.

## Source Layout

The app has one UI implementation:

```text
app/*.kry -> k2c -> generated C -> desktop / Android / web
native/pass_core.c -> password derivation
native/pass_runtime.c -> app-facing runtime externs
native/pass_cli.c -> command-line frontend
```

## License

Copyright © 2026 Waozi. Distributed under the BSD 3-Clause License. See
[`LICENSE`](LICENSE).
