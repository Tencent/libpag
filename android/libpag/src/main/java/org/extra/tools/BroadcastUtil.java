package org.extra.tools;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

public class BroadcastUtil implements ScreenBroadcastReceiver.ScreenStateListener {
    private static List<WeakReference<ScreenBroadcastReceiver.ScreenStateListener>> mDataList = new ArrayList<>();
    private final Object mSync = new Object();
    private ScreenBroadcastReceiver receiver = null;

    private static class Factory {
        private final static BroadcastUtil INSTANCE = new BroadcastUtil();
    }

    public static BroadcastUtil getInstance() {
        return Factory.INSTANCE;
    }

    public void registerScreenBroadcast() {
        if (receiver != null) {
            return;
        }
        receiver = new ScreenBroadcastReceiver(this);
        receiver.register();
    }

    public void unregisterScreenBroadcast() {
        if (receiver != null) {
            receiver.unregister();
            receiver = null;
        }
    }

    public void registerScreenBroadcast(ScreenBroadcastReceiver.ScreenStateListener listener) {
        if (receiver == null) {
            return;
        }

        removeUnUse();
        if (listener == null) {
            return;
        }

        synchronized (mSync) {
            for (WeakReference<ScreenBroadcastReceiver.ScreenStateListener> weakReference : mDataList) {
                if (listener == weakReference.get()) {
                    return;
                }
            }
            WeakReference<ScreenBroadcastReceiver.ScreenStateListener> weakReference = new WeakReference<>(listener);
            mDataList.add(weakReference);
        }
    }

    public void unregisterScreenBroadcast(ScreenBroadcastReceiver.ScreenStateListener listener) {
        if (receiver == null) {
            return;
        }

        removeUnUse();
        if (listener == null) {
            return;
        }
        synchronized (mSync) {
            WeakReference<ScreenBroadcastReceiver.ScreenStateListener> remove = null;
            for (WeakReference<ScreenBroadcastReceiver.ScreenStateListener> weakReference : mDataList) {
                if (listener == weakReference.get()) {
                    remove = weakReference;
                }
            }
            if (remove != null) {
                mDataList.remove(remove);
            }
        }
    }

    private void removeUnUse() {
        synchronized (mSync) {
            List<WeakReference<ScreenBroadcastReceiver.ScreenStateListener>> removeList = new ArrayList<>();
            for (WeakReference<ScreenBroadcastReceiver.ScreenStateListener> weakReference : mDataList) {
                if (weakReference.get() == null) {
                    removeList.add(weakReference);
                }
            }
            for (WeakReference weakReference : removeList) {
                mDataList.remove(weakReference);
            }
        }
    }

    @Override
    public void onScreenOff() {
        removeUnUse();
        synchronized (mSync) {
            for (int pos = mDataList.size() - 1; pos >= 0; pos--) {
                ScreenBroadcastReceiver.ScreenStateListener listener = mDataList.get(pos).get();
                if (listener != null) {
                    listener.onScreenOff();
                }
            }
        }
    }

    @Override
    public void onScreenOn() {
        removeUnUse();
        synchronized (mSync) {
            for (int pos = mDataList.size() - 1; pos >= 0; pos--) {
                ScreenBroadcastReceiver.ScreenStateListener listener = mDataList.get(pos).get();
                if (listener != null) {
                    listener.onScreenOn();
                }
            }
        }
    }
}
