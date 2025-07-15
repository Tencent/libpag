/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

import android.content.res.AssetManager;
import android.text.TextUtils;

import org.extra.tools.LibraryLoadUtils;

public class PAGFile extends PAGComposition {

    public interface LoadListener {
        /**
         * Callback for asynchronous loading.
         * Returns null if the file does not exist or the data is not a pag file.
         */
        void onLoad(PAGFile result);
    }

    /**
     * The maximum tag level current SDK supports.
     */
    public static native int MaxSupportedTagLevel();

    /**
     * Load a pag file from the specified path, returns null if the file does not exist or the
     * data is not a pag file.
     * Note: All PAGFiles loaded by the same path share the same internal cache. The internal
     * cache is alive until all PAGFiles are released. Use 'PAGFile.Load(byte[])' instead
     * if you don't want to load a PAGFile from the intenal caches.
     */
    public static PAGFile Load(String path) {
        if (!TextUtils.isEmpty(path) && (path.startsWith("http://") || path.startsWith("https://"))) {
            byte[] data = NetworkFetcher.FetchData(path);
            if (data == null) {
                return null;
            }
            return LoadFromBytes(data, data.length, path);
        } else {
            return LoadFromPath(path);
        }
    }

    /**
     * Asynchronously load a pag file from the specific path.
     */
    public static void LoadAsync(String path, LoadListener listener) {
        NativeTask.Run(() -> {
            PAGFile pagFile = Load(path);
            if (listener != null) {
                listener.onLoad(pagFile);
            }
        });
    }

    public static PAGFile Load(byte[] bytes) {
        return LoadFromBytes(bytes, bytes.length, "");
    }

    /**
     * Load a pag file from assets, returns null if the file does not exist or the data is not a
     * pag file.
     * Note: The same path shares resource in memory untill the file is released. If the file
     * may be changed, please use 'Load(byte[])' instead
     */
    public static PAGFile Load(AssetManager manager, String fileName) {
        return LoadFromAssets(manager, fileName);
    }

    private static native PAGFile LoadFromPath(String path);

    private static native PAGFile LoadFromBytes(byte[] bytes, int limit, String path);

    private static native PAGFile LoadFromAssets(AssetManager manager, String fileName);

    private PAGFile(long nativeContext) {
        super(nativeContext);
    }

    /**
     * The tag level this pag file requires.
     */
    public native int tagLevel();

    /**
     * The number of editable texts.
     */
    public native int numTexts();

    /**
     * The number of replaceable images.
     */
    public native int numImages();

    /**
     * The number of video compositions.
     */
    public native int numVideos();

    /**
     * The path string of this file, returns empty string if the file is loaded from byte stream.
     */
    public native String path();

    /**
     * Get a text data of the specified index. The index ranges from 0 to numTexts - 1.
     * Note: It always returns the default text data.
     */
    public native PAGText getTextData(int index);

    /**
     * Replace the text data of the specified index. The index ranges from 0 to PAGFile.numTexts - 1. Passing in null
     * for the textData parameter will reset it to default text data.
     */
    public native void replaceText(int editableTextIndex, PAGText textData);

    /**
     * Replace the image data of the specified index. The index ranges from 0 to PAGFile.numImages - 1. Passing in null
     * for the image parameter will reset it to default image data.
     */
    public void replaceImage(int editableImageIndex, PAGImage image) {
        if (image == null) {
            nativeReplaceImage(editableImageIndex, 0);
        } else {
            nativeReplaceImage(editableImageIndex, image.nativeContext);
        }
    }

    public native void nativeReplaceImage(int editableImageIndex, long image);

    /**
     * Replace the image data of the specified layer name. Passing in null
     * for the image parameter will reset it to default image data.
     */
    public void replaceImageByName(String layerName, PAGImage image) {
        if (image == null) {
            nativeReplaceImageByName(layerName, 0);
        } else {
            nativeReplaceImageByName(layerName, image.nativeContext);
        }
    }

    public native void nativeReplaceImageByName(String layerName, long image);

    /**
     * Return an array of layers by specified editable index and layer type.
     */
    public native PAGLayer[] getLayersByEditableIndex(int editableIndex, int layerType);

    /**
     * Returns the indices of the editable layers in this PAGFile.
     * If the editableIndex of a PAGLayer is not present in the returned indices, the PAGLayer should
     * not be treated as editable.
     */
    public native int[] getEditableIndices(int layerType);

    /**
     * Indicate how to stretch the original duration to fit target duration when file's duration is changed.
     * The default value is PAGTimeStretchMode::Repeat.
     */
    public native int timeStretchMode();

    /**
     * Set the timeStretchMode of this file.
     */
    public native void setTimeStretchMode(int value);

    /**
     * Set the duration of this PAGFile. Passing a value less than or equal to 0 resets the duration to its default value.
     */
    public native void setDuration(long duration);

    /**
     * Make a copy of the original file, any modification to current file has no effect on the result file.
     */
    public native PAGFile copyOriginal();

    private static native final void nativeInit();

    static {
        LibraryLoadUtils.loadLibrary("pag");
        nativeInit();
    }
}
