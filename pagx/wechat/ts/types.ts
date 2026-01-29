import type {TGFX} from '@tgfx/types';

export interface PAGXViewNative {
    width: () => number;
    height: () => number;
    loadPAGX: (data: Uint8Array) => boolean;
    updateSize: (width: number, height: number) => void;
    updateZoomScaleAndOffset: (zoom: number, offsetX: number, offsetY: number) => void;
    onZoomEnd: () => void;
    draw: () => boolean;
    contentWidth: () => number;
    contentHeight: () => number;
    setPerformanceAdaptationEnabled: (enabled: boolean) => void;
    setSlowFrameThreshold: (thresholdMs: number) => void;
    setRecoveryWindow: (windowMs: number) => void;
    isLastFrameSlow: () => boolean;
    getAverageFrameTime: () => number;
    delete: () => void;
}

export interface PAGX extends TGFX {
    PAGXView: {
        MakeFrom: (width: number, height: number) => PAGXViewNative | null;
    };
    VectorString: any;
    module: PAGX;
}

