export enum WorkerMessageType {
  PAGInit = 'PAGInit',
  // PAGView static methods
  PAGView_init = 'PAGView.init',
  // PAGView instance methods
  PAGView_duration = 'PAGView.duration',
  PAGView_play = 'PAGView.play',
  PAGView_pause = 'PAGView.pause',
  PAGView_stop = 'PAGView.stop',
  PAGView_setRepeatCount = 'PAGView.setRepeatCount',
  PAGView_getProgress = 'PAGView.getProgress',
  PAGView_currentFrame = 'PAGView.currentFrame',
  PAGView_setProgress = 'PAGView.setProgress',
  PAGView_scaleMode = 'PAGView.scaleMode',
  PAGView_setScaleMode = 'PAGView.setScaleMode',
  PAGView_flush = 'PAGView.flush',
  PAGView_getDebugData = 'PAGView.getDebugData',
  PAGView_destroy = 'PAGView.destroy',
  // PAGFile static methods
  PAGFile_load = 'PAGFile.load',
  // PAGFile instance methods
  PAGFile_getTextData = 'PAGFile.getTextData',
  PAGFile_replaceText = 'PAGFile.replaceText',
  PAGFile_replaceImage = 'PAGFile.replaceImage',
  PAGFile_destroy = 'PAGFile.destroy',
  // PAGImage static methods
  PAGImage_fromSource = 'PAGImage.fromSource',
  PAGImage_destroy = 'PAGImage.destroy',
  // VideoReader static methods
  VideoReader_constructor = 'VideoReader.constructor',
  // VideoReader instance methods
  VideoReader_prepare = 'VideoReader.prepare',
  VideoReader_play = 'VideoReader.play',
  VideoReader_pause = 'VideoReader.pause',
  VideoReader_stop = 'VideoReader.stop',
  VideoReader_getError = 'VideoReader.getError',
  // Binding video reader
  PAGModule_linkVideoReader = 'PAGModule.linkVideoReader',
  // TextDocument instance methods
  TextDocument_delete = 'TextDocument.delete',
}
