package org.libpag;

import android.media.MediaCodec;
import android.media.MediaFormat;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.SystemClock;

import java.nio.ByteBuffer;
import java.util.concurrent.atomic.AtomicInteger;


public class GPUDecoder {

    private static final int SUCCESS = 0;
    private static final int TRY_AGAIN_LATER = -1;
    private static final int ERROR = -2;
    private static final int END_OF_STREAM = -3;

    private VideoSurface videoSurface = null;

    /**
     * Some devices will crash in some scenes when using MediaCodec, so exclude them.
     */
    private static boolean ForceSoftwareDecoder() {
        return Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP;
    }

    private MediaCodec decoder;

    // decoder.start();
    // decoder.flush();
    // HUAWEI Mate 40 Pro，在连续或者相近的时间执行上面代码会解码失败，
    // 报 `VIDEO-[pps_sps_check_tmp_id]:[5994]pps is null ppsid = 0 havn't decode`
    private boolean disableFlush = true;

    private static final int INIT_DECODER_TIMEOUT_MS = 2000;
    private static final int DECODER_THREAD_MAX_COUNT = 10;
    private static final AtomicInteger decoderThreadCount = new AtomicInteger();

    private static GPUDecoder Create(final VideoSurface videoSurface, final MediaFormat mediaFormat) {
        if (videoSurface == null || decoderThreadCount.get() >= DECODER_THREAD_MAX_COUNT) {
            return null;
        }
        videoSurface.retain();
        decoderThreadCount.getAndIncrement();
        HandlerThread initHandlerThread = new HandlerThread("libpag_GPUDecoder_init_decoder");
        try {
            initHandlerThread.start();
        } catch (Exception | Error e) {
            e.printStackTrace();
            decoderThreadCount.getAndDecrement();
            return null;
        }
        SynchronizeHandler initHandler = new SynchronizeHandler(initHandlerThread.getLooper());
        final MediaCodec[] initDecoder = {null};
        boolean res = initHandler.runSync(new SynchronizeHandler.TimeoutRunnable() {
            private MediaCodec decoder;
            private long startTime;

            @Override
            public void run() {
                startTime = SystemClock.uptimeMillis();
                try {
                    decoder = MediaCodec.createDecoderByType(mediaFormat.getString(MediaFormat.KEY_MIME));
                    decoder.configure(mediaFormat, videoSurface.getOutputSurface(), null, 0);
                    decoder.start();
                } catch (Exception e) {
                    if (decoder != null) {
                        decoder.release();
                        decoder = null;
                        videoSurface.release();
                    }
                }
            }

            @Override
            public void afterRun(boolean isTimeout) {
                if (isTimeout && decoder != null) {
                    long costTime = SystemClock.uptimeMillis() - startTime;
                    try {
                        decoder.stop();
                    } catch (Exception ignored) {
                    }
                    try {
                        decoder.release();
                    } catch (Exception ignored) {
                    }
                    decoder = null;
                    videoSurface.release();
                    String errorMessage = "init decoder timeout. cost: " + costTime + "ms";
                    new RuntimeException(errorMessage).printStackTrace();
                }
                if (!isTimeout) {
                    initDecoder[0] = decoder;
                }
                decoderThreadCount.getAndDecrement();
            }
        }, INIT_DECODER_TIMEOUT_MS);
        initHandlerThread.quitSafely();
        if (res) {
            GPUDecoder decoder = new GPUDecoder();
            decoder.videoSurface = videoSurface;
            decoder.decoder = initDecoder[0];
            return decoder;
        }
        return null;
    }

    private MediaCodec.BufferInfo bufferInfo = new MediaCodec.BufferInfo();
    private static final int TIMEOUT_US = 1000;

    private int dequeueOutputBuffer() {
        try {
            return decoder.dequeueOutputBuffer(bufferInfo, TIMEOUT_US);
        } catch (Exception e) {
            e.printStackTrace();
            return -1;
        }
    }

    private int dequeueInputBuffer() {
        try {
            return decoder.dequeueInputBuffer(TIMEOUT_US);
        } catch (Exception | Error e) {
            e.printStackTrace();
        }
        return -1;
    }

    private ByteBuffer getInputBuffer(int inputBufferIndex) {
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                return decoder.getInputBuffer(inputBufferIndex);
            }
            return decoder.getInputBuffers()[inputBufferIndex];
        } catch (Exception | Error e) {
            e.printStackTrace();
            return null;
        }
    }

    private int queueInputBuffer(int inputBufferIndex, int offset, int size, long presentationTimeUs, int flag) {
        try {
            decoder.queueInputBuffer(inputBufferIndex, offset, size, presentationTimeUs, flag);
            disableFlush = false;
            return SUCCESS;
        } catch (Exception | Error e) {
            e.printStackTrace();
            return ERROR;
        }
    }

    private int lastOutputBufferIndex = -1;

    private void releaseOutputBuffer() {
        if (lastOutputBufferIndex != -1) {
            releaseOutputBuffer(lastOutputBufferIndex, false);
            lastOutputBufferIndex = -1;
        }
    }

    private int releaseOutputBuffer(int outputBufferIndex, boolean render) {
        try {
            decoder.releaseOutputBuffer(outputBufferIndex, render);
            return SUCCESS;
        } catch (Exception | Error e) {
            e.printStackTrace();
            return ERROR;
        }
    }

    private int onSendBytes(ByteBuffer bytes, long frame) {
        int inputBufferIndex = dequeueInputBuffer();
        if (inputBufferIndex >= 0) {
            ByteBuffer inputBuffer = getInputBuffer(inputBufferIndex);
            if (inputBuffer == null) {
                return ERROR;
            }
            inputBuffer.clear();
            bytes.position(0);
            inputBuffer.put(bytes);
            return queueInputBuffer(inputBufferIndex, 0, bytes.limit(), frame, 0);
        }
        return TRY_AGAIN_LATER;
    }

    private int onEndOfStream() {
        int inputBufferIndex = dequeueInputBuffer();
        if (inputBufferIndex >= 0) {
            return queueInputBuffer(inputBufferIndex, 0, 0, 0,
                    MediaCodec.BUFFER_FLAG_END_OF_STREAM);
        }
        return TRY_AGAIN_LATER;
    }

    private int onDecodeFrame() {
        releaseOutputBuffer();
        try {
            int outputBufferIndex = dequeueOutputBuffer();
            if ((bufferInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                if (outputBufferIndex >= 0) {
                    lastOutputBufferIndex = outputBufferIndex;
                }
                return END_OF_STREAM;
            }
            if (outputBufferIndex >= 0) {
                lastOutputBufferIndex = outputBufferIndex;
            }
        } catch (Exception e) {
            e.printStackTrace();
            return ERROR;
        }
        return lastOutputBufferIndex != -1 ? SUCCESS : TRY_AGAIN_LATER;
    }

    private void onFlush() {
        if (disableFlush) {
            return;
        }
        try {
            decoder.flush();
            bufferInfo = new MediaCodec.BufferInfo();
            lastOutputBufferIndex = -1;
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private long presentationTime() {
        return bufferInfo.presentationTimeUs;
    }

    private boolean onRenderFrame() {
        if (lastOutputBufferIndex != -1) {
            int success = releaseOutputBuffer(lastOutputBufferIndex, true);
            lastOutputBufferIndex = -1;
            return success == SUCCESS;
        }
        return false;
    }

    private boolean released = false;

    private void onRelease() {
        if (released) {
            return;
        }
        released = true;
        releaseOutputBuffer();
        releaseDecoder();
    }

    private void releaseDecoder() {
        if (decoder == null) {
            return;
        }
        releaseAsync(new Runnable() {
            @Override
            public void run() {
                try {
                    decoder.stop();
                } catch (Exception e) {
                    e.printStackTrace();
                }

                try {
                    decoder.release();
                } catch (Exception e) {
                    e.printStackTrace();
                }
                decoder = null;
                videoSurface.release();
            }
        });
    }

    private void releaseAsync(final Runnable runnable) {
        if (runnable == null) {
            return;
        }
        decoderThreadCount.getAndIncrement();
        final HandlerThread releaseHandlerThread = new HandlerThread("libpag_GPUDecoder_release_decoder");
        try {
            releaseHandlerThread.start();
        } catch (Exception | Error e) {
            e.printStackTrace();
            runnable.run();
            return;
        }
        Handler releaseHandler = new Handler(releaseHandlerThread.getLooper());
        releaseHandler.post(new Runnable() {
            @Override
            public void run() {
                runnable.run();
                decoderThreadCount.getAndDecrement();
                new Handler(Looper.getMainLooper()).post(new Runnable() {
                    @Override
                    public void run() {
                        releaseHandlerThread.quitSafely();
                    }
                });
            }
        });
    }
}
