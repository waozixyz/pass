package xyz.waozi.pass;

import android.app.NativeActivity;
import android.app.KeyguardManager;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.graphics.Insets;
import android.graphics.Rect;
import android.hardware.biometrics.BiometricPrompt;
import android.hardware.fingerprint.FingerprintManager;
import android.os.Build;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.security.keystore.UserNotAuthenticatedException;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.view.DisplayCutout;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.view.WindowInsets;
import android.content.Context;

import java.nio.charset.StandardCharsets;
import java.security.KeyStore;
import java.util.concurrent.Executor;

import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

/**
 * NativeActivity glue for the kryon UI: routes soft-keyboard input and
 * window insets to the native side. Modeled on inbe's MainActivity.
 */
public class MainActivity extends NativeActivity {
    private static final String SECURE_PREFS = "pass_secure";
    private static final String KEY_ALIAS_LEGACY = "pass_master_key";
    private static final String KEY_ALIAS_PLAIN = "pass_master_key_plain";
    private static final String KEY_ALIAS_AUTH = "pass_master_key_auth";
    private static final String PREF_CIPHERTEXT = "master_ciphertext";
    private static final String PREF_IV = "master_iv";
    private static final String PREF_BIOMETRIC = "master_biometric";
    private static final String PREF_KEY_ALIAS = "master_key_alias";
    private static final int SECURE_IDLE = 0;
    private static final int SECURE_PENDING = 1;
    private static final int SECURE_OK = 2;
    private static final int SECURE_ERROR = 3;

    static {
        // Associate libmain with this class's loader so ART can resolve the
        // native methods below (NativeActivity loads it via the boot loader).
        System.loadLibrary("main");
    }

    private final Object secureLock = new Object();
    private int secureStatus = SECURE_IDLE;
    private String secureResult = "";
    private TextInputBridge textInputBridge;

    private native void nativeSetInsets(int left, int top, int right, int bottom,
        int ime, int cutoutLeft, int cutoutTop, int cutoutRight, int cutoutBottom);
    private native void nativeSetDeviceDensity(float density);
    private native void nativeTextInputCommit(int codepoint);
    private native void nativeTextInputBackspace();
    private native void nativeTextInputEnter();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        applySystemBars();
        setupInsetsListener();
        setupTextInputBridge();
    }

    public int[] systemThemeColors() {
        boolean dark = (getResources().getConfiguration().uiMode
                & Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES;
        int background = dark ? 0xFF141218 : 0xFFFFFBFE;
        int surface = dark ? 0xFF211F26 : 0xFFF7F2FA;
        int text = dark ? 0xFFE6E0E9 : 0xFF1D1B20;
        int accent = dark ? 0xFFD0BCFF : 0xFF6750A4;
        int control = dark ? 0xFFE6E0E9 : 0xFF1D1B20;
        int button = blend(accent, background, dark ? 65 : 80);
        int buttonHover = blend(accent, background, dark ? 45 : 60);

        return new int[] {
            dark ? 1 : 0,
            background,
            surface,
            text,
            accent,
            button,
            buttonHover,
            control,
            accent
        };
    }

    private void applySystemBars() {
        int[] colors = systemThemeColors();
        boolean dark = colors[0] != 0;
        getWindow().setStatusBarColor(colors[1]);
        getWindow().setNavigationBarColor(colors[1]);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            int flags = getWindow().getDecorView().getSystemUiVisibility();
            if (!dark) {
                flags |= View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    flags |= View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
                }
            } else {
                flags &= ~View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    flags &= ~View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
                }
            }
            getWindow().getDecorView().setSystemUiVisibility(flags);
        }
    }

    private static int blend(int from, int to, int percentTo) {
        int p = Math.max(0, Math.min(100, percentTo));
        int a = (((from >>> 24) & 0xff) * (100 - p) + ((to >>> 24) & 0xff) * p) / 100;
        int r = (((from >>> 16) & 0xff) * (100 - p) + ((to >>> 16) & 0xff) * p) / 100;
        int g = (((from >>> 8) & 0xff) * (100 - p) + ((to >>> 8) & 0xff) * p) / 100;
        int b = ((from & 0xff) * (100 - p) + (to & 0xff) * p) / 100;
        return (a << 24) | (r << 16) | (g << 8) | b;
    }

    public boolean hasStoredMasterPassword() {
        SharedPreferences prefs = getSharedPreferences(SECURE_PREFS, MODE_PRIVATE);
        return prefs.contains(PREF_CIPHERTEXT) && prefs.contains(PREF_IV);
    }

    public boolean isStoredMasterBiometric() {
        return getSharedPreferences(SECURE_PREFS, MODE_PRIVATE).getBoolean(PREF_BIOMETRIC, false);
    }

    @SuppressWarnings("deprecation")
    public boolean isBiometricAvailable() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return false;
        try {
            FingerprintManager fm = (FingerprintManager)getSystemService(Context.FINGERPRINT_SERVICE);
            if (fm != null && fm.isHardwareDetected() && fm.hasEnrolledFingerprints()) return true;
        } catch (SecurityException ignored) {
        }
        return false;
    }

    @SuppressWarnings("deprecation")
    public boolean isBiometricSetupRequired() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return false;
        try {
            FingerprintManager fm = (FingerprintManager)getSystemService(Context.FINGERPRINT_SERVICE);
            if (fm == null || !fm.isHardwareDetected()) return false;
            KeyguardManager km = (KeyguardManager)getSystemService(Context.KEYGUARD_SERVICE);
            return !fm.hasEnrolledFingerprints() || km == null || !km.isDeviceSecure();
        } catch (SecurityException ignored) {
            return false;
        }
    }

    private String biometricUnavailableMessage() {
        if (isBiometricSetupRequired()) {
            return "Set up Android screen lock and fingerprint first";
        }
        return "Biometric unlock is not available on this device";
    }

    public int secureMasterStatus() {
        synchronized (secureLock) {
            return secureStatus;
        }
    }

    public String takeSecureMasterResult() {
        synchronized (secureLock) {
            String value = secureResult == null ? "" : secureResult;
            secureResult = "";
            secureStatus = SECURE_IDLE;
            return value;
        }
    }

    public void saveMasterPassword(final String master, final boolean requireBiometric) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (master == null || master.length() == 0) {
                    setSecureError("Enter a master password first");
                    return;
                }
                if (requireBiometric && !isBiometricAvailable()) {
                    setSecureError(biometricUnavailableMessage());
                    return;
                }
                if (requireBiometric) {
                    authenticateThenSave(master);
                } else {
                    encryptAndStoreMaster(master, false);
                }
            }
        });
    }

    public void unlockMasterPassword() {
        if (!hasStoredMasterPassword()) {
            setSecureError("No saved master password");
            return;
        }
        if (isStoredMasterBiometric()) {
            authenticateThenDecrypt();
            return;
        }
        decryptStoredMaster();
    }

    public void clearMasterPassword() {
        getSharedPreferences(SECURE_PREFS, MODE_PRIVATE).edit().clear().apply();
        setSecureOk("Saved master password removed");
    }

    private void authenticateThenDecrypt() {
        promptForBiometric("Unlock master password",
                "Use your fingerprint",
                "pass will decrypt the saved master password after unlock",
                new Runnable() {
            @Override
            public void run() {
                decryptStoredMaster();
            }
        });
    }

    private void authenticateThenSave(final String master) {
        promptForBiometric("Save master password",
                "Use your fingerprint",
                "pass will require fingerprint unlock for the saved master password",
                new Runnable() {
            @Override
            public void run() {
                encryptAndStoreMaster(master, true);
            }
        });
    }

    private void promptForBiometric(String title, String subtitle, String description,
                                    final Runnable onSuccess) {
        if (!isBiometricAvailable()) {
            setSecureError(biometricUnavailableMessage());
            return;
        }
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            setSecureError("Biometric prompt requires Android 9 or newer");
            return;
        }
        synchronized (secureLock) {
            secureStatus = SECURE_PENDING;
            secureResult = "";
        }
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Executor executor = new Executor() {
                    @Override
                    public void execute(Runnable command) {
                        runOnUiThread(command);
                    }
                };
                BiometricPrompt prompt = new BiometricPrompt.Builder(MainActivity.this)
                    .setTitle(title)
                    .setSubtitle(subtitle)
                    .setDescription(description)
                    .setNegativeButton("Cancel", executor, (dialog, which) -> setSecureError("Canceled"))
                    .build();
                prompt.authenticate(new CancellationSignal(), executor,
                    new BiometricPrompt.AuthenticationCallback() {
                        @Override
                        public void onAuthenticationSucceeded(BiometricPrompt.AuthenticationResult result) {
                            onSuccess.run();
                        }

                        @Override
                        public void onAuthenticationError(int errorCode, CharSequence errString) {
                            setSecureError(errString == null ? "Unlock failed" : errString.toString());
                        }

                        @Override
                        public void onAuthenticationFailed() {
                            setSecureError("Biometric unlock failed");
                        }
                    });
            }
        });
    }

    private void encryptAndStoreMaster(String master, boolean requireBiometric) {
        try {
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.ENCRYPT_MODE, getOrCreateMasterKey(requireBiometric));
            byte[] encrypted = cipher.doFinal(master.getBytes(StandardCharsets.UTF_8));
            byte[] iv = cipher.getIV();
            if (iv == null || iv.length == 0) {
                setSecureError("Save failed: missing encryption IV");
                return;
            }

            SharedPreferences.Editor editor = getSharedPreferences(SECURE_PREFS, MODE_PRIVATE).edit();
            editor.putString(PREF_IV, b64(iv));
            editor.putString(PREF_CIPHERTEXT, b64(encrypted));
            editor.putBoolean(PREF_BIOMETRIC, requireBiometric);
            editor.putString(PREF_KEY_ALIAS, requireBiometric ? KEY_ALIAS_AUTH : KEY_ALIAS_PLAIN);
            editor.apply();
            setSecureOk("Master password saved");
        } catch (UserNotAuthenticatedException e) {
            setSecureError("Unlock with fingerprint first");
        } catch (Exception e) {
            setSecureError("Save failed: " + e.getMessage());
        }
    }

    private void decryptStoredMaster() {
        try {
            SharedPreferences prefs = getSharedPreferences(SECURE_PREFS, MODE_PRIVATE);
            String ivText = prefs.getString(PREF_IV, "");
            String encryptedText = prefs.getString(PREF_CIPHERTEXT, "");
            if (ivText.length() == 0 || encryptedText.length() == 0) {
                setSecureError("No saved master password");
                return;
            }
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.DECRYPT_MODE, getStoredMasterKey(), new GCMParameterSpec(128, unb64(ivText)));
            byte[] decrypted = cipher.doFinal(unb64(encryptedText));
            setSecureOk(new String(decrypted, StandardCharsets.UTF_8));
        } catch (UserNotAuthenticatedException e) {
            setSecureError("Unlock with fingerprint first");
        } catch (Exception e) {
            setSecureError("Unlock failed: " + e.getMessage());
        }
    }

    private SecretKey getStoredMasterKey() throws Exception {
        SharedPreferences prefs = getSharedPreferences(SECURE_PREFS, MODE_PRIVATE);
        String alias = prefs.getString(PREF_KEY_ALIAS, KEY_ALIAS_LEGACY);
        boolean requireAuth = KEY_ALIAS_AUTH.equals(alias);
        return getOrCreateMasterKey(alias, requireAuth);
    }

    private SecretKey getOrCreateMasterKey(boolean requireAuth) throws Exception {
        return getOrCreateMasterKey(requireAuth ? KEY_ALIAS_AUTH : KEY_ALIAS_PLAIN, requireAuth);
    }

    private SecretKey getOrCreateMasterKey(String alias, boolean requireAuth) throws Exception {
        KeyStore keyStore = KeyStore.getInstance("AndroidKeyStore");
        keyStore.load(null);
        if (keyStore.containsAlias(alias)) {
            return (SecretKey)keyStore.getKey(alias, null);
        }
        KeyGenerator generator = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore");
        KeyGenParameterSpec.Builder spec = new KeyGenParameterSpec.Builder(
                alias, KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
            .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
            .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
            .setRandomizedEncryptionRequired(true);
        if (requireAuth) {
            spec.setUserAuthenticationRequired(true);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                spec.setUserAuthenticationParameters(30,
                        KeyProperties.AUTH_BIOMETRIC_STRONG | KeyProperties.AUTH_DEVICE_CREDENTIAL);
            } else {
                spec.setUserAuthenticationValidityDurationSeconds(30);
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                spec.setInvalidatedByBiometricEnrollment(true);
            }
        }
        KeyGenParameterSpec builtSpec = spec.build();
        generator.init(builtSpec);
        return generator.generateKey();
    }

    private void setSecureOk(String value) {
        synchronized (secureLock) {
            secureStatus = SECURE_OK;
            secureResult = value == null ? "" : value;
        }
    }

    private void setSecureError(String value) {
        synchronized (secureLock) {
            secureStatus = SECURE_ERROR;
            secureResult = value == null ? "Secure storage failed" : value;
        }
    }

    private static String b64(byte[] data) {
        return android.util.Base64.encodeToString(data, android.util.Base64.NO_WRAP);
    }

    private static byte[] unb64(String text) {
        return android.util.Base64.decode(text, android.util.Base64.NO_WRAP);
    }

    private void setupTextInputBridge() {
        textInputBridge = new TextInputBridge(this, new TextInputBridge.Callbacks() {
            @Override
            public void commitText(int codepoint) {
                nativeTextInputCommit(codepoint);
            }

            @Override
            public void backspace() {
                nativeTextInputBackspace();
            }

            @Override
            public void enter() {
                nativeTextInputEnter();
            }
        });
        addContentView(textInputBridge.getView(), new ViewGroup.LayoutParams(1, 1));
    }

    public void setSoftKeyboardVisible(final boolean visible) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (textInputBridge != null) {
                    textInputBridge.setVisible(visible);
                }
            }
        });
    }

    private void setupInsetsListener() {
        final View decorView = getWindow().getDecorView();

        nativeSetDeviceDensity(getResources().getDisplayMetrics().density);

        decorView.setOnApplyWindowInsetsListener(new View.OnApplyWindowInsetsListener() {
            @Override
            public WindowInsets onApplyWindowInsets(View v, WindowInsets insets) {
                updateInsets(insets);
                return insets;
            }
        });

        // Keep the native UI updated when the soft keyboard changes the
        // visible frame on API levels where IME insets are not reported.
        decorView.getViewTreeObserver().addOnGlobalLayoutListener(
                new ViewTreeObserver.OnGlobalLayoutListener() {
            @Override
            public void onGlobalLayout() {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                    WindowInsets insets = decorView.getRootWindowInsets();
                    if (insets != null) {
                        updateInsets(insets);
                    }
                }
            }
        });

        decorView.post(new Runnable() {
            @Override
            public void run() {
                decorView.requestApplyInsets();
            }
        });
    }

    private void updateInsets(WindowInsets insets) {
        if (insets == null) return;

        nativeSetDeviceDensity(getResources().getDisplayMetrics().density);

        int systemLeft = 0;
        int systemTop = 0;
        int systemRight = 0;
        int systemBottom = 0;
        int imeBottom = 0;
        int cLeft = 0, cTop = 0, cRight = 0, cBottom = 0;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            Insets systemBars = insets.getInsetsIgnoringVisibility(
                    WindowInsets.Type.systemBars());
            Insets ime = insets.getInsets(WindowInsets.Type.ime());
            systemLeft = systemBars.left;
            systemTop = systemBars.top;
            systemRight = systemBars.right;
            systemBottom = systemBars.bottom;
            imeBottom = ime.bottom;
        } else {
            systemLeft = insets.getSystemWindowInsetLeft();
            systemTop = insets.getSystemWindowInsetTop();
            systemRight = insets.getSystemWindowInsetRight();
            systemBottom = insets.getSystemWindowInsetBottom();
            imeBottom = inferImeBottom(systemBottom);
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            DisplayCutout cutout = insets.getDisplayCutout();
            if (cutout != null) {
                cLeft = cutout.getSafeInsetLeft();
                cTop = cutout.getSafeInsetTop();
                cRight = cutout.getSafeInsetRight();
                cBottom = cutout.getSafeInsetBottom();
            }
        }

        nativeSetInsets(systemLeft, systemTop, systemRight, systemBottom,
                imeBottom, cLeft, cTop, cRight, cBottom);
    }

    private int inferImeBottom(int navBar) {
        View decorView = getWindow().getDecorView();
        Rect visible = new Rect();
        decorView.getWindowVisibleDisplayFrame(visible);
        int rootHeight = decorView.getRootView().getHeight();
        int hiddenBottom = rootHeight - visible.bottom;

        if (hiddenBottom <= navBar) return 0;
        return hiddenBottom;
    }
}
