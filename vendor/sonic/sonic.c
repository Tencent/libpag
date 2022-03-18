/* Sonic library
   Copyright 2010
   Bill Cox
   This file is part of the Sonic Library.

   This file is licensed under the Apache 2.0 license.
*/

#include "sonic.h"

#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> // for windows - int16_t

/*
    The following code was used to generate the following sinc lookup table.

    #include <limits.h>
    #include <math.h>
    #include <stdio.h>

    double findHannWeight(int N, double x) {
        return 0.5*(1.0 - cos(2*M_PI*x/N));
    }

    double findSincCoefficient(int N, double x) {
        double hannWindowWeight = findHannWeight(N, x);
        double sincWeight;

        x -= N/2.0;
        if (x > 1e-9 || x < -1e-9) {
            sincWeight = sin(M_PI*x)/(M_PI*x);
        } else {
            sincWeight = 1.0;
        }
        return hannWindowWeight*sincWeight;
    }

    int main() {
        double x;
        int i;
        int N = 12;

        for (i = 0, x = 0.0; x <= N; x += 0.02, i++) {
            printf("%u %d\n", i, (int)(SHRT_MAX*findSincCoefficient(N, x)));
        }
        return 0;
    }
*/

/* The number of points to use in the sinc FIR filter for resampling. */
#define SINC_FILTER_POINTS \
  12 /* I am not able to hear improvement with higher N. */
#define SINC_TABLE_SIZE 601

/* Lookup table for windowed sinc function of SINC_FILTER_POINTS points. */
static int16_t sincTable[SINC_TABLE_SIZE] = {
    0,     0,     0,     0,     0,     0,     0,     -1,    -1,    -2,    -2,
    -3,    -4,    -6,    -7,    -9,    -10,   -12,   -14,   -17,   -19,   -21,
    -24,   -26,   -29,   -32,   -34,   -37,   -40,   -42,   -44,   -47,   -48,
    -50,   -51,   -52,   -53,   -53,   -53,   -52,   -50,   -48,   -46,   -43,
    -39,   -34,   -29,   -22,   -16,   -8,    0,     9,     19,    29,    41,
    53,    65,    79,    92,    107,   121,   137,   152,   168,   184,   200,
    215,   231,   247,   262,   276,   291,   304,   317,   328,   339,   348,
    357,   363,   369,   372,   374,   375,   373,   369,   363,   355,   345,
    332,   318,   300,   281,   259,   234,   208,   178,   147,   113,   77,
    39,    0,     -41,   -85,   -130,  -177,  -225,  -274,  -324,  -375,  -426,
    -478,  -530,  -581,  -632,  -682,  -731,  -779,  -825,  -870,  -912,  -951,
    -989,  -1023, -1053, -1080, -1104, -1123, -1138, -1149, -1154, -1155, -1151,
    -1141, -1125, -1105, -1078, -1046, -1007, -963,  -913,  -857,  -796,  -728,
    -655,  -576,  -492,  -403,  -309,  -210,  -107,  0,     111,   225,   342,
    462,   584,   708,   833,   958,   1084,  1209,  1333,  1455,  1575,  1693,
    1807,  1916,  2022,  2122,  2216,  2304,  2384,  2457,  2522,  2579,  2625,
    2663,  2689,  2706,  2711,  2705,  2687,  2657,  2614,  2559,  2491,  2411,
    2317,  2211,  2092,  1960,  1815,  1658,  1489,  1308,  1115,  912,   698,
    474,   241,   0,     -249,  -506,  -769,  -1037, -1310, -1586, -1864, -2144,
    -2424, -2703, -2980, -3254, -3523, -3787, -4043, -4291, -4529, -4757, -4972,
    -5174, -5360, -5531, -5685, -5819, -5935, -6029, -6101, -6150, -6175, -6175,
    -6149, -6096, -6015, -5905, -5767, -5599, -5401, -5172, -4912, -4621, -4298,
    -3944, -3558, -3141, -2693, -2214, -1705, -1166, -597,  0,     625,   1277,
    1955,  2658,  3386,  4135,  4906,  5697,  6506,  7332,  8173,  9027,  9893,
    10769, 11654, 12544, 13439, 14335, 15232, 16128, 17019, 17904, 18782, 19649,
    20504, 21345, 22170, 22977, 23763, 24527, 25268, 25982, 26669, 27327, 27953,
    28547, 29107, 29632, 30119, 30569, 30979, 31349, 31678, 31964, 32208, 32408,
    32565, 32677, 32744, 32767, 32744, 32677, 32565, 32408, 32208, 31964, 31678,
    31349, 30979, 30569, 30119, 29632, 29107, 28547, 27953, 27327, 26669, 25982,
    25268, 24527, 23763, 22977, 22170, 21345, 20504, 19649, 18782, 17904, 17019,
    16128, 15232, 14335, 13439, 12544, 11654, 10769, 9893,  9027,  8173,  7332,
    6506,  5697,  4906,  4135,  3386,  2658,  1955,  1277,  625,   0,     -597,
    -1166, -1705, -2214, -2693, -3141, -3558, -3944, -4298, -4621, -4912, -5172,
    -5401, -5599, -5767, -5905, -6015, -6096, -6149, -6175, -6175, -6150, -6101,
    -6029, -5935, -5819, -5685, -5531, -5360, -5174, -4972, -4757, -4529, -4291,
    -4043, -3787, -3523, -3254, -2980, -2703, -2424, -2144, -1864, -1586, -1310,
    -1037, -769,  -506,  -249,  0,     241,   474,   698,   912,   1115,  1308,
    1489,  1658,  1815,  1960,  2092,  2211,  2317,  2411,  2491,  2559,  2614,
    2657,  2687,  2705,  2711,  2706,  2689,  2663,  2625,  2579,  2522,  2457,
    2384,  2304,  2216,  2122,  2022,  1916,  1807,  1693,  1575,  1455,  1333,
    1209,  1084,  958,   833,   708,   584,   462,   342,   225,   111,   0,
    -107,  -210,  -309,  -403,  -492,  -576,  -655,  -728,  -796,  -857,  -913,
    -963,  -1007, -1046, -1078, -1105, -1125, -1141, -1151, -1155, -1154, -1149,
    -1138, -1123, -1104, -1080, -1053, -1023, -989,  -951,  -912,  -870,  -825,
    -779,  -731,  -682,  -632,  -581,  -530,  -478,  -426,  -375,  -324,  -274,
    -225,  -177,  -130,  -85,   -41,   0,     39,    77,    113,   147,   178,
    208,   234,   259,   281,   300,   318,   332,   345,   355,   363,   369,
    373,   375,   374,   372,   369,   363,   357,   348,   339,   328,   317,
    304,   291,   276,   262,   247,   231,   215,   200,   184,   168,   152,
    137,   121,   107,   92,    79,    65,    53,    41,    29,    19,    9,
    0,     -8,    -16,   -22,   -29,   -34,   -39,   -43,   -46,   -48,   -50,
    -52,   -53,   -53,   -53,   -52,   -51,   -50,   -48,   -47,   -44,   -42,
    -40,   -37,   -34,   -32,   -29,   -26,   -24,   -21,   -19,   -17,   -14,
    -12,   -10,   -9,    -7,    -6,    -4,    -3,    -2,    -2,    -1,    -1,
    0,     0,     0,     0,     0,     0,     0};

struct sonicStreamStruct {
#ifdef SONIC_SPECTROGRAM
  sonicSpectrogram spectrogram;
#endif  /* SONIC_SPECTROGRAM */
  int16_t* inputBuffer;
  int16_t* outputBuffer;
  int16_t* pitchBuffer;
  int16_t* downSampleBuffer;
  float speed;
  float volume;
  float pitch;
  float rate;
  int oldRatePosition;
  int newRatePosition;
  int useChordPitch;
  int quality;
  int numChannels;
  int inputBufferSize;
  int pitchBufferSize;
  int outputBufferSize;
  int numInputSamples;
  int numOutputSamples;
  int numPitchSamples;
  int minPeriod;
  int maxPeriod;
  int maxRequired;
  int remainingInputToCopy;
  int sampleRate;
  int prevPeriod;
  int prevMinDiff;
  float avePower;
};

#ifdef SONIC_SPECTROGRAM

/* Compute a spectrogram on the fly. */
void sonicComputeSpectrogram(sonicStream stream) {
  stream->spectrogram = sonicCreateSpectrogram(stream->sampleRate);
  /* Force changeSpeed to be called to compute the spectrogram. */
  sonicSetSpeed(stream, 2.0);
}

/* Get the spectrogram. */
sonicSpectrogram sonicGetSpectrogram(sonicStream stream) {
  return stream->spectrogram;
}

#endif

/* Scale the samples by the factor. */
static void scaleSamples(int16_t* samples, int numSamples, float volume) {
  int fixedPointVolume = volume * 4096.0f;
  int value;

  while (numSamples--) {
    value = (*samples * fixedPointVolume) >> 12;
    if (value > 32767) {
      value = 32767;
    } else if (value < -32767) {
      value = -32767;
    }
    *samples++ = value;
  }
}

/* Get the speed of the stream. */
float sonicGetSpeed(sonicStream stream) { return stream->speed; }

/* Set the speed of the stream. */
void sonicSetSpeed(sonicStream stream, float speed) { stream->speed = speed; }

/* Get the pitch of the stream. */
float sonicGetPitch(sonicStream stream) { return stream->pitch; }

/* Set the pitch of the stream. */
void sonicSetPitch(sonicStream stream, float pitch) { stream->pitch = pitch; }

/* Get the rate of the stream. */
float sonicGetRate(sonicStream stream) { return stream->rate; }

/* Set the playback rate of the stream. This scales pitch and speed at the same
   time. */
void sonicSetRate(sonicStream stream, float rate) {
  stream->rate = rate;

  stream->oldRatePosition = 0;
  stream->newRatePosition = 0;
}

/* Get the vocal chord pitch setting. */
int sonicGetChordPitch(sonicStream stream) { return stream->useChordPitch; }

/* Set the vocal chord mode for pitch computation.  Default is off. */
void sonicSetChordPitch(sonicStream stream, int useChordPitch) {
  stream->useChordPitch = useChordPitch;
}

/* Get the quality setting. */
int sonicGetQuality(sonicStream stream) { return stream->quality; }

/* Set the "quality".  Default 0 is virtually as good as 1, but very much
   faster. */
void sonicSetQuality(sonicStream stream, int quality) {
  stream->quality = quality;
}

/* Get the scaling factor of the stream. */
float sonicGetVolume(sonicStream stream) { return stream->volume; }

/* Set the scaling factor of the stream. */
void sonicSetVolume(sonicStream stream, float volume) {
  stream->volume = volume;
}

/* Free stream buffers. */
static void freeStreamBuffers(sonicStream stream) {
  if (stream->inputBuffer != NULL) {
    free(stream->inputBuffer);
  }
  if (stream->outputBuffer != NULL) {
    free(stream->outputBuffer);
  }
  if (stream->pitchBuffer != NULL) {
    free(stream->pitchBuffer);
  }
  if (stream->downSampleBuffer != NULL) {
    free(stream->downSampleBuffer);
  }
}

/* Destroy the sonic stream. */
void sonicDestroyStream(sonicStream stream) {
#ifdef SONIC_SPECTROGRAM
  if (stream->spectrogram != NULL) {
    sonicDestroySpectrogram(stream->spectrogram);
  }
#endif  /* SONIC_SPECTROGRAM */
  freeStreamBuffers(stream);
  free(stream);
}

/* Allocate stream buffers. */
static int allocateStreamBuffers(sonicStream stream, int sampleRate,
                                 int numChannels) {
  int minPeriod = sampleRate / SONIC_MAX_PITCH;
  int maxPeriod = sampleRate / SONIC_MIN_PITCH;
  int maxRequired = 2 * maxPeriod;

  stream->inputBufferSize = maxRequired;
  stream->inputBuffer =
      (int16_t*)calloc(maxRequired, sizeof(int16_t) * numChannels);
  if (stream->inputBuffer == NULL) {
    sonicDestroyStream(stream);
    return 0;
  }
  stream->outputBufferSize = maxRequired;
  stream->outputBuffer =
      (int16_t*)calloc(maxRequired, sizeof(int16_t) * numChannels);
  if (stream->outputBuffer == NULL) {
    sonicDestroyStream(stream);
    return 0;
  }
  stream->pitchBufferSize = maxRequired;
  stream->pitchBuffer =
      (int16_t*)calloc(maxRequired, sizeof(int16_t) * numChannels);
  if (stream->pitchBuffer == NULL) {
    sonicDestroyStream(stream);
    return 0;
  }
  stream->downSampleBuffer = (int16_t*)calloc(maxRequired, sizeof(int16_t));
  if (stream->downSampleBuffer == NULL) {
    sonicDestroyStream(stream);
    return 0;
  }
  stream->sampleRate = sampleRate;
  stream->numChannels = numChannels;
  stream->oldRatePosition = 0;
  stream->newRatePosition = 0;
  stream->minPeriod = minPeriod;
  stream->maxPeriod = maxPeriod;
  stream->maxRequired = maxRequired;
  stream->prevPeriod = 0;
  return 1;
}

/* Create a sonic stream.  Return NULL only if we are out of memory and cannot
   allocate the stream. */
sonicStream sonicCreateStream(int sampleRate, int numChannels) {
  sonicStream stream = (sonicStream)calloc(1, sizeof(struct sonicStreamStruct));

  if (stream == NULL) {
    return NULL;
  }
  if (!allocateStreamBuffers(stream, sampleRate, numChannels)) {
    return NULL;
  }
  stream->speed = 1.0f;
  stream->pitch = 1.0f;
  stream->volume = 1.0f;
  stream->rate = 1.0f;
  stream->oldRatePosition = 0;
  stream->newRatePosition = 0;
  stream->useChordPitch = 0;
  stream->quality = 0;
  stream->avePower = 50.0f;
  return stream;
}

/* Get the sample rate of the stream. */
int sonicGetSampleRate(sonicStream stream) { return stream->sampleRate; }

/* Set the sample rate of the stream.  This will cause samples buffered in the
   stream to be lost. */
void sonicSetSampleRate(sonicStream stream, int sampleRate) {
  freeStreamBuffers(stream);
  allocateStreamBuffers(stream, sampleRate, stream->numChannels);
}

/* Get the number of channels. */
int sonicGetNumChannels(sonicStream stream) { return stream->numChannels; }

/* Set the num channels of the stream.  This will cause samples buffered in the
   stream to be lost. */
void sonicSetNumChannels(sonicStream stream, int numChannels) {
  freeStreamBuffers(stream);
  allocateStreamBuffers(stream, stream->sampleRate, numChannels);
}

/* Enlarge the output buffer if needed. */
static int enlargeOutputBufferIfNeeded(sonicStream stream, int numSamples) {
  if (stream->numOutputSamples + numSamples > stream->outputBufferSize) {
    stream->outputBufferSize += (stream->outputBufferSize >> 1) + numSamples;
    stream->outputBuffer = (int16_t*)realloc(
        stream->outputBuffer,
        stream->outputBufferSize * sizeof(int16_t) * stream->numChannels);
    if (stream->outputBuffer == NULL) {
      return 0;
    }
  }
  return 1;
}

/* Enlarge the input buffer if needed. */
static int enlargeInputBufferIfNeeded(sonicStream stream, int numSamples) {
  if (stream->numInputSamples + numSamples > stream->inputBufferSize) {
    stream->inputBufferSize += (stream->inputBufferSize >> 1) + numSamples;
    stream->inputBuffer = (int16_t*)realloc(
        stream->inputBuffer,
        stream->inputBufferSize * sizeof(int16_t) * stream->numChannels);
    if (stream->inputBuffer == NULL) {
      return 0;
    }
  }
  return 1;
}

/* Add the input samples to the input buffer. */
static int addFloatSamplesToInputBuffer(sonicStream stream, float* samples,
                                        int numSamples) {
  int16_t* buffer;
  int count = numSamples * stream->numChannels;

  if (numSamples == 0) {
    return 1;
  }
  if (!enlargeInputBufferIfNeeded(stream, numSamples)) {
    return 0;
  }
  buffer = stream->inputBuffer + stream->numInputSamples * stream->numChannels;
  while (count--) {
    *buffer++ = (*samples++) * 32767.0f;
  }
  stream->numInputSamples += numSamples;
  return 1;
}

/* Add the input samples to the input buffer. */
static int addShortSamplesToInputBuffer(sonicStream stream, int16_t* samples,
                                        int numSamples) {
  if (numSamples == 0) {
    return 1;
  }
  if (!enlargeInputBufferIfNeeded(stream, numSamples)) {
    return 0;
  }
  memcpy(stream->inputBuffer + stream->numInputSamples * stream->numChannels,
         samples, numSamples * sizeof(int16_t) * stream->numChannels);
  stream->numInputSamples += numSamples;
  return 1;
}

/* Add the input samples to the input buffer. */
static int addUnsignedCharSamplesToInputBuffer(sonicStream stream,
                                               unsigned char* samples,
                                               int numSamples) {
  int16_t* buffer;
  int count = numSamples * stream->numChannels;

  if (numSamples == 0) {
    return 1;
  }
  if (!enlargeInputBufferIfNeeded(stream, numSamples)) {
    return 0;
  }
  buffer = stream->inputBuffer + stream->numInputSamples * stream->numChannels;
  while (count--) {
    *buffer++ = (*samples++ - 128) << 8;
  }
  stream->numInputSamples += numSamples;
  return 1;
}

/* Remove input samples that we have already processed. */
static void removeInputSamples(sonicStream stream, int position) {
  int remainingSamples = stream->numInputSamples - position;

  if (remainingSamples > 0) {
    memmove(stream->inputBuffer,
            stream->inputBuffer + position * stream->numChannels,
            remainingSamples * sizeof(int16_t) * stream->numChannels);
  }
  stream->numInputSamples = remainingSamples;
}

/* Just copy from the array to the output buffer */
static int copyToOutput(sonicStream stream, int16_t* samples, int numSamples) {
  if (!enlargeOutputBufferIfNeeded(stream, numSamples)) {
    return 0;
  }
  memcpy(stream->outputBuffer + stream->numOutputSamples * stream->numChannels,
         samples, numSamples * sizeof(int16_t) * stream->numChannels);
  stream->numOutputSamples += numSamples;
  return 1;
}

/* Just copy from the input buffer to the output buffer.  Return 0 if we fail to
   resize the output buffer.  Otherwise, return numSamples */
static int copyInputToOutput(sonicStream stream, int position) {
  int numSamples = stream->remainingInputToCopy;

  if (numSamples > stream->maxRequired) {
    numSamples = stream->maxRequired;
  }
  if (!copyToOutput(stream,
                    stream->inputBuffer + position * stream->numChannels,
                    numSamples)) {
    return 0;
  }
  stream->remainingInputToCopy -= numSamples;
  return numSamples;
}

/* Read data out of the stream.  Sometimes no data will be available, and zero
   is returned, which is not an error condition. */
int sonicReadFloatFromStream(sonicStream stream, float* samples,
                             int maxSamples) {
  int numSamples = stream->numOutputSamples;
  int remainingSamples = 0;
  int16_t* buffer;
  int count;

  if (numSamples == 0) {
    return 0;
  }
  if (numSamples > maxSamples) {
    remainingSamples = numSamples - maxSamples;
    numSamples = maxSamples;
  }
  buffer = stream->outputBuffer;
  count = numSamples * stream->numChannels;
  while (count--) {
    *samples++ = (*buffer++) / 32767.0f;
  }
  if (remainingSamples > 0) {
    memmove(stream->outputBuffer,
            stream->outputBuffer + numSamples * stream->numChannels,
            remainingSamples * sizeof(int16_t) * stream->numChannels);
  }
  stream->numOutputSamples = remainingSamples;
  return numSamples;
}

/* Read int16_t data out of the stream.  Sometimes no data will be available, and
   zero is returned, which is not an error condition. */
int sonicReadShortFromStream(sonicStream stream, int16_t* samples,
                             int maxSamples) {
  int numSamples = stream->numOutputSamples;
  int remainingSamples = 0;

  if (numSamples == 0) {
    return 0;
  }
  if (numSamples > maxSamples) {
    remainingSamples = numSamples - maxSamples;
    numSamples = maxSamples;
  }
  memcpy(samples, stream->outputBuffer,
         numSamples * sizeof(int16_t) * stream->numChannels);
  if (remainingSamples > 0) {
    memmove(stream->outputBuffer,
            stream->outputBuffer + numSamples * stream->numChannels,
            remainingSamples * sizeof(int16_t) * stream->numChannels);
  }
  stream->numOutputSamples = remainingSamples;
  return numSamples;
}

/* Read unsigned char data out of the stream.  Sometimes no data will be
   available, and zero is returned, which is not an error condition. */
int sonicReadUnsignedCharFromStream(sonicStream stream, unsigned char* samples,
                                    int maxSamples) {
  int numSamples = stream->numOutputSamples;
  int remainingSamples = 0;
  int16_t* buffer;
  int count;

  if (numSamples == 0) {
    return 0;
  }
  if (numSamples > maxSamples) {
    remainingSamples = numSamples - maxSamples;
    numSamples = maxSamples;
  }
  buffer = stream->outputBuffer;
  count = numSamples * stream->numChannels;
  while (count--) {
    *samples++ = (char)((*buffer++) >> 8) + 128;
  }
  if (remainingSamples > 0) {
    memmove(stream->outputBuffer,
            stream->outputBuffer + numSamples * stream->numChannels,
            remainingSamples * sizeof(int16_t) * stream->numChannels);
  }
  stream->numOutputSamples = remainingSamples;
  return numSamples;
}

/* Force the sonic stream to generate output using whatever data it currently
   has.  No extra delay will be added to the output, but flushing in the middle
   of words could introduce distortion. */
int sonicFlushStream(sonicStream stream) {
  int maxRequired = stream->maxRequired;
  int remainingSamples = stream->numInputSamples;
  float speed = stream->speed / stream->pitch;
  float rate = stream->rate * stream->pitch;
  int expectedOutputSamples =
      stream->numOutputSamples +
      (int)((remainingSamples / speed + stream->numPitchSamples) / rate + 0.5f);

  /* Add enough silence to flush both input and pitch buffers. */
  if (!enlargeInputBufferIfNeeded(stream, remainingSamples + 2 * maxRequired)) {
    return 0;
  }
  memset(stream->inputBuffer + remainingSamples * stream->numChannels, 0,
         2 * maxRequired * sizeof(int16_t) * stream->numChannels);
  stream->numInputSamples += 2 * maxRequired;
  if (!sonicWriteShortToStream(stream, NULL, 0)) {
    return 0;
  }
  /* Throw away any extra samples we generated due to the silence we added */
  if (stream->numOutputSamples > expectedOutputSamples) {
    stream->numOutputSamples = expectedOutputSamples;
  }
  /* Empty input and pitch buffers */
  stream->numInputSamples = 0;
  stream->remainingInputToCopy = 0;
  stream->numPitchSamples = 0;
  return 1;
}

/* Return the number of samples in the output buffer */
int sonicSamplesAvailable(sonicStream stream) {
  return stream->numOutputSamples;
}

/* If skip is greater than one, average skip samples together and write them to
   the down-sample buffer.  If numChannels is greater than one, mix the channels
   together as we down sample. */
static void downSampleInput(sonicStream stream, int16_t* samples, int skip) {
  int numSamples = stream->maxRequired / skip;
  int samplesPerValue = stream->numChannels * skip;
  int i, j;
  int value;
  int16_t* downSamples = stream->downSampleBuffer;

  for (i = 0; i < numSamples; i++) {
    value = 0;
    for (j = 0; j < samplesPerValue; j++) {
      value += *samples++;
    }
    value /= samplesPerValue;
    *downSamples++ = value;
  }
}

/* Find the best frequency match in the range, and given a sample skip multiple.
   For now, just find the pitch of the first channel. */
static int findPitchPeriodInRange(int16_t* samples, int minPeriod, int maxPeriod,
                                  int* retMinDiff, int* retMaxDiff) {
  int period, bestPeriod = 0, worstPeriod = 255;
  int16_t* s;
  int16_t* p;
  int16_t sVal, pVal;
  unsigned long diff, minDiff = 1, maxDiff = 0;
  int i;

  for (period = minPeriod; period <= maxPeriod; period++) {
    diff = 0;
    s = samples;
    p = samples + period;
    for (i = 0; i < period; i++) {
      sVal = *s++;
      pVal = *p++;
      diff += sVal >= pVal ? (uint16_t)(sVal - pVal)
                           : (uint16_t)(pVal - sVal);
    }
    /* Note that the highest number of samples we add into diff will be less
       than 256, since we skip samples.  Thus, diff is a 24 bit number, and
       we can safely multiply by numSamples without overflow */
    /* if (bestPeriod == 0 || (bestPeriod*3/2 > period && diff*bestPeriod <
       minDiff*period) || diff*bestPeriod < (minDiff >> 1)*period) {*/
    if (bestPeriod == 0 || diff * bestPeriod < minDiff * period) {
      minDiff = diff;
      bestPeriod = period;
    }
    if (diff * worstPeriod > maxDiff * period) {
      maxDiff = diff;
      worstPeriod = period;
    }
  }
  if (bestPeriod != 0) {
	  *retMinDiff = (int)(minDiff / bestPeriod);
  }
  if (worstPeriod != 0) {
	  *retMaxDiff = (int)(maxDiff / worstPeriod);
  }

  return bestPeriod;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"

/* At abrupt ends of voiced words, we can have pitch periods that are better
   approximated by the previous pitch period estimate.  Try to detect this case.
 */
static int prevPeriodBetter(sonicStream stream, int period, int minDiff,
                            int maxDiff, int preferNewPeriod) {
  if (minDiff == 0 || stream->prevPeriod == 0) {
    return 0;
  }
  if (preferNewPeriod) {
    if (maxDiff > minDiff * 3) {
      /* Got a reasonable match this period */
      return 0;
    }
    if (minDiff * 2 <= stream->prevMinDiff * 3) {
      /* Mismatch is not that much greater this period */
      return 0;
    }
  } else {
    if (minDiff <= stream->prevMinDiff) {
      return 0;
    }
  }
  return 1;
}

#pragma clang diagnostic pop

/* Find the pitch period.  This is a critical step, and we may have to try
   multiple ways to get a good answer.  This version uses Average Magnitude
   Difference Function (AMDF).  To improve speed, we down sample by an integer
   factor get in the 11KHz range, and then do it again with a narrower
   frequency range without down sampling */
static int findPitchPeriod(sonicStream stream, int16_t* samples,
                           int preferNewPeriod) {
  int minPeriod = stream->minPeriod;
  int maxPeriod = stream->maxPeriod;
  int sampleRate = stream->sampleRate;
  int minDiff = 0, maxDiff = 0, retPeriod = 0;
  int skip = 1;
  int period;

  if (sampleRate > SONIC_AMDF_FREQ && stream->quality == 0) {
    skip = sampleRate / SONIC_AMDF_FREQ;
  }
  if (stream->numChannels == 1 && skip == 1) {
    period = findPitchPeriodInRange(samples, minPeriod, maxPeriod, &minDiff,
                                    &maxDiff);
  } else {
    downSampleInput(stream, samples, skip);
    period = findPitchPeriodInRange(stream->downSampleBuffer, minPeriod / skip,
                                    maxPeriod / skip, &minDiff, &maxDiff);
    if (skip != 1) {
      period *= skip;
      minPeriod = period - (skip << 2);
      maxPeriod = period + (skip << 2);
      if (minPeriod < stream->minPeriod) {
        minPeriod = stream->minPeriod;
      }
      if (maxPeriod > stream->maxPeriod) {
        maxPeriod = stream->maxPeriod;
      }
      if (stream->numChannels == 1) {
        period = findPitchPeriodInRange(samples, minPeriod, maxPeriod, &minDiff,
                                        &maxDiff);
      } else {
        downSampleInput(stream, samples, 1);
        period = findPitchPeriodInRange(stream->downSampleBuffer, minPeriod,
                                        maxPeriod, &minDiff, &maxDiff);
      }
    }
  }
  if (prevPeriodBetter(stream, period, minDiff, maxDiff, preferNewPeriod)) {
    retPeriod = stream->prevPeriod;
  } else {
    retPeriod = period;
  }
  stream->prevMinDiff = minDiff;
  stream->prevPeriod = period;
  return retPeriod;
}

/* Overlap two sound segments, ramp the volume of one down, while ramping the
   other one from zero up, and add them, storing the result at the output. */
static void overlapAdd(int numSamples, int numChannels, int16_t* out,
                       int16_t* rampDown, int16_t* rampUp) {
  int16_t* o;
  int16_t* u;
  int16_t* d;
  int i, t;

  for (i = 0; i < numChannels; i++) {
    o = out + i;
    u = rampUp + i;
    d = rampDown + i;
    for (t = 0; t < numSamples; t++) {
#ifdef SONIC_USE_SIN
      float ratio = sin(t * M_PI / (2 * numSamples));
      *o = *d * (1.0f - ratio) + *u * ratio;
#else
      *o = (*d * (numSamples - t) + *u * t) / numSamples;
#endif
      o += numChannels;
      d += numChannels;
      u += numChannels;
    }
  }
}

/* Overlap two sound segments, ramp the volume of one down, while ramping the
   other one from zero up, and add them, storing the result at the output. */
static void overlapAddWithSeparation(int numSamples, int numChannels,
                                     int separation, int16_t* out,
                                     int16_t* rampDown, int16_t* rampUp) {
  int16_t *o, *u, *d;
  int i, t;

  for (i = 0; i < numChannels; i++) {
    o = out + i;
    u = rampUp + i;
    d = rampDown + i;
    for (t = 0; t < numSamples + separation; t++) {
      if (t < separation) {
        *o = *d * (numSamples - t) / numSamples;
        d += numChannels;
      } else if (t < numSamples) {
        *o = (*d * (numSamples - t) + *u * (t - separation)) / numSamples;
        d += numChannels;
        u += numChannels;
      } else {
        *o = *u * (t - separation) / numSamples;
        u += numChannels;
      }
      o += numChannels;
    }
  }
}

/* Just move the new samples in the output buffer to the pitch buffer */
static int moveNewSamplesToPitchBuffer(sonicStream stream,
                                       int originalNumOutputSamples) {
  int numSamples = stream->numOutputSamples - originalNumOutputSamples;
  int numChannels = stream->numChannels;

  if (stream->numPitchSamples + numSamples > stream->pitchBufferSize) {
    stream->pitchBufferSize += (stream->pitchBufferSize >> 1) + numSamples;
    stream->pitchBuffer =
        (int16_t*)realloc(stream->pitchBuffer,
                        stream->pitchBufferSize * sizeof(int16_t) * numChannels);
    if (stream->pitchBuffer == NULL) {
      return 0;
    }
  }
  memcpy(stream->pitchBuffer + stream->numPitchSamples * numChannels,
         stream->outputBuffer + originalNumOutputSamples * numChannels,
         numSamples * sizeof(int16_t) * numChannels);
  stream->numOutputSamples = originalNumOutputSamples;
  stream->numPitchSamples += numSamples;
  return 1;
}

/* Remove processed samples from the pitch buffer. */
static void removePitchSamples(sonicStream stream, int numSamples) {
  int numChannels = stream->numChannels;
  int16_t* source = stream->pitchBuffer + numSamples * numChannels;

  if (numSamples == 0) {
    return;
  }
  if (numSamples != stream->numPitchSamples) {
    memmove(
        stream->pitchBuffer, source,
        (stream->numPitchSamples - numSamples) * sizeof(int16_t) * numChannels);
  }
  stream->numPitchSamples -= numSamples;
}

/* Change the pitch.  The latency this introduces could be reduced by looking at
   past samples to determine pitch, rather than future. */
static int adjustPitch(sonicStream stream, int originalNumOutputSamples) {
  float pitch = stream->pitch;
  int numChannels = stream->numChannels;
  int period, newPeriod, separation;
  int position = 0;
  int16_t* out;
  int16_t* rampDown;
  int16_t* rampUp;

  if (stream->numOutputSamples == originalNumOutputSamples) {
    return 1;
  }
  if (!moveNewSamplesToPitchBuffer(stream, originalNumOutputSamples)) {
    return 0;
  }
  while (stream->numPitchSamples - position >= stream->maxRequired) {
    period = findPitchPeriod(stream,
                             stream->pitchBuffer + position * numChannels, 0);
    newPeriod = period / pitch;
    if (!enlargeOutputBufferIfNeeded(stream, newPeriod)) {
      return 0;
    }
    out = stream->outputBuffer + stream->numOutputSamples * numChannels;
    if (pitch >= 1.0f) {
      rampDown = stream->pitchBuffer + position * numChannels;
      rampUp =
          stream->pitchBuffer + (position + period - newPeriod) * numChannels;
      overlapAdd(newPeriod, numChannels, out, rampDown, rampUp);
    } else {
      rampDown = stream->pitchBuffer + position * numChannels;
      rampUp = stream->pitchBuffer + position * numChannels;
      separation = newPeriod - period;
      if (period != 0) {
		  overlapAddWithSeparation(period, numChannels, separation, out, rampDown,
								   rampUp);
      }
    }
    stream->numOutputSamples += newPeriod;
    position += period;
  }
  removePitchSamples(stream, position);
  return 1;
}

/* Aproximate the sinc function times a Hann window from the sinc table. */
static int findSincCoefficient(int i, int ratio, int width) {
  int lobePoints = (SINC_TABLE_SIZE - 1) / SINC_FILTER_POINTS;
  int left = i * lobePoints + (ratio * lobePoints) / width;
  int right = left + 1;
  int position = i * lobePoints * width + ratio * lobePoints - left * width;
  int leftVal = sincTable[left];
  int rightVal = sincTable[right];

  return ((leftVal * (width - position) + rightVal * position) << 1) / width;
}

/* Return 1 if value >= 0, else -1.  This represents the sign of value. */
static int getSign(int value) { return value >= 0 ? 1 : -1; }

/* Interpolate the new output sample. */
static int16_t interpolate(sonicStream stream, int16_t* in, int oldSampleRate,
                         int newSampleRate) {
  /* Compute N-point sinc FIR-filter here.  Clip rather than overflow. */
  int i;
  int total = 0;
  int position = stream->newRatePosition * oldSampleRate;
  int leftPosition = stream->oldRatePosition * newSampleRate;
  int rightPosition = (stream->oldRatePosition + 1) * newSampleRate;
  int ratio = rightPosition - position - 1;
  int width = rightPosition - leftPosition;
  int weight, value;
  int oldSign;
  int overflowCount = 0;

  for (i = 0; i < SINC_FILTER_POINTS; i++) {
    weight = findSincCoefficient(i, ratio, width);
    /* printf("%u %f\n", i, weight); */
    value = in[i * stream->numChannels] * weight;
    oldSign = getSign(total);
    total += value;
    if (oldSign != getSign(total) && getSign(value) == oldSign) {
      /* We must have overflowed.  This can happen with a sinc filter. */
      overflowCount += oldSign;
    }
  }
  /* It is better to clip than to wrap if there was a overflow. */
  if (overflowCount > 0) {
    return SHRT_MAX;
  } else if (overflowCount < 0) {
    return SHRT_MIN;
  }
  return total >> 16;
}

/* Change the rate.  Interpolate with a sinc FIR filter using a Hann window. */
static int adjustRate(sonicStream stream, float rate,
                      int originalNumOutputSamples) {
  int newSampleRate = stream->sampleRate / rate;
  int oldSampleRate = stream->sampleRate;
  int numChannels = stream->numChannels;
  int position = 0;
  int16_t *in, *out;
  int i;
  int N = SINC_FILTER_POINTS;

  /* Set these values to help with the integer math */
  while (newSampleRate > (1 << 14) || oldSampleRate > (1 << 14)) {
    newSampleRate >>= 1;
    oldSampleRate >>= 1;
  }
  if (stream->numOutputSamples == originalNumOutputSamples) {
    return 1;
  }
  if (!moveNewSamplesToPitchBuffer(stream, originalNumOutputSamples)) {
    return 0;
  }
  /* Leave at least N pitch sample in the buffer */
  for (position = 0; position < stream->numPitchSamples - N; position++) {
    while ((stream->oldRatePosition + 1) * newSampleRate >
           stream->newRatePosition * oldSampleRate) {
      if (!enlargeOutputBufferIfNeeded(stream, 1)) {
        return 0;
      }
      out = stream->outputBuffer + stream->numOutputSamples * numChannels;
      in = stream->pitchBuffer + position * numChannels;
      for (i = 0; i < numChannels; i++) {
        *out++ = interpolate(stream, in, oldSampleRate, newSampleRate);
        in++;
      }
      stream->newRatePosition++;
      stream->numOutputSamples++;
    }
    stream->oldRatePosition++;
    if (stream->oldRatePosition == oldSampleRate) {
      stream->oldRatePosition = 0;
      if (stream->newRatePosition != newSampleRate) {
        fprintf(stderr,
                "Assertion failed: stream->newRatePosition != newSampleRate\n");
        exit(1);
      }
      stream->newRatePosition = 0;
    }
  }
  removePitchSamples(stream, position);
  return 1;
}

/* Skip over a pitch period, and copy period/speed samples to the output */
static int skipPitchPeriod(sonicStream stream, int16_t* samples, float speed,
                           int period) {
  long newSamples;
  int numChannels = stream->numChannels;

  if (speed >= 2.0f) {
    newSamples = period / (speed - 1.0f);
  } else {
    newSamples = period;
    stream->remainingInputToCopy = period * (2.0f - speed) / (speed - 1.0f);
  }
  if (!enlargeOutputBufferIfNeeded(stream, (int)newSamples)) {
    return 0;
  }
  overlapAdd((int)newSamples, numChannels,
             stream->outputBuffer + stream->numOutputSamples * numChannels,
             samples, samples + period * numChannels);
  stream->numOutputSamples += newSamples;
  return (int)newSamples;
}

/* Insert a pitch period, and determine how much input to copy directly. */
static int insertPitchPeriod(sonicStream stream, int16_t* samples, float speed,
                             int period) {
  long newSamples;
  int16_t* out;
  int numChannels = stream->numChannels;

  if (speed < 0.5f) {
    newSamples = period * speed / (1.0f - speed);
  } else {
    newSamples = period;
    stream->remainingInputToCopy =
        period * (2.0f * speed - 1.0f) / (1.0f - speed);
  }
  if (!enlargeOutputBufferIfNeeded(stream, (int)(period + newSamples))) {
    return 0;
  }
  out = stream->outputBuffer + stream->numOutputSamples * numChannels;
  memcpy(out, samples, period * sizeof(int16_t) * numChannels);
  out =
      stream->outputBuffer + (stream->numOutputSamples + period) * numChannels;
  overlapAdd((int)newSamples, numChannels, out, samples + period * numChannels,
             samples);
  stream->numOutputSamples += period + (int)newSamples;
  return (int)newSamples;
}

/* Resample as many pitch periods as we have buffered on the input.  Return 0 if
   we fail to resize an input or output buffer. */
static int changeSpeed(sonicStream stream, float speed) {
  int16_t* samples;
  int numSamples = stream->numInputSamples;
  int position = 0, period, newSamples;
  int maxRequired = stream->maxRequired;

  /* printf("Changing speed to %f\n", speed); */
  if (stream->numInputSamples < maxRequired) {
    return 1;
  }
  do {
    if (stream->remainingInputToCopy > 0) {
      newSamples = copyInputToOutput(stream, position);
      position += newSamples;
    } else {
      samples = stream->inputBuffer + position * stream->numChannels;
      period = findPitchPeriod(stream, samples, 1);
#ifdef SONIC_SPECTROGRAM
      if (stream->spectrogram != NULL) {
        sonicAddPitchPeriodToSpectrogram(stream->spectrogram, samples, period,
                                         stream->numChannels);
        newSamples = period;
        position += period;
      } else
#endif  /* SONIC_SPECTROGRAM */
          if (speed > 1.0) {
        newSamples = skipPitchPeriod(stream, samples, speed, period);
        position += period + newSamples;
      } else {
        newSamples = insertPitchPeriod(stream, samples, speed, period);
        position += newSamples;
      }
    }
    if (newSamples == 0) {
      return 0; /* Failed to resize output buffer */
    }
  } while (position + maxRequired <= numSamples);
  removeInputSamples(stream, position);
  return 1;
}

/* Resample as many pitch periods as we have buffered on the input.  Return 0 if
   we fail to resize an input or output buffer.  Also scale the output by the
   volume. */
static int processStreamInput(sonicStream stream) {
  int originalNumOutputSamples = stream->numOutputSamples;
  float speed = stream->speed / stream->pitch;
  float rate = stream->rate;

  if (!stream->useChordPitch) {
    rate *= stream->pitch;
  }
  if (speed > 1.00001 || speed < 0.99999) {
    changeSpeed(stream, speed);
  } else {
    if (!copyToOutput(stream, stream->inputBuffer, stream->numInputSamples)) {
      return 0;
    }
    stream->numInputSamples = 0;
  }
  if (stream->useChordPitch) {
    if (stream->pitch != 1.0f) {
      if (!adjustPitch(stream, originalNumOutputSamples)) {
        return 0;
      }
    }
  } else if (rate != 1.0f) {
    if (!adjustRate(stream, rate, originalNumOutputSamples)) {
      return 0;
    }
  }
  if (stream->volume != 1.0f) {
    /* Adjust output volume. */
    scaleSamples(
        stream->outputBuffer + originalNumOutputSamples * stream->numChannels,
        (stream->numOutputSamples - originalNumOutputSamples) *
            stream->numChannels,
        stream->volume);
  }
  return 1;
}

/* Write floating point data to the input buffer and process it. */
int sonicWriteFloatToStream(sonicStream stream, float* samples,
                            int numSamples) {
  if (!addFloatSamplesToInputBuffer(stream, samples, numSamples)) {
    return 0;
  }
  return processStreamInput(stream);
}

/* Simple wrapper around sonicWriteFloatToStream that does the int16_t to float
   conversion for you. */
int sonicWriteShortToStream(sonicStream stream, int16_t* samples,
                            int numSamples) {
  if (!addShortSamplesToInputBuffer(stream, samples, numSamples)) {
    return 0;
  }
  return processStreamInput(stream);
}

/* Simple wrapper around sonicWriteFloatToStream that does the unsigned char to
   float conversion for you. */
int sonicWriteUnsignedCharToStream(sonicStream stream, unsigned char* samples,
                                   int numSamples) {
  if (!addUnsignedCharSamplesToInputBuffer(stream, samples, numSamples)) {
    return 0;
  }
  return processStreamInput(stream);
}

/* This is a non-stream oriented interface to just change the speed of a sound
 * sample */
int sonicChangeFloatSpeed(float* samples, int numSamples, float speed,
                          float pitch, float rate, float volume,
                          int useChordPitch, int sampleRate, int numChannels) {
  sonicStream stream = sonicCreateStream(sampleRate, numChannels);

  sonicSetSpeed(stream, speed);
  sonicSetPitch(stream, pitch);
  sonicSetRate(stream, rate);
  sonicSetVolume(stream, volume);
  sonicSetChordPitch(stream, useChordPitch);
  sonicWriteFloatToStream(stream, samples, numSamples);
  sonicFlushStream(stream);
  numSamples = sonicSamplesAvailable(stream);
  sonicReadFloatFromStream(stream, samples, numSamples);
  sonicDestroyStream(stream);
  return numSamples;
}

/* This is a non-stream oriented interface to just change the speed of a sound
 * sample */
int sonicChangeShortSpeed(int16_t* samples, int numSamples, float speed,
                          float pitch, float rate, float volume,
                          int useChordPitch, int sampleRate, int numChannels) {
  sonicStream stream = sonicCreateStream(sampleRate, numChannels);

  sonicSetSpeed(stream, speed);
  sonicSetPitch(stream, pitch);
  sonicSetRate(stream, rate);
  sonicSetVolume(stream, volume);
  sonicSetChordPitch(stream, useChordPitch);
  sonicWriteShortToStream(stream, samples, numSamples);
  sonicFlushStream(stream);
  numSamples = sonicSamplesAvailable(stream);
  sonicReadShortFromStream(stream, samples, numSamples);
  sonicDestroyStream(stream);
  return numSamples;
}
