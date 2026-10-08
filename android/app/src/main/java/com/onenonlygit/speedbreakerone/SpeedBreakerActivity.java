package com.onenonlygit.speedbreakerone;
import android.os.Bundle;
import android.view.WindowManager;
import org.libsdl.app.SDLActivity;
public final class SpeedBreakerActivity extends SDLActivity {
    @Override protected String[] getLibraries() { return new String[] {"SDL3", "main"}; }
    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        WindowManager.LayoutParams params = getWindow().getAttributes();
        params.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        getWindow().setAttributes(params);
    }
}
