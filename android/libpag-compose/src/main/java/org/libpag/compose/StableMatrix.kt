package org.libpag.compose

import androidx.compose.runtime.Immutable
import androidx.compose.ui.graphics.Matrix

/**
 * A stable matrix.
 */
@Immutable
sealed interface StableMatrix {

    /**
     * An identity matrix.
     */
    @Immutable
    data object Identity : StableMatrix

    /**
     * A custom matrix.
     */
    @Immutable
    data class Custom(val matrix: Matrix) : StableMatrix

    companion object {
        fun identity(): StableMatrix = Identity

        fun from(matrix: Matrix?): StableMatrix {
            if (matrix == null) return Identity
            return Custom(matrix)
        }
    }
}