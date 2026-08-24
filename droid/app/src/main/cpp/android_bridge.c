/* Minimal JNI bridge for the pass Android app: safe-area insets, display
 * density, soft-keyboard visibility, and soft-keyboard text input. Modeled
 * on inbe's android_device.c / android_insets.c. */

#include "android_bridge.h"

#include <stddef.h>

#if ANDROID_BUILD

#include "kryon.h"
#include "theme.h"
#include "ui_dpi.h"

#include <android/log.h>
#include <android_native_app_glue.h>
#include <jni.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

extern struct android_app *GetAndroidApp(void);

#ifndef JNI_VERSION_1_6
#define JNI_VERSION_1_6 0x10060000
#endif

#define LOG_TAG "PASS_JNI"

static pthread_mutex_t bridge_mutex = PTHREAD_MUTEX_INITIALIZER;
static int insets_system_left = 0;
static int insets_system_top = 0;
static int insets_system_right = 0;
static int insets_system_bottom = 0;
static int insets_ime_bottom = 0;
static int insets_cutout_left = 0;
static int insets_cutout_top = 0;
static int insets_cutout_right = 0;
static int insets_cutout_bottom = 0;
static int insets_ready = 0;
static float device_density = 0.0f;

static int
scaled_inset(int java_px, float density)
{
    if(density <= 0.0f)
        density = 1.0f;
    return (int)(java_px / density + 0.5f);
}

static int
activity_env(JNIEnv **env_out, JavaVM **jvm_out, jobject *activity_out)
{
    struct android_app *app = GetAndroidApp();
    JavaVM *jvm;
    JNIEnv *env = NULL;
    int attached = 0;

    if(env_out == NULL || jvm_out == NULL || activity_out == NULL ||
       app == NULL || app->activity == NULL || app->activity->vm == NULL ||
       app->activity->clazz == NULL)
        return -1;

    jvm = app->activity->vm;
    if((*jvm)->GetEnv(jvm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
        if((*jvm)->AttachCurrentThread(jvm, &env, NULL) != JNI_OK || env == NULL)
            return -1;
        attached = 1;
    }
    *env_out = env;
    *jvm_out = jvm;
    *activity_out = app->activity->clazz;
    return attached;
}

static void
activity_env_done(JNIEnv *env, JavaVM *jvm, int attached)
{
    if(env != NULL && (*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
    }
    if(attached && jvm != NULL)
        (*jvm)->DetachCurrentThread(jvm);
}

void
android_bridge_init(void)
{
    pthread_mutex_lock(&bridge_mutex);
    insets_system_left = 0;
    insets_system_top = 0;
    insets_system_right = 0;
    insets_system_bottom = 0;
    insets_ime_bottom = 0;
    insets_cutout_left = 0;
    insets_cutout_top = 0;
    insets_cutout_right = 0;
    insets_cutout_bottom = 0;
    insets_ready = 0;
    device_density = 0.0f;
    pthread_mutex_unlock(&bridge_mutex);
}

static Color
color_from_argb(jint argb)
{
    Color color;

    color.a = (unsigned char)((argb >> 24) & 0xff);
    color.r = (unsigned char)((argb >> 16) & 0xff);
    color.g = (unsigned char)((argb >> 8) & 0xff);
    color.b = (unsigned char)(argb & 0xff);
    if(color.a == 0)
        color.a = 0xff;
    return color;
}

void
android_bridge_apply_system_theme(void)
{
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jobject activity;
    jclass activity_class;
    jmethodID method;
    jintArray array;
    jint values[9];
    jsize len;
    int attached;

    attached = activity_env(&env, &jvm, &activity);
    if(attached < 0)
        return;

    memset(values, 0, sizeof(values));
    activity_class = (*env)->GetObjectClass(env, activity);
    if(activity_class == NULL)
        goto done;
    method = (*env)->GetMethodID(env, activity_class, "systemThemeColors", "()[I");
    if(method == NULL) {
        __android_log_write(ANDROID_LOG_ERROR, LOG_TAG, "systemThemeColors not found");
        goto done;
    }
    array = (jintArray)(*env)->CallObjectMethod(env, activity, method);
    if(array == NULL)
        goto done;
    len = (*env)->GetArrayLength(env, array);
    if(len < 9)
        goto done;
    (*env)->GetIntArrayRegion(env, array, 0, 9, values);
    SetSystemThemePalette("Android",
                          color_from_argb(values[1]),
                          color_from_argb(values[2]),
                          color_from_argb(values[3]),
                          color_from_argb(values[4]),
                          color_from_argb(values[5]),
                          color_from_argb(values[6]),
                          color_from_argb(values[7]),
                          color_from_argb(values[8]),
                          values[0] != 0,
                          1);

done:
    activity_env_done(env, jvm, attached);
}

int
android_bridge_left_reserved(void)
{
    int system, cutout, ready;
    float density;

    pthread_mutex_lock(&bridge_mutex);
    system = insets_system_left;
    cutout = insets_cutout_left;
    density = device_density;
    ready = insets_ready;
    pthread_mutex_unlock(&bridge_mutex);

    if(!ready)
        return 0;
    return scaled_inset(system > cutout ? system : cutout, density);
}

int
android_bridge_top_reserved(void)
{
    int system, cutout, top, ready;
    float density;

    pthread_mutex_lock(&bridge_mutex);
    system = insets_system_top;
    cutout = insets_cutout_top;
    density = device_density;
    ready = insets_ready;
    pthread_mutex_unlock(&bridge_mutex);

    if(!ready)
        return 28; /* conservative status-bar guess until Java reports */
    top = system > cutout ? system : cutout;
    return scaled_inset(top, density);
}

int
android_bridge_right_reserved(void)
{
    int system, cutout, ready;
    float density;

    pthread_mutex_lock(&bridge_mutex);
    system = insets_system_right;
    cutout = insets_cutout_right;
    density = device_density;
    ready = insets_ready;
    pthread_mutex_unlock(&bridge_mutex);

    if(!ready)
        return 0;
    return scaled_inset(system > cutout ? system : cutout, density);
}

int
android_bridge_bottom_reserved(void)
{
    int system, ime, cutout, bottom, ready;
    float density;

    pthread_mutex_lock(&bridge_mutex);
    system = insets_system_bottom;
    ime = insets_ime_bottom;
    cutout = insets_cutout_bottom;
    density = device_density;
    ready = insets_ready;
    pthread_mutex_unlock(&bridge_mutex);

    if(!ready)
        return 48; /* conservative nav-bar guess until Java reports */
    bottom = system > cutout ? system : cutout;
    if(ime > bottom)
        bottom = ime;
    return scaled_inset(bottom, density);
}

void
android_bridge_set_soft_keyboard(int visible)
{
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jobject activity;
    jclass activity_class;
    jmethodID method;
    int attached;

    attached = activity_env(&env, &jvm, &activity);
    if(attached < 0) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "soft keyboard %d: no activity env", visible);
        return;
    }

    activity_class = (*env)->GetObjectClass(env, activity);
    if(activity_class == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "soft keyboard %d: no activity class", visible);
        goto done;
    }

    method = (*env)->GetMethodID(env, activity_class, "setSoftKeyboardVisible", "(Z)V");
    if(method == NULL) {
        __android_log_write(ANDROID_LOG_ERROR, LOG_TAG, "setSoftKeyboardVisible not found");
        goto done;
    }

    (*env)->CallVoidMethod(env, activity, method, visible ? JNI_TRUE : JNI_FALSE);

done:
    activity_env_done(env, jvm, attached);
}

static int
call_boolean_method(const char *name)
{
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jobject activity;
    jclass activity_class;
    jmethodID method;
    int attached;
    int result = 0;

    attached = activity_env(&env, &jvm, &activity);
    if(attached < 0)
        return 0;
    activity_class = (*env)->GetObjectClass(env, activity);
    if(activity_class == NULL)
        goto done;
    method = (*env)->GetMethodID(env, activity_class, name, "()Z");
    if(method == NULL)
        goto done;
    result = (*env)->CallBooleanMethod(env, activity, method) ? 1 : 0;

done:
    activity_env_done(env, jvm, attached);
    return result;
}

int
android_bridge_biometric_available(void)
{
    return call_boolean_method("isBiometricAvailable");
}

int
android_bridge_biometric_setup_required(void)
{
    return call_boolean_method("isBiometricSetupRequired");
}

int
android_bridge_master_saved(void)
{
    return call_boolean_method("hasStoredMasterPassword");
}

int
android_bridge_master_biometric(void)
{
    return call_boolean_method("isStoredMasterBiometric");
}

void
android_bridge_save_master(const char *master, int require_biometric)
{
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jobject activity;
    jclass activity_class;
    jmethodID method;
    jstring jmaster;
    int attached;

    attached = activity_env(&env, &jvm, &activity);
    if(attached < 0)
        return;
    activity_class = (*env)->GetObjectClass(env, activity);
    if(activity_class == NULL)
        goto done;
    method = (*env)->GetMethodID(env, activity_class, "saveMasterPassword", "(Ljava/lang/String;Z)V");
    if(method == NULL)
        goto done;
    jmaster = (*env)->NewStringUTF(env, master != NULL ? master : "");
    if(jmaster == NULL)
        goto done;
    (*env)->CallVoidMethod(env, activity, method, jmaster, require_biometric ? JNI_TRUE : JNI_FALSE);
    (*env)->DeleteLocalRef(env, jmaster);

done:
    activity_env_done(env, jvm, attached);
}

static void
call_void_method(const char *name)
{
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jobject activity;
    jclass activity_class;
    jmethodID method;
    int attached;

    attached = activity_env(&env, &jvm, &activity);
    if(attached < 0)
        return;
    activity_class = (*env)->GetObjectClass(env, activity);
    if(activity_class == NULL)
        goto done;
    method = (*env)->GetMethodID(env, activity_class, name, "()V");
    if(method == NULL)
        goto done;
    (*env)->CallVoidMethod(env, activity, method);

done:
    activity_env_done(env, jvm, attached);
}

void
android_bridge_unlock_master(void)
{
    call_void_method("unlockMasterPassword");
}

void
android_bridge_clear_master(void)
{
    call_void_method("clearMasterPassword");
}

int
android_bridge_secure_status(void)
{
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jobject activity;
    jclass activity_class;
    jmethodID method;
    int attached;
    int result = 0;

    attached = activity_env(&env, &jvm, &activity);
    if(attached < 0)
        return 0;
    activity_class = (*env)->GetObjectClass(env, activity);
    if(activity_class == NULL)
        goto done;
    method = (*env)->GetMethodID(env, activity_class, "secureMasterStatus", "()I");
    if(method == NULL)
        goto done;
    result = (int)(*env)->CallIntMethod(env, activity, method);

done:
    activity_env_done(env, jvm, attached);
    return result;
}

int
android_bridge_take_secure_result(char *out, int out_size)
{
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jobject activity;
    jclass activity_class;
    jmethodID method;
    jstring result;
    const char *chars;
    int attached;
    int status = 0;

    if(out != NULL && out_size > 0)
        out[0] = '\0';
    attached = activity_env(&env, &jvm, &activity);
    if(attached < 0)
        return 0;
    activity_class = (*env)->GetObjectClass(env, activity);
    if(activity_class == NULL)
        goto done;
    method = (*env)->GetMethodID(env, activity_class, "secureMasterStatus", "()I");
    if(method == NULL)
        goto done;
    status = (int)(*env)->CallIntMethod(env, activity, method);
    if(status != 2 && status != 3)
        goto done;
    method = (*env)->GetMethodID(env, activity_class, "takeSecureMasterResult", "()Ljava/lang/String;");
    if(method == NULL)
        goto done;
    result = (jstring)(*env)->CallObjectMethod(env, activity, method);
    if(result == NULL)
        goto done;
    chars = (*env)->GetStringUTFChars(env, result, NULL);
    if(chars != NULL) {
        if(out != NULL && out_size > 0)
            snprintf(out, (size_t)out_size, "%s", chars);
        (*env)->ReleaseStringUTFChars(env, result, chars);
    }
    (*env)->DeleteLocalRef(env, result);

done:
    activity_env_done(env, jvm, attached);
    return status;
}

/* ---- Java -> C natives (exported JNI symbols; RegisterNatives from
 * JNI_OnLoad resolves through the boot loader here and does not bind to the
 * activity's class, so we rely on standard dlsym resolution instead) ---- */

JNIEXPORT void JNICALL
Java_xyz_waozi_pass_MainActivity_nativeSetInsets(JNIEnv *env, jobject thiz,
                                                   jint system_left, jint system_top,
                                                   jint system_right, jint system_bottom,
                                                   jint ime_bottom,
                                                   jint cutout_left, jint cutout_top,
                                                   jint cutout_right, jint cutout_bottom)
{
    (void)env;
    (void)thiz;

    pthread_mutex_lock(&bridge_mutex);
    insets_system_left = system_left;
    insets_system_top = system_top;
    insets_system_right = system_right;
    insets_system_bottom = system_bottom;
    insets_ime_bottom = ime_bottom;
    insets_cutout_left = cutout_left;
    insets_cutout_top = cutout_top;
    insets_cutout_right = cutout_right;
    insets_cutout_bottom = cutout_bottom;
    insets_ready = 1;
    pthread_mutex_unlock(&bridge_mutex);
}

JNIEXPORT void JNICALL
Java_xyz_waozi_pass_MainActivity_nativeSetDeviceDensity(JNIEnv *env, jobject thiz, jfloat density)
{
    (void)env;
    (void)thiz;

    if(density <= 0.0f)
        return;
    pthread_mutex_lock(&bridge_mutex);
    device_density = density;
    pthread_mutex_unlock(&bridge_mutex);
    SetUIDeviceDensity(density);
}

JNIEXPORT void JNICALL
Java_xyz_waozi_pass_MainActivity_nativeTextInputCommit(JNIEnv *env, jobject thiz, jint codepoint)
{
    (void)env;
    (void)thiz;
    QueueTextInputCodepoint((int)codepoint);
}

JNIEXPORT void JNICALL
Java_xyz_waozi_pass_MainActivity_nativeTextInputBackspace(JNIEnv *env, jobject thiz)
{
    (void)env;
    (void)thiz;
    QueueTextInputBackspace();
}

JNIEXPORT void JNICALL
Java_xyz_waozi_pass_MainActivity_nativeTextInputEnter(JNIEnv *env, jobject thiz)
{
    (void)env;
    (void)thiz;
    QueueTextInputEnter();
}

#else /* !ANDROID_BUILD */

void android_bridge_init(void) {}
void android_bridge_apply_system_theme(void) {}
int android_bridge_left_reserved(void) { return 0; }
int android_bridge_top_reserved(void) { return 0; }
int android_bridge_right_reserved(void) { return 0; }
int android_bridge_bottom_reserved(void) { return 0; }
void android_bridge_set_soft_keyboard(int visible) { (void)visible; }
int android_bridge_biometric_available(void) { return 0; }
int android_bridge_biometric_setup_required(void) { return 0; }
int android_bridge_master_saved(void) { return 0; }
int android_bridge_master_biometric(void) { return 0; }
void android_bridge_save_master(const char *master, int require_biometric) { (void)master; (void)require_biometric; }
void android_bridge_unlock_master(void) {}
void android_bridge_clear_master(void) {}
int android_bridge_secure_status(void) { return 0; }
int android_bridge_take_secure_result(char *out, int out_size) { if(out != NULL && out_size > 0) out[0] = '\0'; return 0; }

#endif
