/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

package org.libpag;

import static android.os.Environment.isExternalStorageRemovable;

import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.os.Build;
import android.os.Environment;

import org.extra.tools.LibraryLoadUtils;

import java.io.File;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.locks.ReentrantReadWriteLock;

class PAGCacheManager {
    static PAGCacheManager pagCacheManager;
    private String cacheDir;
    Context context;

    public static class CacheItem {
        private PAGImageCache _pagImageCache;
        ReentrantReadWriteLock lock = new ReentrantReadWriteLock();

        static CacheItem Make(String path, int width, int height, int frameCount) {
            PAGImageCache cache = PAGImageCache.Make(path, width, height, frameCount);
            if (cache == null) {
                return null;
            }
            CacheItem cacheItem = new CacheItem();
            cacheItem._pagImageCache = cache;
            return cacheItem;
        }

        public PAGImageCache pagCache() {
            return _pagImageCache;
        }

        public boolean saveBitmap(int frame, Bitmap bitmap) {
            writeLock();
            boolean success = _pagImageCache.saveBitmap(frame, bitmap);
            writeUnlock();
            return success;
        }

        public boolean inflateBitmap(int frame, Bitmap bitmap) {
            readLock();
            boolean success = _pagImageCache.inflateBitmap(frame, bitmap);
            readUnlock();
            return success;
        }

        public boolean isCached(int frame) {
            readLock();
            boolean inCache = _pagImageCache.isCached(frame);
            readUnlock();
            return inCache;
        }
        public boolean isAllCached() {
            readLock();
            boolean allCached = _pagImageCache.isAllCached();
            readUnlock();
            return allCached;
        }

        public void readLock() {
            lock.readLock().lock();
        }

        public void readUnlock() {
            lock.readLock().unlock();
        }

        public void writeLock() {
            lock.writeLock().lock();
        }

        public void writeUnlock() {
            lock.writeLock().unlock();
        }

        public void release() {
            _pagImageCache.release();
        }
    }

    ConcurrentHashMap<String, CacheItem> pagCaches = new ConcurrentHashMap<>();

    private PAGCacheManager() {
    }

    public static PAGCacheManager Get(Context context) {
        if (pagCacheManager != null) {
            return pagCacheManager;
        }
        if (!CheckPermission(context)) {
            return null;
        }
        synchronized (PAGCacheManager.class) {
            if (pagCacheManager != null) {
                return pagCacheManager;
            }
            pagCacheManager = new PAGCacheManager();
            pagCacheManager.context = context.getApplicationContext();
            pagCacheManager.cacheDir = getDefaultDiskCacheDir(context, "libpag");
            return pagCacheManager;
        }
    }

    public CacheItem getOrCreate(String key, int width, int height, int frameCount) {
        CacheItem cacheItem = pagCaches.get(key);
        if (cacheItem != null) {
            return cacheItem;
        }
        synchronized (PAGCacheManager.this) {
            cacheItem = pagCaches.get(key);
            if (cacheItem == null) {
                cacheItem = CacheItem.Make(getPath(key), width, height, frameCount);
            }
            if (cacheItem == null) {
                return null;
            }
            pagCaches.put(key, cacheItem);
        }
        return cacheItem;
    }

    public CacheItem get(String key) {
        return pagCaches.get(key);
    }

    public void put(String key, CacheItem item) {
        pagCaches.put(key, item);
    }

    public void remove(String key) {
        CacheItem item = pagCaches.get(key);
        if (item == null) {
            return;
        }
        item.writeLock();
        item.release();
        pagCaches.remove(key);
        item.writeUnlock();
    }

    public String getPath(String key) {
        return cacheDir + File.separator + key;
    }

    private static String getDefaultDiskCacheDir(Context context, String uniqueName) {
        // Check if media is mounted or storage is built-in, if so, try and use external cache dir
        // otherwise use internal cache dir
        final String cachePath =
                Environment.MEDIA_MOUNTED.equals(Environment.getExternalStorageState()) ||
                        !isExternalStorageRemovable() ?
                        context.getExternalCacheDir().getPath() :
                        context.getCacheDir().getPath();
        return cachePath + File.separator + uniqueName;
    }

    private static boolean CheckPermission(Context context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            String[] permissions = new String[]{
                    "android.permission.READ_EXTERNAL_STORAGE",
                    "android.permission.WRITE_EXTERNAL_STORAGE"
            };
            for (String permission : permissions) {
                if (context.checkSelfPermission(permission) != PackageManager.PERMISSION_GRANTED) {
                    return false;
                }
            }

        }
        return true;
    }

    private static class PAGImageCache {
        private long nativeContext = 0;

        static PAGImageCache Make(String path, int width, int height, int frameCount) {
            File file = new File(path);
            boolean needInit = false;
            if (!file.exists()) {
                file.getParentFile().mkdirs();
                needInit = true;
            }
            long nativeHandle = SetupCache(path, width, height, frameCount, needInit);
            if (nativeHandle == 0) {
                file.delete();
                return null;
            }
            return new PAGImageCache(nativeHandle);
        }

        native boolean saveBitmap(int frame, Bitmap bitmap);

        native boolean inflateBitmap(int frame, Bitmap bitmap);

        native boolean isCached(int frame);

        native boolean isAllCached();

        native void release();

        private static native long SetupCache(String path, int width, int height, int frameCount, boolean needInit);

        private PAGImageCache(long nativeContext) {
            this.nativeContext = nativeContext;
        }

        private static native void nativeInit();

        static {
            LibraryLoadUtils.loadLibrary("pag");
            nativeInit();
        }
    }
}
