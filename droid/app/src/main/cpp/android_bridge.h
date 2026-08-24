#ifndef PASS_ANDROID_BRIDGE_H
#define PASS_ANDROID_BRIDGE_H

#if defined(__cplusplus)
extern "C" {
#endif

/* Call once before InitWindow. */
void android_bridge_init(void);
void android_bridge_apply_system_theme(void);

/* Safe-area insets converted to UI units (java px / density). */
int android_bridge_left_reserved(void);
int android_bridge_top_reserved(void);
int android_bridge_right_reserved(void);
int android_bridge_bottom_reserved(void);

/* Shows or hides the soft keyboard through the activity. */
void android_bridge_set_soft_keyboard(int visible);

int android_bridge_biometric_available(void);
int android_bridge_biometric_setup_required(void);
int android_bridge_master_saved(void);
int android_bridge_master_biometric(void);
void android_bridge_save_master(const char *master, int require_biometric);
void android_bridge_unlock_master(void);
void android_bridge_clear_master(void);
int android_bridge_secure_status(void);
int android_bridge_take_secure_result(char *out, int out_size);

#if defined(__cplusplus)
}
#endif

#endif
