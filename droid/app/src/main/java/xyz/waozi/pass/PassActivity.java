package xyz.waozi.pass;

import android.os.Bundle;
import android.view.WindowManager;

import com.kryonlabs.kryon.KryonActivity;

public final class PassActivity extends KryonActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setSoftInputMode(
                WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING);
    }
}
