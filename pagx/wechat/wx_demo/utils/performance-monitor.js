/**
 * Copyright (C) 2026 Tencent. All Rights Reserved.
 * Performance monitoring utility for measuring rendering smoothness
 */

class PerformanceMonitor {
  constructor() {
    this.enabled = false;
    this.startTime = 0;
    this.lastFrameTime = 0;
    this.frameTimes = [];
    this.gestureStartTime = 0;
    this.gestureFrameTimes = [];
    
    // Statistics
    this.currentFPS = 0;
    this.averageFPS = 0;
    this.minFPS = Infinity;
    this.maxFPS = 0;
    this.averageFrameTime = 0;
    this.maxFrameTime = 0;
    this.droppedFrames = 0;
    this.totalFrames = 0;
    this.validFrames = 0; // Only count frames used in calculations
    
    // Gesture-specific metrics
    this.gestureResponseTime = 0;
    this.gestureAverageFPS = 0;
    this.gestureMinFPS = Infinity;
    
    // Config
    this.targetFrameTime = 1000 / 60; // 60 FPS = ~16.67ms per frame
    this.droppedFrameThreshold = this.targetFrameTime * 1.5; // 25ms
    this.sampleWindow = 60; // Keep last 60 frame times for rolling average
    this.minValidFrameTime = 5; // Minimum frame time to consider valid (ms)
    
    // Callbacks
    this.onStatsUpdate = null;
  }
  
  start() {
    this.enabled = true;
    this.reset();
  }
  
  stop() {
    this.enabled = false;
  }
  
  reset() {
    this.startTime = Date.now();
    this.lastFrameTime = this.startTime;
    this.frameTimes = [];
    this.gestureFrameTimes = [];
    this.minFPS = Infinity;
    this.maxFPS = 0;
    this.droppedFrames = 0;
    this.totalFrames = 0;
    this.validFrames = 0;
    this.currentFPS = 0;
    this.averageFPS = 0;
    this.gestureMinFPS = Infinity;
    this.gestureAverageFPS = 0;
    this.gestureResponseTime = 0;
  }
  
  recordFrame() {
    if (!this.enabled) return;
    
    const now = Date.now();
    const frameTime = now - this.lastFrameTime;
    
    this.totalFrames++;
    
    // Always update lastFrameTime to avoid accumulating errors
    this.lastFrameTime = now;
    
    // Skip first frame (initialization timing is unreliable)
    if (this.totalFrames === 1) {
      return;
    }
    
    // Skip abnormally small intervals (< 5ms) to avoid timer precision issues
    // But don't block forever - if we get 10+ frames, start accepting them
    if (frameTime < this.minValidFrameTime && this.totalFrames < 10) {
      return;
    }
    
    this.validFrames++;
    
    // Update frame timing array
    this.frameTimes.push(frameTime);
    if (this.frameTimes.length > this.sampleWindow) {
      this.frameTimes.shift();
    }
    
    // Track dropped frames (frame time > 25ms means dropped frames)
    if (frameTime > this.droppedFrameThreshold) {
      this.droppedFrames++;
    }
    
    // Calculate current FPS (instantaneous) with reasonable cap
    // Cap at 120 FPS (most displays are 60-120Hz)
    this.currentFPS = Math.min(120, Math.round(1000 / frameTime));
    
    // Track min/max FPS
    if (this.currentFPS < this.minFPS) {
      this.minFPS = this.currentFPS;
    }
    if (this.currentFPS > this.maxFPS) {
      this.maxFPS = this.currentFPS;
    }
    
    // Calculate average FPS based on valid frames only
    const elapsed = now - this.startTime;
    if (elapsed > 0 && this.validFrames > 0) {
      this.averageFPS = Math.min(120, Math.round((this.validFrames * 1000) / elapsed));
    }
    
    // Calculate average and max frame time
    if (this.frameTimes.length > 0) {
      const sum = this.frameTimes.reduce((a, b) => a + b, 0);
      this.averageFrameTime = Math.round(sum / this.frameTimes.length * 10) / 10;
      this.maxFrameTime = Math.round(Math.max(...this.frameTimes) * 10) / 10;
    }
    
    // Warn when FPS drops below 30
    if (this.currentFPS < 30 && this.validFrames > 10) {
      console.warn(`[Performance] Low FPS detected: ${this.currentFPS} (frameTime: ${frameTime}ms)`);
    }
    
    // Gesture-specific tracking
    if (this.gestureStartTime > 0) {
      this.gestureFrameTimes.push(frameTime);
      
      // Calculate gesture instantaneous FPS with cap
      const gestureFPS = Math.min(120, Math.round(1000 / frameTime));
      if (gestureFPS < this.gestureMinFPS) {
        this.gestureMinFPS = gestureFPS;
      }
      
      // Calculate gesture average FPS
      const gestureElapsed = now - this.gestureStartTime;
      if (gestureElapsed > 0 && this.gestureFrameTimes.length > 0) {
        this.gestureAverageFPS = Math.min(120, Math.round((this.gestureFrameTimes.length * 1000) / gestureElapsed));
      }
    }
    
    // Trigger callback every 10 valid frames
    if (this.onStatsUpdate && this.validFrames % 10 === 0) {
      this.onStatsUpdate(this.getStats());
    }
  }
  
  onGestureStart() {
    if (!this.enabled) return;
    
    this.gestureStartTime = Date.now();
    this.gestureFrameTimes = [];
    this.gestureMinFPS = Infinity;
    this.gestureResponseTime = 0;
  }
  
  onGestureFirstFrame() {
    if (!this.enabled || this.gestureStartTime === 0) return;
    
    // Measure time from gesture start to first rendered frame
    this.gestureResponseTime = Date.now() - this.gestureStartTime;
  }
  
  onGestureEnd() {
    if (!this.enabled) return;
    
    this.gestureStartTime = 0;
    
    // Trigger final gesture stats
    if (this.onStatsUpdate) {
      this.onStatsUpdate(this.getStats());
    }
  }
  
  getStats() {
    const droppedFrameRate = this.validFrames > 0 
      ? Math.round((this.droppedFrames / this.validFrames) * 100)
      : 0;
    
    return {
      // Overall metrics
      currentFPS: this.currentFPS,
      averageFPS: this.averageFPS,
      minFPS: this.minFPS === Infinity ? 0 : this.minFPS,
      maxFPS: this.maxFPS,
      
      // Frame timing
      averageFrameTime: this.averageFrameTime,
      maxFrameTime: this.maxFrameTime,
      targetFrameTime: this.targetFrameTime,
      
      // Frame drops
      droppedFrames: this.droppedFrames,
      totalFrames: this.totalFrames,
      validFrames: this.validFrames,
      droppedFrameRate: droppedFrameRate,
      
      // Gesture-specific
      gestureResponseTime: this.gestureResponseTime,
      gestureAverageFPS: this.gestureAverageFPS,
      gestureMinFPS: this.gestureMinFPS === Infinity ? 0 : this.gestureMinFPS,
      isGestureActive: this.gestureStartTime > 0,
      
      // Smoothness rating (0-100)
      smoothness: this.calculateSmoothness()
    };
  }
  
  calculateSmoothness() {
    // Calculate smoothness score based on multiple factors
    // 100 = perfectly smooth, 0 = very stuttery
    
    if (this.validFrames < 10) {
      return 100; // Not enough data
    }
    
    // Factor 1: Average FPS (40% weight)
    // Perfect score at 60+ FPS, linear scale below
    const fpsScore = Math.min(100, (this.averageFPS / 60) * 100);
    
    // Factor 2: Frame drops (30% weight)
    // Perfect score at 0%, linearly decrease (2% drop = -4 points)
    const dropRate = this.droppedFrames / this.validFrames;
    const dropScore = Math.max(0, 100 - dropRate * 100 * 2);
    
    // Factor 3: Frame time variance (30% weight)
    // Low variance = smooth, high variance = jittery
    let varianceScore = 100;
    if (this.frameTimes.length > 1) {
      const stdDev = this.calculateStdDev(this.frameTimes);
      // Standard deviation > 5ms indicates noticeable jitter
      // Linearly penalize: 5ms stdDev = 50 points, 10ms = 0 points
      varianceScore = Math.max(0, 100 - (stdDev / 10) * 100);
    }
    
    // Weighted average
    const score = fpsScore * 0.4 + dropScore * 0.3 + varianceScore * 0.3;
    
    return Math.round(score);
  }
  
  calculateStdDev(values) {
    if (values.length === 0) return 0;
    
    const mean = values.reduce((a, b) => a + b, 0) / values.length;
    const squaredDiffs = values.map(value => Math.pow(value - mean, 2));
    const variance = squaredDiffs.reduce((a, b) => a + b, 0) / values.length;
    return Math.sqrt(variance);
  }
  
  getSummary() {
    const stats = this.getStats();
    
    let quality = 'Excellent';
    if (stats.smoothness < 90) quality = 'Good';
    if (stats.smoothness < 75) quality = 'Fair';
    if (stats.smoothness < 60) quality = 'Poor';
    if (stats.smoothness < 40) quality = 'Very Poor';
    
    return {
      quality: quality,
      smoothness: stats.smoothness,
      averageFPS: stats.averageFPS,
      droppedFrameRate: stats.droppedFrameRate,
      gestureAverageFPS: stats.gestureAverageFPS,
      gestureResponseTime: stats.gestureResponseTime
    };
  }
  
  logStats() {
    const stats = this.getStats();
    console.log('=== Performance Statistics ===');
    console.log(`FPS: ${stats.currentFPS} (avg: ${stats.averageFPS}, min: ${stats.minFPS}, max: ${stats.maxFPS})`);
    console.log(`Frame Time: ${stats.averageFrameTime}ms (max: ${stats.maxFrameTime}ms, target: ${stats.targetFrameTime}ms)`);
    console.log(`Frames: ${stats.validFrames} valid / ${stats.totalFrames} total`);
    console.log(`Dropped Frames: ${stats.droppedFrames}/${stats.validFrames} (${stats.droppedFrameRate}%)`);
    console.log(`Gesture Response: ${stats.gestureResponseTime}ms`);
    console.log(`Gesture FPS: ${stats.gestureAverageFPS} (min: ${stats.gestureMinFPS})`);
    console.log(`Smoothness Score: ${stats.smoothness}/100 (${this.getSummary().quality})`);
  }
}

export { PerformanceMonitor };
