import { Log } from '../../src/utils/log';
export class AudioPlayer {
  private audioEl: HTMLAudioElement = null;
  private isEffective = true;
  private isDestroyed = false;

  public constructor(audioBytes: Uint8Array) {
    if (audioBytes.byteLength > 0){
      this.audioEl = document.createElement('audio');
      this.audioEl.style.display = 'none';
      this.audioEl.controls = true;
      this.audioEl.preload = 'preload';
      const blob = new Blob([audioBytes], { type: 'audio/mp3' });
      this.audioEl.src = URL.createObjectURL(blob);
      this.isDestroyed = false
    } else {
      this.isEffective = false;
    }
    Log.log(`${audioBytes.byteLength === 0 ? '无音频文件' : '有音频文件'}`)
  }
  public play() {
    if (!this.isEffective || this.isDestroyed) return;
    this.audioEl.play();
    Log.log('音频播放');
  }
  public pause() {
    if (!this.isEffective || this.isDestroyed) return;
    this.audioEl.pause();
    Log.log('音频暂停');
  }
  public stop() {
    if (!this.isEffective || this.isDestroyed) return;
    this.audioEl.currentTime = 0;
    this.audioEl.pause();
    Log.log('音频停止');
  }
  public destroy() {
    if (!this.isEffective || this.isDestroyed) return;
    Log.log('音频销毁');
    this.audioEl.pause();
    this.audioEl = null;
    this.isDestroyed = true
  }
}
