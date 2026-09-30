.DEFAULT_GOAL := all
ZIRAN ?= ./scripts/ziran.sh
# Local overrides refer to the real root repositories. Published builds
# require the exact versions recorded in the committed lock.
LOCK_FLAGS := $(if $(wildcard ziran.local.toml),,--locked)
ZIRAN_ROOT = $(shell $(ZIRAN) pkg path ziran $(LOCK_FLAGS))
KRYON_DIR = $(shell $(ZIRAN) pkg path Kryon $(LOCK_FLAGS))
RAYLIB_DIR = $(shell $(ZIRAN) pkg path raylib $(LOCK_FLAGS))/src
CC ?= cc
BIN_DIR ?= $(HOME)/bin
DATA_DIR ?= $(if $(XDG_DATA_HOME),$(XDG_DATA_HOME),$(HOME)/.local/share)
WEB_CC ?= $(if $(wildcard $(HOME)/emsdk/upstream/emscripten/emcc),$(HOME)/emsdk/upstream/emscripten/emcc,emcc)
SOURCES := $(wildcard src/*.zi) pass_core.zi pass_core_test.zi
LIBRARY_SOURCES = $(wildcard $(KRYON_DIR)/src/ui/*.zi $(KRYON_DIR)/src/ui/*/module.zi $(KRYON_DIR)/src/kss/*.zi $(KRYON_DIR)/src/backend/*.zi $(KRYON_DIR)/src/backend/*/module.zi $(ZIRAN_ROOT)/std/*.zi)
CFLAGS := -std=c99 -O2 -Wno-unused-variable -I$(ZIRAN_ROOT)/include
PROJECT := --project $(LOCK_FLAGS)
ANDROID_ABIS ?= armeabi-v7a,arm64-v8a

.PHONY: all cli gui native run test source-check core-test ziran-core-test cli-test runtime-test lesspass-compat-test gui-smoke web web-canvas site web-smoke android-debug android-emulator android-release android-smoke android-input-test e2e check-package-urls install install-cli install-gui uninstall-cli uninstall-gui package-deb package-appimage plan9-c
all: cli gui
native: all

build/generated/cli/.complete: $(SOURCES) ziran.toml ziran.lock
	mkdir -p $(@D)
	$(ZIRAN) build $(PROJECT) --target=c --entry cli:main -o $(@D) src/cli.zi
	touch $@
build/pass: build/generated/cli/.complete
	$(CC) $(CFLAGS) -Ibuild/generated/cli build/generated/cli/*.c -o $@
cli: build/pass
	cp build/pass pass

build/generated/desktop/.complete: $(SOURCES) $(LIBRARY_SOURCES) ziran.toml ziran.lock
	mkdir -p $(@D)
	$(ZIRAN) build $(PROJECT) --target=c --entry desktop:main -o $(@D) src/desktop.zi
	touch $@
build/pass-gui: build/generated/desktop/.complete
	$(CC) $(CFLAGS) -Ibuild/generated/desktop build/generated/desktop/*.c $(shell pkg-config --libs sdl2 cairo) -lm -o $@
gui: build/pass-gui
run: gui
	./build/pass-gui

source-check:
	python3 scripts/source_check.py
	$(ZIRAN) fmt --check $(SOURCES) tests/*.zi
core-test ziran-core-test:
	sh scripts/ziran_core_test.sh "$(ZIRAN)"
runtime-test:
	sh scripts/runtime_test.sh "$(ZIRAN)"
cli-test: cli
	sh scripts/cli_test.sh ./pass
lesspass-compat-test: cli
	python3 scripts/lesspass_compat_test.py --cli ./pass
test: source-check core-test runtime-test cli-test lesspass-compat-test
gui-smoke: gui
	sh scripts/gui_smoke.sh

build/generated/browser/.complete: $(SOURCES) $(LIBRARY_SOURCES) ziran.toml ziran.lock
	mkdir -p $(@D)
	$(ZIRAN) build $(PROJECT) --define PLATFORM_WEB --target=c -o $(@D) src/browser.zi
	touch $@
build/web-app/index.js: build/generated/browser/.complete assets/app/fingerprint.png
	mkdir -p $(@D)
	$(WEB_CC) -O2 -Wno-parentheses-equality -I$(ZIRAN_ROOT)/include -iquote build/generated/browser build/generated/browser/*.c \
		--js-library $(ZIRAN_ROOT)/web/ziran_web.js -sEXPORTED_RUNTIME_METHODS=FS,IDBFS -lidbfs.js \
		-sASYNCIFY -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=16777216 -sASYNCIFY_STACK_SIZE=1048576 -sENVIRONMENT=web \
		--embed-file $(KRYON_DIR)/assets/fonts/LiberationSans-Regular.ttf@/kryon-font.ttf --embed-file assets/app/fingerprint.png@/fingerprint.png -o $@
build/web-app/index.html: build/web-app/index.js web/site/app/index.html
	cp web/site/app/index.html $@
build/generated/worker/.complete: src/service_worker.zi ziran.toml ziran.lock
	mkdir -p $(@D)
	$(ZIRAN) build $(PROJECT) --target=c -o $(@D) src/service_worker.zi
	touch $@
build/web-app/sw.js: build/generated/worker/.complete
	$(WEB_CC) -O2 -I$(ZIRAN_ROOT)/include -iquote build/generated/worker build/generated/worker/*.c \
		--js-library $(ZIRAN_ROOT)/web/ziran_web.js -sENVIRONMENT=worker -sNO_EXIT_RUNTIME=1 \
		-sSINGLE_FILE=1 -sWASM_ASYNC_COMPILATION=0 -o $@
web-canvas: build/web-app/index.html build/web-app/sw.js
web site: web-canvas
	./web/build.sh
web-smoke: site
	bash scripts/web_smoke.sh

# CMake emits JNI and foreign ABI types separately for each Android ABI.
android-debug:
	cd droid && env PASS_ZIRAN_COMMAND="$(ZIRAN)" KRYON_DIR="$(KRYON_DIR)" ZIRAN_ROOT="$(ZIRAN_ROOT)" RAYLIB_DIR="$(RAYLIB_DIR)" ./gradlew assembleDebug -Pabi=$(ANDROID_ABIS) -PsplitApks=true $(GRADLE_ARGS)
	mkdir -p build
	cp droid/app/build/outputs/apk/debug/app-universal-debug.apk build/pass-android-debug.apk
	python3 scripts/android_apk_check.py build/pass-android-debug.apk
	cp droid/app/build/outputs/apk/debug/app-arm64-v8a-debug.apk build/pass-android-arm64-v8a-debug.apk
	cp droid/app/build/outputs/apk/debug/app-armeabi-v7a-debug.apk build/pass-android-armeabi-v7a-debug.apk
android-emulator:
	cd droid && env PASS_ZIRAN_COMMAND="$(ZIRAN)" KRYON_DIR="$(KRYON_DIR)" ZIRAN_ROOT="$(ZIRAN_ROOT)" RAYLIB_DIR="$(RAYLIB_DIR)" ./gradlew assembleDebug -Pabi=x86_64 $(GRADLE_ARGS)
	mkdir -p build
	cp droid/app/build/outputs/apk/debug/app-debug.apk build/pass-android-emulator.apk
	python3 scripts/android_apk_check.py build/pass-android-emulator.apk
android-release:
	cd droid && env PASS_ZIRAN_COMMAND="$(ZIRAN)" KRYON_DIR="$(KRYON_DIR)" ZIRAN_ROOT="$(ZIRAN_ROOT)" RAYLIB_DIR="$(RAYLIB_DIR)" ./gradlew assembleRelease -Pabi=$(ANDROID_ABIS) -PsplitApks=true $(GRADLE_ARGS)
android-smoke:
	bash scripts/android_smoke.sh
android-input-test:
	python3 scripts/android_input_test.py

plan9-c:
	mkdir -p build/plan9/core
	$(ZIRAN) build $(PROJECT) --target=plan9-c -o build/plan9/core pass_core.zi

check-package-urls:
	bash scripts/check_package_urls.sh
e2e: check-package-urls test gui-smoke web-smoke android-debug

install: install-cli install-gui
install-cli: cli
	mkdir -p $(BIN_DIR)
	install -m 0755 pass $(BIN_DIR)/pass
install-gui: gui
	mkdir -p $(BIN_DIR) $(DATA_DIR)/applications $(DATA_DIR)/icons/hicolor/512x512/apps $(DATA_DIR)/pass
	install -m 0755 build/pass-gui $(BIN_DIR)/pass-gui
	cp packaging/linux/xyz.waozi.pass.desktop $(DATA_DIR)/applications/xyz.waozi.pass.desktop
	cp assets/app/icon.png $(DATA_DIR)/icons/hicolor/512x512/apps/pass.png
	cp assets/app/fingerprint.png $(DATA_DIR)/pass/fingerprint.png
	cp assets/fonts/emoji-OFL.txt $(DATA_DIR)/pass/emoji-OFL.txt
uninstall-cli:
	rm -f $(BIN_DIR)/pass
uninstall-gui:
	rm -f $(BIN_DIR)/pass-gui $(DATA_DIR)/applications/xyz.waozi.pass.desktop $(DATA_DIR)/icons/hicolor/512x512/apps/pass.png $(DATA_DIR)/pass/fingerprint.png $(DATA_DIR)/pass/emoji-OFL.txt
package-deb: gui
	./scripts/package-deb.sh
package-appimage: gui
	./scripts/package-appimage.sh
