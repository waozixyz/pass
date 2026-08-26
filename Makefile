.PHONY: all cli kry-c gui native run test native-test cli-test lesspass-compat-test check-submodule-urls kry-smoke web web-canvas site web-smoke android-debug android-smoke android-input-test e2e install install-cli uninstall-cli install-gui uninstall-gui package-deb package-appimage

BIN_DIR ?= $(HOME)/bin
DATA_DIR ?= $(if $(XDG_DATA_HOME),$(XDG_DATA_HOME),$(HOME)/.local/share)
CC ?= cc

KRYON_ARCH ?= $(shell uname -m)
KRYON_BUILD_DIR := vendor/kryon/build/linux-$(KRYON_ARCH)
KRYON_RUNTIME_SOURCES := $(shell find vendor/kryon/src vendor/kryon/include -type f)
K2C_SOURCES := $(shell find vendor/kryon/cmd/k2c vendor/kryon/cmd/kir -type f)
KRY_APP_SRCS := $(shell find app -type f -name '*.kry' | LC_ALL=C sort)
KRY_C_GENERATED_DIR := build/krygen/c
KRY_C_STAMP := $(KRY_C_GENERATED_DIR)/.stamp
KRY_C_APP_SRCS := $(KRY_C_GENERATED_DIR)/app/nav.c $(KRY_C_GENERATED_DIR)/app/pass.c $(KRY_C_GENERATED_DIR)/app/profiles.c $(KRY_C_GENERATED_DIR)/app/settings.c
K2C := $(KRYON_BUILD_DIR)/bin/k2c
PASS_VERSION := $(shell sed -n '1p' VERSION)
WEB_EMSDK_BIN ?= $(HOME)/emsdk/upstream/emscripten
WEB_CC ?= $(if $(wildcard $(WEB_EMSDK_BIN)/emcc),$(WEB_EMSDK_BIN)/emcc,emcc)
ifneq ($(wildcard $(WEB_EMSDK_BIN)/emcc),)
export PATH := $(WEB_EMSDK_BIN):$(PATH)
endif
WEB_APP_BUILD_DIR := build/web-app
WEB_APP_TARGET := $(WEB_APP_BUILD_DIR)/index.html
WEB_APP_EMBEDDED_ASSETS_C := build/web/pass_embedded_assets.c
WEB_APP_ASSETS := vendor/kryon/fonts/noto/NotoSans-Regular.ttf assets/fonts/emoji.ttf
KRYON_ICON_ASSETS_C := vendor/kryon/src/ui/ui_icon_assets.c
KRYON_WEB_SRCS_ALL := $(filter-out $(KRYON_ICON_ASSETS_C),$(shell find vendor/kryon/src -type f -name '*.c' | LC_ALL=C sort)) $(KRYON_ICON_ASSETS_C)
KRYON_WEB_SRCS := $(filter-out vendor/kryon/src/backend/libdraw_% vendor/kryon/src/backend/termi_% vendor/kryon/src/file_dialog/file_dialog.c vendor/kryon/src/ksync/% vendor/kryon/src/runtime_assets/% vendor/kryon/src/notification/% vendor/kryon/src/platform/plan9/% vendor/kryon/src/scene/physics_world.c vendor/kryon/src/scene/node_body2d.c vendor/kryon/src/scene/node_area2d.c vendor/kryon/src/scene/node_collision_shape2d.c,$(KRYON_WEB_SRCS_ALL))
PASS_WEB_SRCS := droid/app/src/main/cpp/main.c droid/app/src/main/cpp/android_bridge.c native/pass_core.c native/pass_runtime.c $(KRY_C_APP_SRCS) $(WEB_APP_EMBEDDED_ASSETS_C)
WEB_CFLAGS := -Wall -Wextra -std=gnu99 -Os -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 -D_DEFAULT_SOURCE -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -DUI_EMBEDDED_ONLY=1 -DKRYON_WITH_PHYSICS=0 -Ivendor/kryon/include -Ivendor/kryon/src -I$(KRY_C_GENERATED_DIR) -Inative -Idroid/app/src/main/cpp
WEB_LDFLAGS := -sASYNCIFY -sASYNCIFY_STACK_SIZE=1048576 -sFORCE_FILESYSTEM=1 -sFETCH=1 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=134217728 -sSTACK_SIZE=16777216 -lidbfs.js -lm
GUI_CFLAGS := -Wall -Wextra -std=gnu99 -O2 -D_DEFAULT_SOURCE -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -DUI_EMBEDDED_ONLY=1 -DKRYON_WITH_PHYSICS=0 -Ivendor/kryon/include -Ivendor/kryon/src -I$(KRY_C_GENERATED_DIR) -Inative -Idroid/app/src/main/cpp $(shell pkg-config --cflags sdl2 gtk+-3.0 2>/dev/null)
GUI_LDLIBS := $(shell pkg-config --libs sdl2 2>/dev/null) $(shell pkg-config --libs libdrm gbm egl glesv2 2>/dev/null) $(shell pkg-config --libs gtk+-3.0 gio-2.0 glib-2.0 2>/dev/null) -ldl -lpthread -lm
KRYON_STATIC_LIBS := $(KRYON_BUILD_DIR)/libkryon.a $(KRYON_BUILD_DIR)/raylib/libraylib.a

all: cli gui

cli: build/pass
	cp build/pass pass

build/pass: native/pass_cli.c native/pass_core.c native/pass_core.h | build
	$(CC) -Wall -Wextra -O2 -std=gnu99 -Inative -DPASS_VERSION=\"$(PASS_VERSION)\" native/pass_cli.c native/pass_core.c -o $@

build build/gui:
	mkdir -p $@

$(KRYON_BUILD_DIR)/libkryon.a: $(KRYON_RUNTIME_SOURCES) vendor/kryon/Makefile
	$(MAKE) -C vendor/kryon -f Makefile all

$(K2C): $(K2C_SOURCES) vendor/kryon/Makefile
	$(MAKE) -C vendor/kryon -f Makefile k2c

kry-c: $(KRY_C_STAMP)

$(KRY_C_STAMP): $(K2C) $(KRY_APP_SRCS) | build
	rm -rf $(KRY_C_GENERATED_DIR)
	mkdir -p $(KRY_C_GENERATED_DIR)
	$(K2C) --no-main --root . -o $(KRY_C_GENERATED_DIR) $(KRY_APP_SRCS)
	touch $@

gui: build/pass-gui

build/pass-gui: $(KRYON_BUILD_DIR)/libkryon.a $(KRY_C_STAMP) droid/app/src/main/cpp/main.c droid/app/src/main/cpp/android_bridge.c native/pass_core.c native/pass_runtime.c native/pass_runtime.h | build
	$(CC) $(GUI_CFLAGS) -o $@ \
		droid/app/src/main/cpp/main.c \
		droid/app/src/main/cpp/android_bridge.c \
		native/pass_core.c \
		native/pass_runtime.c \
		$(KRY_C_APP_SRCS) \
		-Wl,-export-dynamic \
		$(KRYON_STATIC_LIBS) \
		$(GUI_LDLIBS)

native: all

run: gui
	./build/pass-gui

install: install-cli install-gui

install-cli: cli
	mkdir -p $(BIN_DIR)
	install -m 0755 pass $(BIN_DIR)/pass

uninstall-cli:
	rm -f $(BIN_DIR)/pass

install-gui: gui
	mkdir -p $(BIN_DIR) $(DATA_DIR)/applications $(DATA_DIR)/icons/hicolor/512x512/apps
	install -m 0755 build/pass-gui $(BIN_DIR)/pass-gui
	cp packaging/linux/xyz.waozi.pass.desktop $(DATA_DIR)/applications/xyz.waozi.pass.desktop
	cp assets/app/icon.png $(DATA_DIR)/icons/hicolor/512x512/apps/pass.png

uninstall-gui:
	rm -f $(BIN_DIR)/pass-gui $(DATA_DIR)/applications/xyz.waozi.pass.desktop $(DATA_DIR)/icons/hicolor/512x512/apps/pass.png

test:
	$(MAKE) native-test cli-test lesspass-compat-test kry-smoke

# Checks the C generator used by the CLI and every Kry app target.
native-test:
	mkdir -p build
	cc -Wall -Wextra -O2 -Inative native/pass_core.c native/pass_core_test.c -o build/pass_core_test
	./build/pass_core_test

cli-test: cli
	test "$$(./pass lesspass.com contact@lesspass.com password)" = '\g-A1-.OHEwrXjT#'
	test "$$(./pass --length 20 --counter 2 service.test person@example.net master)" = 'j:x_Lo5b1XL_j0we%z`e'

lesspass-compat-test: cli
	python3 scripts/lesspass_compat_test.py --cli ./pass

check-submodule-urls:
	bash scripts/check_submodule_urls.sh

kry-smoke:
	sh scripts/kry_smoke.sh

ANDROID_DIR := droid
ANDROID_ABIS ?= armeabi-v7a,arm64-v8a

android-debug: kry-c
	cd $(ANDROID_DIR) && ./gradlew assembleDebug -Pabi=$(ANDROID_ABIS) -PsplitApks=true
	mkdir -p build
	cp $(ANDROID_DIR)/app/build/outputs/apk/debug/app-universal-debug.apk build/pass-android-debug.apk
	cp $(ANDROID_DIR)/app/build/outputs/apk/debug/app-arm64-v8a-debug.apk build/pass-android-arm64-v8a-debug.apk
	cp $(ANDROID_DIR)/app/build/outputs/apk/debug/app-armeabi-v7a-debug.apk build/pass-android-armeabi-v7a-debug.apk

android-smoke: android-debug
	bash scripts/android_smoke.sh

android-input-test: kry-c
	cd $(ANDROID_DIR) && ./gradlew connectedDebugAndroidTest -Pabi=x86_64 -PsplitApks=false

web:
	./web/build.sh

web-canvas: $(WEB_APP_TARGET)

$(WEB_APP_BUILD_DIR) build/web:
	mkdir -p $@

$(WEB_APP_EMBEDDED_ASSETS_C): $(WEB_APP_ASSETS) vendor/kryon/scripts/embed-assets.sh | build/web
	sh vendor/kryon/scripts/embed-assets.sh $@ $(WEB_APP_ASSETS)

$(WEB_APP_TARGET): Makefile $(KRY_C_STAMP) $(PASS_WEB_SRCS) $(KRYON_WEB_SRCS) web/site/app/index.html web/site/app/app.js | $(WEB_APP_BUILD_DIR)
	$(WEB_CC) $(WEB_CFLAGS) \
		-o $(WEB_APP_BUILD_DIR)/index.js \
		$(PASS_WEB_SRCS) \
		$(KRYON_WEB_SRCS) \
		$(WEB_LDFLAGS)
	cp web/site/app/index.html $(WEB_APP_TARGET)
	cp web/site/app/app.js $(WEB_APP_BUILD_DIR)/app.js

site: web
	test -f build/site/index.html
	test -f build/site/app/index.wasm

web-smoke: site
	bash scripts/web_smoke.sh

e2e: check-submodule-urls test native-test gui web-canvas site web-smoke android-debug android-smoke

package-deb: gui
	./scripts/package-deb.sh

package-appimage: gui
	./scripts/package-appimage.sh
