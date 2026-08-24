package xyz.waozi.pass;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class MainActivityImeTest {
    @Rule
    public ActivityScenarioRule<TextInputBridgeHarnessActivity> activityRule =
        new ActivityScenarioRule<>(TextInputBridgeHarnessActivity.class);

    @Test
    public void keyboardBridgeCanHideAndReopen() {
        activityRule.getScenario().onActivity(activity -> {
            int startShows = activity.getShowRequests();
            int startHides = activity.getHideRequests();

            activity.setTextInputVisible(true);
            assertTrue(activity.isTextInputFocused());

            activity.setTextInputVisible(false);
            activity.setTextInputVisible(true);

            assertTrue(activity.isTextInputFocused());
            assertTrue(activity.getShowRequests() >= startShows + 2);
            assertTrue(activity.getHideRequests() >= startHides + 1);
        });
    }

    @Test
    public void inputConnectionForwardsTextEditingOnce() {
        activityRule.getScenario().onActivity(activity -> {
            activity.setTextInputVisible(true);
            activity.takeCommittedText();
            activity.takeBackspaceCount();
            activity.takeEnterCount();

            EditorInfo info = new EditorInfo();
            InputConnection connection = activity.createInputConnection(info);
            assertNotNull(connection);

            connection.commitText("site", 1);
            connection.deleteSurroundingText(1, 0);
            connection.performEditorAction(EditorInfo.IME_ACTION_DONE);

            assertEquals("site", activity.takeCommittedText());
            assertEquals(1, activity.takeBackspaceCount());
            assertEquals(1, activity.takeEnterCount());
        });
    }
}
