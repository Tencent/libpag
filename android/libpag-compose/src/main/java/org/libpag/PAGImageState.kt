package org.libpag

import android.content.Context
import android.graphics.Bitmap
import android.hardware.HardwareBuffer
import android.os.Build
import android.util.Log
import androidx.compose.runtime.Stable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Matrix
import androidx.compose.ui.graphics.Paint
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.drawIntoCanvas
import androidx.compose.ui.graphics.withSave
import androidx.compose.ui.unit.IntSize
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import org.libpag.compose.PAGCompositionSource
import org.libpag.compose.StableMatrix
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.atomic.AtomicBoolean
import kotlin.math.floor

/**
 * PAGImageState is a state object that holds the configuration and internal state of a PAGImage.
 */
@Stable
class PAGImageState internal constructor(
    private val context: Context,
    private val coroutineScope: CoroutineScope,
    private val source: PAGCompositionSource,
    private val onAnimationStart: (() -> Unit)?,
    private val onAnimationEnd: (() -> Unit)?,
    private val onAnimationCancel: (() -> Unit)?,
    private val onAnimationRepeat: (() -> Unit)?,
    private val onAnimationUpdate: ((Int) -> Unit)?
): PAGAnimator.Listener {
    // Configuration properties
    private  var _composition: PAGComposition ? = null
    private var _scaleMode: Int = PAGScaleMode.LetterBox
    private var _renderScale: Float = 1.0f
    private var _maxFrameRate: Float = 30f
    private var _cacheAllFramesInMemory: Boolean = false
    private var _repeatCount: Int = 1

    // Internal state
    private var viewSize by mutableStateOf(IntSize.Zero)
    private var renderBitmap by mutableStateOf<Bitmap?>(null)
    private var renderMatrix by mutableStateOf<Matrix?>(null)
    private var _matrix by mutableStateOf<Matrix?>(null)

    private var _currentFrame by mutableIntStateOf(0)
    private var _numFrames by mutableIntStateOf(0)

    private var width = 0  // Render width
    private var height = 0 // Render height

    // PAG
    private val decoderInfo = DecoderInfo()
    private val freezeDraw = AtomicBoolean(false)
    private val bitmapCache = ConcurrentHashMap<Int, Bitmap>()
    private var frontBitmap: Bitmap? = null
    private var backBitmap: Bitmap? = null
    private var frontHardwareBuffer: HardwareBuffer? = null
    private var backHardwareBuffer: HardwareBuffer? = null
    private val useFirst = AtomicBoolean(true)

    private var lastContentVersion = -1
    private var forceFlush = false
    private var isDispose = false
    private var memoryCacheStatusHasChanged = false

    private var animator: PAGAnimator = PAGAnimator.MakeFrom(context, this).apply {
        this.setRepeatCount(_repeatCount)
    }

    init {

        if (source is PAGCompositionSource.Composition) {
            _composition = source.composition

            if (hasSize()) {
                initDecoder()
            }
            refreshResource()
        } else if (source is PAGCompositionSource.Path) {
            if (source.isAsyncLoad) {
                coroutineScope.launch(Dispatchers.IO) {
                    _composition = loadComposition(context, source.path) ?: throw IllegalArgumentException("PAGImageState: load composition failed")

                    if (hasSize()) {
                        initDecoder()
                    }

                    refreshResource()
                }
            } else {
                _composition = loadComposition(context, source.path) ?: throw IllegalArgumentException("PAGImageState: load composition failed")

                if (hasSize()) {
                    initDecoder()
                }
                refreshResource()
            }
        } else {
            throw IllegalArgumentException("PAGImageState: composition or path must be not null")
        }
    }

    // ==================== PAGAnimator.Listener ====================

    override fun onAnimationStart(animator: PAGAnimator) {
        onAnimationStart?.invoke()
    }

    override fun onAnimationEnd(animator: PAGAnimator) {
        onAnimationEnd?.invoke()
    }

    override fun onAnimationCancel(animator: PAGAnimator) {
        onAnimationCancel?.invoke()
    }

    override fun onAnimationRepeat(animator: PAGAnimator) {
        onAnimationRepeat?.invoke()
    }

    override fun onAnimationUpdate(animator: PAGAnimator) {
        if (isDispose) {
            return
        }
        if (_composition != null) {
            animator.setDuration(_composition!!.duration())
        }
        flush()
        onAnimationUpdate?.invoke(_currentFrame)
    }

    // ==================== Public Methods ====================

    fun applyConfig(
        scaleMode: Int,
        renderScale: Float,
        maxFrameRate: Float,
        repeatCount: Int,
        matrix: StableMatrix = StableMatrix.Identity
    ) {
        setScaleMode(scaleMode)
        setRenderScale(renderScale)
        setMaxFrameRate(maxFrameRate)
        setRepeatCount(repeatCount)
        setMatrix(matrix)
    }

    fun setScaleMode(mode: Int) {
        if (mode == _scaleMode) {
            return
        }
        _scaleMode = mode
        if (hasSize()) {
            refreshMatrixFromScaleMode()
        } else {
            _matrix = null
        }
    }

    fun setRenderScale(scale: Float) {
        if (this._renderScale == scale) {
            return
        }
        _renderScale = scale.coerceIn(0f, 1f)
        width = (viewSize.width * _renderScale).toInt()
        height = (viewSize.height * _renderScale).toInt()
        refreshMatrixFromScaleMode()

        if (_renderScale < 1.0f) {
            renderMatrix = Matrix().apply {
                scale(1 / _renderScale, 1 / _renderScale)
            }
        }
        refreshResource()
    }

    fun setMaxFrameRate(frameRate: Float) {
        if (this._maxFrameRate == frameRate) {
            return
        }
        _maxFrameRate = frameRate
        refreshResource()
    }

    fun setCacheAllFrames(cache: Boolean) {
        memoryCacheStatusHasChanged = _cacheAllFramesInMemory != cache
        _cacheAllFramesInMemory = cache
    }

    fun setRepeatCount(count: Int) {
        if (this._repeatCount == count) {
            return
        }
        _repeatCount = count
        animator.setRepeatCount(count)
    }

    fun setMatrix(matrix: StableMatrix) {
        _scaleMode = PAGScaleMode.None
        if (matrix is StableMatrix.Identity) {
            _matrix = null // draw will be triggered automatically
        } else if (matrix is StableMatrix.Custom) {
            _matrix = matrix.matrix // draw will be triggered automatically
        }
    }

    fun play() {
        animator.start()
    }

    fun pause() {
        animator.cancel()
    }

    fun isPlaying(): Boolean = animator.isRunning

    fun getCurrentFrame(): Int = _currentFrame

    fun setCurrentFrame(frame: Int) {
        refreshNumFrames()
        if (_numFrames == 0 || !decoderInfo.isValid() || frame < 0) {
            return
        }
        if (frame >= _numFrames) {
            return
        }
        _currentFrame = frame
        animator.setProgress(frameToProgress(frame, _numFrames))
        animator.update()
    }

    fun getNumFrames(): Int = _numFrames

    fun getCurrentImage(): Bitmap? = renderBitmap

    fun flush(): Boolean {
        if (!decoderInfo.isValid()) {
            initDecoder()
            if (!decoderInfo.isValid()) {
                return false
            }
        }

        if (decoderInfo.hasPAGDecoder()) {
            _numFrames = decoderInfo.numFrames()
        }

        _currentFrame = progressToFrame(animator.progress(), _numFrames)

        if (!handleFrame(_currentFrame)) {
            forceFlush = false
            return false
        }

        forceFlush = false
        return true
    }

    // ==================== Internal Methods ====================

    internal fun onSizeChanged(size: IntSize) {
        if (size == viewSize) return

        Log.d("PAGImage", "onSizeChanged: $size")

        freezeDraw.set(true)
        decoderInfo.reset()
        releaseBitmaps()
        bitmapCache.clear()

        viewSize = size
        width = (_renderScale * size.width).toInt()
        height = (_renderScale * size.height).toInt()

        forceFlush = true
        checkVisible()

        freezeDraw.set(false)
    }

    private val bitmapPaint = Paint()

    internal fun draw(drawScope: DrawScope) {
        drawScope.drawIntoCanvas { canvas ->
            renderBitmap?.let { bitmap ->
                if (!freezeDraw.get() && !bitmap.isRecycled) {
                    canvas.withSave {
                        renderMatrix?.let { canvas.concat(it) }
                        _matrix?.let { canvas.concat(it) }

                        try {
                            canvas.drawImage(bitmap.asImageBitmap(), Offset.Zero, bitmapPaint)
                        } catch (e: Exception) {
                            e.printStackTrace()
                        }
                    }
                }
            }
        }
    }

    internal fun dispose() {
        isDispose = true
        animator.release()
        decoderInfo.reset()
        releaseBitmaps()
        bitmapCache.clear()
        lastContentVersion = -1
        memoryCacheStatusHasChanged = false
        freezeDraw.set(false)
    }

    private fun refreshResource() {
        freezeDraw.set(true)
        decoderInfo.reset()
        releaseBitmaps()
        bitmapCache.clear()
        _currentFrame = 0

        val comp = _composition

        _matrix = null
        _currentFrame = 0
        animator.setProgress(comp?.progress ?: 0.0)
        val duration = comp?.duration() ?: 0
        if (!isDispose && hasSize()) {
            animator.setDuration(duration)
        }
        animator.update()

        freezeDraw.set(false)
    }

    private fun initDecoder() {
        synchronized(decoderInfo) {
            if (!decoderInfo.isValid()) {
                if (decoderInfo.initDecoder(_composition, width, height, _maxFrameRate)) {
                    return
                }

                if (!decoderInfo.isValid()) {
                    return
                }
            }
            refreshMatrixFromScaleMode()
            freezeDraw.set(false)
        }
    }

    private fun handleFrame(frame: Int): Boolean {
        if (!decoderInfo.isValid() || freezeDraw.get()) {
            return false
        }

        checkStatusChange()
        releaseDecoder()

        val bitmap = bitmapCache[frame]
        if (bitmap != null) {
            renderBitmap = bitmap
            return true
        }

        if (freezeDraw.get()) {
            return false
        }

        if (!decoderInfo.hasPAGDecoder()) {
            return false
        }

        if (!forceFlush && !decoderInfo.checkFrameChanged(frame)) {
            return true
        }

        synchronized(decoderInfo) {
            if (frontBitmap == null || _cacheAllFramesInMemory) {
                val pair = BitmapHelper.CreateBitmap(decoderInfo._width, decoderInfo._height, false)
                frontBitmap = pair.first
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    frontHardwareBuffer = pair.second as? HardwareBuffer
                }
            }

            if (frontBitmap == null) return false

            val flushBitmap: Bitmap
            val flushBuffer: HardwareBuffer?

            if (!_cacheAllFramesInMemory) {
                if (backBitmap == null) {
                    val pair = BitmapHelper.CreateBitmap(decoderInfo._width, decoderInfo._height, false)
                    backBitmap = pair.first
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                        backHardwareBuffer = pair.second as? HardwareBuffer
                    }
                }

                if (useFirst.get()) {
                    flushBitmap = frontBitmap!!
                    flushBuffer = frontHardwareBuffer
                } else {
                    flushBitmap = backBitmap!!
                    flushBuffer = backHardwareBuffer
                }
                useFirst.set(!useFirst.get())
            } else {
                flushBitmap = frontBitmap!!
                flushBuffer = frontHardwareBuffer
            }

            val success = if (flushBuffer != null) {
                decoderInfo.readFrame(frame, flushBuffer)
            } else {
                decoderInfo.copyFrameTo(flushBitmap, frame)
            }

            if (!success) return false

            flushBitmap.prepareToDraw()

            if (_cacheAllFramesInMemory) {
                bitmapCache[frame] = flushBitmap
            }

            renderBitmap = flushBitmap
        }

        return true
    }

    private fun releaseBitmaps() {
        frontBitmap = null
        backBitmap = null
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            frontHardwareBuffer?.close()
            backHardwareBuffer?.close()
            frontHardwareBuffer = null
            backHardwareBuffer = null
        }
        renderBitmap = null
    }

    private fun allInMemoryCache(): Boolean {
        if (decoderInfo.isValid() && decoderInfo.hasPAGDecoder()) {
            _numFrames = decoderInfo.numFrames()
        }
        return bitmapCache.size == _numFrames
    }

    private fun releaseDecoder() {
        if (allInMemoryCache()) {
            decoderInfo.releaseDecoder()
        }
    }

    private fun checkStatusChange() {
        var needResetBitmapCache = false
        if (memoryCacheStatusHasChanged) {
            needResetBitmapCache = true
            memoryCacheStatusHasChanged = false
        }

        if (_composition != null) {
            val nVersion = PAGImageView.ContentVersion(_composition)
            if (lastContentVersion >= 0 && lastContentVersion != nVersion) {
                needResetBitmapCache = true
            }
            lastContentVersion = nVersion
        }

        if (needResetBitmapCache) {
            bitmapCache.clear()
            if (!decoderInfo.hasPAGDecoder()) {
                val comp = _composition
                decoderInfo.initDecoder(comp, width, height, _maxFrameRate)
            }
        }
    }

    private fun hasSize(): Boolean = width > 0 && height > 0

    private fun refreshNumFrames() {
        if (!decoderInfo.isValid() && _numFrames == 0 && hasSize()) {
            initDecoder()
        }
        if (decoderInfo.isValid() && decoderInfo.hasPAGDecoder()) {
            _numFrames = decoderInfo.numFrames()
        }
    }

    private fun refreshMatrixFromScaleMode() {
        if (_scaleMode == PAGScaleMode.None) {
            return
        }
        _matrix = applyScaleMode(
            _scaleMode,
            decoderInfo._width,
            decoderInfo._height,
            width,
            height
        )
    }

    private fun checkVisible() {
        val visible = !isDispose && hasSize()
        if (isVisible == visible) {
            return
        }
        isVisible = visible
        if (isVisible) {
            val duration = _composition?.duration() ?: 0
            animator.setDuration(duration)
        } else {
            animator.setDuration(0)
        }
        animator.update()
    }

    private var isVisible = false
}

/**
 * Decoder info wrapper class
 */
private class DecoderInfo {
    var _width: Int = 0
    var _height: Int = 0
    var duration: Long = 0
    private var _pagDecoder: PAGDecoder? = null

    @Synchronized
    fun isValid(): Boolean = _width > 0 && _height > 0

    @Synchronized
    fun hasPAGDecoder(): Boolean = _pagDecoder != null

    @Synchronized
    fun checkFrameChanged(currentFrame: Int): Boolean {
        return _pagDecoder?.checkFrameChanged(currentFrame) ?: false
    }

    @Synchronized
    fun readFrame(currentFrame: Int, hardwareBuffer: HardwareBuffer): Boolean {
        return _pagDecoder?.readFrame(currentFrame, hardwareBuffer) ?: false
    }

    @Synchronized
    fun copyFrameTo(bitmap: Bitmap, currentFrame: Int): Boolean {
        return _pagDecoder?.copyFrameTo(bitmap, currentFrame) ?: false
    }

    @Synchronized
    fun initDecoder(
        composition: PAGComposition?,
        width: Int,
        height: Int,
        maxFrameRate: Float
    ): Boolean {
        if (composition == null || width <= 0 || height <= 0 || maxFrameRate <= 0) {
            return false
        }

        val scaleFactor = if (composition.width() >= composition.height()) {
            width * 1.0f / composition.width()
        } else {
            height * 1.0f / composition.height()
        }

        _pagDecoder = PAGDecoder.Make(composition, maxFrameRate, scaleFactor)
        _width = _pagDecoder?.width() ?: 0
        _height = _pagDecoder?.height() ?: 0
        duration = composition.duration()

        return true
    }

    @Synchronized
    fun releaseDecoder() {
        _pagDecoder?.release()
        _pagDecoder = null
    }

    @Synchronized
    fun reset() {
        releaseDecoder()
        _width = 0
        _height = 0
        duration = 0
    }

    @Synchronized
    fun numFrames(): Int = _pagDecoder?.numFrames() ?: 0
}

// ==================== Helper Functions ====================

private  fun loadComposition(context: Context, path: String): PAGComposition? {
    return if (path.startsWith("assets://")) {
        PAGFile.Load(context.assets, path.substring(9))
    } else {
        PAGFile.Load(path)
    }
}


private fun progressToFrame(progress: Double, totalFrames: Int): Int {
    if (totalFrames <= 1) return 0

    var percent = progress % 1.0
    if (percent <= 0 && progress != 0.0) {
        percent += 1.0
    }

    val currentFrame = floor(percent * totalFrames).toInt()
    return if (currentFrame == totalFrames) totalFrames - 1 else currentFrame
}

private fun frameToProgress(currentFrame: Int, totalFrames: Int): Double {
    if (totalFrames <= 1 || currentFrame < 0) return 0.0
    if (currentFrame >= totalFrames - 1) return 1.0
    return (currentFrame * 1.0 + 0.1) / totalFrames
}

private fun applyScaleMode(
    scaleMode: Int,
    sourceWidth: Int,
    sourceHeight: Int,
    targetWidth: Int,
    targetHeight: Int
): Matrix {
    val matrix = Matrix()
    if (scaleMode == PAGScaleMode.None || sourceWidth <= 0 || sourceHeight <= 0 ||
        targetWidth <= 0 || targetHeight <= 0) {
        return matrix
    }

    val scaleX = targetWidth * 1.0f / sourceWidth
    val scaleY = targetHeight * 1.0f / sourceHeight

    when (scaleMode) {
        PAGScaleMode.Stretch -> {
            matrix.scale(scaleX, scaleY)
        }
        PAGScaleMode.Zoom -> {
            val scale = maxOf(scaleX, scaleY)
            matrix.scale(scale, scale)
            if (scaleX > scaleY) {
                matrix.translate(0f, (targetHeight - sourceHeight * scale) * 0.5f)
            } else {
                matrix.translate((targetWidth - sourceWidth * scale) * 0.5f, 0f)
            }
        }
        PAGScaleMode.LetterBox -> {
            val scale = minOf(scaleX, scaleY)
            matrix.scale(scale, scale)
            if (scaleX < scaleY) {
                matrix.translate(0f, (targetHeight - sourceHeight * scale) * 0.5f)
            } else {
                matrix.translate((targetWidth - sourceWidth * scale) * 0.5f, 0f)
            }
        }
    }

    return matrix
}