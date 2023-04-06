/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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
import android.graphics.Bitmap;
import android.os.Build;
import android.os.Environment;
import android.text.TextUtils;

import org.extra.tools.LibraryLoadUtils;

import java.io.File;
import java.util.Arrays;
import java.util.HashMap;
import java.util.concurrent.locks.ReentrantReadWriteLock;

class CacheManager {
    static CacheManager pagCacheManager;
    private String cacheDir;
    private final static float autoCleanThreshold = 0.6f;
    Context context;

    static class CacheItem {
        private ImageCache _pagImageCache;
        ReentrantReadWriteLock lock = new ReentrantReadWriteLock();

        static CacheItem Make(String path, int width, int height, int frameCount) {
            ImageCache cache = ImageCache.Make(path, width, height, frameCount);
            if (cache == null) {
                return null;
            }
            CacheItem cacheItem = new CacheItem();
            cacheItem._pagImageCache = cache;
            return cacheItem;
        }

        protected ImageCache pagCache() {
            return _pagImageCache;
        }

        protected boolean saveBitmap(int frame, Bitmap bitmap) {
            writeLock();
            boolean success = _pagImageCache.saveBitmap(frame, bitmap);
            writeUnlock();
            return success;
        }

        protected boolean flushSave() {
            writeLock();
            boolean success = _pagImageCache.flushSave();
            writeUnlock();
            return success;
        }

        protected boolean inflateBitmap(int frame, Bitmap bitmap) {
            readLock();
            boolean success = _pagImageCache.inflateBitmap(frame, bitmap);
            readUnlock();
            return success;
        }

        protected boolean isCached(int frame) {
            readLock();
            boolean inCache = _pagImageCache.isCached(frame);
            readUnlock();
            return inCache;
        }

        protected boolean isAllCached() {
            readLock();
            boolean allCached = _pagImageCache.isAllCached();
            readUnlock();
            return allCached;
        }

        protected void releaseSaveBuffer() {
            writeLock();
            _pagImageCache.releaseSaveBuffer();
            writeUnlock();
        }

        protected void readLock() {
            lock.readLock().lock();
        }

        protected void readUnlock() {
            lock.readLock().unlock();
        }

        protected void writeLock() {
            lock.writeLock().lock();
        }

        protected void writeUnlock() {
            lock.writeLock().unlock();
        }

        protected void release() {
            _pagImageCache.release();
        }
    }

    HashMap<String, CacheItem> pagCaches = new HashMap();
    HashMap<String, Integer> pagCachesReferenceCount = new HashMap();

    private CacheManager() {
    }

    protected static CacheManager Get(Context context) {
        if (pagCacheManager != null) {
            return pagCacheManager;
        }
        if (context == null) {
            return null;
        }
        synchronized (CacheManager.class) {
            if (pagCacheManager != null) {
                return pagCacheManager;
            }
            pagCacheManager = new CacheManager();
            pagCacheManager.context = context.getApplicationContext();
            pagCacheManager.cacheDir = GetDefaultDiskCacheDir(pagCacheManager.context, null);
            return pagCacheManager;
        }
    }

    protected static void ClearAllDiskCache(Context context) {
        CacheManager manager = CacheManager.Get(context);
        if (manager == null) {
            return;
        }
        try {
            new File(manager.cacheDir).delete();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    protected void autoClean() {
        try {
            File[] files = new File(cacheDir).listFiles();
            if (files.length == 0) {
                return;
            }
            // Avoid large number of query lastModified operations
            FilePair[] pairs = new FilePair[files.length];
            long totalSize = 0;
            for (int i = 0; i < files.length; i++) {
                totalSize += files[i].length();
                pairs[i] = new FilePair(files[i]);
            }
            if (totalSize < PAGImageView.g_MaxDiskCacheSize) {
                return;
            }
            Arrays.sort(pairs);
            long deleteSize = 0;
            long needDeleteSize = (long) (PAGImageView.g_MaxDiskCacheSize * (1.f - autoCleanThreshold));
            for (FilePair pair : pairs) {
                deleteSize += pair.f.length();
                pair.f.delete();
                if ((totalSize - deleteSize) <= needDeleteSize) {
                    return;
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }

    }

    protected CacheItem getOrCreate(String key, int width, int height, int frameCount) {
        synchronized (CacheManager.this) {
            CacheItem cacheItem = pagCaches.get(key);
            if (cacheItem != null) {
                return cacheItem;
            }
            if (cacheItem == null) {
                cacheItem = CacheItem.Make(getPath(key), width, height, frameCount);
            }
            if (cacheItem == null) {
                return null;
            }
            pagCaches.put(key, cacheItem);
            Integer i = pagCachesReferenceCount.get(key);
            if (i == null) {
                i = 0;
            }
            pagCachesReferenceCount.put(key, i + 1);
            return cacheItem;
        }
    }

    protected void remove(String key) {
        synchronized (this) {
            Integer i = pagCachesReferenceCount.get(key);
            if (i == null) {
                return;
            }
            if (--i <= 0) {
                pagCachesReferenceCount.remove(key);
                CacheItem item = pagCaches.get(key);
                if (item == null) {
                    return;
                }
                item.writeLock();
                item.release();
                pagCaches.remove(key);
                item.writeUnlock();
                pagCachesReferenceCount.remove(key);
            } else {
                pagCachesReferenceCount.put(key, i);
            }
        }
    }

    protected String getPath(String key) {
        return cacheDir + File.separator + key;
    }

    private static String GetDefaultDiskCacheDir(Context context, String uniqueName) {
        // Check if media is mounted or storage is built-in, if so, try and use external cache dir
        // otherwise use internal cache dir
        if (TextUtils.isEmpty(uniqueName)) {
            uniqueName = "PAGImageViewCache";
        }
        final String cachePath =
                Environment.MEDIA_MOUNTED.equals(Environment.getExternalStorageState()) ||
                        !isExternalStorageRemovable() ?
                        context.getExternalCacheDir().getPath() :
                        context.getCacheDir().getPath();
        return cachePath + File.separator + uniqueName;
    }

    static native int ContentVersion(PAGComposition pagComposition);

    private static class ImageCache {
        private long nativeContext = 0;

        static ImageCache Make(String path, int width, int height, int frameCount) {
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
            return new ImageCache(nativeHandle);
        }

        boolean saveBitmap(int frame, Bitmap bitmap) {
            if (bitmap == null) {
                return false;
            }
            boolean isHardware = false;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                isHardware = bitmap.getConfig() == Bitmap.Config.HARDWARE;
            }
            return putBitmapToSaveBuffer(frame, bitmap, bitmap.getByteCount(), isHardware);
        }

        native boolean putBitmapToSaveBuffer(int frame, Bitmap bitmap, int byteCount, boolean isHardware);

        native boolean flushSave();

        boolean inflateBitmap(int frame, Bitmap bitmap) {
            if (bitmap == null) {
                return false;
            }
            boolean isHardware = false;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                isHardware = bitmap.getConfig() == Bitmap.Config.HARDWARE;
            }
            return inflateBitmap(frame, bitmap, bitmap.getByteCount(), isHardware);
        }

        native boolean inflateBitmap(int frame, Bitmap bitmap, int byteCount, boolean isHardware);

        native boolean isCached(int frame);

        native boolean isAllCached();

        native void releaseSaveBuffer();

        native void release();

        private static native long SetupCache(String path, int width, int height, int frameCount, boolean needInit);

        private ImageCache(long nativeContext) {
            this.nativeContext = nativeContext;
        }

        private static native void nativeInit();

        static {
            LibraryLoadUtils.loadLibrary("pag");
            nativeInit();
        }
    }

    private static class FilePair implements Comparable {
        protected long t;
        protected File f;

        protected FilePair(File file) {
            f = file;
            t = file.lastModified();
        }

        public int compareTo(Object o) {
            long u = ((FilePair) o).t;
            return t < u ? -1 : t == u ? 0 : 1;
        }
    }

}
