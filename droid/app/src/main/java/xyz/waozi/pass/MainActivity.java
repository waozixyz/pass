package xyz.waozi.pass;

import android.app.NativeActivity;
import android.content.res.Configuration;
import android.graphics.Insets;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.view.DisplayCutout;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.view.WindowInsets;

import com.kryonlabs.kryon.SecureStore;

/**
 * NativeActivity glue for the kryon UI: routes soft-keyboard input and
 * window insets to the native side. Modeled on inbe's MainActivity.
 */
public class MainActivity extends NativeActivity {
    private static final String SECURE_PREFS = "pass_secure";

    static {
        // Associate libmain with this class's loader so ART can resolve the
        // native methods below (NativeActivity loads it via the boot loader).
        System.loadLibrary("main");
    }

    private final Object secureLock = new Object();
    private int secureStatus = SecureStore.STATUS_IDLE;
    private String secureResult = "";
    private SecureStore secureStore;
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
        secureStore = new SecureStore(this, SECURE_PREFS, "pass_master_key");
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
        return secureStore != null && secureStore.hasStoredSecret();
    }

    public boolean isStoredMasterBiometric() {
        return hasStoredMasterPassword();
    }

    public boolean isBiometricAvailable() {
        return secureStore != null && secureStore.isBiometricAvailable();
    }

    public boolean isBiometricSetupRequired() {
        return secureStore != null && secureStore.isBiometricSetupRequired();
    }

    private String biometricUnavailableMessage() {
        if (isBiometricSetupRequired()) {
            return "Set up Android screen lock and fingerprint first";
        }
        return "Biometric unlock is not available on this device";
    }

    public int secureMasterStatus() {
        if (secureStore != null)
            return secureStore.status();
        synchronized (secureLock) {
            return secureStatus;
        }
    }

    public String takeSecureMasterResult() {
        if (secureStore != null)
            return secureStore.takeResult();
        synchronized (secureLock) {
            String value = secureResult == null ? "" : secureResult;
            secureResult = "";
            secureStatus = SecureStore.STATUS_IDLE;
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
                if (!isBiometricAvailable()) {
                    setSecureError(biometricUnavailableMessage());
                    return;
                }
                if (secureStore != null)
                    secureStore.saveSecret(master, "master password");
            }
        });
    }

    public void unlockMasterPassword() {
        if (secureStore == null || !hasStoredMasterPassword()) {
            setSecureError("No saved master password");
            return;
        }
        secureStore.unlockSecret("master password");
    }

    public void clearMasterPassword() {
        if (secureStore != null) {
            secureStore.clearSecret();
        } else {
            setSecureOk("Saved master password removed");
        }
    }

    private void setSecureOk(String value) {
        synchronized (secureLock) {
            secureStatus = SecureStore.STATUS_OK;
            secureResult = value == null ? "" : value;
        }
    }

    private void setSecureError(String value) {
        synchronized (secureLock) {
            secureStatus = SecureStore.STATUS_ERROR;
            secureResult = value == null ? "Secure storage failed" : value;
        }
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
