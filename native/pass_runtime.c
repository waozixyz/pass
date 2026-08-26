#include "pass_runtime.h"

#include "pass_core.h"

#include "android_bridge.h"
#include "kryon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PASS_MAX_PROFILES 64
#define PASS_NAME_SIZE 64
#define PASS_SITE_SIZE 256
#define PASS_LOGIN_SIZE 256
#define PASS_EXCLUDE_SIZE 128
#define PASS_PASSWORD_SIZE 256
#define PASS_STATUS_SIZE 256

typedef struct {
    char name[PASS_NAME_SIZE];
    char site[PASS_SITE_SIZE];
    char login[PASS_LOGIN_SIZE];
    int length;
    int counter;
    int lower;
    int upper;
    int digits;
    int symbols;
    char exclude[PASS_EXCLUDE_SIZE];
} PassProfile;

typedef struct {
    int auto_copy;
    int clear_seconds;
    int show_fingerprint;
    int length;
    int counter;
    int lower;
    int upper;
    int digits;
    int symbols;
    char exclude[PASS_EXCLUDE_SIZE];
    int theme_source;
    int theme_mode;
    int theme_id;
    int theme_style;
} PassRuntimeSettings;

typedef struct {
    PassRuntimeSettings settings;
    PassProfile profiles[PASS_MAX_PROFILES];
    int profile_count;
    char generated[PASS_PASSWORD_SIZE];
    char status[PASS_STATUS_SIZE];
    char fingerprint_status[PASS_STATUS_SIZE];
    char master_emoji[32];
    char profile_label[PASS_SITE_SIZE + PASS_LOGIN_SIZE + PASS_NAME_SIZE + 16];
    char unlocked_master[1024];
    int has_unlocked_master;
    int secure_action;
    double clipboard_clear_at;
} PassRuntime;

enum {
    PASS_SECURE_ACTION_NONE = 0,
    PASS_SECURE_ACTION_SAVE,
    PASS_SECURE_ACTION_UNLOCK
};

static PassRuntime runtime;

static void
copy_text(char *dst, size_t dst_size, const char *src)
{
    size_t len;

    if(dst == NULL || dst_size == 0)
        return;
    if(src == NULL)
        src = "";
    len = strlen(src);
    if(len >= dst_size)
        len = dst_size - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static int
clamp_length(int length)
{
    if(length < PASS_MIN_LENGTH)
        return PASS_MIN_LENGTH;
    if(length > PASS_MAX_LENGTH)
        return PASS_MAX_LENGTH;
    return length;
}

static void
runtime_defaults(void)
{
    memset(&runtime, 0, sizeof(runtime));
    runtime.settings.auto_copy = 0;
    runtime.settings.clear_seconds = 20;
    runtime.settings.show_fingerprint = 1;
    runtime.settings.length = 16;
    runtime.settings.counter = 1;
    runtime.settings.lower = 1;
    runtime.settings.upper = 1;
    runtime.settings.digits = 1;
    runtime.settings.symbols = 1;
    runtime.settings.exclude[0] = '\0';
    runtime.settings.theme_source = THEME_SOURCE_APP;
    runtime.settings.theme_mode = THEME_MODE_SYSTEM;
    runtime.settings.theme_id = THEME_SWEET;
    runtime.settings.theme_style = THEME_STYLE_MATERIAL;
    copy_text(runtime.status, sizeof(runtime.status), "Ready");
    copy_text(runtime.fingerprint_status, sizeof(runtime.fingerprint_status), "No saved master password");
}

static void
trim_newline(char *s)
{
    size_t len;

    if(s == NULL)
        return;
    len = strlen(s);
    while(len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[len - 1] = '\0';
        len--;
    }
}

static void
load_settings(void)
{
    FILE *f = fopen("settings.cfg", "r");
    char line[256];

    if(f == NULL)
        return;
    while(fgets(line, sizeof(line), f) != NULL) {
        char *value;

        trim_newline(line);
        value = strchr(line, '=');
        if(value == NULL)
            continue;
        *value++ = '\0';
        if(strcmp(line, "auto_copy") == 0)
            runtime.settings.auto_copy = atoi(value) != 0;
        else if(strcmp(line, "clear_after_seconds") == 0)
            runtime.settings.clear_seconds = atoi(value);
        else if(strcmp(line, "show_fingerprint") == 0)
            runtime.settings.show_fingerprint = atoi(value) != 0;
        else if(strcmp(line, "length") == 0)
            runtime.settings.length = clamp_length(atoi(value));
        else if(strcmp(line, "counter") == 0)
            runtime.settings.counter = atoi(value);
        else if(strcmp(line, "lower") == 0)
            runtime.settings.lower = atoi(value) != 0;
        else if(strcmp(line, "upper") == 0)
            runtime.settings.upper = atoi(value) != 0;
        else if(strcmp(line, "digits") == 0)
            runtime.settings.digits = atoi(value) != 0;
        else if(strcmp(line, "symbols") == 0)
            runtime.settings.symbols = atoi(value) != 0;
        else if(strcmp(line, "exclude") == 0)
            copy_text(runtime.settings.exclude, sizeof(runtime.settings.exclude), value);
        else if(strcmp(line, "theme_source") == 0)
            runtime.settings.theme_source = atoi(value);
        else if(strcmp(line, "theme_mode") == 0)
            runtime.settings.theme_mode = atoi(value);
        else if(strcmp(line, "theme_id") == 0)
            runtime.settings.theme_id = atoi(value);
        else if(strcmp(line, "theme_style") == 0)
            runtime.settings.theme_style = atoi(value);
    }
    fclose(f);
}

static void
migrate_default_theme_settings(void)
{
    int legacy_system_default =
        runtime.settings.theme_source == THEME_SOURCE_SYSTEM &&
        runtime.settings.theme_mode == THEME_MODE_SYSTEM &&
        (runtime.settings.theme_id == THEME_MINT ||
         runtime.settings.theme_id == THEME_SWEET) &&
        (runtime.settings.theme_style == THEME_STYLE_SYSTEM ||
         runtime.settings.theme_style == THEME_STYLE_MATERIAL);

    if(!legacy_system_default)
        return;

    runtime.settings.theme_source = THEME_SOURCE_APP;
    runtime.settings.theme_mode = THEME_MODE_SYSTEM;
    runtime.settings.theme_id = THEME_SWEET;
    runtime.settings.theme_style = THEME_STYLE_MATERIAL;
}

static int
write_settings(void)
{
    FILE *f = fopen("settings.cfg", "w");

    if(f == NULL) {
        copy_text(runtime.status, sizeof(runtime.status), "Could not save settings");
        return 1;
    }
    fprintf(f, "auto_copy=%d\n", runtime.settings.auto_copy ? 1 : 0);
    fprintf(f, "clear_after_seconds=%d\n", runtime.settings.clear_seconds);
    fprintf(f, "show_fingerprint=%d\n", runtime.settings.show_fingerprint ? 1 : 0);
    fprintf(f, "length=%d\n", runtime.settings.length);
    fprintf(f, "counter=%d\n", runtime.settings.counter);
    fprintf(f, "lower=%d\n", runtime.settings.lower ? 1 : 0);
    fprintf(f, "upper=%d\n", runtime.settings.upper ? 1 : 0);
    fprintf(f, "digits=%d\n", runtime.settings.digits ? 1 : 0);
    fprintf(f, "symbols=%d\n", runtime.settings.symbols ? 1 : 0);
    fprintf(f, "exclude=%s\n", runtime.settings.exclude);
    fprintf(f, "theme_source=%d\n", runtime.settings.theme_source);
    fprintf(f, "theme_mode=%d\n", runtime.settings.theme_mode);
    fprintf(f, "theme_id=%d\n", runtime.settings.theme_id);
    fprintf(f, "theme_style=%d\n", runtime.settings.theme_style);
    fclose(f);
    copy_text(runtime.status, sizeof(runtime.status), "Settings saved");
    return 0;
}

static int
read_field(char **cursor, char *dst, size_t dst_size)
{
    char *start;
    char *tab;

    if(cursor == NULL || *cursor == NULL)
        return 0;
    start = *cursor;
    tab = strchr(start, '\t');
    if(tab != NULL) {
        *tab = '\0';
        *cursor = tab + 1;
    } else {
        *cursor = NULL;
    }
    copy_text(dst, dst_size, start);
    return 1;
}

static void
load_profiles(void)
{
    FILE *f = fopen("profiles.tsv", "r");
    char line[1024];

    runtime.profile_count = 0;
    if(f == NULL)
        return;
    while(runtime.profile_count < PASS_MAX_PROFILES &&
          fgets(line, sizeof(line), f) != NULL) {
        PassProfile *p = &runtime.profiles[runtime.profile_count];
        char *cursor = line;
        char scratch[64];

        trim_newline(line);
        memset(p, 0, sizeof(*p));
        if(!read_field(&cursor, p->name, sizeof(p->name)) ||
           !read_field(&cursor, p->site, sizeof(p->site)) ||
           !read_field(&cursor, p->login, sizeof(p->login)) ||
           !read_field(&cursor, scratch, sizeof(scratch)))
            continue;
        p->length = clamp_length(atoi(scratch));
        if(!read_field(&cursor, scratch, sizeof(scratch)))
            continue;
        p->counter = atoi(scratch);
        if(!read_field(&cursor, scratch, sizeof(scratch)))
            continue;
        p->lower = atoi(scratch) != 0;
        if(!read_field(&cursor, scratch, sizeof(scratch)))
            continue;
        p->upper = atoi(scratch) != 0;
        if(!read_field(&cursor, scratch, sizeof(scratch)))
            continue;
        p->digits = atoi(scratch) != 0;
        if(!read_field(&cursor, scratch, sizeof(scratch)))
            continue;
        p->symbols = atoi(scratch) != 0;
        if(cursor != NULL)
            copy_text(p->exclude, sizeof(p->exclude), cursor);
        runtime.profile_count++;
    }
    fclose(f);
}

static int
write_profiles(void)
{
    FILE *f = fopen("profiles.tsv", "w");
    int i;

    if(f == NULL) {
        copy_text(runtime.status, sizeof(runtime.status), "Could not save profiles");
        return 1;
    }
    for(i = 0; i < runtime.profile_count; i++) {
        PassProfile *p = &runtime.profiles[i];

        fprintf(f, "%s\t%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%s\n",
                p->name, p->site, p->login, p->length, p->counter,
                p->lower ? 1 : 0, p->upper ? 1 : 0,
                p->digits ? 1 : 0, p->symbols ? 1 : 0, p->exclude);
    }
    fclose(f);
    return 0;
}

static int
find_profile(const char *name)
{
    int i;

    for(i = 0; i < runtime.profile_count; i++) {
        if(strcmp(runtime.profiles[i].name, name) == 0)
            return i;
    }
    return -1;
}

void
pass_runtime_init(void)
{
    runtime_defaults();
    load_settings();
    migrate_default_theme_settings();
    load_profiles();
}

void
pass_runtime_tick(void)
{
    char result[1024];
    int status = android_bridge_take_secure_result(result, sizeof(result));

    if(status == 2) {
        if(runtime.secure_action == PASS_SECURE_ACTION_UNLOCK) {
            copy_text(runtime.unlocked_master, sizeof(runtime.unlocked_master), result);
            runtime.has_unlocked_master = result[0] != '\0';
            copy_text(runtime.status, sizeof(runtime.status), "Master unlocked");
        } else {
            copy_text(runtime.status, sizeof(runtime.status),
                      result[0] ? result : "Master password saved");
        }
        runtime.secure_action = PASS_SECURE_ACTION_NONE;
    } else if(status == 3) {
        copy_text(runtime.status, sizeof(runtime.status), result[0] ? result : "Secure master failed");
        runtime.secure_action = PASS_SECURE_ACTION_NONE;
    }

    if(runtime.clipboard_clear_at > 0.0 && GetTime() >= runtime.clipboard_clear_at) {
        SetClipboardText("");
        runtime.clipboard_clear_at = 0.0;
        copy_text(runtime.status, sizeof(runtime.status), "Clipboard cleared");
    }
}

void
pass_runtime_shutdown(void)
{
    memset(runtime.generated, 0, sizeof(runtime.generated));
    memset(runtime.unlocked_master, 0, sizeof(runtime.unlocked_master));
    runtime.has_unlocked_master = 0;
}

const int *
pass_runtime_master_emoji_codepoints(int *count)
{
    return pass_core_master_emoji_codepoints(count);
}

int pass_safe_left(void) { return android_bridge_left_reserved(); }
int pass_safe_top(void) { return android_bridge_top_reserved(); }
int pass_safe_right(void) { return android_bridge_right_reserved(); }
int pass_safe_bottom(void) { return android_bridge_bottom_reserved(); }

int
pass_generate(const char *site, const char *login, const char *master,
              int length, int counter,
              int lower, int upper, int digits, int symbols,
              const char *exclude)
{
    PassOptions options;
    char err[PASS_STATUS_SIZE];

    memset(&options, 0, sizeof(options));
    options.length = length;
    options.counter = (unsigned long long)(counter < 0 ? 0 : counter);
    options.lowercase = lower != 0;
    options.uppercase = upper != 0;
    options.digits = digits != 0;
    options.symbols = symbols != 0;
    options.exclude = exclude != NULL ? exclude : "";
    memset(err, 0, sizeof(err));

    if(pass_core_generate(site, login, master, &options,
                          runtime.generated, sizeof(runtime.generated),
                          err, sizeof(err)) != 0) {
        memset(runtime.generated, 0, sizeof(runtime.generated));
        copy_text(runtime.status, sizeof(runtime.status), err);
        return 1;
    }

    if(runtime.settings.auto_copy)
        pass_copy();
    else
        copy_text(runtime.status, sizeof(runtime.status), "Password generated locally");
    return 0;
}

char *
pass_generated(void)
{
    return runtime.generated;
}

char *
pass_master_emoji(const char *master)
{
    if(master == NULL || master[0] == '\0') {
        runtime.master_emoji[0] = '\0';
        return runtime.master_emoji;
    }
    pass_core_master_emoji(master, runtime.master_emoji,
                           sizeof(runtime.master_emoji));
    return runtime.master_emoji;
}

char *
pass_status(void)
{
    return runtime.status;
}

int
pass_copy(void)
{
    if(runtime.generated[0] == '\0') {
        copy_text(runtime.status, sizeof(runtime.status), "Generate a password first");
        return 1;
    }
    SetClipboardText(runtime.generated);
    if(runtime.settings.clear_seconds > 0) {
        runtime.clipboard_clear_at = GetTime() + runtime.settings.clear_seconds;
        snprintf(runtime.status, sizeof(runtime.status),
                 "Copied for %d seconds", runtime.settings.clear_seconds);
    } else {
        runtime.clipboard_clear_at = 0.0;
        copy_text(runtime.status, sizeof(runtime.status), "Copied");
    }
    return 0;
}

int
pass_save_profile(const char *name, const char *site, const char *login,
                  int length, int counter,
                  int lower, int upper, int digits, int symbols,
                  const char *exclude)
{
    int index;
    PassProfile *p;

    if(name == NULL || name[0] == '\0') {
        copy_text(runtime.status, sizeof(runtime.status), "Profile name required");
        return 1;
    }
    index = find_profile(name);
    if(index < 0) {
        if(runtime.profile_count >= PASS_MAX_PROFILES) {
            copy_text(runtime.status, sizeof(runtime.status), "Profile limit reached");
            return 1;
        }
        index = runtime.profile_count++;
    }
    p = &runtime.profiles[index];
    copy_text(p->name, sizeof(p->name), name);
    copy_text(p->site, sizeof(p->site), site);
    copy_text(p->login, sizeof(p->login), login);
    p->length = length;
    p->counter = counter;
    p->lower = lower != 0;
    p->upper = upper != 0;
    p->digits = digits != 0;
    p->symbols = symbols != 0;
    copy_text(p->exclude, sizeof(p->exclude), exclude);

    if(write_profiles() != 0)
        return 1;
    copy_text(runtime.status, sizeof(runtime.status), "Profile saved");
    return 0;
}

int
pass_profile_count(void)
{
    return runtime.profile_count;
}

char *
pass_profile_label(int index)
{
    PassProfile *p;

    if(index < 0 || index >= runtime.profile_count) {
        runtime.profile_label[0] = '\0';
        return runtime.profile_label;
    }
    p = &runtime.profiles[index];
    snprintf(runtime.profile_label, sizeof(runtime.profile_label),
             "%s  %s / %s", p->name, p->site, p->login);
    return runtime.profile_label;
}

int
pass_generate_profile(int index, const char *master)
{
    PassProfile *p;

    if(index < 0 || index >= runtime.profile_count) {
        copy_text(runtime.status, sizeof(runtime.status), "Profile not found");
        return 1;
    }
    p = &runtime.profiles[index];
    return pass_generate(p->site, p->login, master, p->length, p->counter,
                         p->lower, p->upper, p->digits, p->symbols, p->exclude);
}

int
pass_delete_profile(int index)
{
    int i;

    if(index < 0 || index >= runtime.profile_count) {
        copy_text(runtime.status, sizeof(runtime.status), "Profile not found");
        return 1;
    }
    for(i = index; i + 1 < runtime.profile_count; i++)
        runtime.profiles[i] = runtime.profiles[i + 1];
    runtime.profile_count--;
    if(write_profiles() != 0)
        return 1;
    copy_text(runtime.status, sizeof(runtime.status), "Profile deleted");
    return 0;
}

int
pass_save_settings(int auto_copy, int clear_seconds, int show_fingerprint,
                   int length, int counter,
                   int lower, int upper, int digits, int symbols,
                   const char *exclude,
                   int theme_source, int theme_mode,
                   int theme_id, int theme_style)
{
    runtime.settings.auto_copy = auto_copy != 0;
    runtime.settings.clear_seconds = clear_seconds < 0 ? 0 : clear_seconds;
    runtime.settings.show_fingerprint = show_fingerprint != 0;
    runtime.settings.length = clamp_length(length);
    runtime.settings.counter = counter < 1 ? 1 : counter;
    runtime.settings.lower = lower != 0;
    runtime.settings.upper = upper != 0;
    runtime.settings.digits = digits != 0;
    runtime.settings.symbols = symbols != 0;
    copy_text(runtime.settings.exclude, sizeof(runtime.settings.exclude), exclude);
    runtime.settings.theme_source = theme_source;
    runtime.settings.theme_mode = theme_mode;
    runtime.settings.theme_id = theme_id;
    runtime.settings.theme_style = theme_style;
    return write_settings();
}

int
pass_load_settings(int *auto_copy, int *clear_seconds, int *show_fingerprint,
                   int *length, int *counter,
                   int *lower, int *upper, int *digits, int *symbols,
                   char *exclude, int exclude_size,
                   int *theme_source, int *theme_mode,
                   int *theme_id, int *theme_style)
{
    if(auto_copy != NULL)
        *auto_copy = runtime.settings.auto_copy;
    if(clear_seconds != NULL)
        *clear_seconds = runtime.settings.clear_seconds;
    if(show_fingerprint != NULL)
        *show_fingerprint = runtime.settings.show_fingerprint;
    if(length != NULL)
        *length = runtime.settings.length;
    if(counter != NULL)
        *counter = runtime.settings.counter;
    if(lower != NULL)
        *lower = runtime.settings.lower;
    if(upper != NULL)
        *upper = runtime.settings.upper;
    if(digits != NULL)
        *digits = runtime.settings.digits;
    if(symbols != NULL)
        *symbols = runtime.settings.symbols;
    if(exclude != NULL && exclude_size > 0)
        copy_text(exclude, (size_t)exclude_size, runtime.settings.exclude);
    if(theme_source != NULL)
        *theme_source = runtime.settings.theme_source;
    if(theme_mode != NULL)
        *theme_mode = runtime.settings.theme_mode;
    if(theme_id != NULL)
        *theme_id = runtime.settings.theme_id;
    if(theme_style != NULL)
        *theme_style = runtime.settings.theme_style;
    return 0;
}

int
pass_save_master(const char *master, int require_biometric)
{
    if(master == NULL || master[0] == '\0') {
        runtime.secure_action = PASS_SECURE_ACTION_NONE;
        copy_text(runtime.status, sizeof(runtime.status), "Enter master password first");
        return 1;
    }
    if(require_biometric && !android_bridge_biometric_available()) {
        runtime.secure_action = PASS_SECURE_ACTION_NONE;
        copy_text(runtime.status, sizeof(runtime.status),
                  android_bridge_biometric_setup_required()
                      ? "Set up Android fingerprint first"
                      : "Fingerprint unlock unavailable");
        return 1;
    }
    runtime.secure_action = PASS_SECURE_ACTION_SAVE;
    android_bridge_save_master(master, require_biometric);
    copy_text(runtime.status, sizeof(runtime.status), "Saving master");
    return 0;
}

int
pass_unlock_master(void)
{
    if(!android_bridge_master_saved()) {
        runtime.secure_action = PASS_SECURE_ACTION_NONE;
        copy_text(runtime.status, sizeof(runtime.status), "No saved master password");
        return 1;
    }
    runtime.secure_action = PASS_SECURE_ACTION_UNLOCK;
    android_bridge_unlock_master();
    copy_text(runtime.status, sizeof(runtime.status), "Unlock requested");
    return 0;
}

int
pass_clear_master(void)
{
    android_bridge_clear_master();
    memset(runtime.unlocked_master, 0, sizeof(runtime.unlocked_master));
    runtime.has_unlocked_master = 0;
    runtime.secure_action = PASS_SECURE_ACTION_NONE;
    copy_text(runtime.status, sizeof(runtime.status), "Saved master forgotten");
    return 0;
}

int
pass_can_unlock_master(void)
{
    return android_bridge_master_saved() &&
           android_bridge_master_biometric() &&
           android_bridge_biometric_available();
}

int
pass_biometric_available(void)
{
    return android_bridge_biometric_available();
}

int
pass_master_saved(void)
{
    return android_bridge_master_saved();
}

char *
pass_fingerprint_status(void)
{
    if(!android_bridge_biometric_available()) {
        copy_text(runtime.fingerprint_status, sizeof(runtime.fingerprint_status),
                  android_bridge_biometric_setup_required()
                      ? "Android fingerprint setup required"
                      : "Fingerprint unlock is not available");
    } else if(android_bridge_master_saved()) {
        copy_text(runtime.fingerprint_status, sizeof(runtime.fingerprint_status),
                  android_bridge_master_biometric()
                      ? "Saved master uses fingerprint unlock"
                      : "Saved master unlocks without fingerprint");
    } else {
        copy_text(runtime.fingerprint_status, sizeof(runtime.fingerprint_status),
                  "No saved master password");
    }
    return runtime.fingerprint_status;
}

int
pass_take_unlocked_master(char *out)
{
    if(out == NULL || !runtime.has_unlocked_master)
        return 0;
    copy_text(out, 1024, runtime.unlocked_master);
    memset(runtime.unlocked_master, 0, sizeof(runtime.unlocked_master));
    runtime.has_unlocked_master = 0;
    return 1;
}
