/* Android entry point for pass. Raylib's Android backend provides
 * android_main and calls main(); this file owns window, theme, fonts, and
 * the frame loop. */

#include "kryon.h"
#include "embedded_assets.h"
#include "ui_dpi.h"
#include "ui_core.h"
#include "ui_scaling.h"
#include "ui_text.h"

#include "android_bridge.h"
#include "pass_app.h"

#include <stdio.h>
#include <string.h>

#if ANDROID_BUILD
#include <android/log.h>
#include <unistd.h>
#endif

static const char *const FONT_ASSET_PATH = "vendor/kryon/fonts/noto/NotoSans-Regular.ttf";
static const char *const EMOJI_FONT_ASSET_PATH = "gui/assets/emoji.ttf";

/* Kryon's shape drawing rides on a 1x1 white texture so rectangles tint
 * cleanly on the GL ES surface (same setup inbe performs on Android). */
static void
setup_shapes_texture(void)
{
    Image white = GenImageColor(1, 1, WHITE);
    Texture2D texture = LoadTextureFromImage(white);

    UnloadImage(white);
    if(texture.id == 0)
        return;
    SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    SetShapesTexture(texture, (Rectangle){0.0f, 0.0f, 1.0f, 1.0f});
}

static int
setup_ui_font(void)
{
    const EmbeddedAsset *asset = GetEmbeddedAsset(FONT_ASSET_PATH);

    if(asset == NULL || asset->data == NULL || asset->size == 0) {
        TraceLog(LOG_WARNING, "PASS: missing embedded font asset");
        return 0;
    }
    if(!RegisterUIFontSource("ui", GetEmbeddedAssetExtension(FONT_ASSET_PATH),
                             asset->data, asset->size, NULL, 0)) {
        TraceLog(LOG_WARNING, "PASS: RegisterUIFontSource failed");
        return 0;
    }
    if(!UseUIFont("ui")) {
        TraceLog(LOG_WARNING, "PASS: UseUIFont failed");
        return 0;
    }
    return 1;
}

static int
setup_emoji_font(void)
{
    const EmbeddedAsset *asset = GetEmbeddedAsset(EMOJI_FONT_ASSET_PATH);
    int codepoint_count = 0;
    const int *codepoints = pass_app_master_emoji_codepoints(&codepoint_count);

    if(asset == NULL || asset->data == NULL || asset->size == 0) {
        TraceLog(LOG_WARNING, "PASS: missing embedded emoji font asset");
        return 0;
    }
    if(!RegisterUIFixedFontSource("pass-emoji", GetEmbeddedAssetExtension(EMOJI_FONT_ASSET_PATH),
                                  asset->data, asset->size, codepoints, codepoint_count)) {
        TraceLog(LOG_WARNING, "PASS: RegisterUIFixedFontSource failed for emoji");
        return 0;
    }
    return 1;
}

int
main(int argc, char **argv)
{
    PassApp *app;
    int window_width = 720;
    int window_height = 740;

    (void)argc;
    (void)argv;

#if ANDROID_BUILD
    __android_log_write(ANDROID_LOG_INFO, "PASS_MAIN", "main start");
    android_bridge_init();
    if(chdir("/data/user/0/xyz.waozi.pass/files") != 0)
        TraceLog(LOG_WARNING, "PASS: failed to switch to files directory");
    window_width = 0;
    window_height = 0;
#elif defined(PLATFORM_WEB)
    SetConfigFlags(GetWebWindowFlags());
    GetWebViewportSize(window_width, window_height, &window_width, &window_height);
#endif

    InitWindow(window_width, window_height, "Pass");
    if(!IsWindowReady()) {
        TraceLog(LOG_ERROR, "PASS: InitWindow failed");
        return 1;
    }
    InitUIDPI();
    SetThemeStyle(THEME_STYLE_MATERIAL);
    SetCurrentTheme(GetDefaultThemeForThemeStyle(THEME_STYLE_MATERIAL), 0);
    android_bridge_apply_system_theme();
    SetThemeMode(THEME_MODE_SYSTEM);
    SetThemeSource(THEME_SOURCE_SYSTEM);
    ApplyCurrentUITheme();
    setup_ui_font();
    setup_emoji_font();
    setup_shapes_texture();
    SetTextInputPlatformCallback(android_bridge_set_soft_keyboard);
    SetTargetFPS(60);

    app = pass_app();
    TraceLog(LOG_INFO, "PASS: app ready");
    while(!WindowShouldClose()) {
        int width = GetScreenWidth();
        int height = GetScreenHeight();
        Vector2 scale = GetWindowScaleDPI();
        float dpi = scale.x > 0.0f ? scale.x : 1.0f;

#if ANDROID_BUILD
        SyncAndroidSurfaceSize(&width, &height);
#endif
#if defined(PLATFORM_WEB)
        SyncWebWindowSize();
        width = GetScreenWidth();
        height = GetScreenHeight();
#endif

        BeginDrawing();
#if ANDROID_BUILD
        SyncAndroidSurfaceSize(&width, &height);
#endif
        BeginUIFrame(width, height, dpi);
        pass_app_draw(app, width, height, dpi,
                        android_bridge_left_reserved(),
                        android_bridge_top_reserved(),
                        android_bridge_right_reserved(),
                        android_bridge_bottom_reserved());
        EndUIFrame();
        EndDrawing();
    }

    pass_app_shutdown(app);
    CloseWindow();
    return 0;
}
