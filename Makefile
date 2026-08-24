.PHONY: all cli gui native run test native-test check-submodule-urls kry-smoke web web-canvas site web-smoke android-debug android-smoke e2e install install-cli uninstall-cli install-gui uninstall-gui package-deb package-appimage

BIN_DIR ?= $(HOME)/bin
DATA_DIR ?= $(if $(XDG_DATA_HOME),$(XDG_DATA_HOME),$(HOME)/.local/share)

KRYON_ARCH ?= $(shell uname -m)
KRYON_BUILD_DIR := vendor/kryon/build/linux-$(KRYON_ARCH)
KRYON_RUNTIME_SOURCES := $(shell find vendor/kryon/src vendor/kryon/include -type f)
WEB_EMSDK_BIN ?= $(HOME)/emsdk/upstream/emscripten
WEB_CC ?= $(if $(wildcard $(WEB_EMSDK_BIN)/emcc),$(WEB_EMSDK_BIN)/emcc,emcc)
ifneq ($(wildcard $(WEB_EMSDK_BIN)/emcc),)
export PATH := $(WEB_EMSDK_BIN):$(PATH)
endif
WEB_APP_BUILD_DIR := build/web-app
WEB_APP_TARGET := $(WEB_APP_BUILD_DIR)/index.html
WEB_APP_EMBEDDED_ASSETS_C := build/web/pass_embedded_assets.c
WEB_APP_ASSETS := vendor/kryon/fonts/noto/NotoSans-Regular.ttf gui/assets/emoji.ttf
KRYON_ICON_ASSETS_C := vendor/kryon/src/ui/ui_icon_assets.c
KRYON_WEB_SRCS_ALL := $(filter-out $(KRYON_ICON_ASSETS_C),$(shell find vendor/kryon/src -type f -name '*.c' | LC_ALL=C sort)) $(KRYON_ICON_ASSETS_C)
KRYON_WEB_SRCS := $(filter-out vendor/kryon/src/file_dialog/file_dialog.c vendor/kryon/src/ksync/% vendor/kryon/src/runtime_assets/% vendor/kryon/src/notification/% vendor/kryon/src/scene/physics_world.c vendor/kryon/src/scene/node_body2d.c vendor/kryon/src/scene/node_area2d.c vendor/kryon/src/scene/node_collision_shape2d.c,$(KRYON_WEB_SRCS_ALL))
PASS_WEB_SRCS := droid/app/src/main/cpp/main.c droid/app/src/main/cpp/pass_app.c droid/app/src/main/cpp/android_bridge.c native/pass_core.c $(WEB_APP_EMBEDDED_ASSETS_C)
WEB_CFLAGS := -Wall -Wextra -std=gnu99 -Os -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 -D_DEFAULT_SOURCE -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -DUI_EMBEDDED_ONLY=1 -DKRYON_WITH_PHYSICS=0 -Ivendor/kryon/include -Ivendor/kryon/src -Inative
WEB_LDFLAGS := -sASYNCIFY -sASYNCIFY_STACK_SIZE=1048576 -sFORCE_FILESYSTEM=1 -sFETCH=1 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=134217728 -sSTACK_SIZE=16777216 -lidbfs.js -lm

all: cli gui

cli:
	go build -o pass ./cmd/pass

$(KRYON_BUILD_DIR)/libkryon.a: $(KRYON_RUNTIME_SOURCES) vendor/kryon/Makefile
	$(MAKE) -C vendor/kryon -f Makefile all

gui: $(KRYON_BUILD_DIR)/libkryon.a
	cd gui && go build -o ../build/pass-gui .

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
	go test ./...
	cd gui && go test ./...

# Checks the C generator (used by the Android app) against the same fixed
# vectors as the Go tests.
native-test:
	cc -Wall -Wextra -O2 -Inative native/pass_core.c native/pass_core_test.c -o build/pass_core_test
	./build/pass_core_test

check-submodule-urls:
	bash scripts/check_submodule_urls.sh

kry-smoke:
	sh scripts/kry_smoke.sh

ANDROID_DIR := droid
ANDROID_ABIS ?= armeabi-v7a,arm64-v8a

android-debug:
	cd $(ANDROID_DIR) && ./gradlew assembleDebug -Pabi=$(ANDROID_ABIS) -PsplitApks=true
	mkdir -p build
	cp $(ANDROID_DIR)/app/build/outputs/apk/debug/app-universal-debug.apk build/pass-android-debug.apk
	cp $(ANDROID_DIR)/app/build/outputs/apk/debug/app-arm64-v8a-debug.apk build/pass-android-arm64-v8a-debug.apk
	cp $(ANDROID_DIR)/app/build/outputs/apk/debug/app-armeabi-v7a-debug.apk build/pass-android-armeabi-v7a-debug.apk

android-smoke: android-debug
	bash scripts/android_smoke.sh

web:
	./web/build.sh

web-canvas: $(WEB_APP_TARGET)

$(WEB_APP_BUILD_DIR) build/web:
	mkdir -p $@

$(WEB_APP_EMBEDDED_ASSETS_C): $(WEB_APP_ASSETS) vendor/kryon/scripts/embed-assets.sh | build/web
	sh vendor/kryon/scripts/embed-assets.sh $@ $(WEB_APP_ASSETS)

$(WEB_APP_TARGET): Makefile $(PASS_WEB_SRCS) $(KRYON_WEB_SRCS) web/site/app/index.html web/site/app/app.js | $(WEB_APP_BUILD_DIR)
	$(WEB_CC) $(WEB_CFLAGS) \
		-o $(WEB_APP_BUILD_DIR)/index.js \
		$(PASS_WEB_SRCS) \
		$(KRYON_WEB_SRCS) \
		$(WEB_LDFLAGS)
	cp web/site/app/index.html $(WEB_APP_TARGET)
	cp web/site/app/app.js $(WEB_APP_BUILD_DIR)/app.js

site: web
	test -f build/site/index.html
	test -f build/site/app/pass.wasm
	test -f build/site/app/index.wasm

web-smoke: site
	bash scripts/web_smoke.sh

e2e: check-submodule-urls test native-test gui web-canvas site web-smoke android-debug android-smoke

package-deb: gui
	./scripts/package-deb.sh

package-appimage: gui
	./scripts/package-appimage.sh
