//
// Created by katacai on 2022/3/10.
//

#include "CommonConfig.h"
#include "H264Remuxer.h"

namespace pag {
const std::string Mp4Generator::HDLR_VIDEO = "video";
const std::string Mp4Generator::HDLR_AUDIO = "audio";

ByteData* Mp4Generator::hdlrVideo =
    ByteData::MakeAdopted(
        new uint8_t[]{
            0x00,  // version 0
            0x00, 0x00,
            0x00,  // flags
            0x00, 0x00, 0x00,
            0x00,  // pre_defined
            0x76, 0x69, 0x64,
            0x65,  // handler_type: 'vide'
            0x00, 0x00, 0x00,
            0x00,  // reserved
            0x00, 0x00, 0x00,
            0x00,  // reserved
            0x00, 0x00, 0x00,
            0x00,  // reserved
            0x56, 0x69, 0x64, 0x65, 0x6f, 0x48, 0x61, 0x6e, 0x64, 0x6c, 0x65, 0x72,
            0x00,  // name: 'VideoHandler'
        },
        37)
        .release();

ByteData* Mp4Generator::hdlrAudio =
    ByteData::MakeAdopted(
        new uint8_t[]{
            0x00,  // version 0
            0x00, 0x00,
            0x00,  // flags
            0x00, 0x00, 0x00,
            0x00,  // pre_defined
            0x73, 0x6f, 0x75,
            0x6e,  // handler_type: 'soun'
            0x00, 0x00, 0x00,
            0x00,  // reserved
            0x00, 0x00, 0x00,
            0x00,  // reserved
            0x00, 0x00, 0x00,
            0x00,  // reserved
            0x53, 0x6f, 0x75, 0x6e, 0x64, 0x48, 0x61, 0x6e, 0x64, 0x6c, 0x65, 0x72,
            0x00,  // name: 'SoundHandler'
        },
        37)
        .release();

ByteData* Mp4Generator::fullbox = ByteData::MakeAdopted(
                                           new uint8_t[]{
                                               0x00,  // version
                                               0x00, 0x00,
                                               0x00,  // flags
                                               0x00, 0x00, 0x00,
                                               0x00,  // entry_count
                                           },
                                           8)
                                           .release();

ByteData* Mp4Generator::stscData = fullbox;
ByteData* Mp4Generator::stcoData = fullbox;

ByteData* Mp4Generator::stszData = ByteData::MakeAdopted(
                                            new uint8_t[]{
                                                0x00,  // version
                                                0x00, 0x00,
                                                0x00,  // flags
                                                0x00, 0x00, 0x00,
                                                0x00,  // sample_size
                                                0x00, 0x00, 0x00,
                                                0x00,  // sample_count
                                            },
                                            12)
                                            .release();

ByteData* Mp4Generator::vmhdData = ByteData::MakeAdopted(
                                            new uint8_t[]{
                                                0x00,  // version
                                                0x00, 0x00,
                                                0x01,  // flags
                                                0x00,
                                                0x00,  // graphicsmode
                                                0x00, 0x00, 0x00, 0x00, 0x00,
                                                0x00,  // opcolor
                                            },
                                            12)
                                            .release();

ByteData* Mp4Generator::smhdData = ByteData::MakeAdopted(
                                            new uint8_t[]{
                                                0x00,  // version
                                                0x00, 0x00,
                                                0x00,  // flags
                                                0x00,
                                                0x00,  // balance
                                                0x00,
                                                0x00,  // reserved
                                            },
                                            8)
                                            .release();

ByteData* Mp4Generator::stsdData = ByteData::MakeAdopted(
                                            new uint8_t[]{
                                                0x00,  // version 0
                                                0x00, 0x00,
                                                0x00,  // flags
                                                0x00, 0x00, 0x00,
                                                0x01,  // entry_count
                                            },
                                            8)
                                            .release();

ByteData* Mp4Generator::drefData = ByteData::MakeAdopted(
                                            new uint8_t[]{
                                                0x00,  // version 0
                                                0x00, 0x00,
                                                0x00,  // flags
                                                0x00, 0x00, 0x00,
                                                0x01,  // entry_count
                                                0x00, 0x00, 0x00,
                                                0x0c,  // entry_size
                                                0x75, 0x72, 0x6c,
                                                0x20,  // 'url' type
                                                0x00,  // version 0
                                                0x00, 0x00,
                                                0x01,  // entry_flags
                                            },
                                            20)
                                            .release();

ByteData* Mp4Generator::decimal2HexadecimalArray(int32_t payload) {
  uint8_t* array = new uint8_t[4];
  array[0] = payload >> 24;
  array[1] = (payload >> 16) & 0xff;
  array[2] = (payload >> 8) & 0xff;
  array[4] = payload & 0xff;

  std::unique_ptr<ByteData> data = ByteData::MakeAdopted(array, 4);
  return data.release();
}

ByteData* Mp4Generator::getCharCode(std::string name) {
  uint8_t* array = new uint8_t[name.length()];
  for (size_t i = 0; i < name.length(); i++) {
    array[i] = name[i];
  }
  std::unique_ptr<ByteData> data = ByteData::MakeAdopted(array, name.length());
  return data.release();
}

ByteData* Mp4Generator::numberToByteData(std::vector<uint8_t> numbers) {
  if (numbers.empty()) {
    LOGE("numberToByteData number empty");
    return nullptr;
  }
  uint8_t* array = new uint8_t[numbers.size()];
  for (int i = 0; i < numbers.size(); i++) {
    array[i] = numbers.at(i);
  }
  std::unique_ptr<ByteData> data = ByteData::MakeAdopted(array, numbers.size());
  return data.release();
}

ByteData* Mp4Generator::ftyp() {
  return box("ftyp", std::vector<ByteData*>{
                         getCharCode("isom"),
                         numberToByteData({0, 0, 0, 1}),
                         getCharCode("isom"),
                         getCharCode("iso2"),
                         getCharCode("avc1"),
                         getCharCode("mp41"),
                     });
}

ByteData* Mp4Generator::moov(std::vector<Mp4Track*> tracks, int32_t duration, int timescale) {
  std::vector<ByteData*> trackBoxes;
  for (auto mp4Track : tracks) {
    trackBoxes.emplace_back(trak(mp4Track));
  }

  std::vector<ByteData*> moovData;
  moovData.emplace_back(mvhd(timescale, duration));
  moovData.insert(moovData.end(), trackBoxes.begin(), trackBoxes.end());
  moovData.emplace_back(mvex(tracks));

  return box("moov", moovData);
}

ByteData* Mp4Generator::moof(int sequenceNumber, int32_t baseMediaDecodeTime,
                                  Mp4Track* track) {
  return box("moof",
             std::vector<ByteData*>{mfhd(sequenceNumber), traf(baseMediaDecodeTime, track)});
}

ByteData* Mp4Generator::mdat(ByteData* payload) { return box("mdat", toVector(payload)); }

ByteData* Mp4Generator::hdlr(std::string type) {
  if (type == HDLR_VIDEO) {
    return box("hdlr", toVector(hdlrVideo), false);
  } else {
    return box("hdlr", toVector(hdlrAudio), false);
  }
}

ByteData* Mp4Generator::mdhd(Mp4Track* track) {
  int32_t createTime = std::floor(getNowTime() + CORRECTION_UTC);
  int32_t modifitime = createTime;

  ByteArray payload(nullptr, 24);
  payload.setOrder(byteOrder);
  payload.writeUint8(0x00);  // version 0
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);  // flags
  writeNumbersToByteArray(&payload, {createTime, modifitime, track->timescale, 0});
  payload.writeUint8(0x55);
  payload.writeUint8(0xc4);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);

  return box("mdhd", toVector(payload.release().release()));
}

ByteData* Mp4Generator::mdia(Mp4Track* track) {
  return box("mdia", std::vector<ByteData*>{mdhd(track), hdlr(track->type), minf(track)});
}

ByteData* Mp4Generator::mfhd(int sequenceNumber) {
  ByteArray payload(nullptr, 8);
  payload.setOrder(byteOrder);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);  // flags
  payload.writeInt32(sequenceNumber);

  return box("mfhd", toVector(payload.release().release()));
}

ByteData* Mp4Generator::minf(Mp4Track* track) {
  if (track->type == "audio") {
    return box("minf", std::vector<ByteData*>{box("smhd", toVector(smhdData), false), dinf(),
                                                   stbl(track)});
  }
  return box("minf", std::vector<ByteData*>{box("vmhd", toVector(vmhdData), false), dinf(),
                                                 stbl(track)});
}

ByteData* Mp4Generator::mvex(std::vector<Mp4Track*> tracks) {
  std::vector<ByteData*> trexesBoxes;
  for (auto mp4Track : tracks) {
    trexesBoxes.emplace_back(trex(mp4Track));
  }
  return box("mvex", trexesBoxes);
}

ByteData* Mp4Generator::mvhd(int timescale, int32_t duration) {
  int32_t createTime = std::floor(getNowTime() + CORRECTION_UTC);
  int32_t modifitime = createTime;

  ByteArray payload(nullptr, 103);
  payload.setOrder(byteOrder);
  payload.writeUint8(0x00);  // version 0
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);  // version 0
  writeNumbersToByteArray(&payload, {createTime, modifitime, timescale, duration});
  payload.writeUint8(0x00);
  payload.writeUint8(0x01);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);  // 1.0 rate
  payload.writeUint8(0x01);
  payload.writeUint8(0x00);  // 1.0 volume
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);  // reserved
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);  // reserved
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);  // reserved
  payload.writeUint8(0x00);
  payload.writeUint8(0x01);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x01);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x40);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);  // transformation: unity matrix
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);  // pre_defined
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x02);  // next_track_ID

  return box("mvhd", toVector(payload.release().release()));
}

ByteData* Mp4Generator::sdtp(Mp4Track* track) {
  auto samples = track->samples;
  uint8_t* bytes = new uint8_t[4 + samples.size()];
  Mp4Flags flags;
  // leave the full box header (4 bytes) all zero
  // write the sample table
  for (size_t i = 0; i < samples.size(); i++) {
    flags = samples[i]->flags;
    bytes[i + 4] = (flags.dependsOn << 4) | (flags.isDependedOn << 2) | flags.hasRedundancy;
  }

  ByteData* sdtpData = ByteData::MakeAdopted(bytes, 4 + samples.size()).release();
  return box("sdtp", toVector(sdtpData));
}

ByteData* Mp4Generator::stbl(Mp4Track* track) {
  std::vector<ByteData*> payload;
  payload.emplace_back(stsd(track));
  payload.emplace_back(stts(track));
  payload.emplace_back(ctts(track));
  payload.emplace_back(stss(track));
  payload.emplace_back(box("stsc", toVector(stscData), false));
  payload.emplace_back(box("stsz", toVector(stszData), false));
  payload.emplace_back(box("stco", toVector(stcoData), false));

  return box("stbl", payload);
}

ByteData* Mp4Generator::avc1(Mp4Track* track) {
  // generate sps data
  int spsDataLen = 0;
  for (ByteData* spsData : track->sps) {
    spsDataLen += 2 + spsData->length();
  }
  ByteArray spsByteArray(nullptr, spsDataLen);
  spsByteArray.setOrder(byteOrder);
  for (ByteData* spsData : track->sps) {
    int len = spsData->length();
    spsByteArray.writeUint8((len >> 8) & 0xff);
    spsByteArray.writeUint8(len & 0xff);
    spsByteArray.writeBytes(spsData->data(), len);
  }

  // generate pps data
  int ppsDataLen = 0;
  for (ByteData* ppsData : track->pps) {
    ppsDataLen += 2 + ppsData->length();
  }
  ByteArray ppsByteArray(nullptr, ppsDataLen);
  ppsByteArray.setOrder(byteOrder);
  for (ByteData* ppsData : track->pps) {
    int len = ppsData->length();
    ppsByteArray.writeUint8((len >> 8) & 0xff);
    ppsByteArray.writeUint8(len & 0xff);
    ppsByteArray.writeBytes(ppsData->data(), len);
  }

  // generate avcc data
  ByteData* spsByteData = spsByteArray.release().release();
  ByteData* ppsByteData = ppsByteArray.release().release();
  ByteArray avccByteArry(nullptr, 7 + spsByteData->length() + ppsByteData->length());
  avccByteArry.setOrder(byteOrder);
  avccByteArry.writeUint8(0x01);                      // version
  avccByteArry.writeUint8(spsByteData->data()[3]);    // profile
  avccByteArry.writeUint8(spsByteData->data()[4]);    // profile compat
  avccByteArry.writeUint8(spsByteData->data()[5]);    // level
  avccByteArry.writeUint8(0xfc | 3);                  // lengthSizeMinusOne, hard-coded to 4 bytes
  avccByteArry.writeUint8(0xe0 | track->sps.size());  // lengthSizeMinusOne, hard-coded to 4 bytes
  avccByteArry.writeBytes(spsByteData->data(),
                          spsByteData->length());  // lengthSizeMinusOne, hard-coded to 4 bytes
  avccByteArry.writeUint8(track->pps.size());
  avccByteArry.writeBytes(ppsByteData->data(), ppsByteData->length());
  ByteData* avccBoxData = box("avcC", toVector(avccByteArry.release().release()));

  // generate avc1 data
  int width = track->width;
  int height = track->height;
  ByteArray avc1ByteArry(nullptr, 78 + avccBoxData->length());
  avc1ByteArry.setOrder(byteOrder);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);  // reserved
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);  // reserved
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x01);  // data_reference_index
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);  // pre_defined
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);  // reserved
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);  // pre_defined
  avc1ByteArry.writeUint8((width >> 8) & 0xff);
  avc1ByteArry.writeUint8(width & 0xff);  // width
  avc1ByteArry.writeUint8((height >> 8) & 0xff);
  avc1ByteArry.writeUint8(height & 0xff);  // height
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x48);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);  // horizresolution
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x48);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);  // vertresolution
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);  // reserved
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x01);  // frame_count
  avc1ByteArry.writeUint8(0x12);
  avc1ByteArry.writeUint8(0x62);
  avc1ByteArry.writeUint8(0x69);
  avc1ByteArry.writeUint8(0x6e);
  avc1ByteArry.writeUint8(0x65);  // binelpro.ru
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x00);  // compressorname
  avc1ByteArry.writeUint8(0x00);
  avc1ByteArry.writeUint8(0x18);  // depth = 24
  avc1ByteArry.writeUint8(0xff);
  avc1ByteArry.writeUint8(0xff);
  avc1ByteArry.writeBytes(avccBoxData->data(), avccBoxData->length());
  return box("avc1", toVector(avc1ByteArry.release().release()));
}

ByteData* Mp4Generator::stsd(Mp4Track* track) {
  if (track->type == "video") {
    return box("stsd", std::vector<ByteData*>{stsdData, avc1(track)}, false);
  }
  // 按时不需要处理 audio
  return nullptr;
}

ByteData* Mp4Generator::tkhd(Mp4Track* track) {
  int32_t createTime = std::floor(getNowTime() + CORRECTION_UTC);
  int32_t modifitime = createTime;

  int volumn = 1;  // 先自定义音量， 暂时不需要处理音频
  ByteArray payload(nullptr, 83);
  payload.setOrder(byteOrder);
  payload.writeUint8(0x00);  // version 0
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x01);  // flags
  writeNumbersToByteArray(&payload, {createTime, modifitime, track->id});
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);                              // reserved
  writeNumbersToByteArray(&payload, {track->duration});  // duration
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);  // reserved
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);  // layer
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);  // alternate_group
  payload.writeUint8((volumn >> 0) & 0xff);
  payload.writeUint8((((volumn % 1) * 10) >> 0) & 0xff);  // track volume // FIXME
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);  // reserved
  payload.writeUint8(0x00);
  payload.writeUint8(0x01);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x01);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x40);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);  // transformation: unity matrix
  payload.writeUint8((track->width >> 8) & 0xff);
  payload.writeUint8(track->width & 0xff);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);  // width
  payload.writeUint8((track->height >> 8) & 0xff);
  payload.writeUint8(track->height & 0xff);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);  // height

  return box("tkhd", toVector(payload.release().release()));
}

ByteData* Mp4Generator::traf(int32_t baseMediaDecodeTime, Mp4Track* track) {
  int id = track->id;
  ByteArray tfhdByteArray(nullptr, 8);
  tfhdByteArray.setOrder(byteOrder);
  tfhdByteArray.writeUint8(0x00);
  tfhdByteArray.writeUint8(0x00);
  tfhdByteArray.writeUint8(0x00);
  tfhdByteArray.writeUint8(0x00);
  writeNumbersToByteArray(&tfhdByteArray, {id});
  ByteData* tfhdBox = box("tfhd", toVector(tfhdByteArray.release().release()));

  ByteArray tfdtByteArray(nullptr, 8);
  tfdtByteArray.setOrder(byteOrder);
  tfdtByteArray.writeUint8(0x00);
  tfdtByteArray.writeUint8(0x00);
  tfdtByteArray.writeUint8(0x00);
  tfdtByteArray.writeUint8(0x00);
  writeNumbersToByteArray(&tfdtByteArray, {baseMediaDecodeTime});
  ByteData* tfdtBox = box("tfdt", toVector(tfdtByteArray.release().release()));

  ByteData* sdtpBox = sdtp(track);
  ByteData* trunBox = trun(track, sdtpBox->length() + 16 +  // tfhd
                                           16 +                  // tfdt
                                           8 +                   // traf header
                                           16 +                  // mfhd
                                           8 +                   // moof header
                                           8);

  return box("traf", std::vector<ByteData*>{tfhdBox, tfdtBox, trunBox, sdtpBox});
}

ByteData* Mp4Generator::trak(Mp4Track* track) {
  //  track->duration = track->duration || 0xffffffff;
  return box("trak", std::vector<ByteData*>{tkhd(track), edts(track), mdia(track)});
}

ByteData* Mp4Generator::edts(Mp4Track* track) { return box("edts", toVector(elst(track))); }

ByteData* Mp4Generator::elst(Mp4Track* track) {
  int sampleSize = track->samples.size();
  int sampleDelta = std::floor(track->duration / sampleSize);

  ByteArray elstByteArray(nullptr, 20);
  elstByteArray.setOrder(byteOrder);
  elstByteArray.writeUint8(0x00);
  elstByteArray.writeUint8(0x00);
  elstByteArray.writeUint8(0x00);
  elstByteArray.writeUint8(0x00);
  writeNumbersToByteArray(
      &elstByteArray, {1, (int32_t)track->duration, (int32_t)track->implicitOffset * sampleDelta});
  elstByteArray.writeUint8(0x00);
  elstByteArray.writeUint8(0x01);
  elstByteArray.writeUint8(0x00);
  elstByteArray.writeUint8(0x00);

  return box("elst", toVector(elstByteArray.release().release()));
}

ByteData* Mp4Generator::trex(Mp4Track* track) {
  int id = track->id;
  ByteArray payload(nullptr, 24);
  payload.setOrder(byteOrder);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  writeNumbersToByteArray(&payload, {id});
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x01);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x01);
  payload.writeUint8(0x00);
  payload.writeUint8(0x01);

  return box("trex", toVector(payload.release().release()));
}

ByteData* Mp4Generator::trun(Mp4Track* track, int offset) {
  std::vector<Mp4Sample*> samples = track->samples;
  int len = samples.size();
  int arraylen = 12 + 16 * len;

  offset += 8 + arraylen;
  ByteArray payload(nullptr, arraylen);
  payload.setOrder(byteOrder);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x0f);
  payload.writeUint8(0x01);
  writeNumbersToByteArray(&payload, {len, offset});

  for (Mp4Sample* sample : samples) {
    int32_t duration = sample->duration;
    int size = sample->size;
    Mp4Flags flags = sample->flags;
    int32_t cts = sample->cts;

    uint8_t paddingValue = 0;
    writeNumbersToByteArray(&payload, {duration, size});
    payload.writeUint8((flags.isLeading << 2) | flags.dependsOn);
    payload.writeUint8((flags.isDependedOn << 6) | (flags.hasRedundancy << 4) |
                       (paddingValue << 1) | flags.isNonSync);
    payload.writeUint8(flags.degradPrio & (0xf0 << 8));
    payload.writeUint8(flags.degradPrio & 0x0f);
    writeNumbersToByteArray(&payload, {cts});
  }

  return box("trun", toVector(payload.release().release()));
}

ByteData* Mp4Generator::stts(Mp4Track* track) {
  int sampleCount = track->samples.size();
  int32_t sampleDelta = std::floor(track->duration / sampleCount);

  ByteArray payload(nullptr, 15);
  payload.setOrder(byteOrder);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  writeNumbersToByteArray(&payload, {1, sampleCount, sampleDelta});

  return box("stts", toVector(payload.release().release()));
}

ByteData* Mp4Generator::ctts(Mp4Track* track) {
  int sampleCount = track->samples.size();
  int32_t sampleDelta = std::floor(track->duration / sampleCount);

  ByteArray payload(nullptr, 7 + sampleCount * 8);
  payload.setOrder(byteOrder);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  writeNumbersToByteArray(&payload, {sampleCount});
  for (int i = 0; i < sampleCount; i++) {
    writeNumbersToByteArray(&payload, {1});
    int32_t dts = i * sampleDelta;
    int32_t pts = track->pts[i] * sampleDelta + track->implicitOffset * sampleDelta;
    writeNumbersToByteArray(&payload, {pts - dts});
  }

  return box("ctts", toVector(payload.release().release()));
}

ByteData* Mp4Generator::stss(Mp4Track* track) {
  std::vector<int> iFrames;
  for (Mp4Sample* sample : track->samples) {
    if (sample->flags.isKeyFrame) {
      iFrames.emplace_back(sample->index + 1);
    }
  }
  ByteArray payload(nullptr, 7 + iFrames.size() * 4);
  payload.setOrder(byteOrder);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  payload.writeUint8(0x00);
  writeNumbersToByteArray(&payload, {(int32_t)iFrames.size()});
  for (int frame : iFrames) {
    writeNumbersToByteArray(&payload, {frame});
  }

  return box("stss", toVector(payload.release().release()));
}

void Mp4Generator::writeNumbersToByteArray(ByteArray* byteArray,
                                           std::vector<int32_t> numbers) {
  for (int32_t num : numbers) {
    byteArray->writeInt32(num);
  }
}

ByteData* Mp4Generator::box(std::string type, std::vector<ByteData*> payloads,
                                 bool release) {
  int size = 8;
  int i = payloads.size();
  while (i) {
    i -= 1;
    size += payloads[i]->length();
  }
  ByteArray result(nullptr, size);
  result.setOrder(byteOrder);
  writeNumbersToByteArray(&result, {size});
  ByteData* typeData = getCharCode(type);
  result.writeBytes(typeData->data(), typeData->length());
  delete typeData;

  for (ByteData* payload : payloads) {
    result.writeBytes(payload->data(), payload->length());
    if (release) {
      delete payload;
    }
  }

  return result.release().release();
}

ByteData* Mp4Generator::dinf() {
  return box("dinf", toVector(box("dref", toVector(drefData), false)));
}

std::vector<ByteData*> Mp4Generator::toVector(ByteData* data) {
  return std::vector<ByteData*>{data};
}

int32_t Mp4Generator::getNowTime() {
  std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tp =
      std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
  size_t timestamp = tp.time_since_epoch().count();
  return timestamp / 1000;
}
}  // namespace pag
