package org.libpag.compose

import androidx.compose.runtime.Immutable
import org.libpag.PAGComposition

/**
 * PAG animation data source
 * Wraps path and composition for stable state management
 */
@Immutable
sealed interface PAGCompositionSource {
    /**
     * Create data source from path
     * @param path path to PAG file
     * @param isAsyncLoad whether to load PAG file asynchronously
     */
    @Immutable
    data class Path(val path: String, val isAsyncLoad: Boolean = true) : PAGCompositionSource

    /**
     * Create data source from composition
     */
    @Immutable
    data class Composition(val composition: PAGComposition) : PAGCompositionSource
}