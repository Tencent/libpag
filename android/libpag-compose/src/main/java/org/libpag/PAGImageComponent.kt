package org.libpag

import androidx.compose.foundation.Canvas
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Modifier
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.platform.LocalContext
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import org.libpag.compose.PAGCompositionSource
import org.libpag.compose.StableMatrix


/**
 * Fully native Compose implementation of PAGImageView
 */
@Composable
fun PAGImageComponent(
    modifier: Modifier = Modifier,
    path: String,
    scaleMode: Int = PAGScaleMode.LetterBox,
    renderScale: Float = 1.0f,
    maxFrameRate: Float = 30f,
    repeatCount: Int = 1,
    autoPlay: Boolean = true,
    currentFrame: Int? = null,
    matrix: StableMatrix = StableMatrix.Identity,
    onAnimationStart: (() -> Unit)? = null,
    onAnimationEnd: (() -> Unit)? = null,
    onAnimationCancel: (() -> Unit)? = null,
    onAnimationRepeat: (() -> Unit)? = null,
    onAnimationUpdate: ((currentFrame: Int) -> Unit)? = null
) {
    PAGImageComponent(
        modifier = modifier,
        source = PAGCompositionSource.Path(path),
        scaleMode = scaleMode,
        renderScale = renderScale,
        maxFrameRate = maxFrameRate,
        repeatCount = repeatCount,
        autoPlay = autoPlay,
        currentFrame = currentFrame,
        matrix = matrix,
        onAnimationStart = onAnimationStart,
        onAnimationEnd = onAnimationEnd,
        onAnimationCancel = onAnimationCancel,
        onAnimationRepeat = onAnimationRepeat,
        onAnimationUpdate = onAnimationUpdate
    )
}

/**
 * Fully native Compose implementation of PAGImageView
 */
@Composable
fun PAGImageComponent(
    modifier: Modifier = Modifier,
    source: PAGCompositionSource,
    scaleMode: Int = PAGScaleMode.LetterBox,
    renderScale: Float = 1.0f,
    maxFrameRate: Float = 30f,
    repeatCount: Int = 1,
    autoPlay: Boolean = true,
    currentFrame: Int? = null,
    matrix: StableMatrix = StableMatrix.Identity,
    onAnimationStart: (() -> Unit)? = null,
    onAnimationEnd: (() -> Unit)? = null,
    onAnimationCancel: (() -> Unit)? = null,
    onAnimationRepeat: (() -> Unit)? = null,
    onAnimationUpdate: ((currentFrame: Int) -> Unit)? = null
) {

    val state = rememberPAGImageState(
        source = source,
        onAnimationStart = onAnimationStart,
        onAnimationEnd = onAnimationEnd,
        onAnimationCancel = onAnimationCancel,
        onAnimationRepeat = onAnimationRepeat,
        onAnimationUpdate = onAnimationUpdate
    )


    PAGImageComponent(
        state = state,
        scaleMode = scaleMode,
        renderScale = renderScale,
        maxFrameRate = maxFrameRate,
        repeatCount = repeatCount,
        autoPlay = autoPlay,
        currentFrame = currentFrame,
        matrix = matrix,
        modifier = modifier
    )
}

/**
 * PAGImage using State
 */
@Composable
fun PAGImageComponent(
    state: PAGImageState,
    scaleMode: Int = PAGScaleMode.LetterBox,
    renderScale: Float = 1.0f,
    maxFrameRate: Float = 30f,
    repeatCount: Int = 1,
    autoPlay: Boolean = true,
    currentFrame: Int? = null,
    matrix: StableMatrix = StableMatrix.Identity,
    modifier: Modifier = Modifier
) {
    val lifecycleOwner = LocalLifecycleOwner.current

    LaunchedEffect(
        scaleMode,
        renderScale,
        maxFrameRate,
        repeatCount,
        matrix
    ) {
        state.applyConfig(
            scaleMode = scaleMode,
            renderScale = renderScale,
            maxFrameRate = maxFrameRate,
            repeatCount = repeatCount,
            matrix = matrix
        )
    }


    LaunchedEffect(currentFrame) {
        currentFrame?.let { state.setCurrentFrame(it) }
    }

    DisposableEffect(lifecycleOwner) {
        // Activity Lifecycle
        val observer = LifecycleEventObserver { _, event ->
            when (event) {
                Lifecycle.Event.ON_PAUSE -> {
                    if (state.isPlaying()) {
                        state.pause()
                    }
                }

                Lifecycle.Event.ON_RESUME -> {
                    if (autoPlay && !state.isPlaying()) {
                        state.play()
                    }
                }

                else -> {}
            }
        }
        lifecycleOwner.lifecycle.addObserver(observer)

        onDispose {
            // this Compose dispose
            lifecycleOwner.lifecycle.removeObserver(observer)
            state.dispose()
        }
    }


    Canvas(
        modifier = modifier.onSizeChanged { size ->
            state.onSizeChanged(size)
        }
    ) {
        state.draw(this)
    }
}

/**
 * PAGImage 状态管理类
 */

/**
 * 创建并记住 PAGImageState
 */
@Composable
fun rememberPAGImageState(
    source: PAGCompositionSource,
    onAnimationStart: (() -> Unit)? = null,
    onAnimationEnd: (() -> Unit)? = null,
    onAnimationCancel: (() -> Unit)? = null,
    onAnimationRepeat: (() -> Unit)? = null,
    onAnimationUpdate: ((Int) -> Unit)? = null
): PAGImageState {
    val context = LocalContext.current
    val coroutineScope = rememberCoroutineScope()

    val state = remember(source, context) {
        PAGImageState(
            context = context,
            coroutineScope = coroutineScope,
            source = source,
            onAnimationStart = onAnimationStart,
            onAnimationEnd = onAnimationEnd,
            onAnimationCancel = onAnimationCancel,
            onAnimationRepeat = onAnimationRepeat,
            onAnimationUpdate = onAnimationUpdate
        )
    }

    return state
}



