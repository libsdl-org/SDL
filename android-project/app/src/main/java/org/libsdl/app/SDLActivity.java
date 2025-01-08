package org.libsdl.app;

import android.app.Activity;
import android.content.Intent;
import android.content.res.Configuration;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.View;

public abstract class SDLActivity extends Activity implements View.OnSystemUiVisibilityChangeListener, SDLComponentReceiver {
    private SDLActivityComponent component;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        component = new SDLActivityComponent(this);
        component.onCreate();
    }

    @Override
    protected void onPause() {
        super.onPause();
        component.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        component.onResume();
    }

    @Override
    protected void onStop() {
        super.onStop();
        component.onStop();
    }

    @Override
    protected void onStart() {
        super.onStart();
        component.onStart();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        component.onWindowFocusChanged(hasFocus);
    }

    @Override
    public void onTrimMemory(int level) {
        super.onTrimMemory(level);
        component.onTrimMemory(level);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        component.onConfigurationChanged(newConfig);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        component.onDestroy();
    }

    @Override
    public void onBackPressed() {
        if (component.onBackPressed()) {
            super.onBackPressed();
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        component.onActivityResult(requestCode, resultCode, data);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (component.dispatchKeyEvent(event)) {
            return super.dispatchKeyEvent(event);
        }
        return false;
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        component.onRequestPermissionsResult(requestCode, permissions, grantResults);
    }

    @Override
    public void superOnBackPressed() {
        super.onBackPressed();
    }
}
