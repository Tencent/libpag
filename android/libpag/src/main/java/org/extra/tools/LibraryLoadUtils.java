package org.extra.tools;

import android.app.Application;
import android.content.Context;
import android.text.TextUtils;
import android.util.Log;

import org.extra.relinker.ReLinker;

import java.io.File;

/**
 * Copyright © 2019 tencent. All rights reserved.
 * <p>
 * File: LibraryLoadUtils.java <p>
 * Project: android <p>
 * Package: org.libpag.utils <p>
 * Description: Library loading auxiliary classes
 * <p>
 * author: hamlingong <p>
 * date: 2019/1/2 11:36 AM <p>
 * version: V1.0 <p>
 */
public class LibraryLoadUtils {
    public static final String TAG = LibraryLoadUtils.class.getSimpleName();
    private static Context appContext;

    /**
     * load Library
     *
     * @param libName
     */
    public static void loadLibrary(String libName) {
        try {
            Application app = (Application) Class.forName("android.app.ActivityThread")
                    .getMethod("currentApplication").invoke(null, (Object[]) null);
            appContext = app.getApplicationContext();
        } catch (Exception e) {
        }

        loadLibrary(appContext, libName);
    }

    public static Context getAppContext() {
        return appContext;
    }

    /**
     * Use System.loadLibrary, System.load, Relinker API load lib.
     *
     * @param context
     * @param libName
     */
    private static void loadLibrary(Context context, String libName) {
        if (load(libName)) {
            return;
        }
        if (load(context, libName)) {
            return;
        }

        relinker(context, libName);
    }

    /**
     * use System.loadLibrary API load so file
     *
     * @param libName
     * @return
     */
    private static boolean load(String libName) {
        if (TextUtils.isEmpty(libName)) {
            return false;
        }
        boolean result = true;
        try {
            System.loadLibrary(libName);
        } catch (Throwable t) {
            result = false;
            Log.i(TAG, "loadLibrary " + libName + " fail! Error: " + t.getMessage());
        }
        return result;
    }

    /**
     * use System.load API load so file
     *
     * @param context
     * @param libName
     * @return
     */
    private static boolean load(Context context, String libName) {
        if (context == null || TextUtils.isEmpty(libName)) {
            return false;
        }
        boolean result = true;
        try {
            String libDir = context.getApplicationInfo().dataDir + "/lib";
            String libPath = libDir + File.separator + "lib" + libName + ".so";
            System.load(libPath);
        } catch (Throwable t) {
            result = false;
            Log.i(TAG, "load " + " fail! Error: " + t.getMessage());
        }
        return result;
    }

    /**
     * use ReLinker load so file
     *
     * @param context
     * @param libName
     * @return
     */
    private static boolean relinker(Context context, final String libName) {
        if (context == null || TextUtils.isEmpty(libName)) {
            return false;
        }
        try {
            ReLinker.loadLibrary(context, libName);
        } catch (Throwable t) {
            return false;
        }
        return true;
    }
}
