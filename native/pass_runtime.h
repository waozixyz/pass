#ifndef PASS_RUNTIME_H
#define PASS_RUNTIME_H

#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

void pass_runtime_init(void);
void pass_runtime_tick(void);
void pass_runtime_shutdown(void);

const int *pass_runtime_master_emoji_codepoints(int *count);
char *pass_master_emoji(const char *master);

int pass_safe_left(void);
int pass_safe_top(void);
int pass_safe_right(void);
int pass_safe_bottom(void);

int pass_generate(const char *site, const char *login, const char *master,
                  int length, int counter,
                  int lower, int upper, int digits, int symbols,
                  const char *exclude);
char *pass_generated(void);
char *pass_status(void);
int pass_copy(void);

int pass_save_profile(const char *name, const char *site, const char *login,
                      int length, int counter,
                      int lower, int upper, int digits, int symbols,
                      const char *exclude);
int pass_profile_count(void);
char *pass_profile_label(int index);
int pass_generate_profile(int index, const char *master);
int pass_delete_profile(int index);

int pass_save_settings(int auto_copy, int clear_seconds, int show_fingerprint,
                       int length, int counter,
                       int lower, int upper, int digits, int symbols,
                       const char *exclude,
                       int theme_source, int theme_mode,
                       int theme_id, int theme_style);
int pass_load_settings(int *auto_copy, int *clear_seconds, int *show_fingerprint,
                       int *length, int *counter,
                       int *lower, int *upper, int *digits, int *symbols,
                       char *exclude, int exclude_size,
                       int *theme_source, int *theme_mode,
                       int *theme_id, int *theme_style);
int pass_save_master(const char *master, int require_biometric);
int pass_unlock_master(void);
int pass_clear_master(void);
int pass_can_unlock_master(void);
int pass_biometric_available(void);
int pass_master_saved(void);
char *pass_fingerprint_status(void);
int pass_take_unlocked_master(char *out);

#if defined(__cplusplus)
}
#endif

#endif
