package org.extra.tools;

import android.app.Application;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

public class ScreenBroadcastReceiver extends BroadcastReceiver {
    private ScreenStateListener listener;

    ScreenBroadcastReceiver(ScreenStateListener listener) {
        this.listener = listener;
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        String action = intent.getAction();
        if (Intent.ACTION_SCREEN_ON.equals(action)) {
            listener.onScreenOn();
        } else if (Intent.ACTION_SCREEN_OFF.equals(action)) {
            listener.onScreenOff();
        }
    }

    public void unregister() {
        try {
            Context context = getApplicationContext();
            if (context != null) {
                context.unregisterReceiver(this);
            }
        } catch (Exception e) {
            //
        }
    }

    public void register() {
        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_SCREEN_ON);
        filter.addAction(Intent.ACTION_SCREEN_OFF);
        try {
            Context context = getApplicationContext();
            if (context != null) {
                context.registerReceiver(this, filter);
            }
        } catch (Exception e) {
        }

    }

    public interface ScreenStateListener {
        void onScreenOn();

        void onScreenOff();
    }

    private Context getApplicationContext() {
        Context context = null;
        try {
            Application app = (Application) Class.forName("android.app.ActivityThread")
                    .getMethod("currentApplication").invoke(null, (Object[]) null);
            context = app.getApplicationContext();
        } catch (Exception e) {
        }

        return context;
    }
}