package org.extra.tools;

import android.app.Fragment;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
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
        List<LifecycleListener> list = new ArrayList<>(lifecycleListeners.size());
        for (LifecycleListener item : lifecycleListeners) {
            if (item != null) {
                list.add(item);
            }
        }
        for (LifecycleListener lifecycleListener : list) {
            if (lifecycleListener != null) {
                lifecycleListener.onResume();
            }
        }
    }
}
