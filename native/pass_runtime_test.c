#include "pass_runtime.h"

#include "android_host.h"
#include "pass_core.h"
#include "kryon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int failures = 0;
static double fake_time = 0.0;
static char fake_clipboard[256];
static int fake_biometric_available = 0;
static int fake_biometric_setup_required = 0;
static int fake_master_saved = 0;
static int fake_master_biometric = 0;
static int fake_secure_status = 0;
static char fake_secure_result[1024];
static char fake_saved_master[1024];
static int fake_saved_require_biometric = 0;

void AndroidHostInit(void) {}
void AndroidHostApplySystemTheme(void) {}
int AndroidHostLeftReserved(void) { return 0; }
int AndroidHostTopReserved(void) { return 0; }
int AndroidHostRightReserved(void) { return 0; }
int AndroidHostBottomReserved(void) { return 0; }
void AndroidHostSetSoftKeyboardVisible(int visible) { (void)visible; }
int AndroidSecureStoreBiometricAvailable(void) { return fake_biometric_available; }
int AndroidSecureStoreBiometricSetupRequired(void) { return fake_biometric_setup_required; }
int AndroidSecureStoreHasSecret(const char *key) { (void)key; return fake_master_saved; }
int AndroidSecureStoreSecretUsesBiometric(const char *key) { (void)key; return fake_master_biometric; }
int AndroidSecureStoreStatus(const char *key) { (void)key; return fake_secure_status; }

void
AndroidSecureStoreSaveSecret(const char *key, const char *master,
                             int require_biometric, const char *label)
{
    (void)key;
    (void)label;
    snprintf(fake_saved_master, sizeof(fake_saved_master), "%s", master != NULL ? master : "");
    fake_saved_require_biometric = require_biometric != 0;
    fake_master_saved = 1;
    fake_master_biometric = require_biometric != 0;
}

void
AndroidSecureStoreUnlockSecret(const char *key, const char *label)
{
    (void)key;
    (void)label;
}

void
AndroidSecureStoreClearSecret(const char *key)
{
    (void)key;
    fake_master_saved = 0;
    fake_master_biometric = 0;
    fake_saved_master[0] = '\0';
}

int
AndroidSecureStoreTakeResult(const char *key, char *out, int out_size)
{
    int status = fake_secure_status;

    (void)key;
    if(out != NULL && out_size > 0)
        snprintf(out, (size_t)out_size, "%s", fake_secure_result);
    fake_secure_status = 0;
    fake_secure_result[0] = '\0';
    return status;
}

double GetTime(void) { return fake_time; }

void
SetClipboardText(const char *text)
{
    snprintf(fake_clipboard, sizeof(fake_clipboard), "%s", text != NULL ? text : "");
}

static void
check_int(const char *name, int got, int want)
{
    if(got != want) {
        printf("FAIL runtime %s: got %d, want %d\n", name, got, want);
        failures++;
    } else {
        printf("ok   runtime %s: %d\n", name, got);
    }
}

static void
check_str(const char *name, const char *got, const char *want)
{
    if(strcmp(got != NULL ? got : "", want) != 0) {
        printf("FAIL runtime %s: got \"%s\", want \"%s\"\n",
               name, got != NULL ? got : "", want);
        failures++;
    } else {
        printf("ok   runtime %s: \"%s\"\n", name, got != NULL ? got : "");
    }
}

static void
write_initial_files(void)
{
    FILE *f = fopen("settings.cfg", "w");

    if(f == NULL) {
        perror("settings.cfg");
        exit(1);
    }
    fputs("ignored line\n", f);
    fputs("auto_copy=1\n", f);
    fputs("clear_after_seconds=3\n", f);
    fputs("show_fingerprint=0\n", f);
    fputs("length=99\n", f);
    fputs("counter=7\n", f);
    fputs("lower=1\n", f);
    fputs("upper=0\n", f);
    fputs("digits=1\n", f);
    fputs("symbols=0\n", f);
    fputs("exclude=01\n", f);
    fputs("theme_source=1\n", f);
    fputs("theme_mode=0\n", f);
    fputs("theme_id=10\n", f);
    fputs("theme_style=0\n", f);
    fclose(f);

    f = fopen("profiles.tsv", "w");
    if(f == NULL) {
        perror("profiles.tsv");
        exit(1);
    }
    fputs("broken\n", f);
    fputs("Loaded\tloaded.example\talice\t99\t2\t1\t0\t1\t0\t0\n", f);
    fclose(f);
}

static void
remove_storage_files(void)
{
    remove(".kryon_pass_auto_copy.txt");
    remove(".kryon_pass_clear_after_seconds.txt");
    remove(".kryon_pass_show_fingerprint.txt");
    remove(".kryon_pass_length.txt");
    remove(".kryon_pass_counter.txt");
    remove(".kryon_pass_lower.txt");
    remove(".kryon_pass_upper.txt");
    remove(".kryon_pass_digits.txt");
    remove(".kryon_pass_symbols.txt");
    remove(".kryon_pass_exclude.txt");
    remove(".kryon_pass_theme_source.txt");
    remove(".kryon_pass_theme_mode.txt");
    remove(".kryon_pass_theme_id.txt");
    remove(".kryon_pass_theme_style.txt");
}

static void
check_loaded_settings(void)
{
    int auto_copy = -1;
    int clear_seconds = -1;
    int show_fingerprint = -1;
    int length = -1;
    int counter = -1;
    int lower = -1;
    int upper = -1;
    int digits = -1;
    int symbols = -1;
    int theme_source = -1;
    int theme_mode = -1;
    int theme_id = -1;
    int theme_style = -1;
    char exclude[16];

    memset(exclude, 0, sizeof(exclude));
    pass_load_settings(&auto_copy, &clear_seconds, &show_fingerprint,
                       &length, &counter, &lower, &upper, &digits,
                       &symbols, exclude, sizeof(exclude),
                       &theme_source, &theme_mode, &theme_id, &theme_style);
    check_int("loaded auto_copy", auto_copy, 1);
    check_int("loaded clear_seconds", clear_seconds, 3);
    check_int("loaded show_fingerprint", show_fingerprint, 0);
    check_int("loaded length clamp", length, PASS_MAX_LENGTH);
    check_int("loaded counter", counter, 7);
    check_int("loaded lower", lower, 1);
    check_int("loaded upper", upper, 0);
    check_int("loaded digits", digits, 1);
    check_int("loaded symbols", symbols, 0);
    check_str("loaded exclude", exclude, "01");
    check_int("migrated theme_source", theme_source, THEME_SOURCE_APP);
    check_int("migrated theme_mode", theme_mode, THEME_MODE_SYSTEM);
    check_int("migrated theme_id", theme_id, THEME_SWEET);
    check_int("migrated theme_style", theme_style, THEME_STYLE_MATERIAL);

    pass_load_settings(NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                       NULL, NULL, 0, NULL, NULL, NULL, NULL);
}

static void
check_generation_and_clipboard(void)
{
    check_str("initial status", pass_status(), "Ready");
    check_str("empty emoji", pass_master_emoji(""), "");
    check_int("nonempty emoji", (int)strlen(pass_master_emoji("master")) > 0, 1);

    check_int("generate", pass_generate("lesspass.com", "contact@lesspass.com",
                                        "password", 16, 1, 1, 1, 1, 1, ""), 0);
    check_str("generated", pass_generated(), "\\g-A1-.OHEwrXjT#");
    check_str("auto clipboard", fake_clipboard, "\\g-A1-.OHEwrXjT#");
    check_str("copy status", pass_status(), "Copied for 3 seconds");

    fake_time = 4.0;
    pass_runtime_tick();
    check_str("clipboard cleared", fake_clipboard, "");
    check_str("clipboard clear status", pass_status(), "Clipboard cleared");

    check_int("save settings", pass_save_settings(0, -2, 1, 1, 0, 1, 1, 1, 1,
                                                  NULL, 9, 8, 7, 6), 0);
    check_int("generate invalid", pass_generate("site", "login", "master",
                                               8, 1, 0, 0, 0, 0, ""), 1);
    check_str("invalid clears generated", pass_generated(), "");
    check_int("copy empty", pass_copy(), 1);

    check_int("generate no autocopy", pass_generate("site", "login", "master",
                                                    16, -4, 1, 1, 1, 1, ""), 0);
    check_str("manual generate status", pass_status(), "Password generated locally");
    check_int("copy no timeout", pass_copy(), 0);
    check_str("copy no timeout status", pass_status(), "Copied");
}

static void
check_profiles(void)
{
    check_int("loaded profile count", pass_profile_count(), 1);
    check_str("loaded profile label", pass_profile_label(0),
              "Loaded  loaded.example / alice");
    check_str("missing profile label", pass_profile_label(-1), "");
    check_int("generate loaded profile", pass_generate_profile(0, "secret"), 0);
    check_int("missing profile generate", pass_generate_profile(100, "secret"), 1);
    check_int("empty profile name", pass_save_profile("", "site", "login", 16, 1,
                                                      1, 1, 1, 1, ""), 1);
    check_int("save profile", pass_save_profile("Work", "work.example", "me", 16, 1,
                                                1, 1, 1, 1, ""), 0);
    check_int("overwrite profile", pass_save_profile("Work", "work.example", "you", 18, 2,
                                                     1, 0, 1, 0, "01"), 0);
    check_str("saved profile label", pass_profile_label(1),
              "Work  work.example / you");
    check_int("delete missing profile", pass_delete_profile(99), 1);
    check_int("save second profile", pass_save_profile("Home", "home.example", "me", 16, 1,
                                                       1, 1, 1, 1, ""), 0);
    check_int("delete first profile", pass_delete_profile(0), 0);
    check_str("shifted profile label", pass_profile_label(0),
              "Work  work.example / you");
    check_int("delete profile", pass_delete_profile(0), 0);

    for(int i = pass_profile_count(); i < 64; i++) {
        char name[32];

        snprintf(name, sizeof(name), "Fill%d", i);
        if(pass_save_profile(name, "fill.example", "me", 16, 1,
                             1, 1, 1, 1, "") != 0) {
            printf("FAIL runtime fill profile %d: %s\n", i, pass_status());
            failures++;
            return;
        }
    }
    check_int("profile limit", pass_save_profile("Overflow", "fill.example", "me", 16, 1,
                                                 1, 1, 1, 1, ""), 1);
}

static void
check_secure_master(void)
{
    char unlocked[1024];
    int count = 0;

    check_int("emoji count", pass_runtime_master_emoji_codepoints(&count) != NULL, 1);
    check_int("emoji count value", count, 64);
    check_int("save empty master", pass_save_master("", 1), 1);
    check_str("save empty status", pass_status(), "Enter master password first");

    fake_biometric_available = 0;
    fake_biometric_setup_required = 0;
    check_int("save unavailable master", pass_save_master("secret", 1), 1);
    check_str("save unavailable status", pass_status(), "Fingerprint unlock unavailable");

    fake_biometric_setup_required = 1;
    check_int("save setup master", pass_save_master("secret", 1), 1);
    check_str("save setup status", pass_status(), "Set up Android fingerprint first");

    fake_biometric_available = 1;
    fake_biometric_setup_required = 0;
    check_int("save master", pass_save_master("secret", 1), 0);
    check_str("saving status", pass_status(), "Saving master");
    check_str("saved master stub", fake_saved_master, "secret");
    check_int("saved master biometric flag", fake_saved_require_biometric, 1);
    snprintf(fake_secure_result, sizeof(fake_secure_result), "%s", "");
    fake_secure_status = 2;
    pass_runtime_tick();
    check_str("saved status result", pass_status(), "Master password saved");

    check_int("can unlock", AndroidSecureStoreHasSecret("default") &&
              AndroidSecureStoreSecretUsesBiometric("default") &&
              AndroidSecureStoreBiometricAvailable(), 1);
    check_int("master saved", AndroidSecureStoreHasSecret("default"), 1);
    check_int("biometric available", AndroidSecureStoreBiometricAvailable(), 1);
    check_str("fingerprint saved", pass_fingerprint_status(),
              "Saved master uses fingerprint unlock");
    fake_master_biometric = 0;
    check_str("fingerprint saved no biometric", pass_fingerprint_status(),
              "Saved master unlocks without fingerprint");
    fake_master_biometric = 1;

    check_int("unlock master", pass_unlock_master(), 0);
    check_str("unlock requested", pass_status(), "Unlock requested");
    snprintf(fake_secure_result, sizeof(fake_secure_result), "%s", "secret");
    fake_secure_status = 2;
    pass_runtime_tick();
    check_str("unlocked status", pass_status(), "Master unlocked");
    memset(unlocked, 0, sizeof(unlocked));
    check_int("take unlocked", pass_take_unlocked_master(unlocked), 1);
    check_str("unlocked value", unlocked, "secret");
    check_int("take unlocked empty", pass_take_unlocked_master(unlocked), 0);
    check_int("take unlocked null", pass_take_unlocked_master(NULL), 0);

    check_int("clear master", pass_clear_master(), 0);
    check_int("unlock missing master", pass_unlock_master(), 1);
    fake_biometric_available = 0;
    fake_biometric_setup_required = 1;
    check_str("fingerprint setup required", pass_fingerprint_status(),
              "Android fingerprint setup required");
    fake_biometric_setup_required = 0;
    check_str("fingerprint unavailable", pass_fingerprint_status(),
              "Fingerprint unlock is not available");
    fake_biometric_available = 1;
    check_str("fingerprint none saved", pass_fingerprint_status(),
              "No saved master password");

    check_int("save master no biometric", pass_save_master("plain", 0), 0);
    snprintf(fake_secure_result, sizeof(fake_secure_result), "%s", "denied");
    fake_secure_status = 3;
    pass_runtime_tick();
    check_str("secure failure", pass_status(), "denied");
}

int
main(void)
{
    char original_cwd[1024];

    if(getcwd(original_cwd, sizeof(original_cwd)) == NULL) {
        perror("getcwd");
        return 1;
    }
    mkdir("build", 0777);
    mkdir("build/coverage", 0777);
    mkdir("build/coverage/runtime-work", 0777);
    if(chdir("build/coverage/runtime-work") != 0) {
        perror("chdir");
        return 1;
    }
    unlink("settings.cfg");
    unlink("profiles.tsv");
    remove_storage_files();
    write_initial_files();

    pass_runtime_init();
    check_loaded_settings();
    check_generation_and_clipboard();
    check_profiles();
    check_secure_master();
    pass_runtime_shutdown();
    check_str("shutdown generated", pass_generated(), "");
    unlink("settings.cfg");
    unlink("profiles.tsv");
    remove_storage_files();
    pass_runtime_init();
    check_int("default profile count", pass_profile_count(), 0);
    check_str("default status", pass_status(), "Ready");
    remove_storage_files();

    if(chdir(original_cwd) != 0) {
        perror("restore cwd");
        return 1;
    }
    if(failures != 0) {
        printf("%d runtime failure(s)\n", failures);
        return 1;
    }
    printf("all runtime tests pass\n");
    return 0;
}
