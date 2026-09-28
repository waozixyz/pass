cmake_minimum_required(VERSION 3.22)
project(Pass C)

set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../..")
set(KRYON_DIR "$ENV{KRYON_DIR}" CACHE PATH "Ziran-locked Kryon source")
if(NOT KRYON_DIR)
    message(FATAL_ERROR "Set KRYON_DIR to the Ziran-locked Kryon checkout")
endif()
set(ZIGEN_DIR "$ENV{ZIGEN_DIR}" CACHE PATH "Ziran-generated Pass core")
if(NOT ZIGEN_DIR)
    message(FATAL_ERROR "Set ZIGEN_DIR to the Ziran-generated Pass core")
endif()
set(KRYON_GENERATED_SRC_DIR "$ENV{KRYON_GENERATED_SRC_DIR}" CACHE PATH "Ziran-locked Kryon generated runtime")
if(NOT KRYON_GENERATED_SRC_DIR)
    message(FATAL_ERROR "Set KRYON_GENERATED_SRC_DIR to the Ziran-locked Kryon generated runtime")
endif()
set(KRYON_GENERATED_INCLUDE_DIR "$ENV{KRYON_GENERATED_INCLUDE_DIR}" CACHE PATH "Ziran-locked Kryon generated headers")
if(NOT KRYON_GENERATED_INCLUDE_DIR)
    message(FATAL_ERROR "Set KRYON_GENERATED_INCLUDE_DIR to the Ziran-locked Kryon generated headers")
endif()
set(KRY_GENERATED_DIR "${CMAKE_BINARY_DIR}/krygen/c")
set(RAYLIB_DIR "${KRYON_DIR}/vendor/raylib/src")
set(KRYON_RAYLIB_GENERATED_DIR "${CMAKE_BINARY_DIR}/kryon/raylib_generated")
set(KRYON_COMPAT_GENERATED_HEADER "${KRYON_RAYLIB_GENERATED_DIR}/kryon_compat.generated.h")
set(KRYON_RAYLIB_BACKEND_RENAME_HEADER "${KRYON_RAYLIB_GENERATED_DIR}/raylib_backend_rename.h")
set(KRYON_RAYLIB_WRAPPERS_C "${KRYON_RAYLIB_GENERATED_DIR}/kryon_raylib_wrappers.c")
set(KRYON_NULL_BACKEND_C "${KRYON_RAYLIB_GENERATED_DIR}/kryon_null_backend.c")
set(PASS_NATIVE_DIR "${REPO_ROOT}/native")
set(NATIVE_APP_GLUE_DIR "${ANDROID_NDK}/sources/android/native_app_glue")
include("${KRYON_DIR}/cmake/KryonKrySources.cmake")

file(GLOB_RECURSE KRYON_SOURCES CONFIGURE_DEPENDS "${KRYON_DIR}/src/*.c")
file(GLOB_RECURSE KRYON_GENERATED_SOURCES CONFIGURE_DEPENDS "${KRYON_GENERATED_SRC_DIR}/*.c")
list(REMOVE_ITEM KRYON_GENERATED_SOURCES "${KRYON_GENERATED_SRC_DIR}/kryon_project.c")
# The src/backend/ software renderer implements raylib-named functions for
# no-raylib hosts; Android links real raylib, so compiling both is a
# duplicate-symbol link error.
list(FILTER KRYON_SOURCES EXCLUDE REGEX "${KRYON_DIR}/src/backend/.*")
list(APPEND KRYON_SOURCES
    "${KRYON_DIR}/src/backend/kry_backend.c"
    "${KRYON_DIR}/src/backend/kry_backend_draw.c"
    "${KRYON_DIR}/src/backend/kry_dpi.c"
    "${KRYON_DIR}/src/backend/kry_event_wait.c"
    "${KRYON_DIR}/src/backend/kry_icon_atlas.c"
    "${KRYON_DIR}/src/backend/kry_image.c"
    "${KRYON_DIR}/src/backend/kry_image_decode.c"
    "${KRYON_DIR}/src/backend/kry_input.c"
    "${KRYON_DIR}/src/backend/kry_instance.c"
    "${KRYON_DIR}/src/backend/kry_layer_texture.c"
    "${KRYON_DIR}/src/backend/kry_screenshot.c"
    "${KRYON_DIR}/src/backend/kry_style_build_mode.c"
    "${KRYON_DIR}/src/backend/kry_sw_png.c"
    "${KRYON_DIR}/src/backend/kry_surface_math.c"
    "${KRYON_DIR}/src/backend/kry_ui_platform.c"
    "${KRYON_DIR}/src/backend/kry_unicode.c")
# pass uses no 2D physics; mirror the desktop Makefile's physics filter.
list(REMOVE_ITEM KRYON_SOURCES
    "${KRYON_DIR}/src/scene/physics_world.c"
    "${KRYON_DIR}/src/scene/node_body2d.c"
    "${KRYON_DIR}/src/scene/node_area2d.c"
    "${KRYON_DIR}/src/scene/node_collision_shape2d.c")
# Ksync account crypto requires liboqs and pass never syncs; runtime asset
# downloads need an Android download backend pass does not provide.
list(FILTER KRYON_SOURCES EXCLUDE REGEX "${KRYON_DIR}/src/ksync/.*")
list(FILTER KRYON_SOURCES EXCLUDE REGEX "${KRYON_DIR}/src/sync/.*")
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

add_custom_command(
    OUTPUT
        "${KRYON_COMPAT_GENERATED_HEADER}"
        "${KRYON_RAYLIB_BACKEND_RENAME_HEADER}"
        "${KRYON_RAYLIB_WRAPPERS_C}"
        "${KRYON_NULL_BACKEND_C}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${KRYON_RAYLIB_GENERATED_DIR}"
    COMMAND ${CMAKE_COMMAND} -E env KRYON_COMPAT_AUDIO=0
        sh "${KRYON_DIR}/tools/generate-kryon-compat.sh"
        "${RAYLIB_DIR}/raylib.h"
        "${KRYON_COMPAT_GENERATED_HEADER}"
        "${KRYON_RAYLIB_BACKEND_RENAME_HEADER}"
        "${KRYON_RAYLIB_WRAPPERS_C}"
        "${KRYON_NULL_BACKEND_C}"
    DEPENDS "${KRYON_DIR}/tools/generate-kryon-compat.sh" "${RAYLIB_DIR}/raylib.h"
    VERBATIM
)
add_custom_target(kryon_raylib_generated DEPENDS
    "${KRYON_COMPAT_GENERATED_HEADER}"
    "${KRYON_RAYLIB_BACKEND_RENAME_HEADER}"
    "${KRYON_RAYLIB_WRAPPERS_C}"
    "${KRYON_NULL_BACKEND_C}"
)

set(PASS_ASSET_DIR "${CMAKE_BINARY_DIR}/assets")
set(PASS_FONT_FILE "${PASS_ASSET_DIR}/fonts/noto/NotoSans-Regular.ttf")
set(PASS_EMOJI_FONT_FILE "${PASS_ASSET_DIR}/fonts/emoji.ttf")
set(PASS_EMBEDDED_ASSETS_C "${CMAKE_BINARY_DIR}/pass_embedded_assets.c")
add_custom_command(
    OUTPUT "${PASS_FONT_FILE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PASS_ASSET_DIR}/fonts/noto"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${KRYON_DIR}/fonts/noto/NotoSans-Regular.ttf" "${PASS_FONT_FILE}"
    DEPENDS "${KRYON_DIR}/fonts/noto/NotoSans-Regular.ttf"
    VERBATIM
)
add_custom_command(
    OUTPUT "${PASS_EMOJI_FONT_FILE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PASS_ASSET_DIR}/fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${REPO_ROOT}/assets/fonts/emoji.ttf" "${PASS_EMOJI_FONT_FILE}"
    DEPENDS "${REPO_ROOT}/assets/fonts/emoji.ttf"
    VERBATIM
)
add_custom_command(
    OUTPUT "${PASS_EMBEDDED_ASSETS_C}"
    COMMAND ${CMAKE_COMMAND} -E chdir "${PASS_ASSET_DIR}" sh
        "${KRYON_DIR}/scripts/embed-assets.sh" "${PASS_EMBEDDED_ASSETS_C}"
        "fonts/noto/NotoSans-Regular.ttf" "fonts/emoji.ttf"
    DEPENDS "${KRYON_DIR}/scripts/embed-assets.sh"
        "${PASS_FONT_FILE}" "${PASS_EMOJI_FONT_FILE}"
    VERBATIM
)

add_library(raylib STATIC
    "${RAYLIB_DIR}/rcore.c"
    "${RAYLIB_DIR}/rshapes.c"
    "${RAYLIB_DIR}/rtextures.c"
    "${RAYLIB_DIR}/rtext.c"
    "${RAYLIB_DIR}/raudio.c"
)
add_dependencies(raylib kryon_raylib_generated)

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
    -include "${KRYON_RAYLIB_BACKEND_RENAME_HEADER}"
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
    "${PASS_NATIVE_DIR}/pass_core_zi.c"
    "${ZIGEN_DIR}/pass_core.c"
    "${PASS_NATIVE_DIR}/pass_runtime.c"
    ${PASS_KRY_GENERATED_SOURCES}
    "${PASS_EMBEDDED_ASSETS_C}"
    ${KRYON_SOURCES}
    ${KRYON_GENERATED_SOURCES}
    "${KRYON_RAYLIB_WRAPPERS_C}"
    "${NATIVE_APP_GLUE_DIR}/android_native_app_glue.c"
)
add_dependencies(main pass_kry_generated kryon_raylib_generated)

target_include_directories(main PRIVATE
    "${KRYON_DIR}/include"
    "${KRYON_DIR}/src"
    "${KRYON_DIR}/src/ui"
    "${KRYON_DIR}/src/platform"
    "${KRYON_DIR}/src/backend"
    "${KRYON_DIR}/vendor/utf8proc"
    "${KRYON_GENERATED_SRC_DIR}"
    "${KRYON_GENERATED_SRC_DIR}/runtime"
    "${KRYON_GENERATED_INCLUDE_DIR}"
    "${PASS_KRY_INCLUDE_DIR}"
    "${ZIGEN_DIR}"
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
    UTF8PROC_STATIC=1
    KRYON_WITH_PHYSICS=0
    KRYON_WITH_SYNC=0
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
