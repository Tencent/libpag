package com.example.flutter_pag_plugin

import android.content.Context
import android.os.Environment
import android.util.Log
import android.util.LruCache
import com.example.flutter_pag_plugin.utils.EncodeUtil
import com.jakewharton.disklrucache.DiskLruCache
import java.io.*
import java.net.HttpURLConnection
import java.net.URL
import java.security.MessageDigest
import java.security.NoSuchAlgorithmException

//数据加载器
object DataLoadHelper {
    private var diskCache: DiskLruCache? = null
    private val memoryCache by lazy { LruCache<String, ByteArray>(Runtime.getRuntime().maxMemory().toInt() / 500) }
    private const val TAG = "DataLoadHelper"
    const val DEFAULT_DIS_SIZE = 30 * 1024 * 1024L;

    //初始化pag动画
    fun loadPag(src: String, addPag: (ByteArray?) -> Unit) {
        val bytes = memoryCache.get(src)

        if (bytes != null) {
            addPag(bytes)
        } else {
            Thread {
                loadPagByDisk(src, addPag)
            }.start()
        }
    }

    fun initDiskCache(context: Context, size: Long = DEFAULT_DIS_SIZE) {
        if (diskCache != null) {
            Log.w(TAG, "diskCache do not need init again!")
            return
        }

        val cacheDir = getDiskCacheDir(context, "pag")
        if (!cacheDir.exists()) {
            cacheDir.mkdirs()
        }
        try {
            diskCache = diskCache ?: DiskLruCache.open(
                    cacheDir,
                    context.packageManager.getPackageInfo(context.packageName, 0).versionCode,
                    1, size
            )
        } catch (e: IOException) {
            Log.e(TAG, "initDiskCache error: $e")
        }
    }


    //获取硬盘缓存路径
    private fun getDiskCacheDir(context: Context, uniqueName: String): File {
        val cachePath: String = if (Environment.MEDIA_MOUNTED == Environment.getExternalStorageState()
                || !Environment.isExternalStorageRemovable()
        ) {
            context.externalCacheDir?.path
        } else {
            context.cacheDir.path
        } ?: ""
        return File(cachePath + File.separator.toString() + uniqueName)
    }


    //硬盘或者网络获取，
    @Synchronized
    private fun loadPagByDisk(src: String, addPag: (ByteArray?) -> Unit) {
        //从硬盘缓存中获取
        val key = hashKeyForDisk(src)
        var snapShot: DiskLruCache.Snapshot? = null
        try {
            snapShot = diskCache?.get(key)
            if (snapShot == null) {
                Log.d(TAG, "loadPag load from network")
                //没有，进行网络操作
                val editor = diskCache?.edit(key)
                if (editor != null) {
                    val outputStream = editor.newOutputStream(0)
                    if (downloadUrlToStream(src, outputStream)) {
                        editor.commit()
                    } else {
                        editor.abort()
                    }
                }
                //存入硬盘后再次读取
                diskCache?.flush()
                snapShot = diskCache?.get(key)
            }
        } catch (e: IOException) {
            Log.e(TAG, "loadPag load from network erro: $e")
        }

        //进行bytes读取
        var bytes: ByteArray? = null
        if (snapShot != null) {
            Log.d(TAG, "loadPag load from snapShot")
            try {
                val inputStream = snapShot.getInputStream(0)
                val outputStrem = ByteArrayOutputStream()
                inputStream.use { ins ->
                    outputStrem.use { bos ->
                        val buffer = ByteArray(1024)
                        var len: Int

                        while (ins.read(buffer).also { len = it } != -1) {
                            bos.write(buffer, 0, len)
                        }
                        bytes = bos.toByteArray()
                    }
                }
            } catch (e: IOException) {
                e.printStackTrace()
                Log.e(TAG, "loadPag load from snapShot erro: $e")
            }
        }

        Log.d(TAG, "loadPag bytes size: ${bytes?.size}")
        //存储进内存缓存
        if (bytes != null && bytes!!.isNotEmpty() && memoryCache.get(key) == null) {
            memoryCache.put(key, bytes)
        }
        addPag(bytes)
    }


    //下载
    private fun downloadUrlToStream(urlString: String, outputStream: OutputStream): Boolean {
        var urlConnection: HttpURLConnection? = null
        var outStream: BufferedOutputStream? = null
        var inStream: BufferedInputStream? = null
        try {
            val url = URL(urlString)
            urlConnection = url.openConnection() as HttpURLConnection
            inStream = BufferedInputStream(urlConnection.inputStream, 8 * 1024)
            outStream = BufferedOutputStream(outputStream, 8 * 1024)
            var b: Int
            while (inStream.read().also { b = it } != -1) {
                outStream.write(b)
            }
            return true
        } catch (e: IOException) {
            e.printStackTrace()
        } finally {
            urlConnection?.disconnect()
            try {
                outStream?.close()
                inStream?.close()
            } catch (e: IOException) {
                e.printStackTrace()
            }
        }
        return false
    }

    //使用MD5算法对传入的key进行加密并返回
    private fun hashKeyForDisk(key: String): String? {
        return try {
            val mDigest: MessageDigest = MessageDigest.getInstance("MD5")
            mDigest.update(key.toByteArray())
            EncodeUtil.bytesToHexString(mDigest.digest())
        } catch (e: NoSuchAlgorithmException) {
            key.hashCode().toString()
        }
    }
}