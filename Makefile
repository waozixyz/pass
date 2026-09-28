.PHONY: all cli kry-c kry-c-plan9 gui native run test coverage native-test cli-test runtime-test lesspass-compat-test check-package-urls kry-smoke ziran-core-test ziran-compat-test web web-canvas site web-smoke android-debug android-smoke android-input-test e2e install install-cli uninstall-cli install-gui uninstall-gui package-deb package-appimage

BIN_DIR ?= $(HOME)/bin
DATA_DIR ?= $(if $(XDG_DATA_HOME),$(XDG_DATA_HOME),$(HOME)/.local/share)
CC ?= cc

KRYON_ARCH ?= $(shell uname -m)
ZIRAN ?= ./scripts/ziran.sh
ZIRAN_ROOT = $(shell $(ZIRAN) pkg path ziran --locked)
KRYON_DIR = $(shell $(ZIRAN) pkg path Kryon --locked --submodules)
KRYON_BUILD_DIR = $(KRYON_DIR)/build/linux-$(KRYON_ARCH)
KRYON_GENERATED_SRC_DIR = $(KRYON_BUILD_DIR)/generated/src
KRYON_RUNTIME_SOURCES = $(shell find $(KRYON_DIR)/src $(KRYON_DIR)/include -type f)
K2C_SOURCES = $(shell find $(KRYON_DIR)/cmd/k2c $(KRYON_DIR)/cmd/kir -type f)
KRY_APP_SRCS := $(shell find app -type f -name '*.kry' | LC_ALL=C sort)
KRY_C_GENERATED_DIR := build/krygen/c
KRY_C_STAMP := $(KRY_C_GENERATED_DIR)/.stamp
KRY_C_APP_SRCS := $(KRY_C_GENERATED_DIR)/app/common.c $(KRY_C_GENERATED_DIR)/app/nav.c $(KRY_C_GENERATED_DIR)/app/pass.c $(KRY_C_GENERATED_DIR)/app/profiles.c $(KRY_C_GENERATED_DIR)/app/settings.c
K2C := $(KRYON_BUILD_DIR)/bin/k2c
ZIRAN_INCLUDE = $(ZIRAN_ROOT)/include
ZIGEN_DIR := build/zigen
ZIGEN_CFLAGS := -Wno-unused-variable
PASS_VERSION := $(shell sed -n '1p' VERSION)
WEB_EMSDK_BIN ?= $(HOME)/emsdk/upstream/emscripten
WEB_CC ?= $(if $(wildcard $(WEB_EMSDK_BIN)/emcc),$(WEB_EMSDK_BIN)/emcc,emcc)
ifneq ($(wildcard $(WEB_EMSDK_BIN)/emcc),)
export PATH := $(WEB_EMSDK_BIN):$(PATH)
endif
WEB_APP_BUILD_DIR := build/web-app
WEB_APP_TARGET := $(WEB_APP_BUILD_DIR)/index.html
WEB_APP_EMBEDDED_ASSETS_C := build/web/pass_embedded_assets.c
ASSET_DIR := build/assets
PASS_FONT_ASSET := $(ASSET_DIR)/fonts/noto/NotoSans-Regular.ttf
PASS_EMOJI_FONT_ASSET := $(ASSET_DIR)/fonts/emoji.ttf
KRYON_GENERATED_SRCS = $(filter-out $(KRYON_GENERATED_SRC_DIR)/kryon_project.c,$(shell find $(KRYON_GENERATED_SRC_DIR) -type f -name '*.c' | LC_ALL=C sort))
KRYON_WEB_SRCS_ALL = $(shell find $(KRYON_DIR)/src -type f -name '*.c' | LC_ALL=C sort) $(KRYON_GENERATED_SRCS)
KRYON_WEB_SRCS = $(filter-out $(KRYON_DIR)/src/backend/dom_% $(KRYON_DIR)/src/backend/libdraw_% $(KRYON_DIR)/src/backend/termi_% $(KRYON_DIR)/src/file_dialog/file_dialog.c $(KRYON_DIR)/src/ksync/% $(KRYON_DIR)/src/sync/% $(KRYON_DIR)/src/runtime_assets/% $(KRYON_DIR)/src/notification/% $(KRYON_DIR)/src/platform/plan9/% $(KRYON_DIR)/src/scene/physics_world.c $(KRYON_DIR)/src/scene/node_body2d.c $(KRYON_DIR)/src/scene/node_area2d.c $(KRYON_DIR)/src/scene/node_collision_shape2d.c,$(KRYON_WEB_SRCS_ALL))
PASS_WEB_SRCS := droid/app/src/main/cpp/main.c native/pass_core_zi.c $(ZIGEN_DIR)/pass_core.c native/pass_runtime.c $(KRY_C_APP_SRCS) $(WEB_APP_EMBEDDED_ASSETS_C)
WEB_CFLAGS := -Wall -Wextra -std=gnu99 -Os -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 -D_DEFAULT_SOURCE -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -DUI_EMBEDDED_ONLY=1 -DKRYON_WITH_PHYSICS=0 -DKRYON_WITH_SYNC=0 -DUTF8PROC_STATIC -I$(KRYON_DIR)/include -I$(KRYON_DIR)/src -I$(KRYON_DIR)/src/ui -I$(KRYON_DIR)/src/platform -I$(KRYON_DIR)/src/backend -I$(KRYON_DIR)/vendor/utf8proc -I$(KRYON_BUILD_DIR)/generated/include -I$(KRYON_GENERATED_SRC_DIR) -I$(KRYON_GENERATED_SRC_DIR)/runtime -I$(KRY_C_GENERATED_DIR) -I$(ZIGEN_DIR) -I$(ZIRAN_INCLUDE) -Inative -Idroid/app/src/main/cpp
WEB_LDFLAGS := -sASYNCIFY -sASYNCIFY_STACK_SIZE=1048576 -sFORCE_FILESYSTEM=1 -sFETCH=1 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=134217728 -sSTACK_SIZE=16777216 -lidbfs.js -lm
GUI_CFLAGS := -Wall -Wextra -std=gnu99 -O2 -D_DEFAULT_SOURCE -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -DUI_EMBEDDED_ONLY=1 -DKRYON_WITH_PHYSICS=0 -I$(KRYON_DIR)/include -I$(KRYON_DIR)/src -I$(KRYON_GENERATED_SRC_DIR) -I$(KRY_C_GENERATED_DIR) -I$(ZIGEN_DIR) -I$(ZIRAN_INCLUDE) -Inative -Idroid/app/src/main/cpp $(shell pkg-config --cflags sdl2 gtk+-3.0 2>/dev/null)
GUI_LDLIBS := $(shell pkg-config --libs sdl2 2>/dev/null) $(shell pkg-config --libs libdrm gbm egl glesv2 2>/dev/null) $(shell pkg-config --libs gtk+-3.0 gio-2.0 glib-2.0 2>/dev/null) -ldl -lpthread -lm
KRYON_STATIC_LIBS := $(KRYON_BUILD_DIR)/libkryon.a $(KRYON_BUILD_DIR)/raylib/libraylib.a

all: cli gui

cli: build/pass
	cp build/pass pass

$(ZIGEN_DIR)/pass_core.c: pass_core.zi | build
	rm -rf $(ZIGEN_DIR)
	$(ZIRAN) build --project --locked --target=c -o $(ZIGEN_DIR) pass_core.zi

build/pass: native/pass_cli.c $(ZIGEN_DIR)/pass_core.c | build
	$(CC) -Wall -Wextra -O2 -std=gnu99 $(ZIGEN_CFLAGS) -I$(ZIGEN_DIR) -I$(ZIRAN_INCLUDE) -DPASS_VERSION=\"$(PASS_VERSION)\" native/pass_cli.c $(ZIGEN_DIR)/pass_core.c -o $@

build build/gui:
	mkdir -p $@

$(KRYON_BUILD_DIR)/libkryon.a: $(KRYON_RUNTIME_SOURCES) $(KRYON_DIR)/Makefile
	$(MAKE) -C $(KRYON_DIR) -f Makefile all

$(K2C): $(K2C_SOURCES) $(KRYON_DIR)/Makefile
	$(MAKE) -C $(KRYON_DIR) -f Makefile k2c

kry-c: $(KRY_C_STAMP)

$(KRY_C_STAMP): $(K2C) $(KRY_APP_SRCS) | build
	rm -rf $(KRY_C_GENERATED_DIR)
	mkdir -p $(KRY_C_GENERATED_DIR)
	$(K2C) --no-main --root . -o $(KRY_C_GENERATED_DIR) $(KRY_APP_SRCS)
	touch $@

$(ASSET_DIR)/fonts/noto $(ASSET_DIR)/fonts:
	mkdir -p $@

$(PASS_FONT_ASSET): $(KRYON_DIR)/fonts/noto/NotoSans-Regular.ttf | $(ASSET_DIR)/fonts/noto
	cp $< $@

$(PASS_EMOJI_FONT_ASSET): assets/fonts/emoji.ttf | $(ASSET_DIR)/fonts
	cp $< $@

PLAN9_DIR := build/plan9
PLAN9_GENERATED := $(PLAN9_DIR)/generated
ZIGEN_PLAN9_DIR := $(PLAN9_DIR)/zigen
PLAN9_FILE_LIST := $(PLAN9_DIR)/generated-c-files.txt
PLAN9_EMBEDDED_ASSETS_C := $(PLAN9_DIR)/pass_embedded_assets.c
# Native Plan 9 build inputs: k2c emits 8c-safe C directly, the embedded
# table carries the two fonts, and the file list feeds the mkfile.
$(ZIGEN_PLAN9_DIR)/pass_core.c: pass_core.zi
	mkdir -p $(PLAN9_DIR)
	rm -rf $(ZIGEN_PLAN9_DIR)
	$(ZIRAN) build --project --locked --target=plan9-c \
		-o $(ZIGEN_PLAN9_DIR) pass_core.zi

kry-c-plan9: kry-c $(ZIGEN_PLAN9_DIR)/pass_core.c $(PASS_FONT_ASSET) $(PASS_EMOJI_FONT_ASSET)
	rm -rf $(PLAN9_GENERATED)
	$(K2C) --no-main --plan9 --root . \
		--include-dir $(KRYON_DIR)/include --include-dir $(KRYON_DIR)/src --include-dir native \
		--include-dir droid/app/src/main/cpp \
		-o $(PLAN9_GENERATED) $(KRY_APP_SRCS)
	find $(PLAN9_GENERATED) -type f -name '*.c' | LC_ALL=C sort > $(PLAN9_FILE_LIST)
	cd $(ASSET_DIR) && sh $(KRYON_DIR)/scripts/embed-assets.sh $(CURDIR)/$(PLAN9_EMBEDDED_ASSETS_C) fonts/noto/NotoSans-Regular.ttf fonts/emoji.ttf

gui: build/pass-gui

build/pass-gui: $(KRYON_BUILD_DIR)/libkryon.a $(KRY_C_STAMP) $(ZIGEN_DIR)/pass_core.c droid/app/src/main/cpp/main.c native/pass_core_zi.c native/pass_runtime.c native/pass_runtime.h | build
	$(CC) $(GUI_CFLAGS) -o $@ \
		droid/app/src/main/cpp/main.c \
		native/pass_core_zi.c \
		$(ZIGEN_DIR)/pass_core.c \
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
	$(MAKE) native-test cli-test runtime-test lesspass-compat-test kry-smoke

coverage: $(KRYON_BUILD_DIR)/libkryon.a
	mkdir -p build/coverage
	rm -f build/coverage/*.gcda build/coverage/*.gcno build/coverage/*.gcov *.gcov
	$(CC) --coverage -O0 -g -Wall -Wextra -std=gnu99 -Inative -c native/pass_core.c -o build/coverage/pass_core.o
	$(CC) --coverage -O0 -g -Wall -Wextra -std=gnu99 -Inative -c native/pass_core_test.c -o build/coverage/pass_core_test.o
	$(CC) --coverage build/coverage/pass_core.o build/coverage/pass_core_test.o -o build/coverage/pass_core_test
	./build/coverage/pass_core_test
	$(CC) --coverage -O0 -g -Wall -Wextra -std=gnu99 -Inative -DPASS_VERSION=\"$(PASS_VERSION)\" -c native/pass_cli.c -o build/coverage/pass_cli.o
	$(CC) --coverage build/coverage/pass_cli.o build/coverage/pass_core.o -o build/coverage/pass_cli
	sh scripts/cli_test.sh build/coverage/pass_cli
	$(CC) --coverage -O0 -g -Wall -Wextra -std=gnu99 -Inative -Idroid/app/src/main/cpp -I$(KRYON_DIR)/include -I$(KRYON_DIR)/src -I$(KRYON_GENERATED_SRC_DIR) -c native/pass_runtime.c -o build/coverage/pass_runtime.o
	$(CC) --coverage -O0 -g -Wall -Wextra -std=gnu99 -I$(KRYON_DIR)/include -c $(KRYON_DIR)/src/core/app_storage.c -o build/coverage/app_storage.o
	$(CC) --coverage -O0 -g -Wall -Wextra -std=gnu99 -I$(KRYON_DIR)/include -c $(KRYON_DIR)/src/kry_std/kry_filesystem.c -o build/coverage/kry_filesystem.o
	$(CC) --coverage -O0 -g -Wall -Wextra -std=gnu99 -Inative -Idroid/app/src/main/cpp -I$(KRYON_DIR)/include -c native/pass_runtime_test.c -o build/coverage/pass_runtime_test.o
	$(CC) --coverage build/coverage/pass_runtime.o build/coverage/pass_runtime_test.o build/coverage/pass_core.o build/coverage/app_storage.o build/coverage/kry_filesystem.o -o build/coverage/pass_runtime_test
	./build/coverage/pass_runtime_test
	for data in build/coverage/*.gcda; do gcov -b -c "$$data"; done
	mv *.gcov build/coverage/

# Checks the C generator used by the CLI and every Kry app target.
native-test:
	mkdir -p build
	cc -Wall -Wextra -O2 -Inative native/pass_core.c native/pass_core_test.c -o build/pass_core_test
	./build/pass_core_test

ziran-core-test:
	sh scripts/ziran_core_test.sh "$(ZIRAN)"

ziran-compat-test: $(ZIGEN_DIR)/pass_core.c
	mkdir -p build
	$(CC) -Wall -Wextra -O2 -std=gnu99 $(ZIGEN_CFLAGS) \
		-I$(ZIGEN_DIR) -I$(ZIRAN_INCLUDE) -Inative \
		native/pass_core_zi.c $(ZIGEN_DIR)/pass_core.c \
		native/pass_core_test.c -o build/pass_core_zi_test
	./build/pass_core_zi_test

cli-test: cli
	test "$$(./pass lesspass.com contact@lesspass.com password)" = '\g-A1-.OHEwrXjT#'
	test "$$(./pass --length 20 --counter 2 service.test person@example.net master)" = 'j:x_Lo5b1XL_j0we%z`e'
	sh scripts/cli_test.sh ./pass

runtime-test: $(KRYON_BUILD_DIR)/libkryon.a $(ZIGEN_DIR)/pass_core.c
	mkdir -p build
	$(CC) -Wall -Wextra -O2 -std=gnu99 -I$(ZIGEN_DIR) -I$(ZIRAN_INCLUDE) -Inative -Idroid/app/src/main/cpp -I$(KRYON_DIR)/include -I$(KRYON_DIR)/src -I$(KRYON_GENERATED_SRC_DIR) native/pass_runtime.c native/pass_core_zi.c $(ZIGEN_DIR)/pass_core.c $(KRYON_DIR)/src/core/app_storage.c $(KRYON_DIR)/src/kry_std/kry_filesystem.c native/pass_runtime_test.c -o build/pass_runtime_test
	./build/pass_runtime_test

lesspass-compat-test: cli
	python3 scripts/lesspass_compat_test.py --cli ./pass

check-package-urls:
	bash scripts/check_package_urls.sh

kry-smoke:
	sh scripts/kry_smoke.sh

ANDROID_DIR := droid
ANDROID_ABIS ?= armeabi-v7a,arm64-v8a

android-debug: kry-c $(ZIGEN_DIR)/pass_core.c
	cd $(ANDROID_DIR) && KRYON_DIR="$(KRYON_DIR)" ZIGEN_DIR="$(abspath $(ZIGEN_DIR))" KRYON_GENERATED_SRC_DIR="$(KRYON_GENERATED_SRC_DIR)" KRYON_GENERATED_INCLUDE_DIR="$(KRYON_BUILD_DIR)/generated/include" ./gradlew assembleDebug -Pabi=$(ANDROID_ABIS) -PsplitApks=true
	mkdir -p build
	cp $(ANDROID_DIR)/app/build/outputs/apk/debug/app-universal-debug.apk build/pass-android-debug.apk
	cp $(ANDROID_DIR)/app/build/outputs/apk/debug/app-arm64-v8a-debug.apk build/pass-android-arm64-v8a-debug.apk
	cp $(ANDROID_DIR)/app/build/outputs/apk/debug/app-armeabi-v7a-debug.apk build/pass-android-armeabi-v7a-debug.apk

android-smoke: android-debug
	bash scripts/android_smoke.sh

android-input-test: kry-c $(ZIGEN_DIR)/pass_core.c
	cd $(ANDROID_DIR) && KRYON_DIR="$(KRYON_DIR)" ZIGEN_DIR="$(abspath $(ZIGEN_DIR))" KRYON_GENERATED_SRC_DIR="$(KRYON_GENERATED_SRC_DIR)" KRYON_GENERATED_INCLUDE_DIR="$(KRYON_BUILD_DIR)/generated/include" ./gradlew connectedDebugAndroidTest -Pabi=x86_64 -PsplitApks=false

web:
	./web/build.sh

web-canvas: $(WEB_APP_TARGET)

$(WEB_APP_BUILD_DIR) build/web:
	mkdir -p $@

$(WEB_APP_EMBEDDED_ASSETS_C): $(PASS_FONT_ASSET) $(PASS_EMOJI_FONT_ASSET) $(KRYON_DIR)/scripts/embed-assets.sh | build/web
	cd $(ASSET_DIR) && sh $(KRYON_DIR)/scripts/embed-assets.sh $(CURDIR)/$@ fonts/noto/NotoSans-Regular.ttf fonts/emoji.ttf

$(WEB_APP_TARGET): Makefile $(KRYON_BUILD_DIR)/libkryon.a $(KRY_C_STAMP) $(PASS_WEB_SRCS) $(KRYON_WEB_SRCS) web/site/app/index.html web/site/app/app.js | $(WEB_APP_BUILD_DIR)
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

e2e: check-package-urls test native-test gui web-canvas site web-smoke android-debug android-smoke

package-deb: gui
	./scripts/package-deb.sh

package-appimage: gui
	./scripts/package-appimage.sh
