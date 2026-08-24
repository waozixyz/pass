package xyz.waozi.pass;

import android.app.Activity;
import android.os.Bundle;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

public final class TextInputBridgeHarnessActivity extends Activity {
    private TextInputBridge bridge;
    private StringBuilder committedText = new StringBuilder();
    private int backspaceCount = 0;
    private int enterCount = 0;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        bridge = new TextInputBridge(this, new TextInputBridge.Callbacks() {
            @Override
            public void commitText(int codepoint) {
                committedText.appendCodePoint(codepoint);
            }

            @Override
            public void backspace() {
                backspaceCount++;
            }

            @Override
            public void enter() {
                enterCount++;
            }
        });
        addContentView(bridge.getView(), new ViewGroup.LayoutParams(1, 1));
    }

    void setTextInputVisible(boolean visible) {
        bridge.setVisible(visible);
    }

    int getShowRequests() {
        return bridge.showRequestsForTest();
    }

    int getHideRequests() {
        return bridge.hideRequestsForTest();
    }

    boolean isTextInputFocused() {
        return bridge.hasFocusForTest();
    }

    InputConnection createInputConnection(EditorInfo info) {
        return bridge.createInputConnectionForTest(info);
    }

    String takeCommittedText() {
        String value = committedText.toString();
        committedText.setLength(0);
        return value;
    }

    int takeBackspaceCount() {
        int value = backspaceCount;
        backspaceCount = 0;
        return value;
    }

    int takeEnterCount() {
        int value = enterCount;
        enterCount = 0;
        return value;
    }
}
