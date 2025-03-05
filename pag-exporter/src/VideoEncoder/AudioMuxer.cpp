/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////
#include "AudioMuxer.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define STREAM_DURATION   10.0
#define STREAM_FRAME_RATE 25 /* 25 images/s */
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */

#define SCALE_FLAGS SWS_BICUBIC

static char g_error_string[AV_ERROR_MAX_STRING_SIZE] = {0};
#define av_err2string(errnum) av_make_error_string(g_error_string, AV_ERROR_MAX_STRING_SIZE, errnum)

static int write_frame(AVFormatContext* fmt_ctx, const AVRational* time_base, AVStream* st, AVPacket* pkt) {
  /* rescale output packet timestamp values from codec to stream timebase */
  av_packet_rescale_ts(pkt, *time_base, st->time_base);
  pkt->stream_index = st->index;

  /* Write the compressed frame to the media file. */
  return av_interleaved_write_frame(fmt_ctx, pkt);
}

/* Add an output stream. */
static void add_stream(OutputStream* ost, AVFormatContext* oc,
                       AVCodec** codec,
                       enum AVCodecID codec_id) {
  AVCodecContext* c;
  int i;

  /* find the encoder */
  *codec = avcodec_find_encoder(codec_id);
  if (!(*codec)) {
    fprintf(stderr, "Could not find encoder for '%s'\n",
            avcodec_get_name(codec_id));
    exit(1);
  }

  ost->st = avformat_new_stream(oc, NULL);
  if (!ost->st) {
    fprintf(stderr, "Could not allocate stream\n");
    exit(1);
  }
  ost->st->id = oc->nb_streams - 1;
  c = avcodec_alloc_context3(*codec);
  if (!c) {
    fprintf(stderr, "Could not alloc an encoding context\n");
    exit(1);
  }
  ost->enc = c;

  switch ((*codec)->type) {
    case AVMEDIA_TYPE_AUDIO:
      c->sample_fmt = (*codec)->sample_fmts ?
                      (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
      c->bit_rate = 128000;
      c->sample_rate = 44100;
      if ((*codec)->supported_samplerates) {
        c->sample_rate = (*codec)->supported_samplerates[0];
        for (i = 0; (*codec)->supported_samplerates[i]; i++) {
          if ((*codec)->supported_samplerates[i] == 44100)
            c->sample_rate = 44100;
        }
      }
      c->channels = av_get_channel_layout_nb_channels(c->channel_layout);
      c->channel_layout = AV_CH_LAYOUT_STEREO;
      if ((*codec)->channel_layouts) {
        c->channel_layout = (*codec)->channel_layouts[0];
        for (i = 0; (*codec)->channel_layouts[i]; i++) {
          if ((*codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
            c->channel_layout = AV_CH_LAYOUT_STEREO;
        }
      }
      c->channels = av_get_channel_layout_nb_channels(c->channel_layout);
      ost->st->time_base = AVRational{1, c->sample_rate};
      break;

    case AVMEDIA_TYPE_VIDEO:
      c->codec_id = codec_id;

      c->bit_rate = 400000;
      /* Resolution must be a multiple of two. */
      c->width = 352;
      c->height = 288;
      /* timebase: This is the fundamental unit of time (in seconds) in terms
       * of which frame timestamps are represented. For fixed-fps content,
       * timebase should be 1/framerate and timestamp increments should be
       * identical to 1. */
      ost->st->time_base = AVRational{1, STREAM_FRAME_RATE};
      c->time_base = ost->st->time_base;

      c->gop_size = 12; /* emit one intra frame every twelve frames at most */
      c->pix_fmt = STREAM_PIX_FMT;
      if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
        /* just for testing, we also add B-frames */
        c->max_b_frames = 2;
      }
      if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
        /* Needed to avoid using macroblocks in which some coeffs overflow.
         * This does not happen with normal video, it just happens here as
         * the motion of the chroma plane does not match the luma plane. */
        c->mb_decision = 2;
      }
      break;

    default:
      break;
  }

  /* Some formats want stream headers to be separate. */
  if (oc->oformat->flags & AVFMT_GLOBALHEADER)
    c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
}

/**************************************************************/
/* audio output */

static AVFrame* alloc_audio_frame(enum AVSampleFormat sample_fmt,
                                  uint64_t channel_layout,
                                  int sample_rate, int nb_samples) {
  AVFrame* frame = av_frame_alloc();
  int ret;

  if (!frame) {
    fprintf(stderr, "Error allocating an audio frame\n");
    exit(1);
  }

  frame->format = sample_fmt;
  frame->channel_layout = channel_layout;
  frame->sample_rate = sample_rate;
  frame->nb_samples = nb_samples;

  if (nb_samples) {
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
      fprintf(stderr, "Error allocating an audio buffer\n");
      exit(1);
    }
  }

  return frame;
}

static bool open_audio(AVFormatContext* oc, AVCodec* codec, OutputStream* ost, AVDictionary* opt_arg) {
  AVCodecContext* c;
  int nb_samples;
  int ret;
  AVDictionary* opt = NULL;

  c = ost->enc;

  /* open it */
  av_dict_copy(&opt, opt_arg, 0);
  ret = avcodec_open2(c, codec, &opt);
  av_dict_free(&opt);
  if (ret < 0) {
    fprintf(stderr, "Could not open audio codec: %s\n", av_err2string(ret));
    return false;
  }

  if (c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
    nb_samples = 10000;
  else
    nb_samples = c->frame_size;

  ost->internalFrame = alloc_audio_frame(c->sample_fmt, c->channel_layout,
                                         c->sample_rate, nb_samples);
  ost->tmp_frame = alloc_audio_frame(AV_SAMPLE_FMT_S16, c->channel_layout,
                                     c->sample_rate, nb_samples);

  /* copy the stream parameters to the muxer */
  ret = avcodec_parameters_from_context(ost->st->codecpar, c);
  if (ret < 0) {
    fprintf(stderr, "Could not copy the stream parameters\n");
    return false;
  }

  /* create resampler context */
  ost->swr_ctx = swr_alloc();
  if (!ost->swr_ctx) {
    fprintf(stderr, "Could not allocate resampler context\n");
    return false;
  }

  /* set options */
  av_opt_set_int(ost->swr_ctx, "in_channel_count", c->channels, 0);
  av_opt_set_int(ost->swr_ctx, "in_sample_rate", c->sample_rate, 0);
  av_opt_set_sample_fmt(ost->swr_ctx, "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);
  av_opt_set_int(ost->swr_ctx, "out_channel_count", c->channels, 0);
  av_opt_set_int(ost->swr_ctx, "out_sample_rate", c->sample_rate, 0);
  av_opt_set_sample_fmt(ost->swr_ctx, "out_sample_fmt", c->sample_fmt, 0);

  /* initialize the resampling context */
  if ((ret = swr_init(ost->swr_ctx)) < 0) {
    fprintf(stderr, "Failed to initialize the resampling context\n");
    return false;
  }

  return true;
}

/* Prepare a 16 bit dummy audio frame of 'frame_size' samples and
 * 'nb_channels' channels. */
static AVFrame* get_audio_frame(OutputStream* ost, int16_t* data, int channel, int totalSamples, int* pConsumeSamples) {
  AVFrame* frame = ost->tmp_frame;
  int j, i;
  int16_t* q = (int16_t*)frame->data[0];
  int16_t* p = data;
  int samples = std::min(frame->nb_samples, totalSamples);

  for (j = 0; j < samples; j++) {
    for (i = 0; i < ost->enc->channels; i++) {
      *q++ = *p++;
    }
  }

  frame->pts = ost->next_pts;
  ost->next_pts += samples;
  *pConsumeSamples = samples;

  return frame;
}

/*
 * encode one audio frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */
static bool write_audio_frame(AVFormatContext* oc, OutputStream* ost, AVFrame* frame) {
  AVCodecContext* c;
  AVPacket pkt = {0}; // data and size must be 0;
  int ret;
  int got_packet;
  int dst_nb_samples;

  av_init_packet(&pkt);
  c = ost->enc;

  if (frame) {
    /* convert samples from native format to destination codec format, using the resampler */
    /* compute destination number of samples */
    dst_nb_samples = av_rescale_rnd(swr_get_delay(ost->swr_ctx, c->sample_rate) + frame->nb_samples,
                                    c->sample_rate, c->sample_rate, AV_ROUND_UP);
    av_assert0(dst_nb_samples == frame->nb_samples);

    /* when we pass a frame to the encoder, it may keep a reference to it
     * internally;
     * make sure we do not overwrite it here
     */
    ret = av_frame_make_writable(ost->internalFrame);
    if (ret < 0) {
      return false;
    }

    /* convert to destination format */
    ret = swr_convert(ost->swr_ctx,
                      ost->internalFrame->data, dst_nb_samples,
                      (const uint8_t**)frame->data, frame->nb_samples);
    if (ret < 0) {
      fprintf(stderr, "Error while converting\n");
      return false;
    }
    frame = ost->internalFrame;

    frame->pts = av_rescale_q(ost->samples_count, AVRational{1, c->sample_rate}, c->time_base);
    ost->samples_count += dst_nb_samples;
  }

  ret = avcodec_encode_audio2(c, &pkt, frame, &got_packet);
  if (ret < 0) {
    fprintf(stderr, "Error encoding audio frame: %s\n", av_err2string(ret));
    return false;
  }

  if (got_packet) {
    ret = write_frame(oc, &c->time_base, ost->st, &pkt);
    if (ret < 0) {
      fprintf(stderr, "Error while writing audio frame: %s\n", av_err2string(ret));
      return false;
    }
  }

  return (frame || got_packet);
}


static void close_stream(AVFormatContext* oc, OutputStream* ost) {
  avcodec_free_context(&ost->enc);
  av_frame_free(&ost->internalFrame);
  av_frame_free(&ost->tmp_frame);
  swr_free(&ost->swr_ctx);
}

/**************************************************************/
/* media file output */

bool AudioMuxer::init(std::string& outputPath, int channel, int sampleRate) {
  int ret;
  AVDictionary* opt = NULL;

  this->channel = channel;
  this->sampleRate = sampleRate;
  filename = outputPath + "_tmp.mp4";

  //av_dict_set(&opt, argv[i]+1, argv[i+1], 0);
  av_dict_set(&opt, "movflags", "faststart", 0);

  /* allocate the output media context */
  avformat_alloc_output_context2(&oc, NULL, NULL, filename.c_str());
  if (!oc) {
    printf("Could not deduce output format from file extension: using MPEG.\n");
    avformat_alloc_output_context2(&oc, NULL, "mpeg", filename.c_str());
  }
  if (!oc) {
    return false;
  }

  fmt = oc->oformat;

  if (fmt->audio_codec != AV_CODEC_ID_AAC) {
    return false;
  }
  add_stream(&audio_st, oc, &audio_codec, fmt->audio_codec);

  open_audio(oc, audio_codec, &audio_st, opt);

  av_dump_format(oc, 0, filename.c_str(), 1);

  /* open the output file, if needed */
  if (!(fmt->flags & AVFMT_NOFILE)) {
    ret = avio_open(&oc->pb, filename.c_str(), AVIO_FLAG_WRITE);
    if (ret < 0) {
      fprintf(stderr, "Could not open '%s': %s\n", filename.c_str(),
              av_err2string(ret));
      return 1;
    }
  }

  /* Write the stream header, if any. */
  ret = avformat_write_header(oc, &opt);
  if (ret < 0) {
    fprintf(stderr, "Error occurred when opening output file: %s\n",
            av_err2string(ret));
    return false;
  }

  return true;
}

int AudioMuxer::putData(int16_t* data, int numSamples) {
  int samples = 0;

  while (samples < numSamples) {
    int comsumeSamples = 0;
    auto frame = get_audio_frame(&audio_st, data + samples * channel, channel, numSamples - samples, &comsumeSamples);
    if (frame == nullptr) {
      break;
    }

    write_audio_frame(oc, &audio_st, frame);

    samples += comsumeSamples;
  }
  return numSamples;
}


void AudioMuxer::close() {
  /* Write the trailer, if any. The trailer must be written before you
   * close the CodecContexts open when you wrote the header; otherwise
   * av_write_trailer() may try to use memory that was freed on
   * av_codec_close(). */
  av_write_trailer(oc);

  /* Close each codec. */
  close_stream(oc, &audio_st);

  if (!(fmt->flags & AVFMT_NOFILE))
    /* Close the output file. */
    avio_closep(&oc->pb);

  /* free the stream */
  avformat_free_context(oc);
}

std::unique_ptr<pag::ByteData> AudioMuxer::getByteData() {
  return pag::ByteData::FromPath(filename);
}

AudioMuxer::~AudioMuxer() {
  remove(filename.c_str());
}
