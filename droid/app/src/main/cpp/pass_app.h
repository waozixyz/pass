#ifndef PASS_APP_H
#define PASS_APP_H

typedef struct PassApp PassApp;

PassApp *pass_app(void);
const int *pass_app_master_emoji_codepoints(int *count);

/* Draws one frame. Surface sizes are physical pixels; dpi is the display
 * scale. Reserved edges are safe-area insets in UI units (dp). */
void pass_app_draw(PassApp *app, int surface_w, int surface_h, float dpi,
                     int left_reserved, int top_reserved,
                     int right_reserved, int bottom_reserved);

/* Clears secrets held in memory. */
void pass_app_shutdown(PassApp *app);

#endif
