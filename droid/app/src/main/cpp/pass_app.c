/* C port of the pass desktop GUI (gui/main.go) for the Android build.
 * The desktop branch keeps the Go layout; this port adds a narrow-screen
 * layout for phones: single column, two-abreast rules, scrollable card. */

#include "pass_app.h"
#include "pass_core.h"

#include "android_bridge.h"
#include "kryon.h"
#include "embedded_assets.h"
#include "ui_icons.h"
#include "ui_nav.h"
#include "ui_scroll.h"
#include "ui_scaling.h"
#include "ui_text.h"
#include "ui_color.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CLIPBOARD_LIFETIME_MS 20000
#define SECRET_FIELD_LIFETIME_MS 20000
#define MAX_PROFILES 32
#define MASTER_EMOJI_COUNT 4

typedef enum {
    VIEW_GENERATE = 0,
    VIEW_PROFILES = 1,
    VIEW_SETTINGS = 2
} AppView;

typedef struct {
    char buffer[1024];
    int cursor;
    int focused;
    int commit;
    int focus_id;
    int max_codepoints;
    int secure;
} Field;

typedef struct {
    char name[64];
    char site[256];
    char login[256];
    char exclude[128];
    int length;
    int counter;
    int lower, upper, digits, symbols;
} SavedProfile;

typedef struct {
    int auto_copy;
    int clear_after_seconds;
    int show_fingerprint;
    int use_biometric;
    int length;
    int counter;
    int lower, upper, digits, symbols;
    char exclude[128];
} AppSettings;

static const int master_emoji_codepoints[] = {
    0x1F436, 0x1F431, 0x1F42D, 0x1F439, 0x1F430, 0x1F98A, 0x1F43B, 0x1F43C,
    0x1F428, 0x1F42F, 0x1F981, 0x1F42E, 0x1F437, 0x1F438, 0x1F435, 0x1F414,
    0x1F427, 0x1F989, 0x1F43A, 0x1F434, 0x1F984, 0x1F41D, 0x1F98B, 0x1F422,
    0x1F34E, 0x1F34A, 0x1F34B, 0x1F349, 0x1F347, 0x1F353, 0x1F352, 0x1F351,
    0x1F951, 0x1F33D, 0x1F355, 0x1F354, 0x1F35F, 0x1F369,
    0x1F335, 0x1F332, 0x1F333, 0x1F334, 0x1F331, 0x1F33B, 0x1F338, 0x1F308,
    0x2B50, 0x1F319,
    0x1F525, 0x1F4A7, 0x26C4, 0x1F389, 0x1F3B8, 0x1F3AF, 0x1F3B2, 0x1F381,
    0x1F680, 0x1F697, 0x2693, 0x1F3A8, 0x1F511, 0x1F4A1, 0x1F4DA, 0x1F3A7,
};

const int *
pass_app_master_emoji_codepoints(int *count)
{
    if(count != NULL)
        *count = (int)(sizeof(master_emoji_codepoints) / sizeof(master_emoji_codepoints[0]));
    return master_emoji_codepoints;
}

static void
field_init(Field *f, int focus_id, int capacity_codepoints)
{
    memset(f, 0, sizeof(*f));
    f->focus_id = focus_id;
    f->max_codepoints = capacity_codepoints;
}

static const char *
field_text(const Field *f)
{
    return f->buffer;
}

static void
field_clear(Field *f)
{
    memset(f->buffer, 0, sizeof(f->buffer));
    f->cursor = 0;
}

static void
field_set(Field *f, const char *value)
{
    if(value == NULL)
        value = "";
    snprintf(f->buffer, sizeof(f->buffer), "%s", value);
    f->cursor = (int)strlen(f->buffer);
}

struct PassApp {
    Field site, login, master, exclude, profile_name;
    int length, counter;
    int lower, upper, digits, symbols;
    int reveal;
    char generated[136];
    char message[160];
    AppView view;
    AppSettings settings;
    AppSettings persisted_settings;
    SavedProfile profiles[MAX_PROFILES];
    int profile_count;
    int selected_profile;
    int secure_action;
    Texture2D icons[UI_ICON_TYPE_COUNT];
    /* clipboard lease */
    char clip_value[136];
    int64_t clip_expires_ms;
    int clip_has_value;
    int64_t master_expires_ms;
    int64_t generated_expires_ms;
    int scroll_offset;
    int last_layout_w;
    int last_layout_h;
};

static void
sanitize_field(char *s)
{
    if(s == NULL)
        return;
    for(; *s != '\0'; s++) {
        if(*s == '\t' || *s == '\n' || *s == '\r')
            *s = ' ';
    }
}

static void
copy_sanitized(char *dst, size_t dst_size, const char *src)
{
    if(dst == NULL || dst_size == 0)
        return;
    snprintf(dst, dst_size, "%s", src != NULL ? src : "");
    sanitize_field(dst);
}

static void
default_settings(AppSettings *settings)
{
    settings->auto_copy = 0;
    settings->clear_after_seconds = 20;
    settings->show_fingerprint = 1;
    settings->use_biometric = 0;
    settings->length = 16;
    settings->counter = 1;
    settings->lower = settings->upper = settings->digits = settings->symbols = 1;
    settings->exclude[0] = '\0';
}

static void
load_settings(PassApp *a)
{
    FILE *f = fopen("settings.cfg", "r");
    char line[128];

    default_settings(&a->settings);
    if(f == NULL)
        return;
    while(fgets(line, sizeof(line), f) != NULL) {
        char *eq = strchr(line, '=');
        char *key, *value;

        if(eq == NULL)
            continue;
        *eq = '\0';
        key = line;
        value = eq + 1;
        value[strcspn(value, "\r\n")] = '\0';
        if(strcmp(key, "auto_copy") == 0)
            a->settings.auto_copy = atoi(value) != 0;
        else if(strcmp(key, "clear_after_seconds") == 0)
            a->settings.clear_after_seconds = atoi(value);
        else if(strcmp(key, "show_fingerprint") == 0)
            a->settings.show_fingerprint = atoi(value) != 0;
        else if(strcmp(key, "use_biometric") == 0)
            a->settings.use_biometric = atoi(value) != 0;
        else if(strcmp(key, "length") == 0)
            a->settings.length = atoi(value);
        else if(strcmp(key, "counter") == 0)
            a->settings.counter = atoi(value);
        else if(strcmp(key, "lower") == 0)
            a->settings.lower = atoi(value) != 0;
        else if(strcmp(key, "upper") == 0)
            a->settings.upper = atoi(value) != 0;
        else if(strcmp(key, "digits") == 0)
            a->settings.digits = atoi(value) != 0;
        else if(strcmp(key, "symbols") == 0)
            a->settings.symbols = atoi(value) != 0;
        else if(strcmp(key, "exclude") == 0)
            copy_sanitized(a->settings.exclude, sizeof(a->settings.exclude), value);
    }
    fclose(f);
    if(a->settings.clear_after_seconds < 0)
        a->settings.clear_after_seconds = 0;
    if(a->settings.clear_after_seconds > 3600)
        a->settings.clear_after_seconds = 3600;
    if(a->settings.length <= 0)
        a->settings.length = 16;
    if(a->settings.length > 128)
        a->settings.length = 128;
    if(a->settings.counter <= 0)
        a->settings.counter = 1;
    if(a->settings.counter > 999999)
        a->settings.counter = 999999;
    a->length = a->settings.length;
    a->counter = a->settings.counter;
    a->lower = a->settings.lower;
    a->upper = a->settings.upper;
    a->digits = a->settings.digits;
    a->symbols = a->settings.symbols;
    field_set(&a->exclude, a->settings.exclude);
    a->persisted_settings = a->settings;
}

static void
write_settings(PassApp *a, int announce)
{
    FILE *f = fopen("settings.cfg", "w");

    if(f == NULL) {
        snprintf(a->message, sizeof(a->message), "%s", "Could not save settings");
        return;
    }
    a->settings.length = a->length;
    a->settings.counter = a->counter;
    a->settings.lower = a->lower;
    a->settings.upper = a->upper;
    a->settings.digits = a->digits;
    a->settings.symbols = a->symbols;
    copy_sanitized(a->settings.exclude, sizeof(a->settings.exclude), field_text(&a->exclude));
    fprintf(f, "auto_copy=%d\n", a->settings.auto_copy ? 1 : 0);
    fprintf(f, "clear_after_seconds=%d\n", a->settings.clear_after_seconds);
    fprintf(f, "show_fingerprint=%d\n", a->settings.show_fingerprint ? 1 : 0);
    fprintf(f, "use_biometric=%d\n", a->settings.use_biometric ? 1 : 0);
    fprintf(f, "length=%d\n", a->settings.length);
    fprintf(f, "counter=%d\n", a->settings.counter);
    fprintf(f, "lower=%d\n", a->settings.lower ? 1 : 0);
    fprintf(f, "upper=%d\n", a->settings.upper ? 1 : 0);
    fprintf(f, "digits=%d\n", a->settings.digits ? 1 : 0);
    fprintf(f, "symbols=%d\n", a->settings.symbols ? 1 : 0);
    fprintf(f, "exclude=%s\n", a->settings.exclude);
    fclose(f);
    a->persisted_settings = a->settings;
#if defined(PLATFORM_WEB)
    ScheduleWebStorageSync(250, 0);
#endif
    if(announce)
        snprintf(a->message, sizeof(a->message), "%s", "Settings saved");
}

static void
save_settings(PassApp *a)
{
    write_settings(a, 1);
}

static int
settings_match_current(PassApp *a)
{
    char exclude[128];

    copy_sanitized(exclude, sizeof(exclude), field_text(&a->exclude));
    return a->persisted_settings.length == a->length &&
           a->persisted_settings.counter == a->counter &&
           a->persisted_settings.lower == a->lower &&
           a->persisted_settings.upper == a->upper &&
           a->persisted_settings.digits == a->digits &&
           a->persisted_settings.symbols == a->symbols &&
           a->persisted_settings.auto_copy == a->settings.auto_copy &&
           a->persisted_settings.clear_after_seconds == a->settings.clear_after_seconds &&
           a->persisted_settings.show_fingerprint == a->settings.show_fingerprint &&
           a->persisted_settings.use_biometric == a->settings.use_biometric &&
           strcmp(a->persisted_settings.exclude, exclude) == 0;
}

static void
autosave_settings(PassApp *a)
{
    if(!settings_match_current(a))
        write_settings(a, 0);
}

static void
load_profiles(PassApp *a)
{
    FILE *f = fopen("profiles.tsv", "r");
    char line[1024];

    a->profile_count = 0;
    a->selected_profile = -1;
    if(f == NULL)
        return;
    while(a->profile_count < MAX_PROFILES && fgets(line, sizeof(line), f) != NULL) {
        char *parts[10];
        char *save = NULL;
        char *part;
        int i = 0;
        SavedProfile *p = &a->profiles[a->profile_count];

        line[strcspn(line, "\r\n")] = '\0';
        part = strtok_r(line, "\t", &save);
        while(part != NULL && i < 10) {
            parts[i++] = part;
            part = strtok_r(NULL, "\t", &save);
        }
        if(i < 9 || parts[0][0] == '\0')
            continue;
        memset(p, 0, sizeof(*p));
        copy_sanitized(p->name, sizeof(p->name), parts[0]);
        copy_sanitized(p->site, sizeof(p->site), parts[1]);
        copy_sanitized(p->login, sizeof(p->login), parts[2]);
        p->length = atoi(parts[3]);
        p->counter = atoi(parts[4]);
        p->lower = atoi(parts[5]) != 0;
        p->upper = atoi(parts[6]) != 0;
        p->digits = atoi(parts[7]) != 0;
        p->symbols = atoi(parts[8]) != 0;
        if(i >= 10)
            copy_sanitized(p->exclude, sizeof(p->exclude), parts[9]);
        if(p->length <= 0)
            p->length = 16;
        if(p->counter <= 0)
            p->counter = 1;
        a->profile_count++;
    }
    fclose(f);
}

static void
save_profiles(PassApp *a)
{
    FILE *f = fopen("profiles.tsv", "w");
    int i;

    if(f == NULL) {
        snprintf(a->message, sizeof(a->message), "%s", "Could not save profiles");
        return;
    }
    for(i = 0; i < a->profile_count; i++) {
        SavedProfile *p = &a->profiles[i];
        sanitize_field(p->name);
        sanitize_field(p->site);
        sanitize_field(p->login);
        sanitize_field(p->exclude);
        fprintf(f, "%s\t%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%s\n",
                p->name, p->site, p->login, p->length, p->counter,
                p->lower ? 1 : 0, p->upper ? 1 : 0,
                p->digits ? 1 : 0, p->symbols ? 1 : 0, p->exclude);
    }
    fclose(f);
#if defined(PLATFORM_WEB)
    ScheduleWebStorageSync(250, 0);
#endif
}

static void
profile_from_current(PassApp *a, SavedProfile *p)
{
    memset(p, 0, sizeof(*p));
    copy_sanitized(p->name, sizeof(p->name), field_text(&a->profile_name));
    copy_sanitized(p->site, sizeof(p->site), field_text(&a->site));
    copy_sanitized(p->login, sizeof(p->login), field_text(&a->login));
    copy_sanitized(p->exclude, sizeof(p->exclude), field_text(&a->exclude));
    p->length = a->length;
    p->counter = a->counter;
    p->lower = a->lower;
    p->upper = a->upper;
    p->digits = a->digits;
    p->symbols = a->symbols;
}

static void
apply_profile(PassApp *a, const SavedProfile *p)
{
    if(p == NULL)
        return;
    field_set(&a->profile_name, p->name);
    field_set(&a->site, p->site);
    field_set(&a->login, p->login);
    field_set(&a->exclude, p->exclude);
    a->length = p->length > 0 ? p->length : 16;
    a->counter = p->counter > 0 ? p->counter : 1;
    a->lower = p->lower;
    a->upper = p->upper;
    a->digits = p->digits;
    a->symbols = p->symbols;
    snprintf(a->message, sizeof(a->message), "Loaded profile %s", p->name);
}

static void
save_current_profile(PassApp *a)
{
    SavedProfile p;
    int i;

    profile_from_current(a, &p);
    if(p.name[0] == '\0') {
        snprintf(a->message, sizeof(a->message), "%s", "Name the profile before saving");
        return;
    }
    for(i = 0; i < a->profile_count; i++) {
        if(strcmp(a->profiles[i].name, p.name) == 0) {
            a->profiles[i] = p;
            a->selected_profile = i;
            save_profiles(a);
            snprintf(a->message, sizeof(a->message), "Updated profile %s", p.name);
            return;
        }
    }
    if(a->profile_count >= MAX_PROFILES) {
        snprintf(a->message, sizeof(a->message), "%s", "Profile limit reached");
        return;
    }
    a->profiles[a->profile_count] = p;
    a->selected_profile = a->profile_count;
    a->profile_count++;
    save_profiles(a);
    snprintf(a->message, sizeof(a->message), "Saved profile %s", p.name);
}

static void
delete_selected_profile(PassApp *a)
{
    int i;

    if(a->selected_profile < 0 || a->selected_profile >= a->profile_count) {
        snprintf(a->message, sizeof(a->message), "%s", "Select a profile first");
        return;
    }
    for(i = a->selected_profile; i + 1 < a->profile_count; i++)
        a->profiles[i] = a->profiles[i + 1];
    a->profile_count--;
    a->selected_profile = -1;
    save_profiles(a);
    snprintf(a->message, sizeof(a->message), "%s", "Profile deleted");
}

static int64_t
now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void
clipboard_copy(PassApp *a, const char *value)
{
    SetClipboardText(value);
    snprintf(a->clip_value, sizeof(a->clip_value), "%s", value);
    a->clip_expires_ms = now_ms() + CLIPBOARD_LIFETIME_MS;
    a->clip_has_value = 1;
}

static void
clipboard_copy_for(PassApp *a, const char *value, int seconds)
{
    SetClipboardText(value);
    snprintf(a->clip_value, sizeof(a->clip_value), "%s", value);
    if(seconds <= 0)
        a->clip_expires_ms = 0;
    else
        a->clip_expires_ms = now_ms() + (int64_t)seconds * 1000;
    a->clip_has_value = 1;
}

static void
clipboard_tick(PassApp *a)
{
    const char *current;

    if(!a->clip_has_value || a->clip_expires_ms <= 0 || now_ms() < a->clip_expires_ms)
        return;
    current = GetClipboardText();
    if(current != NULL && strcmp(current, a->clip_value) == 0)
        SetClipboardText("");
    a->clip_has_value = 0;
    a->clip_value[0] = '\0';
}

static void
clipboard_clear(PassApp *a)
{
    const char *current;

    if(a->clip_has_value) {
        current = GetClipboardText();
        if(current != NULL && strcmp(current, a->clip_value) == 0)
            SetClipboardText("");
    }
    a->clip_has_value = 0;
    a->clip_value[0] = '\0';
}

static void
clear_generated(PassApp *a)
{
    memset(a->generated, 0, sizeof(a->generated));
    a->generated_expires_ms = 0;
}

static void
arm_master_timeout(PassApp *a)
{
    if(a->master.buffer[0] == '\0')
        a->master_expires_ms = 0;
    else
        a->master_expires_ms = now_ms() + SECRET_FIELD_LIFETIME_MS;
}

static void
arm_generated_timeout(PassApp *a)
{
    if(a->generated[0] == '\0')
        a->generated_expires_ms = 0;
    else
        a->generated_expires_ms = now_ms() + SECRET_FIELD_LIFETIME_MS;
}

static void
secret_tick(PassApp *a)
{
    int64_t now = now_ms();

    if(a->master_expires_ms > 0 && now >= a->master_expires_ms) {
        field_clear(&a->master);
        a->master_expires_ms = 0;
        a->reveal = 0;
        snprintf(a->message, sizeof(a->message), "%s", "Master password cleared");
    }
    if(a->generated_expires_ms > 0 && now >= a->generated_expires_ms) {
        clear_generated(a);
        if(a->message[0] == '\0' || strcmp(a->message, "Master password cleared") != 0)
            snprintf(a->message, sizeof(a->message), "%s", "Generated password cleared");
    }
}

static void
generate(PassApp *a)
{
    PassOptions options;
    char err[160];

    memset(&options, 0, sizeof(options));
    options.length = a->length;
    options.counter = (uint64_t)a->counter;
    options.lowercase = a->lower;
    options.uppercase = a->upper;
    options.digits = a->digits;
    options.symbols = a->symbols;
    options.exclude = a->exclude.buffer;

    if(pass_generate(field_text(&a->site), field_text(&a->login),
                       field_text(&a->master), &options,
                       a->generated, sizeof(a->generated),
                       err, sizeof(err)) != 0) {
        clear_generated(a);
        snprintf(a->message, sizeof(a->message), "%s", err);
        return;
    }
    arm_master_timeout(a);
    arm_generated_timeout(a);
    if(a->settings.auto_copy) {
        clipboard_copy_for(a, a->generated, a->settings.clear_after_seconds);
        if(a->settings.clear_after_seconds > 0)
            snprintf(a->message, sizeof(a->message), "Generated and copied for %ds", a->settings.clear_after_seconds);
        else
            snprintf(a->message, sizeof(a->message), "%s", "Generated and copied");
    } else {
        snprintf(a->message, sizeof(a->message), "%s", "Password generated locally");
    }
}

static void
poll_secure_result(PassApp *a)
{
    int status = android_bridge_secure_status();
    char result[1024];

    if(status == 0)
        return;
    if(status == 1) {
        snprintf(a->message, sizeof(a->message), "%s", "Waiting for biometric unlock");
        return;
    }
    status = android_bridge_take_secure_result(result, sizeof(result));
    if(status == 2) {
        if(a->secure_action == 1) {
            field_set(&a->master, result);
            arm_master_timeout(a);
            snprintf(a->message, sizeof(a->message), "%s", "Master password unlocked");
        } else {
            field_clear(&a->master);
            a->master_expires_ms = 0;
            snprintf(a->message, sizeof(a->message), "%s", result);
        }
    } else if(status == 3) {
        snprintf(a->message, sizeof(a->message), "%s", result[0] != '\0' ? result : "Secure storage failed");
    }
    a->secure_action = 0;
    memset(result, 0, sizeof(result));
}

static TextInputStyle
input_style(void)
{
    UIMaterialScheme s = GetUIMaterialScheme();
    TextInputStyle style;

    memset(&style, 0, sizeof(style));
    style.background = s.surface_container;
    style.border = s.outline;
    style.focus_border = s.primary;
    style.text = s.on_surface;
    style.cursor = s.primary;
    style.radius = 6.0f;
    style.padding_x = 10;
    style.padding_y = 8;
    return style;
}

static int
draw_field(Field *f, Rectangle bounds, int font, TextInputStyle style)
{
    TextFieldProps props;

    memset(&props, 0, sizeof(props));
    props.bounds = bounds;
    props.text = f->buffer;
    props.text_size = sizeof(f->buffer);
    props.cursor_position = &f->cursor;
    props.focused = &f->focused;
    props.max_codepoints = f->max_codepoints;
    props.font = font;
    props.focus_id = f->focus_id;
    props.style = style;
    props.commit_pressed = &f->commit;
    props.secure = f->secure;
    return TextField(props);
}

static void
keep_focused_field_visible(UIScrollArea area, Field *f, Rectangle bounds)
{
    if(f != NULL && f->focused)
        EnsureUIScrollRectVisible(area, bounds, ScaleUIPx(28));
}

static int
button(int x, int y, int w, int h, const char *label, ButtonStyle style, int disabled)
{
    UIMaterialScheme scheme = GetUIMaterialScheme();
    Color bg = scheme.primary;
    Color hover_bg = scheme.primary;
    Color text = scheme.on_primary;

    if(style == ButtonStyleSecondary) {
        bg = scheme.surface_variant;
        hover_bg = scheme.surface_variant;
        text = scheme.on_surface_variant;
    } else if(style == ButtonStyleDanger) {
        bg = scheme.error;
        hover_bg = scheme.error;
        text = scheme.on_error;
    }
    if(disabled) {
        bg = scheme.disabled_container;
        hover_bg = scheme.disabled_container;
        text = scheme.disabled_content;
    }

    return ButtonNode((ButtonSpec){
        .bounds = (Rectangle){(float)x, (float)y, (float)w, (float)h},
        .label = label,
        .font = GetUISmallFontSize(),
        .disabled = disabled,
        .background = bg,
        .hover_background = hover_bg,
        .text = text,
        .border = LightenUIColor(bg, 32),
        .radius = 0.08f
    });
}

static int
can_biometric_unlock_master(void)
{
    return android_bridge_master_saved() && android_bridge_master_biometric() &&
           android_bridge_biometric_available();
}

static const char *
fingerprint_unavailable_message(void)
{
    if(!android_bridge_biometric_available()) {
        if(android_bridge_biometric_setup_required())
            return "Set up Android fingerprint first";
        return "Fingerprint unlock is not available";
    }
    if(!android_bridge_master_saved())
        return "Save master with fingerprint in Settings first";
    if(!android_bridge_master_biometric())
        return "Saved master does not require fingerprint";
    return "Fingerprint unlock is not ready";
}

static int
should_show_fingerprint_button(void)
{
    return can_biometric_unlock_master();
}

static int
fingerprint_button(PassApp *a, int x, int y, int size, int disabled)
{
    UIMaterialScheme scheme = GetUIMaterialScheme();
    Rectangle bounds = (Rectangle){(float)x, (float)y, (float)size, (float)size};
    Color background = disabled ? scheme.surface_variant : scheme.primary;
    Color border = disabled ? scheme.outline : LightenUIColor(scheme.outline, 18);
    Color icon = disabled ? scheme.on_surface_variant : WHITE;
    int clicked;

    DrawRectangleRounded((Rectangle){bounds.x, bounds.y + (float)ScaleUIPx(2), bounds.width, bounds.height}, 0.18f, 10, DarkenUIColor(background, 16));
    DrawRectangleRounded(bounds, 0.18f, 10, background);
    DrawRectangleRoundedLinesEx(bounds, 0.18f, 10, 1.0f, border);

    clicked = IconButton((IconButtonProps){
        .bounds = bounds,
        .icon = a->icons[UI_ICON_TYPE_FINGERPRINT],
        .icon_size = size - ScaleUIPx(16),
        .icon_padding = ScaleUIPx(8),
        .disabled = 0,
        .background = BLANK,
        .hover_background = BLANK,
        .icon_color = icon,
        .border = BLANK,
        .radius = 0.18f,
    });
    if(clicked) {
        if(disabled) {
            snprintf(a->message, sizeof(a->message), "%s", fingerprint_unavailable_message());
        } else {
            android_bridge_set_soft_keyboard(0);
            a->secure_action = 1;
            android_bridge_unlock_master();
        }
    }
    return clicked;
}

static void
checkbox(int x, int y, const char *label, int *value)
{
    Checkbox((int)(Key(label) & 0x7fffffff), x, y, label, value);
}

static Rectangle
scaled_rect(float x, float y, float w, float h)
{
    Rectangle r;

    r.x = (float)ScaleUIPx((int)x);
    r.y = (float)ScaleUIPx((int)y);
    r.width = (float)ScaleUIPx((int)w);
    r.height = (float)ScaleUIPx((int)h);
    return r;
}

static void
label_text(const char *text, int x, int y, int font, Color color)
{
    Text(text, ScaleUIPx(x), ScaleUIPx(y), font, color);
}

static void
label_text_px(const char *text, int x, int y, int font, Color color)
{
    Text(text, x, y, font, color);
}

PassApp *
pass_app(void)
{
    static PassApp app;
    static int initialized;

    if(!initialized) {
        initialized = 1;
        field_init(&app.site, 101, 256);
        field_init(&app.login, 102, 256);
        field_init(&app.master, 103, 1024);
        field_init(&app.exclude, 104, 128);
        field_init(&app.profile_name, 105, 64);
        app.length = 16;
        app.counter = 1;
        app.lower = app.upper = app.digits = app.symbols = 1;
        app.selected_profile = -1;
        app.view = VIEW_GENERATE;
        LoadAllUIIconTextures(app.icons);
        load_settings(&app);
        load_profiles(&app);
    }
    return &app;
}

/* ------------------------------------------------------------------ */
/* Wide layout: mirrors gui/main.go                                    */
/* ------------------------------------------------------------------ */

static void
draw_wide(PassApp *a, int view_x, int width, int height, int top_reserved)
{
    UIMaterialScheme scheme = GetUIMaterialScheme();
    Color text = GetThemeText();
    TextInputStyle style = input_style();
    int content_w = width - 64;
    int x, y;

    (void)height;
    if(content_w > 720)
        content_w = 720;
    x = view_x + (width - content_w) / 2;
    y = top_reserved + 12;

    DrawRectangleRounded(scaled_rect((float)x, (float)y, (float)content_w, 550), 0.035f, 10, scheme.surface);
    DrawRectangleLinesEx(scaled_rect((float)x, (float)y, (float)content_w, 550), 1.0f, scheme.outline);
    label_text("PASSWORD DETAILS", x + 24, y + 24, 12, scheme.primary);

    y += 54;
    label_text("Site", x + 24, y, 14, text);
    draw_field(&a->site, scaled_rect((float)x + 24, (float)y + 24, (float)content_w - 48, 38), 16, style);
    y += 76;
    label_text("Login", x + 24, y, 14, text);
    draw_field(&a->login, scaled_rect((float)x + 24, (float)y + 24, (float)content_w - 48, 38), 16, style);
    y += 76;
    label_text("Master password", x + 24, y, 14, text);
    a->master.secure = !a->reveal;
    {
        int fp = should_show_fingerprint_button();
        int fp_disabled = !can_biometric_unlock_master();
        int field_w = content_w - (fp ? 196 : 150);

        draw_field(&a->master, scaled_rect((float)x + 24, (float)y + 24, (float)field_w, 38), 16, style);
        if(fp)
            fingerprint_button(a, ScaleUIPx(x + content_w - 158), ScaleUIPx(y + 24), ScaleUIPx(38), fp_disabled);
    }
    if(button(ScaleUIPx(x + content_w - 112), ScaleUIPx(y + 24), ScaleUIPx(88), ScaleUIPx(38),
              a->reveal ? "Hide" : "Reveal", ButtonStyleSecondary, 0))
        a->reveal = !a->reveal;
    y += 76;

    label_text("PASSWORD RULES", x + 24, y - 10, 12, scheme.primary);
    y += 14;
    label_text("Length", x + 24, y, 14, text);
    {
        SpinboxProps props;

        memset(&props, 0, sizeof(props));
        props.bounds = scaled_rect((float)x + 24, (float)y + 24, 130, 38);
        props.id = 301;
        props.min = 1;
        props.max = 128;
        props.step = 1;
        props.value = &a->length;
        Spinbox(props);
    }
    label_text("Counter", x + 182, y, 14, text);
    {
        SpinboxProps props;

        memset(&props, 0, sizeof(props));
        props.bounds = scaled_rect((float)x + 182, (float)y + 24, 130, 38);
        props.id = 302;
        props.min = 1;
        props.max = 999999;
        props.step = 1;
        props.value = &a->counter;
        Spinbox(props);
    }
    draw_field(&a->exclude, scaled_rect((float)x + 340, (float)y + 24, (float)content_w - 364, 38), 16, style);
    label_text("Excluded characters", x + 340, y, 14, text);
    y += 86;

    checkbox(ScaleUIPx(x + 24), ScaleUIPx(y), "Lowercase", &a->lower);
    checkbox(ScaleUIPx(x + 174), ScaleUIPx(y), "Uppercase", &a->upper);
    checkbox(ScaleUIPx(x + 324), ScaleUIPx(y), "Digits", &a->digits);
    checkbox(ScaleUIPx(x + 444), ScaleUIPx(y), "Symbols", &a->symbols);
    y += 58;

    if(button(ScaleUIPx(x + 24), ScaleUIPx(y), ScaleUIPx(150), ScaleUIPx(42),
              "Generate", ButtonStylePrimary, 0))
        generate(a);
    if(button(ScaleUIPx(x + 190), ScaleUIPx(y), ScaleUIPx(150), ScaleUIPx(42),
              "Copy for 20s", ButtonStyleSecondary, a->generated[0] == '\0')) {
        clipboard_copy(a, a->generated);
        snprintf(a->message, sizeof(a->message), "%s", "Copied; clipboard clears in 20 seconds");
    }
    if(button(ScaleUIPx(x + 356), ScaleUIPx(y), ScaleUIPx(120), ScaleUIPx(42),
              "Clear", ButtonStyleSecondary, 0)) {
        field_clear(&a->master);
        a->master_expires_ms = 0;
        clear_generated(a);
        snprintf(a->message, sizeof(a->message), "%s", "Cleared");
    }
    y += 56;

    DrawRectangleRounded(scaled_rect((float)x + 24, (float)y, (float)content_w - 48, 72), 0.08f, 10, scheme.surface_container);
    if(a->generated[0] != '\0')
        label_text(a->generated, x + 42, y + 15, 20, text);
    else
        label_text("Your generated password appears here", x + 42, y + 17, 16, scheme.on_surface_variant);
    if(a->message[0] != '\0')
        label_text(a->message, x + 42, y + 45, 12, scheme.on_surface_variant);
}

/* ------------------------------------------------------------------ */
/* Narrow phone layout                                                 */
/* ------------------------------------------------------------------ */

static void
draw_bottom_nav(PassApp *a, int view_w_px, int view_h_px, int bottom_margin_px)
{
    BottomNavItem items[3];
    BottomNavResult result;

    if(bottom_margin_px > 0)
        DrawRectangle(0, view_h_px - bottom_margin_px, view_w_px, bottom_margin_px, BLACK);

    memset(items, 0, sizeof(items));
    items[0] = (BottomNavItem){VIEW_GENERATE, "Generate", a->icons[UI_ICON_TYPE_PLAY], a->view == VIEW_GENERATE, 0};
    items[1] = (BottomNavItem){VIEW_PROFILES, "Profiles", a->icons[UI_ICON_TYPE_SAVE], a->view == VIEW_PROFILES, 0};
    items[2] = (BottomNavItem){VIEW_SETTINGS, "Settings", a->icons[UI_ICON_TYPE_GEAR], a->view == VIEW_SETTINGS, 0};

    result = BottomNav((BottomNavProps){
        .view_width = view_w_px,
        .view_height = view_h_px,
        .bottom_margin = bottom_margin_px,
        .count = 3,
        .items = items,
    });
    if(result.clicked_route >= VIEW_GENERATE && result.clicked_route <= VIEW_SETTINGS) {
        a->view = (AppView)result.clicked_route;
        a->scroll_offset = 0;
    }
}

static size_t
append_utf8(char *out, size_t used, size_t out_size, uint32_t codepoint)
{
    if(out == NULL || out_size == 0)
        return used;
    if(codepoint <= 0x7fu) {
        if(used + 1 >= out_size)
            return used;
        out[used++] = (char)codepoint;
    } else if(codepoint <= 0x7ffu) {
        if(used + 2 >= out_size)
            return used;
        out[used++] = (char)(0xc0u | (codepoint >> 6));
        out[used++] = (char)(0x80u | (codepoint & 0x3fu));
    } else if(codepoint <= 0xffffu) {
        if(used + 3 >= out_size)
            return used;
        out[used++] = (char)(0xe0u | (codepoint >> 12));
        out[used++] = (char)(0x80u | ((codepoint >> 6) & 0x3fu));
        out[used++] = (char)(0x80u | (codepoint & 0x3fu));
    } else {
        if(used + 4 >= out_size)
            return used;
        out[used++] = (char)(0xf0u | (codepoint >> 18));
        out[used++] = (char)(0x80u | ((codepoint >> 12) & 0x3fu));
        out[used++] = (char)(0x80u | ((codepoint >> 6) & 0x3fu));
        out[used++] = (char)(0x80u | (codepoint & 0x3fu));
    }
    out[used] = '\0';
    return used;
}

static void
fingerprint_emoji(const char *master, char *out, size_t out_size)
{
    uint8_t sum[32];
    size_t used = 0;
    int table_count;
    const int *table = pass_app_master_emoji_codepoints(&table_count);
    int i;

    if(out == NULL || out_size == 0)
        return;
    out[0] = '\0';
    pass_sha256(master != NULL ? master : "", master != NULL ? strlen(master) : 0, sum);
    for(i = 0; i < MASTER_EMOJI_COUNT && table_count > 0; i++)
        used = append_utf8(out, used, out_size, (uint32_t)table[sum[i] % table_count]);
}

static int
draw_master_fingerprint_symbols(PassApp *a, int x, int y)
{
    UIMaterialScheme scheme = GetUIMaterialScheme();
    char fp[64];
    int font_token;

    if(a == NULL || !a->settings.show_fingerprint || a->master.buffer[0] == '\0')
        return 0;

    fingerprint_emoji(a->master.buffer, fp, sizeof(fp));
    font_token = PushUIFont("pass-emoji");
    label_text_px(fp, x, y, 20, scheme.on_surface_variant);
    PopUIFont(font_token);
    return 30;
}

static int
draw_generate_page(PassApp *a, UIScrollArea area, int cx, int cy, int inner_w, int y)
{
    UIMaterialScheme scheme = GetUIMaterialScheme();
    Color text = GetThemeText();
    TextInputStyle style = input_style();
    int x = 0;
    int half_w = (inner_w - 12) / 2;

    label_text_px("Site", cx, cy + ScaleUIPx(y), 14, text);
    {
        Rectangle field = (Rectangle){(float)cx, (float)(cy + ScaleUIPx(y + 22)), (float)ScaleUIPx(inner_w), (float)ScaleUIPx(38)};
        draw_field(&a->site, field, 16, style);
        keep_focused_field_visible(area, &a->site, field);
    }
    y += 66;
    label_text_px("Login", cx, cy + ScaleUIPx(y), 14, text);
    {
        Rectangle field = (Rectangle){(float)cx, (float)(cy + ScaleUIPx(y + 22)), (float)ScaleUIPx(inner_w), (float)ScaleUIPx(38)};
        draw_field(&a->login, field, 16, style);
        keep_focused_field_visible(area, &a->login, field);
    }
    y += 66;

    label_text_px("Master password", cx, cy + ScaleUIPx(y), 14, text);
    a->master.secure = !a->reveal;
    {
        int fp = should_show_fingerprint_button();
        int fp_disabled = !can_biometric_unlock_master();
        int field_w = inner_w - (fp ? 130 : 80);
        Rectangle field = (Rectangle){(float)cx, (float)(cy + ScaleUIPx(y + 22)), (float)ScaleUIPx(field_w), (float)ScaleUIPx(38)};

        draw_field(&a->master, field, 16, style);
        keep_focused_field_visible(area, &a->master, field);
        if(fp)
            fingerprint_button(a, cx + ScaleUIPx(field_w + 8), cy + ScaleUIPx(y + 22), ScaleUIPx(42), fp_disabled);
        if(button(cx + ScaleUIPx(field_w + (fp ? 58 : 8)), cy + ScaleUIPx(y + 22), ScaleUIPx(72), ScaleUIPx(38),
                  a->reveal ? "Hide" : "Reveal", ButtonStyleSecondary, 0))
            a->reveal = !a->reveal;
    }
    y += 64;
    y += draw_master_fingerprint_symbols(a, cx, cy + ScaleUIPx(y));

    y += 6;
    label_text_px("Length", cx, cy + ScaleUIPx(y), 14, text);
    {
        SpinboxProps props;
        memset(&props, 0, sizeof(props));
        props.bounds = (Rectangle){(float)cx, (float)(cy + ScaleUIPx(y + 20)), (float)ScaleUIPx(half_w), (float)ScaleUIPx(38)};
        props.id = 301;
        props.min = 1;
        props.max = 128;
        props.step = 1;
        props.value = &a->length;
        Spinbox(props);
    }
    label_text_px("Counter", cx + ScaleUIPx(half_w + 12), cy + ScaleUIPx(y), 14, text);
    {
        SpinboxProps props;
        memset(&props, 0, sizeof(props));
        props.bounds = (Rectangle){(float)(cx + ScaleUIPx(half_w + 12)), (float)(cy + ScaleUIPx(y + 20)), (float)ScaleUIPx(half_w), (float)ScaleUIPx(38)};
        props.id = 302;
        props.min = 1;
        props.max = 999999;
        props.step = 1;
        props.value = &a->counter;
        Spinbox(props);
    }
    y += 76;

    label_text_px("Excluded characters", cx, cy + ScaleUIPx(y), 14, text);
    {
        Rectangle field = (Rectangle){(float)cx, (float)(cy + ScaleUIPx(y + 22)), (float)ScaleUIPx(inner_w), (float)ScaleUIPx(38)};
        draw_field(&a->exclude, field, 16, style);
        keep_focused_field_visible(area, &a->exclude, field);
    }
    y += 78;

    checkbox(cx, cy + ScaleUIPx(y), "Lowercase", &a->lower);
    checkbox(cx + ScaleUIPx(half_w + 12), cy + ScaleUIPx(y), "Uppercase", &a->upper);
    y += 36;
    checkbox(cx, cy + ScaleUIPx(y), "Digits", &a->digits);
    checkbox(cx + ScaleUIPx(half_w + 12), cy + ScaleUIPx(y), "Symbols", &a->symbols);
    y += 56;

    if(button(cx, cy + ScaleUIPx(y), ScaleUIPx(inner_w), ScaleUIPx(42), "Generate", ButtonStylePrimary, 0))
        generate(a);
    y += 50;
    if(button(cx, cy + ScaleUIPx(y), ScaleUIPx(half_w), ScaleUIPx(42), "Copy", ButtonStyleSecondary, a->generated[0] == '\0')) {
        clipboard_copy_for(a, a->generated, a->settings.clear_after_seconds);
        snprintf(a->message, sizeof(a->message), a->settings.clear_after_seconds > 0 ? "Copied for %ds" : "Copied", a->settings.clear_after_seconds);
    }
    if(button(cx + ScaleUIPx(half_w + 12), cy + ScaleUIPx(y), ScaleUIPx(half_w), ScaleUIPx(42), "Clear", ButtonStyleSecondary, 0)) {
        field_clear(&a->master);
        a->master_expires_ms = 0;
        clear_generated(a);
        snprintf(a->message, sizeof(a->message), "%s", "Cleared");
    }
    y += 50;

    DrawRectangleRounded((Rectangle){(float)cx, (float)(cy + ScaleUIPx(y)), (float)ScaleUIPx(inner_w), (float)ScaleUIPx(84)}, 0.08f, 10, scheme.surface_container);
    if(a->generated[0] != '\0')
        TextInRect(a->generated, (Rectangle){(float)(cx + ScaleUIPx(x + 14)), (float)(cy + ScaleUIPx(y + 8)), (float)ScaleUIPx(inner_w - 28), (float)ScaleUIPx(42)}, 16, text);
    else
        label_text_px("Your generated password appears here", cx + ScaleUIPx(14), cy + ScaleUIPx(y + 14), 14, scheme.on_surface_variant);
    if(a->message[0] != '\0')
        label_text_px(a->message, cx + ScaleUIPx(14), cy + ScaleUIPx(y + 56), 11, scheme.on_surface_variant);
    return y + 100;
}

static int
draw_profiles_page(PassApp *a, UIScrollArea area, int cx, int cy, int inner_w, int y)
{
    UIMaterialScheme scheme = GetUIMaterialScheme();
    Color text = GetThemeText();
    TextInputStyle style = input_style();
    int half_w = (inner_w - 12) / 2;
    int i;

    label_text_px("PROFILE", cx, cy + ScaleUIPx(y), 12, scheme.primary);
    y += 20;
    label_text_px("Name", cx, cy + ScaleUIPx(y), 14, text);
    {
        Rectangle field = (Rectangle){(float)cx, (float)(cy + ScaleUIPx(y + 22)), (float)ScaleUIPx(inner_w), (float)ScaleUIPx(38)};
        draw_field(&a->profile_name, field, 16, style);
        keep_focused_field_visible(area, &a->profile_name, field);
    }
    y += 70;
    if(button(cx, cy + ScaleUIPx(y), ScaleUIPx(half_w), ScaleUIPx(42), "Save Current", ButtonStylePrimary, 0))
        save_current_profile(a);
    if(button(cx + ScaleUIPx(half_w + 12), cy + ScaleUIPx(y), ScaleUIPx(half_w), ScaleUIPx(42), "Delete", ButtonStyleSecondary, a->selected_profile < 0))
        delete_selected_profile(a);
    y += 58;

    label_text_px("SAVED PROFILES", cx, cy + ScaleUIPx(y), 12, scheme.primary);
    y += 24;
    if(a->profile_count == 0) {
        label_text_px("No profiles saved yet", cx, cy + ScaleUIPx(y), 14, scheme.on_surface_variant);
        y += 34;
    }
    for(i = 0; i < a->profile_count; i++) {
        char label[256];
        SavedProfile *p = &a->profiles[i];

        snprintf(label, sizeof(label), "%s  -  %s", p->name, p->site[0] != '\0' ? p->site : "no site");
        if(button(cx, cy + ScaleUIPx(y), ScaleUIPx(inner_w), ScaleUIPx(42),
                  label, i == a->selected_profile ? ButtonStylePrimary : ButtonStyleSecondary, 0)) {
            a->selected_profile = i;
            apply_profile(a, p);
            a->view = VIEW_GENERATE;
        }
        y += 50;
    }
    if(a->message[0] != '\0') {
        label_text_px(a->message, cx, cy + ScaleUIPx(y + 8), 12, scheme.on_surface_variant);
        y += 32;
    }
    return y + 20;
}

static int
draw_settings_page(PassApp *a, UIScrollArea area, int cx, int cy, int inner_w, int y)
{
    UIMaterialScheme scheme = GetUIMaterialScheme();
    Color text = GetThemeText();
    TextInputStyle style = input_style();
    int half_w = (inner_w - 12) / 2;

    label_text_px("PASSWORD DEFAULTS", cx, cy + ScaleUIPx(y), 12, scheme.primary);
    y += 24;
    label_text_px("Length", cx, cy + ScaleUIPx(y), 14, text);
    {
        SpinboxProps props;
        memset(&props, 0, sizeof(props));
        props.bounds = (Rectangle){(float)cx, (float)(cy + ScaleUIPx(y + 20)), (float)ScaleUIPx(half_w), (float)ScaleUIPx(38)};
        props.id = 511;
        props.min = 1;
        props.max = 128;
        props.step = 1;
        props.value = &a->length;
        Spinbox(props);
    }
    label_text_px("Counter", cx + ScaleUIPx(half_w + 12), cy + ScaleUIPx(y), 14, text);
    {
        SpinboxProps props;
        memset(&props, 0, sizeof(props));
        props.bounds = (Rectangle){(float)(cx + ScaleUIPx(half_w + 12)), (float)(cy + ScaleUIPx(y + 20)), (float)ScaleUIPx(half_w), (float)ScaleUIPx(38)};
        props.id = 512;
        props.min = 1;
        props.max = 999999;
        props.step = 1;
        props.value = &a->counter;
        Spinbox(props);
    }
    y += 62;
    label_text_px("Excluded characters", cx, cy + ScaleUIPx(y), 14, text);
    {
        Rectangle field = (Rectangle){(float)cx, (float)(cy + ScaleUIPx(y + 22)), (float)ScaleUIPx(inner_w), (float)ScaleUIPx(38)};
        draw_field(&a->exclude, field, 16, style);
        keep_focused_field_visible(area, &a->exclude, field);
    }
    y += 78;
    checkbox(cx, cy + ScaleUIPx(y), "Lowercase", &a->lower);
    checkbox(cx + ScaleUIPx(half_w + 12), cy + ScaleUIPx(y), "Uppercase", &a->upper);
    y += 36;
    checkbox(cx, cy + ScaleUIPx(y), "Digits", &a->digits);
    checkbox(cx + ScaleUIPx(half_w + 12), cy + ScaleUIPx(y), "Symbols", &a->symbols);
    y += 62;

    if(button(cx, cy + ScaleUIPx(y), ScaleUIPx(inner_w), ScaleUIPx(42), "Save Settings", ButtonStylePrimary, 0))
        save_settings(a);
    y += 64;

    label_text_px("MASTER PASSWORD", cx, cy + ScaleUIPx(y), 12, scheme.primary);
    y += 24;
    label_text_px("Master password", cx, cy + ScaleUIPx(y), 14, text);
    a->master.secure = !a->reveal;
    {
        int fp = should_show_fingerprint_button();
        int fp_disabled = !can_biometric_unlock_master();
        int field_w = inner_w - (fp ? 130 : 80);
        Rectangle field = (Rectangle){(float)cx, (float)(cy + ScaleUIPx(y + 22)), (float)ScaleUIPx(field_w), (float)ScaleUIPx(38)};

        draw_field(&a->master, field, 16, style);
        keep_focused_field_visible(area, &a->master, field);
        if(fp)
            fingerprint_button(a, cx + ScaleUIPx(field_w + 8), cy + ScaleUIPx(y + 22), ScaleUIPx(42), fp_disabled);
        if(button(cx + ScaleUIPx(field_w + (fp ? 58 : 8)), cy + ScaleUIPx(y + 22), ScaleUIPx(72), ScaleUIPx(38),
                  a->reveal ? "Hide" : "Reveal", ButtonStyleSecondary, 0))
            a->reveal = !a->reveal;
    }
    y += 82;
    y += draw_master_fingerprint_symbols(a, cx, cy + ScaleUIPx(y));
    if(a->settings.show_fingerprint && a->master.buffer[0] != '\0')
        y += 12;
    checkbox(cx, cy + ScaleUIPx(y), "Use fingerprint unlock", &a->settings.use_biometric);
    y += 44;
    checkbox(cx, cy + ScaleUIPx(y), "Show master fingerprint", &a->settings.show_fingerprint);
    y += 48;
    if(!android_bridge_biometric_available())
        label_text_px(android_bridge_biometric_setup_required()
                          ? "Android fingerprint setup required"
                          : "Biometric unlock is not available on this device",
                      cx, cy + ScaleUIPx(y), 12, scheme.on_surface_variant);
    else if(android_bridge_master_saved())
        label_text_px(android_bridge_master_biometric() ? "Saved master uses fingerprint unlock" : "Saved master unlocks without fingerprint",
                      cx, cy + ScaleUIPx(y), 12, scheme.on_surface_variant);
    else
        label_text_px("No saved master password", cx, cy + ScaleUIPx(y), 12, scheme.on_surface_variant);
    y += 36;
    if(button(cx, cy + ScaleUIPx(y), ScaleUIPx(inner_w), ScaleUIPx(42), "Save With Fingerprint", ButtonStylePrimary, !android_bridge_biometric_available())) {
        android_bridge_set_soft_keyboard(0);
        a->settings.use_biometric = 1;
        a->secure_action = 2;
        android_bridge_save_master(a->master.buffer, 1);
    }
    y += 50;
    if(button(cx, cy + ScaleUIPx(y), ScaleUIPx(half_w), ScaleUIPx(42), "Save Without", ButtonStyleSecondary, 0)) {
        android_bridge_set_soft_keyboard(0);
        a->settings.use_biometric = 0;
        a->secure_action = 2;
        android_bridge_save_master(a->master.buffer, 0);
    }
    if(button(cx + ScaleUIPx(half_w + 12), cy + ScaleUIPx(y), ScaleUIPx(half_w), ScaleUIPx(42), "Unlock", ButtonStyleSecondary, !android_bridge_master_saved())) {
        android_bridge_set_soft_keyboard(0);
        a->secure_action = 1;
        android_bridge_unlock_master();
    }
    y += 50;
    if(button(cx, cy + ScaleUIPx(y), ScaleUIPx(half_w), ScaleUIPx(42), "Save Settings", ButtonStyleSecondary, 0))
        save_settings(a);
    if(button(cx + ScaleUIPx(half_w + 12), cy + ScaleUIPx(y), ScaleUIPx(half_w), ScaleUIPx(42), "Forget Master", ButtonStyleSecondary, !android_bridge_master_saved())) {
        a->secure_action = 2;
        android_bridge_clear_master();
        field_clear(&a->master);
        a->master_expires_ms = 0;
    }
    y += 58;
    if(a->message[0] != '\0') {
        DrawRectangleRounded((Rectangle){(float)cx, (float)(cy + ScaleUIPx(y)), (float)ScaleUIPx(inner_w), (float)ScaleUIPx(64)}, 0.08f, 10, scheme.surface_container);
        label_text_px(a->message, cx + ScaleUIPx(14), cy + ScaleUIPx(y + 22), 12, scheme.on_surface_variant);
        y += 80;
    }
    return y + 20;
}

static void
draw_narrow(PassApp *a, int view_x, int width, int height, int top_reserved, int bottom_reserved)
{
    UIScrollArea area;
    UIScrollView view;
    int card_x = view_x;
    int card_y = top_reserved + 8;
    int card_w = width;
    int nav_reserved = 64;
    int card_h = height - bottom_reserved - nav_reserved - card_y;
    int pad = 24;
    int inner_x = card_x + pad;
    int inner_w = card_w - pad * 2;
    int content_h;
    int y, cx, cy;
    Rectangle bounds;

    if(a->last_layout_w != width || a->last_layout_h != height) {
        a->scroll_offset = 0;
        a->last_layout_w = width;
        a->last_layout_h = height;
    }

    if(card_h < 200)
        card_h = 200;
    switch(a->view) {
    case VIEW_PROFILES:
        content_h = 210 + a->profile_count * 50 + pad;
        break;
    case VIEW_SETTINGS:
        content_h = 880 + pad;
        break;
    default:
        content_h = 650 + pad;
        break;
    }

    bounds = scaled_rect((float)card_x, (float)card_y, (float)card_w, (float)card_h);
    memset(&area, 0, sizeof(area));
    area.bounds = bounds;
    area.content_height = ScaleUIPx(content_h);
    area.content_x = ScaleUIPx(inner_x);
    area.content_width = ScaleUIPx(inner_w);
    area.scroll_offset = &a->scroll_offset;
    view = BeginUIScrollContainer(area);
    cx = view.content_x;
    cy = view.content_y;
    y = 20;

    switch(a->view) {
    case VIEW_PROFILES:
        draw_profiles_page(a, area, cx, cy, inner_w, y);
        break;
    case VIEW_SETTINGS:
        draw_settings_page(a, area, cx, cy, inner_w, y);
        break;
    default:
        draw_generate_page(a, area, cx, cy, inner_w, y);
        break;
    }
    EndUIScrollContainer(area, view);
}

void
pass_app_draw(PassApp *a, int surface_w, int surface_h, float dpi,
                int left_reserved, int top_reserved,
                int right_reserved, int bottom_reserved)
{
    int ui_w = (int)(surface_w / (dpi > 0.0f ? dpi : 1.0f) + 0.5f);
    int ui_h = (int)(surface_h / (dpi > 0.0f ? dpi : 1.0f) + 0.5f);
    int safe_x = left_reserved;
    int safe_w = ui_w - left_reserved - right_reserved;

    if(safe_w < 280) {
        safe_x = 0;
        safe_w = ui_w;
    }
    Background(GetThemeBackground());
    clipboard_tick(a);
    secret_tick(a);
    poll_secure_result(a);
    if(safe_w >= 760 && safe_w > ui_h && ui_h >= 760)
        draw_wide(a, safe_x, safe_w, ui_h, top_reserved);
    else
        draw_narrow(a, safe_x, safe_w, ui_h, top_reserved, bottom_reserved);
    autosave_settings(a);
    draw_bottom_nav(a, surface_w - ScaleUIPx(right_reserved), surface_h,
                    ScaleUIPx(bottom_reserved));
}

void
pass_app_shutdown(PassApp *a)
{
    if(a == NULL)
        return;
    field_clear(&a->master);
    clipboard_clear(a);
    clear_generated(a);
    UnloadAllUIIconTextures(a->icons);
}
