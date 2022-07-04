export class AudioPlayer {
  private audioEl: HTMLAudioElement | null = null;
  private isEffective = true;
  private isDestroyed = false;

  public constructor(audioBytes: Uint8Array | null) {
    if (audioBytes && audioBytes.byteLength > 0) {
      this.audioEl = document.createElement('audio');
      this.audioEl.style.display = 'none';
      this.audioEl.controls = true;
      this.audioEl.preload = 'auto';
      const blob = new Blob([audioBytes], { type: 'audio/mp3' });
      this.audioEl.src = URL.createObjectURL(blob);
      this.isDestroyed = false;
    } else {
      this.isEffective = false;
    }
    console.log(`${audioBytes && audioBytes.byteLength > 0 ? '有音频文件' : '无音频文件'}`);
  }
  public play() {
    if (!this.isEffective || this.isDestroyed) return;
    this.audioEl?.play();
    console.log('音频播放');
  }
  public pause() {
    if (!this.isEffective || this.isDestroyed) return;
    this.audioEl?.pause();
    console.log('音频暂停');
  }
  public stop() {
    if (!this.isEffective || this.isDestroyed) return;
    this.audioEl!.currentTime = 0;
    this.audioEl?.pause();
    console.log('音频停止');
  }
  public destroy() {
    if (!this.isEffective || this.isDestroyed) return;
    console.log('音频销毁');
    this.audioEl?.pause();
    this.audioEl = null;
    this.isDestroyed = true;
  }
}
