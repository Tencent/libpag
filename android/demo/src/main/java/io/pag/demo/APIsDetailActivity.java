package io.pag.demo;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Matrix;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.media.MediaMuxer;
import android.os.Bundle;
import android.os.Environment;
import android.support.v4.app.ActivityCompat;
import android.support.v7.app.AppCompatActivity;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.RelativeLayout;

import org.libpag.PAGComposition;
import org.libpag.PAGFile;
import org.libpag.PAGImage;
import org.libpag.PAGPlayer;
import org.libpag.PAGSurface;
import org.libpag.PAGText;
import org.libpag.PAGView;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;

public class APIsDetailActivity extends AppCompatActivity {

    private static final String TAG = "APIsDetailActivity";
    // video export
    private static final String MIME_TYPE = "video/avc";    // H.264 Advanced Video Coding
    private static final int FRAME_RATE = 30;
    private static final int IFRAME_INTERVAL = 10;          // 10 seconds between I-frames
    private static final boolean VERBOSE = true;
    private static final File OUTPUT_DIR = Environment.getExternalStorageDirectory();
    private static final int REQUEST_EXTERNAL_STORAGE = 1;
    private static String[] PERMISSIONS_STORAGE = {
            "android.permission.READ_EXTERNAL_STORAGE",
            "android.permission.WRITE_EXTERNAL_STORAGE"};
    private PAGFile pagFile;
    private Button exportButton;
    private MediaCodec mEncoder;
    private MediaMuxer mMuxer;
    private int mTrackIndex;
    private boolean mMuxerStarted;
    private MediaCodec.BufferInfo mBufferInfo;
    private int mBitRate = 8000000;
    private PAGPlayer pagPlayer;
    private PAGComposition pagComposition;

    private static void verifyStoragePermissions(Activity activity) {
        try {
            //检测是否有写的权限
            int permission = ActivityCompat.checkSelfPermission(activity,
                    "android.permission.WRITE_EXTERNAL_STORAGE");
            if (permission != PackageManager.PERMISSION_GRANTED) {
                // 没有写的权限，去申请写的权限，会弹出对话框
                ActivityCompat.requestPermissions(activity, PERMISSIONS_STORAGE, REQUEST_EXTERNAL_STORAGE);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.layout_detail);
        exportButton = (Button) findViewById(R.id.export);
        exportButton.setVisibility(View.INVISIBLE);
        initPAGView();
    }

    private void initPAGView() {
        RelativeLayout backgroundView = findViewById(R.id.background_view);
        final PAGView pagView = new PAGView(this);
        pagView.setLayoutParams(new RelativeLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        backgroundView.addView(pagView);
        Intent intent = getIntent();
        if (intent == null) {
            return;
        }
        PAGFile pagFile1 = null;
        int index = intent.getIntExtra("API_TYPE", 0);
        switch (index) {
            // Basic usage
            case 0:
                pagFile1 = PAGFile.Load(getAssets(), "replacement.pag");
                pagView.setComposition(pagFile1);
                exportButton.setVisibility(View.VISIBLE);
                break;
            // Replace text in pag file
            case 1:
                pagFile1 = PAGFile.Load(getAssets(), "test2.pag");
                testEditText(pagFile1, pagView);
                pagView.setComposition(pagFile1);
                break;
            // Replace image in pag file
            case 2:
                pagFile1 = PAGFile.Load(getAssets(), "replacement.pag");
                testReplaceImage(pagFile1, pagView);
                pagView.setComposition(pagFile1);
                break;
            // Use multiple PAGFiles in the same Surface
            case 3:
                WindowManager manager = this.getWindowManager();
                DisplayMetrics outMetrics = new DisplayMetrics();
                manager.getDefaultDisplay().getMetrics(outMetrics);
                int width = outMetrics.widthPixels;
                int height = outMetrics.heightPixels;
                pagComposition = PAGComposition.Make(width, height);
                pagFile1 = PAGFile.Load(getAssets(), "data-TimeStretch.pag");
                pagFile1.replaceImage(0, PAGImage.FromAssets(getAssets(), "test.png"));
                Matrix matrix = new Matrix();
                matrix.setTranslate(200, 200);
                matrix.preScale(0.3f, 0.3f);
                pagFile1.setMatrix(matrix);
                pagFile1.setDuration(7000000);
                pagFile1.setStartTime(3000000);
                pagComposition.addLayer(pagFile1);

                PAGFile file = PAGFile.Load(getAssets(), "data_video.pag");
                file.setDuration(10000000);
                Matrix matrix1 = new Matrix();
                matrix1.setTranslate((width - file.width()) / 2.0f, (height - file.height()) / 2.0f);
                matrix1.setScale(width * 1.0f / file.width(), height * 1.0f / file.height());
                file.setMatrix(matrix1);

                pagComposition.addLayerAt(file, 0);
                pagView.setComposition(pagComposition);
                break;
            default:
                break;
        }

        pagView.setRepeatCount(-1);
        pagView.play();
    }

    private PAGImage createPAGImage() {
        AssetManager assetManager = getAssets();
        InputStream stream = null;
        try {
            stream = assetManager.open("test.png");
        } catch (IOException e) {
            e.printStackTrace();
        }
        Bitmap bitmap = BitmapFactory.decodeStream(stream);
        if (bitmap == null) {
            return null;
        }

        return PAGImage.FromBitmap(bitmap);
    }

    /**
     * Test replace image.
     */
    void testReplaceImage(PAGFile pagFile, PAGView pagView) {
        if (pagFile == null || pagView == null || pagFile.numImages() <= 0) return;
        pagFile.replaceImage(0, createPAGImage());
    }

    /**
     * Test edit text.
     */
    void testEditText(PAGFile pagFile, PAGView pagView) {
        if (pagFile == null || pagView == null || pagFile.numTexts() <= 0) return;
        PAGText textData = pagFile.getTextData(0);
        textData.text = "replacement test";
        pagFile.replaceText(0, textData);
    }

    public void export(View view) {
        verifyStoragePermissions(this);
        pagExportToMP4();
    }

    // video export
    private void pagExportToMP4() {
        try {
            prepareEncoder();
            int totalFrames = (int) (pagFile.duration() * pagFile.frameRate() / 1000000);
            for (int i = 0; i < totalFrames; i++) {
                // Feed any pending encoder output into the muxer.
                drainEncoder(false);
                generateSurfaceFrame(i);
                if (VERBOSE) Log.d(TAG, "sending frame " + i + " to encoder");
            }
            drainEncoder(true);
        } finally {
            releaseEncoder();
        }
        Log.d(TAG, "encode finished!!! \n");
    }

    private void prepareEncoder() {
        pagFile = PAGFile.Load(getAssets(), "replacement.pag");
        mBufferInfo = new MediaCodec.BufferInfo();
        int width = pagFile.width();
        int height = pagFile.height();
        if (width % 2 == 1) {
            width--;
        }
        if (height % 2 == 1) {
            height--;
        }

        MediaFormat format = MediaFormat.createVideoFormat(MIME_TYPE, width, height);
        format.setInteger(MediaFormat.KEY_COLOR_FORMAT,
                MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);
        format.setInteger(MediaFormat.KEY_BIT_RATE, mBitRate);
        format.setInteger(MediaFormat.KEY_FRAME_RATE, FRAME_RATE);
        format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, IFRAME_INTERVAL);
        try {
            mEncoder = MediaCodec.createEncoderByType(MIME_TYPE);
        } catch (IOException e) {
            e.printStackTrace();
        }
        mEncoder.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);

        if (pagPlayer == null) {
            PAGSurface pagSurface = PAGSurface.FromSurface(mEncoder.createInputSurface());
            pagPlayer = new PAGPlayer();
            pagPlayer.setSurface(pagSurface);
            pagPlayer.setComposition(pagFile);
            pagPlayer.setProgress(0);
        }

        mEncoder.start();
        String outputPath = new File(OUTPUT_DIR,
                "test." + width + "x" + height + ".mp4").toString();
        Log.d(TAG, "video output file is " + outputPath);
        try {
            mMuxer = new MediaMuxer(outputPath, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
        } catch (IOException ioe) {
            throw new RuntimeException("MediaMuxer creation failed", ioe);
        }

        mTrackIndex = -1;
        mMuxerStarted = false;
    }

    /**
     * Releases encoder resources.  May be called after partial / failed initialization.
     */
    private void releaseEncoder() {
        if (mEncoder != null) {
            mEncoder.stop();
            mEncoder.release();
            mEncoder = null;
        }
        if (mMuxer != null) {
            mMuxer.stop();
            mMuxer = null;
        }
    }

    private void drainEncoder(boolean endOfStream) {
        final int TIMEOUT_USEC = (int) (10000 * 60 / FRAME_RATE);
        if (VERBOSE) Log.d(TAG, "drainEncoder(" + endOfStream + ")");

        if (endOfStream) {
            if (VERBOSE) Log.d(TAG, "sending EOS to encoder");
            mEncoder.signalEndOfInputStream();
        }

        ByteBuffer[] encoderOutputBuffers = mEncoder.getOutputBuffers();
        while (true) {
            int encoderStatus = mEncoder.dequeueOutputBuffer(mBufferInfo, TIMEOUT_USEC);
            if (encoderStatus == MediaCodec.INFO_TRY_AGAIN_LATER) {
                // no output available yet
                if (!endOfStream) {
                    break;      // out of while
                } else {
                    if (VERBOSE) Log.d(TAG, "no output available, spinning to await EOS");
                }
            } else if (encoderStatus == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
                // not expected for an encoder
                encoderOutputBuffers = mEncoder.getOutputBuffers();
            } else if (encoderStatus == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                // should happen before receiving buffers, and should only happen once
                if (mMuxerStarted) {
                    throw new RuntimeException("format changed twice");
                }
                MediaFormat newFormat = mEncoder.getOutputFormat();
                Log.d(TAG, "encoder output format changed: " + newFormat);

                // now that we have the Magic Goodies, start the muxer
                mTrackIndex = mMuxer.addTrack(newFormat);
                mMuxer.start();
                mMuxerStarted = true;
            } else if (encoderStatus < 0) {
                Log.w(TAG, "unexpected result from encoder.dequeueOutputBuffer: " +
                        encoderStatus);
                // let's ignore it
            } else {
                ByteBuffer encodedData = encoderOutputBuffers[encoderStatus];
                if (encodedData == null) {
                    throw new RuntimeException("encoderOutputBuffer " + encoderStatus +
                            " was null");
                }

                if ((mBufferInfo.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0) {
                    // The codec config data was pulled out and fed to the muxer when we got
                    // the INFO_OUTPUT_FORMAT_CHANGED status.  Ignore it.
                    if (VERBOSE) Log.d(TAG, "ignoring BUFFER_FLAG_CODEC_CONFIG");
                    mBufferInfo.size = 0;
                }

                if (mBufferInfo.size != 0) {
                    if (!mMuxerStarted) {
                        throw new RuntimeException("muxer hasn't started");
                    }
                    // adjust the ByteBuffer values to match BufferInfo (not needed?)
                    encodedData.position(mBufferInfo.offset);
                    encodedData.limit(mBufferInfo.offset + mBufferInfo.size);

                    mMuxer.writeSampleData(mTrackIndex, encodedData, mBufferInfo);
                    if (VERBOSE) Log.d(TAG, "sent " + mBufferInfo.size + " bytes to muxer");
                }

                mEncoder.releaseOutputBuffer(encoderStatus, false);

                if ((mBufferInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                    if (!endOfStream) {
                        Log.w(TAG, "reached end of stream unexpectedly");
                    } else {
                        if (VERBOSE) Log.d(TAG, "end of stream reached");
                    }
                    break;      // out of while
                }
            }
        }
    }

    private void generateSurfaceFrame(int frameIndex) {
        int totalFrames = (int) (pagFile.duration() * pagFile.frameRate() / 1000000);
        float progress = frameIndex % totalFrames * 1.0f / totalFrames;
        pagPlayer.setProgress(progress);
        pagPlayer.flush();
    }
}
