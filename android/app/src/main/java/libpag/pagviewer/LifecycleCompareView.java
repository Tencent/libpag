package libpag.pagviewer;

import android.content.Context;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;
import android.widget.FrameLayout;

/**
 * A test View that compares Fragment-based lifecycle (via addOnAttachStateChangeListener)
 * with View.onWindowVisibilityChanged to prove they fire at the same time and have the
 * same behavior for PAGView's screen-lock recovery purpose.
 *
 * To use: replace PAGView in the demo layout with this View, then trigger the scenarios
 * below and watch logcat for "LifecycleCompare" tags.
 */
public class LifecycleCompareView extends FrameLayout {

    private static final String TAG = "LifecycleCompare";

    private boolean isWindowVisible = false;
    private boolean isAttachedToWindow = false;

    public LifecycleCompareView(Context context) {
        super(context);
        init();
    }

    public LifecycleCompareView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    private void init() {
        Log.i(TAG, "=== init() ===");

        // Simulate the OLD Fragment-based lifecycle: add a listener that fires
        // when the View is attached to a window (analogous to Fragment.onResume).
        addOnAttachStateChangeListener(new OnAttachStateChangeListener() {
            @Override
            public void onViewAttachedToWindow(View v) {
                isAttachedToWindow = true;
                Log.i(TAG, "[OLD-way Fragment proxy] onViewAttachedToWindow → simulate onResume()");
                onLifecycleResume();
            }

            @Override
            public void onViewDetachedFromWindow(View v) {
                isAttachedToWindow = false;
                Log.i(TAG, "[OLD-way Fragment proxy] onViewDetachedFromWindow → simulate onPause()");
            }
        });
    }

    // This is the NEW way: View's own callback.
    @Override
    protected void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);
        boolean visible = (visibility == View.VISIBLE);
        Log.i(TAG, "[NEW-way View callback] onWindowVisibilityChanged("
                + (visible ? "VISIBLE" : "GONE/INVISIBLE") + ")");
        if (visible && isWindowVisible) {
            Log.i(TAG, "[NEW-way View callback] → screen-lock recovery: toggle visibility");
            // This is what the PAGView fix does.
            setVisibility(View.INVISIBLE);
            setVisibility(View.VISIBLE);
        }
        isWindowVisible = visible;
    }

    // Simulates what PAGView.onResume() did in the old code.
    private void onLifecycleResume() {
        Log.i(TAG, "[OLD-way Fragment proxy] → screen-lock recovery: toggle visibility");
        // Old PAGView.onResume() body:
        setVisibility(View.INVISIBLE);
        setVisibility(View.VISIBLE);
    }
}
