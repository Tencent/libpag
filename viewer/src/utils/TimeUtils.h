#ifndef UTILS_TIMEUTILS_H_
#define UTILS_TIMEUTILS_H_

#include <pag/pag.h>

inline pag::Frame TimeToFrame(int64_t time, float frameRate) {
  return static_cast<pag::Frame>(floor(time * frameRate / 1000000ll));
}

inline int64_t FrameToTime(pag::Frame frame, float frameRate) {
  return static_cast<pag::Frame>(ceil(frame * 1000000ll / frameRate));
}

inline pag::Frame ProgressToFrame(double progress, pag::Frame totalFrames) {
  if (totalFrames <= 1) {
    return 0;
  }
  auto percent = fmod(progress, 1.0);
  if ((percent <= 0) && (progress != 0)) {
    percent += 1.0;
  }

  auto currentFrame = static_cast<pag::Frame>(floor(percent * static_cast<double>(totalFrames)));
  return currentFrame == totalFrames ? totalFrames - 1 : currentFrame;
}

inline double FrameToProgress(pag::Frame currentFrame, pag::Frame totalFrames) {
  if (totalFrames <= 1 || currentFrame < 0) {
    return 0;
  }
  if (currentFrame >= totalFrames - 1) {
    return 1;
  }

  return (currentFrame * 1.0 + 0.1) / totalFrames;
}

#endif // UTILS_TIMEUTILS_H_