/**
 * Copyright (C) 2026 Tencent. All Rights Reserved.
 * PAGX Viewer MVP for WeChat Miniprogram
 */

import {
  PAGXInit,
} from '../../utils/pagx-viewer';
import { WXGestureManager } from '../../utils/gesture-manager';
import { PerformanceMonitor } from '../../utils/performance-monitor';

// PAGX sample files configuration
const SAMPLE_FILES = [
  { 
    name: 'Guidelines', 
    url: 'https://pag.io/wx_pagx_demo/Guidelines.pagx'
  },
  { 
    name: 'ColorPicker', 
    url: 'https://pag.io/wx_pagx_demo/ColorPicker.pagx'
  },
  { 
    name: 'Baseline', 
    url: 'https://pag.io/wx_pagx_demo/Baseline.pagx'
  },
  { 
    name: 'Overview', 
    url: 'https://pag.io/wx_pagx_demo/Overview.pagx'
  },
];

Page({
  data: {
    loading: true,
    samples: SAMPLE_FILES,
    sampleNames: SAMPLE_FILES.map(item => item.name),
    currentIndex: 0,
    loadingFile: false,
    
    // Performance monitoring
    showPerf: false,
    perfStats: {
      fps: 0,
      avgFPS: 0,
      frameTime: 0,
      dropRate: 0,
      smoothness: 0,
      quality: 'N/A'
    }
  },

  // State
  View: null,
  module: null,
  canvas: null,
  animationFrameId: 0,
  gestureManager: null,
  perfMonitor: null,
  dpr: 2,
  gestureJustStarted: false,

  async onLoad(options) {
    try {
      // Get device pixel ratio for converting logical pixels to physical pixels
      this.dpr = wx.getSystemInfoSync().pixelRatio || 2;
      
      // Create gesture manager
      this.gestureManager = new WXGestureManager();
      
      // Bind zoom event listeners
      this.gestureManager.on('zoomStart', this.onZoomStart.bind(this));
      this.gestureManager.on('zoomEnd', this.onZoomEnd.bind(this));
      
      // Create performance monitor
      this.perfMonitor = new PerformanceMonitor();
      this.perfMonitor.onStatsUpdate = (stats) => {
        this.updatePerfStats(stats);
      };
      
      // Support custom file index from query params
      if (options && options.index) {
        const index = parseInt(options.index);
        if (index >= 0 && index < SAMPLE_FILES.length) {
          this.setData({ currentIndex: index });
        }
      }
      
      await this.initializeViewer();
      await this.loadCurrentFile();
      
      // Initialize gesture manager with canvas (physical pixels) and content dimensions
      const contentWidth = this.View.contentWidth();
      const contentHeight = this.View.contentHeight();
      const initState = this.gestureManager.init(
        this.canvas.width,   // physical pixels (rect.width * dpr)
        this.canvas.height,  // physical pixels (rect.height * dpr)
        contentWidth,
        contentHeight
      );
      if (!initState) {
        throw new Error('Failed to initialize gesture manager');
      }
      
      // Apply initial state to ensure C++ side is synchronized
      this.applyGestureState(initState);
      
      this.startRendering();
      this.setData({ loading: false });
    } catch (error) {
      console.error('Initialization failed:', error);
      wx.showModal({
        title: 'Error',
        content: 'Failed to initialize viewer: ' + error.message,
        showCancel: false
      });
    }
  },

  onUnload() {
    this.stopRendering();
    
    if (this.gestureManager) {
      this.gestureManager.destroy();
      this.gestureManager = null;
    }
    
    if (this.View) {
      this.View.destroy();
      this.View = null;
    }
  },

  async initializeViewer() {
    // Load WASM module
    this.module = await PAGXInit({
      locateFile: (file) => '/utils/' + file
    });
    
    // Get canvas instance and size
    return new Promise((resolve, reject) => {
      const query = wx.createSelectorQuery();
      query.select('#pagx-canvas')
        .node()
        .exec(async (res) => {
          try {
            if (!res || !res[0]) {
              throw new Error('Failed to get canvas node');
            }
            
            this.canvas = res[0].node;
            
            // Get canvas display size
            const query2 = wx.createSelectorQuery();
            query2.select('#pagx-canvas')
              .boundingClientRect()
              .exec(async (rectRes) => {
                try {
                  if (!rectRes || !rectRes[0]) {
                    throw new Error('Failed to get canvas rect');
                  }
                  
                  const rect = rectRes[0];
                  
                  // Set canvas physical pixel size based on display size and device pixel ratio
                  // This ensures sharp rendering on high-DPI displays
                  const dpr = wx.getSystemInfoSync().pixelRatio || 2;
                  this.canvas.width = Math.floor(rect.width * dpr);
                  this.canvas.height = Math.floor(rect.height * dpr);
                  
                  // Create View
                  this.View = await this.module.View.init(this.module, this.canvas, {
                    useScale: false,
                    firstFrame: false
                  });
                  
                  if (!this.View) {
                    throw new Error('Failed to create View');
                  }
                  
                  resolve();
                } catch (error) {
                  reject(error);
                }
              });
          } catch (error) {
            reject(error);
          }
        });
    });
  },

  async loadCurrentFile() {
    const { currentIndex } = this.data;
    const sample = SAMPLE_FILES[currentIndex];
    
    // Download from CDN
    const data = await this.downloadFile(sample.url);
    
    // Load into View
    this.View.loadPAGX(data);
    
    // Re-initialize gesture manager with new content dimensions
    // NOTE: init() automatically resets all transforms and returns the reset state
    const contentWidth = this.View.contentWidth();
    const contentHeight = this.View.contentHeight();
    const resetState = this.gestureManager.init(
      this.canvas.width,   // physical pixels
      this.canvas.height,  // physical pixels
      contentWidth,
      contentHeight
    );
    
    // Apply reset state to synchronize C++ side
    if (resetState) {
      this.applyGestureState(resetState);
    }
  },

  async switchFile(index) {
    if (index === this.data.currentIndex || !this.View) {
      return;
    }

    try {
      // Update UI (non-blocking) and load file (parallel)
      this.setData({ 
        loadingFile: true,
        currentIndex: index 
      });
      
      // Load new file immediately (don't wait for UI render)
      await this.loadCurrentFile();
      
      wx.showToast({
        title: 'Loaded',
        icon: 'success',
        duration: 1000
      });
    } catch (error) {
      console.error('Failed to switch file:', error);
      wx.showModal({
        title: 'Error',
        content: 'Failed to load file: ' + error.message,
        showCancel: false
      });
    } finally {
      this.setData({ loadingFile: false });
    }
  },

  downloadFile(url) {
    return new Promise((resolve, reject) => {
      wx.showLoading({ title: 'Downloading...' });
      
      wx.request({
        url: url,
        responseType: 'arraybuffer',
        success: (res) => {
          wx.hideLoading();
          if (res.statusCode === 200) {
            const uint8Array = new Uint8Array(res.data);
            resolve(uint8Array);
          } else {
            reject(new Error(`HTTP ${res.statusCode}`));
          }
        },
        fail: (err) => {
          wx.hideLoading();
          reject(new Error(`Download failed: ${err.errMsg}`));
        }
      });
    });
  },

  resetTransform() {
    if (!this.gestureManager || !this.View) {
      return;
    }
    
    const state = this.gestureManager.reset();
    this.applyGestureState(state);
  },

  applyGestureState(state) {
    if (!state || state.action === 'none') {
      return;
    }
    
    if (!this.View) {
      return;
    }
    
    // Always update C++ side immediately for smooth rendering
    try {
      this.View.updateZoomScaleAndOffset(state.zoom, state.offsetX, state.offsetY);
    } catch (error) {
      return;
    }
  },

  startRendering() {
    const render = () => {
      if (!this.View) return;

      this.View.draw();
      
      // Record frame for performance monitoring
      if (this.perfMonitor && this.perfMonitor.enabled) {
        this.perfMonitor.recordFrame();
        
        // Mark first frame after gesture start
        if (this.gestureJustStarted) {
          this.perfMonitor.onGestureFirstFrame();
          this.gestureJustStarted = false;
        }
      }
      
      // Use Canvas.requestAnimationFrame in WeChat Miniprogram
      if (this.canvas && this.canvas.requestAnimationFrame) {
        this.animationFrameId = this.canvas.requestAnimationFrame(render);
      } else {
        // Fallback to setTimeout if canvas not ready
        this.animationFrameId = setTimeout(render, 16);
      }
    };
    render();
  },

  stopRendering() {
    if (this.animationFrameId) {
      if (this.canvas && this.canvas.cancelAnimationFrame) {
        this.canvas.cancelAnimationFrame(this.animationFrameId);
      } else {
        clearTimeout(this.animationFrameId);
      }
      this.animationFrameId = 0;
    }
  },

  // Touch Events
  onTouchStart(e) {
    if (!this.gestureManager) return;
    
    // Notify performance monitor
    if (this.perfMonitor && this.perfMonitor.enabled) {
      this.perfMonitor.onGestureStart();
      this.gestureJustStarted = true;
    }
    
    // Pass dpr to convert touch coordinates (logical pixels) to physical pixels
    const state = this.gestureManager.onTouchStart(e.touches, this.dpr);
    this.applyGestureState(state);
  },

  onTouchMove(e) {
    if (!this.gestureManager) return;
    // Pass dpr to convert touch coordinates (logical pixels) to physical pixels
    const state = this.gestureManager.onTouchMove(e.touches, this.dpr);
    this.applyGestureState(state);
  },

  onTouchEnd(e) {
    if (!this.gestureManager) return;
    // IMPORTANT: Pass e.touches (remaining touches), not e.changedTouches (ended touches)
    const state = this.gestureManager.onTouchEnd(e.touches);
    this.applyGestureState(state);
    
    // Notify performance monitor
    if (this.perfMonitor && this.perfMonitor.enabled && e.touches.length === 0) {
      this.perfMonitor.onGestureEnd();
      this.gestureJustStarted = false;
    }
  },

  // Zoom event handlers
  onZoomStart(state) {
    this.applyGestureState(state);
  },

  onZoomEnd(state) {
    if (!this.View) return;
    
    try {
      // Notify C++ that zoom gesture ended
      this.View.onZoomEnd();
    } catch (error) {
      // Silently ignore errors
    }
    },

  // Reset Button
  onReset() {
    // Prevent multiple rapid clicks
    if (this.data.loadingFile) {
      return;
    }
    
    this.resetTransform();
    wx.showToast({
      title: 'Reset',
      icon: 'success',
      duration: 1000
    });
  },

  // Picker Selection
  onPickerChange(e) {
    const index = parseInt(e.detail.value);
    if (index !== this.data.currentIndex) {
      this.switchFile(index);
    }
  },
  
  // Performance Monitoring
  togglePerfMonitor() {
    const newShowPerf = !this.data.showPerf;
    this.setData({ showPerf: newShowPerf });
    
    if (newShowPerf && this.perfMonitor) {
      this.perfMonitor.reset();
      this.perfMonitor.start();
    } else if (this.perfMonitor) {
      this.perfMonitor.stop();
      this.perfMonitor.logStats();
    }
  },
  
  updatePerfStats(stats) {
    this.setData({
      perfStats: {
        fps: stats.currentFPS,
        avgFPS: stats.averageFPS,
        frameTime: stats.averageFrameTime,
        dropRate: stats.droppedFrameRate,
        smoothness: stats.smoothness,
        quality: this.perfMonitor.getSummary().quality
      }
    });
  },
});
