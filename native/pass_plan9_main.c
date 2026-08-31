/* Native Plan 9 entry point for Pass (Kryon libdraw backend).
 *
 * Mirrors droid/app/src/main/cpp/main.c: owns the window, theme, fonts,
 * and the frame loop. Switches to the user's home directory first so the
 * profiles file and kryon app-storage settings land in $home instead of
 * the source tree. */

#include "kryon.h"
#include "embedded_assets.h"
#include "app_runtime.h"
#include "app/pass.h"
#include "pass_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *const FONT_ASSET_PATH = "vendor/kryon/fonts/noto/NotoSans-Regular.ttf";
static const char *const EMOJI_FONT_ASSET_PATH = "assets/fonts/emoji.ttf";

/* The generated project host references the app lifecycle hooks weakly,
 * and the native Plan 9 linker has no weak symbols; this entry point
 * drives the loop itself, so the hooks stay inert. */
void *CreateApp(const char *project_path)
{
    (void)project_path;
    return NULL;
}

void DestroyApp(void *app)
{
    (void)app;
}

void ApplyRoute(void *app, const AppRouteInfo *route)
{
    (void)app;
    (void)route;
}

void BeginScreenDraw(void *app, Rectangle viewport)
{
    (void)app;
    (void)viewport;
}

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
    const int *codepoints = pass_runtime_master_emoji_codepoints(&codepoint_count);

    if(asset == NULL || asset->data == NULL || asset->size == 0) {
        TraceLog(LOG_WARNING, "PASS: missing embedded emoji font asset");
        return 0;
    }
    if(!RegisterUIFixedFontSource("pass-emoji",
                                  GetEmbeddedAssetExtension(EMOJI_FONT_ASSET_PATH),
                                  asset->data, asset->size, codepoints, codepoint_count)) {
        TraceLog(LOG_WARNING, "PASS: RegisterUIFixedFontSource failed for emoji");
        return 0;
    }
    return 1;
}

int
main(int argc, char **argv)
{
    const char *home;

    (void)argc;
    (void)argv;

    home = getenv("home");
    if(home == NULL)
        home = getenv("HOME");
    if(home != NULL && home[0] != '\0') {
        if(chdir(home) != 0)
            TraceLog(LOG_WARNING, "PASS: failed to switch to home directory");
    }

    InitWindow(540, 700, "Pass");
    if(!IsWindowReady()) {
        TraceLog(LOG_ERROR, "PASS: InitWindow failed");
        return 1;
    }
    InitUIDPI();
    SetThemeStyle(THEME_STYLE_MATERIAL);
    SetCurrentTheme(GetDefaultThemeForThemeStyle(THEME_STYLE_MATERIAL), 0);
    ApplyCurrentUITheme();
    setup_ui_font();
    setup_emoji_font();
    setup_shapes_texture();
    SetTargetFPS(60);

    pass_runtime_init();
    TraceLog(LOG_INFO, "PASS: app ready");
    while(!WindowShouldClose()) {
        BeginFrame();
        BeginUIFrame(GetFrameWidth(), GetFrameHeight(), GetFrameScale());
        pass_runtime_tick();
        pass_frame();
        EndUIFrame();
        EndFrame();
    }

    pass_runtime_shutdown();
    CloseWindow();
    return 0;
}
