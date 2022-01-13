package org.libpag;

import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;

class SynchronizeHandler extends Handler {
    public SynchronizeHandler(Looper looper) {
        super(looper);
    }

    /**
     * copy from hidden method {@link Handler#runWithScissors}
     * @param r The Runnable that will be executed synchronously.
     * @param timeout The timeout in milliseconds, or 0 to wait indefinitely.
     */
    public final boolean runSync(final TimeoutRunnable r, long timeout) {
        if (r == null) {
            throw new IllegalArgumentException("runnable must not be null");
        }
        if (timeout < 0) {
            throw new IllegalArgumentException("timeout must be non-negative");
        }

        if (Looper.myLooper() == getLooper()) {
            r.run();
            return true;
        }

        BlockingRunnable br = new BlockingRunnable(r);
        return br.postAndWait(this, timeout);
    }

    interface TimeoutRunnable extends Runnable {
        void afterRun(boolean isTimeout);
    }

    private static final class BlockingRunnable implements Runnable {
        private final TimeoutRunnable mTask;
        private boolean mDone;
        private boolean isTimeout = false;

        public BlockingRunnable(TimeoutRunnable task) {
            mTask = task;
        }

        @Override
        public void run() {
            try {
                mTask.run();
            } finally {
                synchronized (this) {
                    mDone = true;
                    notifyAll();
                    mTask.afterRun(isTimeout);
                }
            }
        }

        public boolean postAndWait(Handler handler, long timeout) {
            if (!handler.post(this)) {
                return false;
            }

            synchronized (this) {
                if (timeout > 0) {
                    final long expirationTime = SystemClock.uptimeMillis() + timeout;
                    while (!mDone) {
                        long delay = expirationTime - SystemClock.uptimeMillis();
                        if (delay <= 0) {
                            isTimeout = true;
                            return false;
                        }
                        try {
                            wait(delay);
                        } catch (InterruptedException ignored) {
                        }
                    }
                } else {
                    while (!mDone) {
                        try {
                            wait();
                        } catch (InterruptedException ignored) {
                        }
                    }
                }
            }
            return true;
        }
    }
}
