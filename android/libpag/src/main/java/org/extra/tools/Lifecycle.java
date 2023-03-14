package org.extra.tools;

import android.app.Activity;
import android.app.FragmentManager;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.Log;
import android.view.View;

import org.libpag.PAGView;

import java.util.HashMap;
import java.util.Map;

public class Lifecycle implements Handler.Callback {
    private static final String FRAGMENT_TAG = "io.pag.manager";
    private static final String TAG = "Lifecycle";

    private static final int ID_REMOVE_FRAGMENT_MANAGER = 1;
    private static final Lifecycle lifecycle = new Lifecycle();
    private final Handler handler;
    private final Map<FragmentManager, LifecycleFragment> pendingRequestManagerFragments =
            new HashMap<>();

    private Lifecycle() {
        handler = new Handler(Looper.getMainLooper(), this);
    }

    public static Lifecycle getInstance() {
        return lifecycle;
    }

    public void addListener(final View pagView) {
        if (pagView.getContext() instanceof Activity && pagView instanceof LifecycleListener) {
            Activity activity = (Activity) pagView.getContext();
            if (activity.isDestroyed()) {
                return;
            }
            FragmentManager fm = activity.getFragmentManager();
            LifecycleFragment current = pendingRequestManagerFragments.get(fm);
            if (current == null) {
                current = (LifecycleFragment) fm.findFragmentByTag(FRAGMENT_TAG);
                if (current == null) {
                    current = new LifecycleFragment();
                    pendingRequestManagerFragments.put(fm, current);
                    fm.beginTransaction().add(current, FRAGMENT_TAG).commitAllowingStateLoss();
                    handler.obtainMessage(ID_REMOVE_FRAGMENT_MANAGER, fm).sendToTarget();
                }
            }
            current.addListener((LifecycleListener) pagView);
        }
    }

    @Override
    public boolean handleMessage(Message message) {
        boolean handled = true;
        if (message.what == ID_REMOVE_FRAGMENT_MANAGER) {
            FragmentManager fm = (FragmentManager) message.obj;
            LifecycleFragment current = (LifecycleFragment) fm.findFragmentByTag(FRAGMENT_TAG);
            if (fm.isDestroyed()) {
                Log.w(TAG, "Parent was destroyed before our Fragment could be added.");
            } else if (current != pendingRequestManagerFragments.get(fm)) {
                Log.w(TAG, "adding Fragment failed.");
            }
            pendingRequestManagerFragments.remove(fm);
        } else {
            handled = false;
        }
        return handled;
    }
}
