cmake_minimum_required(VERSION 3.22)
project(Pass C)

set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../..")
set(KRYON_DIR "${REPO_ROOT}/vendor/kryon")
set(KRY_GENERATED_DIR "${CMAKE_BINARY_DIR}/krygen/c")
set(RAYLIB_DIR "${KRYON_DIR}/vendor/raylib/src")
set(PASS_NATIVE_DIR "${REPO_ROOT}/native")
set(NATIVE_APP_GLUE_DIR "${ANDROID_NDK}/sources/android/native_app_glue")
include("${KRYON_DIR}/cmake/KryonKrySources.cmake")

file(GLOB_RECURSE KRYON_SOURCES CONFIGURE_DEPENDS "${KRYON_DIR}/src/*.c")
# The src/backend/ software renderer implements raylib-named functions for
# no-raylib hosts; Android links real raylib, so compiling both is a
# duplicate-symbol link error.
list(FILTER KRYON_SOURCES EXCLUDE REGEX "${KRYON_DIR}/src/backend/.*")
# pass uses no 2D physics; mirror the desktop Makefile's physics filter.
list(REMOVE_ITEM KRYON_SOURCES
    "${KRYON_DIR}/src/scene/physics_world.c"
    "${KRYON_DIR}/src/scene/node_body2d.c"
    "${KRYON_DIR}/src/scene/node_area2d.c"
    "${KRYON_DIR}/src/scene/node_collision_shape2d.c")
# Ksync account crypto requires liboqs and pass never syncs; runtime asset
# downloads need an Android download backend pass does not provide.
list(FILTER KRYON_SOURCES EXCLUDE REGEX "${KRYON_DIR}/src/ksync/.*")
list(FILTER KRYON_SOURCES EXCLUDE REGEX "${KRYON_DIR}/src/runtime_assets/.*")
list(FILTER KRYON_SOURCES EXCLUDE REGEX "${KRYON_DIR}/src/notification/.*")
list(REMOVE_ITEM KRYON_SOURCES "${KRYON_DIR}/src/file_dialog/file_dialog.c")
# Keep Android builds off platform implementations for other targets. These
# directories can contain host SDK headers or symbols unavailable to the NDK.
list(FILTER KRYON_SOURCES EXCLUDE REGEX "${KRYON_DIR}/src/platform/desktop/.*")
list(FILTER KRYON_SOURCES EXCLUDE REGEX "${KRYON_DIR}/src/platform/desktop_tray/.*")
list(FILTER KRYON_SOURCES EXCLUDE REGEX "${KRYON_DIR}/src/platform/plan9/.*")
list(FILTER KRYON_SOURCES EXCLUDE REGEX "${KRYON_DIR}/src/platform/web/.*")

file(GLOB_RECURSE PASS_KRY_SOURCES CONFIGURE_DEPENDS "${REPO_ROOT}/app/*.kry")
list(SORT PASS_KRY_SOURCES)
kryon_generate_kry_sources(PASS_KRY_GENERATED_SOURCES PASS_KRY_INCLUDE_DIR
    TARGET pass_kry_generated
    KRYON_DIR "${KRYON_DIR}"
    ROOT "${REPO_ROOT}"
    OUT_DIR "${KRY_GENERATED_DIR}"
    NO_MAIN
    SOURCES ${PASS_KRY_SOURCES}
)

set(PASS_FONT_ASSET "vendor/kryon/fonts/noto/NotoSans-Regular.ttf")
set(PASS_EMOJI_FONT_ASSET "assets/fonts/emoji.ttf")
set(PASS_EMBEDDED_ASSETS_C "${CMAKE_BINARY_DIR}/pass_embedded_assets.c")
add_custom_command(
    OUTPUT "${PASS_EMBEDDED_ASSETS_C}"
    COMMAND sh "${KRYON_DIR}/scripts/embed-assets.sh" "${PASS_EMBEDDED_ASSETS_C}" "${PASS_FONT_ASSET}" "${PASS_EMOJI_FONT_ASSET}"
    DEPENDS "${KRYON_DIR}/scripts/embed-assets.sh" "${KRYON_DIR}/fonts/noto/NotoSans-Regular.ttf" "${REPO_ROOT}/${PASS_EMOJI_FONT_ASSET}"
    WORKING_DIRECTORY "${REPO_ROOT}"
    VERBATIM
)

add_library(raylib STATIC
    "${RAYLIB_DIR}/rcore.c"
    "${RAYLIB_DIR}/rshapes.c"
    "${RAYLIB_DIR}/rtextures.c"
    "${RAYLIB_DIR}/rtext.c"
    "${RAYLIB_DIR}/raudio.c"
)

target_compile_definitions(raylib PRIVATE
    PLATFORM_ANDROID
    GRAPHICS_API_OPENGL_ES2
    SUPPORT_SCREEN_CAPTURE=0
    SUPPORT_COMPRESSION_API=0
    SUPPORT_AUTOMATION_EVENTS=0
    SUPPORT_CLIPBOARD_IMAGE=0
    SUPPORT_FILEFORMAT_BMP=0
    SUPPORT_FILEFORMAT_GIF=0
    SUPPORT_FILEFORMAT_QOI=0
    SUPPORT_FILEFORMAT_DDS=0
    SUPPORT_FILEFORMAT_TTF=1
    SUPPORT_FILEFORMAT_JPG=1
    SUPPORT_FILEFORMAT_PNG=1
    SUPPORT_FILEFORMAT_OGG=1
    SUPPORT_MODULE_RAUDIO=1
)

target_compile_options(raylib PRIVATE
    -Os
    -ffunction-sections
    -fdata-sections
)

target_include_directories(raylib PUBLIC
    "${RAYLIB_DIR}"
    "${NATIVE_APP_GLUE_DIR}"
)

add_library(main SHARED
    main.c
    "${PASS_NATIVE_DIR}/pass_core.c"
    "${PASS_NATIVE_DIR}/pass_runtime.c"
    ${PASS_KRY_GENERATED_SOURCES}
    "${PASS_EMBEDDED_ASSETS_C}"
    "${KRYON_DIR}/src/backend/kry_sw_png.c"
    ${KRYON_SOURCES}
    "${NATIVE_APP_GLUE_DIR}/android_native_app_glue.c"
)
add_dependencies(main pass_kry_generated)

target_include_directories(main PRIVATE
    "${KRYON_DIR}/include"
    "${KRYON_DIR}/src"
    "${PASS_KRY_INCLUDE_DIR}"
    "${PASS_NATIVE_DIR}"
    "${CMAKE_CURRENT_LIST_DIR}"
    "${NATIVE_APP_GLUE_DIR}"
)

target_compile_definitions(main PRIVATE
    PLATFORM_ANDROID
    ANDROID_BUILD=1
    _DEFAULT_SOURCE
    _GNU_SOURCE
    _FILE_OFFSET_BITS=64
    UI_EMBEDDED_ONLY=1
    KRYON_WITH_PHYSICS=0
)

target_compile_options(main PRIVATE
    -Os
    -ffunction-sections
    -fdata-sections
    -fvisibility=hidden
)

target_link_options(main PRIVATE
    -Wl,--gc-sections
    -Wl,--strip-all
    -Wl,--wrap=fopen
    -Wl,-z,max-page-size=16384
    -Wl,-z,common-page-size=16384
)

target_link_libraries(main
    raylib
    android
    log
    OpenSLES
    EGL
    GLESv2
    m
)
