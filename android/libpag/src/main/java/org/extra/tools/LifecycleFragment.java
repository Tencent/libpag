package org.extra.tools;

import android.app.Fragment;
import java.util.Collections;
import java.util.Iterator;
import java.util.Set;
import java.util.WeakHashMap;

public class LifecycleFragment extends Fragment {
    private final Set<LifecycleListener> lifecycleListeners =
            Collections.newSetFromMap(new WeakHashMap<LifecycleListener, Boolean>());

    private final Object lock = new Object();
    public LifecycleFragment() {
    }

    public void addListener(LifecycleListener listener) {
        synchronized (lock) {
            lifecycleListeners.add(listener);
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        synchronized (lock) {
            Iterator<LifecycleListener> iterator = lifecycleListeners.iterator();
            while (iterator.hasNext()) {
                LifecycleListener lifecycleListener = iterator.next();
                if (lifecycleListener != null) {
                    lifecycleListener.onResume();
                }
            }
        }
    }
}
