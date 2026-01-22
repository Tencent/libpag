/**
 * Copyright (C) 2026 Tencent. All Rights Reserved.
 * PAGX Viewer MVP for WeChat Miniprogram
 */

import {
  PAGXInit,
} from '../../utils/pagx-viewer';
import { WXGestureManager } from '../../utils/gesture-manager';

// PAGX sample files configuration
const SAMPLE_FILES = [
  { 
    name: 'ColorPicker', 
    url: 'https://pag.io/pagx/samples/ColorPicker.pagx'
  },
  { 
    name: 'complex3', 
    url: 'https://pag.io/pagx/testFiles/complex3.pagx'
  },
  { 
    name: 'complex6', 
    url: 'https://pag.io/pagx/testFiles/complex6.pagx'
  },
  { 
    name: 'path', 
    url: 'https://pag.io/pagx/testFiles/path.pagx'
  },
  { 
    name: 'refStyle', 
    url: 'https://pag.io/pagx/testFiles/refStyle.pagx'
  }
];

Page({
  data: {
    loading: true,
    zoom: 1.0,
    offsetX: 0,
    offsetY: 0,
    samples: SAMPLE_FILES,
    sampleNames: SAMPLE_FILES.map(item => item.name),
    currentIndex: 0,
    loadingFile: false
  },

  // State
  View: null,
  module: null,
  canvas: null,
  animationFrameId: 0,
  gestureManager: null,
  dpr: 2,
  
  // UI update throttling
  lastUIUpdateTime: 0,
  pendingUIUpdate: null,
  UI_UPDATE_INTERVAL: 100, // Update UI at most every 100ms

  async onLoad(options) {
    try {
      // Get device pixel ratio for converting logical pixels to physical pixels
      this.dpr = wx.getSystemInfoSync().pixelRatio || 2;
      
      // Create gesture manager
      this.gestureManager = new WXGestureManager();
      
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
    
    // Clear pending UI update
    if (this.pendingUIUpdate) {
      clearTimeout(this.pendingUIUpdate);
      this.pendingUIUpdate = null;
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
      console.error('Failed to update C++ view:', error);
      return;
    }
    
    // Throttle UI updates to avoid setData flooding
    const now = Date.now();
    
    if (state.action === 'reset') {
      // Reset should update UI immediately
      this.updateUIState(state);
      return;
    }
    
    // For 'update' action, throttle UI updates
    if (now - this.lastUIUpdateTime >= this.UI_UPDATE_INTERVAL) {
      this.updateUIState(state);
      this.lastUIUpdateTime = now;
      
      // Clear any pending update since we just updated
      if (this.pendingUIUpdate) {
        clearTimeout(this.pendingUIUpdate);
        this.pendingUIUpdate = null;
      }
    } else {
      // Schedule a delayed update if not already scheduled
      if (!this.pendingUIUpdate) {
        this.pendingUIUpdate = setTimeout(() => {
          this.updateUIState(state);
          this.lastUIUpdateTime = Date.now();
          this.pendingUIUpdate = null;
        }, this.UI_UPDATE_INTERVAL - (now - this.lastUIUpdateTime));
      }
    }
  },
  
  updateUIState(state) {
    this.setData({
      zoom: state.zoom.toFixed(2),
      offsetX: Math.round(state.offsetX),
      offsetY: Math.round(state.offsetY)
    });
  },

  startRendering() {
    const render = () => {
      if (!this.View) return;
      this.View.draw();
      
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
});
