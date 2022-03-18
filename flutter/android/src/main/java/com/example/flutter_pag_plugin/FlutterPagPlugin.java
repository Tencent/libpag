package com.example.flutter_pag_plugin;

import android.content.Context;
import android.graphics.SurfaceTexture;
import android.os.Handler;
import android.os.Looper;
import android.view.Surface;

import androidx.annotation.NonNull;

import org.libpag.PAGFile;
import org.libpag.PAGLayer;
import org.libpag.PAGSurface;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

import io.flutter.embedding.engine.plugins.FlutterPlugin;
import io.flutter.plugin.common.MethodCall;
import io.flutter.plugin.common.MethodChannel;
import io.flutter.plugin.common.MethodChannel.MethodCallHandler;
import io.flutter.plugin.common.MethodChannel.Result;
import io.flutter.plugin.common.PluginRegistry;
import io.flutter.view.FlutterNativeView;
import io.flutter.view.TextureRegistry;
import kotlin.Unit;
import kotlin.jvm.functions.Function1;

/**
 * FlutterPagPlugin
 */
public class FlutterPagPlugin implements FlutterPlugin, MethodCallHandler {
    /// The MethodChannel that will the communication between Flutter and native Android
    ///
    /// This local reference serves to register the plugin with the Flutter Engine and unregister it
    /// when the Flutter Engine is detached from the Activity
    private MethodChannel channel;
    TextureRegistry textureRegistry;
    Context context;
    io.flutter.plugin.common.PluginRegistry.Registrar registrar;
    FlutterPlugin.FlutterAssets flutterAssets;
    private Handler handler = new Handler(Looper.getMainLooper());

    HashMap<String, FlutterPagPlayer> layerMap = new HashMap<String, FlutterPagPlayer>();

    public FlutterPagPlugin() {
    }

    public FlutterPagPlugin(io.flutter.plugin.common.PluginRegistry.Registrar registrar) {
        this.registrar = registrar;
        textureRegistry = registrar.textures();
        context = registrar.context();
        DataLoadHelper.INSTANCE.initDiskCache(context, DataLoadHelper.INSTANCE.DEFAULT_DIS_SIZE);
    }

    @Override
    public void onAttachedToEngine(@NonNull FlutterPluginBinding binding) {
        flutterAssets = binding.getFlutterAssets();
        channel = new MethodChannel(binding.getBinaryMessenger(), "flutter_pag_plugin");
        channel.setMethodCallHandler(this);
        context = binding.getApplicationContext();
        textureRegistry = binding.getTextureRegistry();
        DataLoadHelper.INSTANCE.initDiskCache(context, DataLoadHelper.INSTANCE.DEFAULT_DIS_SIZE);
    }

    public static void registerWith(io.flutter.plugin.common.PluginRegistry.Registrar registrar) {
        final FlutterPagPlugin plugin = new FlutterPagPlugin(registrar);
        registrar.addViewDestroyListener(new PluginRegistry.ViewDestroyListener() {
            @Override
            public boolean onViewDestroy(FlutterNativeView flutterNativeView) {
                plugin.onDestroy();
                return false; // We are not interested in assuming ownership of the NativeView.
            }
        });
    }

    @Override
    public void onMethodCall(@NonNull MethodCall call, @NonNull Result result) {
        switch (call.method) {
            case "initPag":
                initPag(call, result);
                break;
            case "start":
                start(call);
                break;
            case "stop":
                stop(call);
                break;
            case "pause":
                pause(call);
                break;
            case "setProgress":
                setProgress(call);
                break;
            case "release":
                release(call);
                break;
            case "getPlatformVersion":
                result.success("Android " + android.os.Build.VERSION.RELEASE);
                break;
            case "getLayersUnderPoint":
                result.success(getLayersUnderPoint(call));
                break;
            default:
                result.notImplemented();
                break;
        }
    }

    private void initPag(final MethodCall call, final Result result) {
        String assetName = call.argument("assetName");
        String url = call.argument("url");

        if (assetName != null) {
            String assetKey = "";
            if (registrar != null) {
                assetKey = registrar.lookupKeyForAsset(assetName);
            } else if (flutterAssets != null) {
                assetKey = flutterAssets.getAssetFilePathByName(assetName);
            }

            if (assetKey == null) {
                result.error("-1100", "asset资源加载错误", null);
                return;
            }

            PAGFile composition = PAGFile.Load(context.getAssets(), assetKey);
            initPagPlayerAndCallback(composition, call, result);
        } else if (url != null) {
            DataLoadHelper.INSTANCE.loadPag(url, new Function1<byte[], Unit>() {
                @Override
                public Unit invoke(final byte[] bytes) {
                    handler.post(new Runnable() {
                        @Override
                        public void run() {
                            if (bytes == null) {
                                result.error("-1100", "url资源加载错误", null);
                                return;
                            }

                            initPagPlayerAndCallback(PAGFile.Load(bytes), call, result);
                        }
                    });

                    return null;
                }
            });
        } else {
            result.error("-1100", "未添加资源", null);
        }
    }

    private void initPagPlayerAndCallback(PAGFile composition, MethodCall call, Result result) {
        final int repeatCount = call.argument("repeatCount");
        final double initProgress = call.argument("initProgress");
        final boolean autoPlay = call.argument("autoPlay");

        final FlutterPagPlayer pagPlayer = new FlutterPagPlayer();
        final TextureRegistry.SurfaceTextureEntry entry = textureRegistry.createSurfaceTexture();

        pagPlayer.init(composition, repeatCount, initProgress);
        SurfaceTexture surfaceTexture = entry.surfaceTexture();
        surfaceTexture.setDefaultBufferSize(composition.width(), composition.height());

        final Surface surface = new Surface(surfaceTexture);
        final PAGSurface pagSurface = PAGSurface.FromSurface(surface);
        pagPlayer.setSurface(pagSurface);
        pagPlayer.setReleaseListener(new FlutterPagPlayer.ReleaseListener() {
            @Override
            public void onRelease() {
                entry.release();
                surface.release();
                pagSurface.release();
            }
        });

        layerMap.put(String.valueOf(entry.id()), pagPlayer);
        HashMap<String, Object> callback = new HashMap<String, Object>();
        callback.put("textureId", entry.id());
        callback.put("width", (double) composition.width());
        callback.put("height", (double) composition.height());

        handler.post(new Runnable() {
            @Override
            public void run() {
                pagPlayer.flush();
                if (autoPlay) {
                    pagPlayer.start();
                }
            }
        });
        result.success(callback);
    }

    void start(MethodCall call) {
        FlutterPagPlayer flutterPagPlayer = getFlutterPagPlayer(call);
        if (flutterPagPlayer != null) {
            flutterPagPlayer.start();
        }
    }

    void stop(MethodCall call) {
        FlutterPagPlayer flutterPagPlayer = getFlutterPagPlayer(call);
        if (flutterPagPlayer != null) {
            flutterPagPlayer.stop();
        }
    }

    void pause(MethodCall call) {
        FlutterPagPlayer flutterPagPlayer = getFlutterPagPlayer(call);
        if (flutterPagPlayer != null) {
            flutterPagPlayer.pause();
        }
    }

    void setProgress(MethodCall call) {
        double progress = call.argument("progress");
        FlutterPagPlayer flutterPagPlayer = getFlutterPagPlayer(call);
        if (flutterPagPlayer != null) {
            flutterPagPlayer.setProgressValue(progress);
        }
    }

    void release(MethodCall call) {
        FlutterPagPlayer flutterPagPlayer = layerMap.remove(getTextureId(call));
        if (flutterPagPlayer != null) {
            flutterPagPlayer.stop();
            flutterPagPlayer.release();
        }
    }

    List<String> getLayersUnderPoint(MethodCall call) {
        FlutterPagPlayer flutterPagPlayer = getFlutterPagPlayer(call);

        List<String> layerNames = new ArrayList();
        PAGLayer[] layers = null;
        if (flutterPagPlayer != null) {
            layers = flutterPagPlayer.getLayersUnderPoint(
                    ((Double) call.argument("x")).floatValue(), ((Double) call.argument("y")).floatValue());
        }

        if (layers != null) {
            for (PAGLayer layer : layers) {
                layerNames.add(layer.layerName());
            }
        }

        return layerNames;
    }

    FlutterPagPlayer getFlutterPagPlayer(MethodCall call) {
        return layerMap.get(getTextureId(call));
    }

    String getTextureId(MethodCall call) {
        return String.valueOf(call.argument("textureId"));
    }

    //插件销毁
    public void onDestroy() {
        for (FlutterPagPlayer pagPlayer : layerMap.values()) {
            pagPlayer.release();
        }
        layerMap.clear();
        channel.setMethodCallHandler(null);
    }

    @Override
    public void onDetachedFromEngine(@NonNull FlutterPluginBinding binding) {
        channel.setMethodCallHandler(null);
    }
}
