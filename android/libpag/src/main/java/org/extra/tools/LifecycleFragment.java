package org.extra.tools;

import android.app.Fragment;

import java.util.Collections;
import java.util.Iterator;
import java.util.Set;
import java.util.WeakHashMap;

public class LifecycleFragment extends Fragment {
    private final Set<LifecycleListener> lifecycleListeners =
            Collections.newSetFromMap(new WeakHashMap<LifecycleListener, Boolean>());

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
