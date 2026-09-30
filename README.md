<p align="center">
  <img src="assets/app/readme-banner.png" alt="Pass" width="100%">
</p>

<p align="center">
  <a href="https://f-droid.org/packages/xyz.waozi.pass/">
    <img src="assets/app/badge-f-droid.png" alt="Get it on F-Droid" height="56">
  </a>
</p>

# Pass

Website and browser app: [pass.waozi.xyz](https://pass.waozi.xyz/)

Pass is a stateless password generator: the same site, login, master
password, and settings produce the same result without a password database.
It is a new, independently written implementation of the LessPass generation
algorithm. Compatibility does not imply endorsement by or affiliation with the
LessPass project.

## Install

Prebuilt CLI and desktop downloads are available from
[pass.waozi.xyz](https://pass.waozi.xyz/#downloads). To build from a checkout:

```sh
make cli
```

## Use

```text
pass [OPTIONS] SITE LOGIN [MASTER_PASSWORD]
```

The default password has 16 characters and includes lowercase letters,
uppercase letters, digits, and symbols. Length must be between 5 and 35,
matching LessPass clients. The default counter is 1.

Passing a master password as an argument can expose it to process inspection
and shell history. Omitting it is safer: `pass` first checks
`LESSPASS_MASTER_PASSWORD`, then reads one line from standard input. Use
`--prompt` to bypass the environment and read from standard input. For example:

```sh
pass --prompt example.com alice
pass --length 24 --counter 2 example.com alice
pass --no-symbols --exclude '0O1Il' example.com alice
```

The CLI is stateless. The app adds clipboard clearing, profiles, settings,
master-password masking, and Android device authentication. Profiles and
settings keep their existing storage formats. A master password saved by the
old Android host must be saved again with the new host; generation does not
require storing a master password.

Run `pass --help` for all flags.

## Build and test

Every maintained Pass implementation is Ziran: password derivation, CLI,
Kryon screens, persistence, clipboard policy, Android JNI calls, and browser
offline caching. C, WebAssembly, and the browser JavaScript loaders are
compiler outputs under `build/`. Shell, Python, and Gradle files orchestrate
builds, packaging, and tests.

Dependencies and the compiler are pinned in `ziran.lock`. Desktop builds need
SDL2 and Cairo development packages; browser builds need Emscripten; Android
builds need JDK 21 and the SDK/NDK versions in `droid/app/build.gradle`.

```sh
make test           # native core/CLI, independent reference, portable runtime
make gui            # build/pass-gui
make gui-smoke      # private Xvfb display
make gui-input-test # typing, Generate/Copy, profiles and restart on private Xvfb
make site           # build/site: browser app and generated offline worker
make web-smoke      # private headless Chromium
make android-debug  # ARM32, ARM64 and universal APKs
make android-emulator # build the x86_64 APK
make android-input-test # keyboard, Generate/Copy and persistence in a private emulator
```

The Android app supports API 21 and newer and declares no Internet permission.
Optional master-password storage uses AndroidKeyStore and device
credential authentication on API 23 and newer. Browser profiles/settings use
IndexedDB; offline use requires an initial successful online load.

For development with the real sibling repositories, create an ignored
`ziran.local.toml`:

```toml
[overrides]
Kryon = "../kryon"
ziran = "../ziran"
```

`make plan9-c` emits the password core in Plan 9 C. Native Plan 9 GUI support
is pending Kryon's native libdraw input and run profile; there is no maintained
legacy C frontend.

## Source layout

```text
pass_core.zi           password derivation and fingerprint
src/cli.zi             command-line entry point
src/app.zi             shared Kryon screens
src/runtime.zi         settings, profiles, clipboard and secure actions
src/theme.zi           native Kryon style rules
src/host.zi            storage and platform capabilities
src/desktop.zi         desktop lifecycle
src/browser.zi         browser lifecycle
src/service_worker.zi  offline caching
src/android*.zi        NativeActivity, JNI and Android lifecycle
```

## License

Copyright © 2026 Waozi. Distributed under the BSD 3-Clause License. See
[`LICENSE`](LICENSE).
