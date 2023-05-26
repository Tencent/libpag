package org.extra.tools;

import android.app.Fragment;

import java.util.Collections;
import java.util.Iterator;
import java.util.Set;
import java.util.WeakHashMap;
import java.util.concurrent.CopyOnWriteArraySet;

public class LifecycleFragment extends Fragment {
    private CopyOnWriteArraySet<LifecycleListener> lifecycleListeners = new CopyOnWriteArraySet<>();

    public LifecycleFragment() {
    }

    public void addListener(LifecycleListener listener) {
        lifecycleListeners.add(listener);
    }

    @Override
    public void onResume() {
        super.onResume();
        Iterator<LifecycleListener> iterator = lifecycleListeners.iterator();
        while (iterator.hasNext()) {
            LifecycleListener lifecycleListener = iterator.next();
            if (lifecycleListener != null) {
                lifecycleListener.onResume();
            }
        }
    }
}
