import type {TGFX} from '@tgfx/types';

/**
 * Native C++ PAGXView interface exposed via Emscripten bindings.
 */
export interface PAGXViewNative {
    /** Returns the canvas width in pixels. */
    width: () => number;
    /** Returns the canvas height in pixels. */
    height: () => number;
    /** Registers fallback fonts for text rendering. */
    registerFonts: (fontData: Uint8Array, emojiFontData: Uint8Array) => void;
    /** Loads a PAGX file from the given data. */
    loadPAGX: (data: Uint8Array) => boolean;
    /** Updates the canvas size and recreates the surface. */
    updateSize: (width: number, height: number) => void;
    /** Updates the zoom scale and content offset for the display list. */
    updateZoomScaleAndOffset: (zoom: number, offsetX: number, offsetY: number) => void;
    /** Notifies that zoom gesture has ended for adaptive tile refinement. */
    onZoomEnd: () => void;
    /** Renders the current frame to the canvas. Returns true if rendering succeeds. */
    draw: () => boolean;
    /** Returns true if the first frame has been rendered successfully. */
    firstFrameRendered: () => boolean;
    /** Returns the width of the PAGX content in content pixels. */
    contentWidth: () => number;
    /** Returns the height of the PAGX content in content pixels. */
    contentHeight: () => number;
    /** Enables or disables performance-based adaptation. */
    setPerformanceAdaptationEnabled: (enabled: boolean) => void;
    /** Sets the slow frame threshold in milliseconds. */
    setSlowFrameThreshold: (thresholdMs: number) => void;
    /** Sets the recovery time window in milliseconds. */
    setRecoveryWindow: (windowMs: number) => void;
    /** Releases the native C++ object. */
    delete: () => void;
}

/**
 * PAGX WebAssembly module interface extending TGFX with PAGXView support.
 */
export interface PAGX extends TGFX {
    /** Factory for creating native PAGXView instances. */
    PAGXView: {
        /** Creates a PAGXView instance with the given canvas dimensions. */
        MakeFrom: (width: number, height: number) => PAGXViewNative | null;
    };
    VectorString: any;
    module: PAGX;
}

