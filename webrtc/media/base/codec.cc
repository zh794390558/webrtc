/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/base/codec.h"

#include <algorithm>
#include <sstream>

#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/base/stringutils.h"

namespace cricket {

const int kMaxPayloadId = 127;

bool FeedbackParam::operator==(const FeedbackParam& other) const {
  return _stricmp(other.id().c_str(), id().c_str()) == 0 &&
      _stricmp(other.param().c_str(), param().c_str()) == 0;
}

bool FeedbackParams::operator==(const FeedbackParams& other) const {
  return params_ == other.params_;
}

bool FeedbackParams::Has(const FeedbackParam& param) const {
  return std::find(params_.begin(), params_.end(), param) != params_.end();
}

void FeedbackParams::Add(const FeedbackParam& param) {
  if (param.id().empty()) {
    return;
  }
  if (Has(param)) {
    // Param already in |this|.
    return;
  }
  params_.push_back(param);
  ASSERT(!HasDuplicateEntries());
}

void FeedbackParams::Intersect(const FeedbackParams& from) {
  std::vector<FeedbackParam>::iterator iter_to = params_.begin();
  while (iter_to != params_.end()) {
    if (!from.Has(*iter_to)) {
      iter_to = params_.erase(iter_to);
    } else {
      ++iter_to;
    }
  }
}

bool FeedbackParams::HasDuplicateEntries() const {
  for (std::vector<FeedbackParam>::const_iterator iter = params_.begin();
       iter != params_.end(); ++iter) {
    for (std::vector<FeedbackParam>::const_iterator found = iter + 1;
         found != params_.end(); ++found) {
      if (*found == *iter) {
        return true;
      }
    }
  }
  return false;
}

Codec::Codec(int id, const std::string& name, int clockrate)
    : id(id), name(name), clockrate(clockrate) {}

Codec::Codec() : id(0), clockrate(0) {}

Codec::Codec(const Codec& c) = default;
Codec::Codec(Codec&& c) = default;
Codec::~Codec() = default;
Codec& Codec::operator=(const Codec& c) = default;
Codec& Codec::operator=(Codec&& c) = default;

bool Codec::operator==(const Codec& c) const {
  return this->id == c.id &&  // id is reserved in objective-c
         name == c.name && clockrate == c.clockrate && params == c.params &&
         feedback_params == c.feedback_params;
}

bool Codec::Matches(const Codec& codec) const {
  // Match the codec id/name based on the typical static/dynamic name rules.
  // Matching is case-insensitive.
  const int kMaxStaticPayloadId = 95;
  return (id <= kMaxStaticPayloadId || codec.id <= kMaxStaticPayloadId)
             ? (id == codec.id)
             : (_stricmp(name.c_str(), codec.name.c_str()) == 0);
}

bool Codec::GetParam(const std::string& name, std::string* out) const {
  CodecParameterMap::const_iterator iter = params.find(name);
  if (iter == params.end())
    return false;
  *out = iter->second;
  return true;
}

bool Codec::GetParam(const std::string& name, int* out) const {
  CodecParameterMap::const_iterator iter = params.find(name);
  if (iter == params.end())
    return false;
  return rtc::FromString(iter->second, out);
}

void Codec::SetParam(const std::string& name, const std::string& value) {
  params[name] = value;
}

void Codec::SetParam(const std::string& name, int value)  {
  params[name] = rtc::ToString(value);
}

bool Codec::RemoveParam(const std::string& name) {
  return params.erase(name) == 1;
}

void Codec::AddFeedbackParam(const FeedbackParam& param) {
  feedback_params.Add(param);
}

bool Codec::HasFeedbackParam(const FeedbackParam& param) const {
  return feedback_params.Has(param);
}

void Codec::IntersectFeedbackParams(const Codec& other) {
  feedback_params.Intersect(other.feedback_params);
}

webrtc::RtpCodecParameters Codec::ToCodecParameters() const {
  webrtc::RtpCodecParameters codec_params;
  codec_params.payload_type = id;
  codec_params.mime_type = name;
  codec_params.clock_rate = clockrate;
  return codec_params;
}

AudioCodec::AudioCodec(int id,
                       const std::string& name,
                       int clockrate,
                       int bitrate,
                       size_t channels)
    : Codec(id, name, clockrate), bitrate(bitrate), channels(channels) {}

AudioCodec::AudioCodec() : Codec(), bitrate(0), channels(0) {
}

AudioCodec::AudioCodec(const AudioCodec& c) = default;
AudioCodec::AudioCodec(AudioCodec&& c) = default;
AudioCodec& AudioCodec::operator=(const AudioCodec& c) = default;
AudioCodec& AudioCodec::operator=(AudioCodec&& c) = default;

bool AudioCodec::operator==(const AudioCodec& c) const {
  return bitrate == c.bitrate && channels == c.channels && Codec::operator==(c);
}

bool AudioCodec::Matches(const AudioCodec& codec) const {
  // If a nonzero clockrate is specified, it must match the actual clockrate.
  // If a nonzero bitrate is specified, it must match the actual bitrate,
  // unless the codec is VBR (0), where we just force the supplied value.
  // The number of channels must match exactly, with the exception
  // that channels=0 is treated synonymously as channels=1, per RFC
  // 4566 section 6: " [The channels] parameter is OPTIONAL and may be
  // omitted if the number of channels is one."
  // Preference is ignored.
  // TODO(juberti): Treat a zero clockrate as 8000Hz, the RTP default clockrate.
  return Codec::Matches(codec) &&
      ((codec.clockrate == 0 /*&& clockrate == 8000*/) ||
          clockrate == codec.clockrate) &&
      (codec.bitrate == 0 || bitrate <= 0 || bitrate == codec.bitrate) &&
      ((codec.channels < 2 && channels < 2) || channels == codec.channels);
}

webrtc::RtpCodecParameters AudioCodec::ToCodecParameters() const {
  webrtc::RtpCodecParameters codec_params = Codec::ToCodecParameters();
  codec_params.channels = channels;
  return codec_params;
}

std::string AudioCodec::ToString() const {
  std::ostringstream os;
  os << "AudioCodec[" << id << ":" << name << ":" << clockrate << ":" << bitrate
     << ":" << channels << "]";
  return os.str();
}

std::string VideoCodec::ToString() const {
  std::ostringstream os;
  os << "VideoCodec[" << id << ":" << name << "]";
  return os.str();
}

VideoCodec::VideoCodec(int id, const std::string& name)
    : Codec(id, name, kVideoCodecClockrate) {}

VideoCodec::VideoCodec(const std::string& name)
    : VideoCodec(0 /* id */, name) {}

VideoCodec::VideoCodec() : Codec() {
  clockrate = kVideoCodecClockrate;
}

VideoCodec::VideoCodec(const VideoCodec& c) = default;
VideoCodec::VideoCodec(VideoCodec&& c) = default;
VideoCodec& VideoCodec::operator=(const VideoCodec& c) = default;
VideoCodec& VideoCodec::operator=(VideoCodec&& c) = default;

bool VideoCodec::operator==(const VideoCodec& c) const {
  return Codec::operator==(c);
}

VideoCodec VideoCodec::CreateRtxCodec(int rtx_payload_type,
                                      int associated_payload_type) {
  VideoCodec rtx_codec(rtx_payload_type, kRtxCodecName);
  rtx_codec.SetParam(kCodecParamAssociatedPayloadType, associated_payload_type);
  return rtx_codec;
}

VideoCodec::CodecType VideoCodec::GetCodecType() const {
  const char* payload_name = name.c_str();
  if (_stricmp(payload_name, kRedCodecName) == 0) {
    return CODEC_RED;
  }
  if (_stricmp(payload_name, kUlpfecCodecName) == 0) {
    return CODEC_ULPFEC;
  }
  if (_stricmp(payload_name, kFlexfecCodecName) == 0) {
    return CODEC_FLEXFEC;
  }
  if (_stricmp(payload_name, kRtxCodecName) == 0) {
    return CODEC_RTX;
  }

  return CODEC_VIDEO;
}

bool VideoCodec::ValidateCodecFormat() const {
  if (id < 0 || id > 127) {
    LOG(LS_ERROR) << "Codec with invalid payload type: " << ToString();
    return false;
  }
  if (GetCodecType() != CODEC_VIDEO) {
    return true;
  }

  // Video validation from here on.
  int min_bitrate = -1;
  int max_bitrate = -1;
  if (GetParam(kCodecParamMinBitrate, &min_bitrate) &&
      GetParam(kCodecParamMaxBitrate, &max_bitrate)) {
    if (max_bitrate < min_bitrate) {
      LOG(LS_ERROR) << "Codec with max < min bitrate: " << ToString();
      return false;
    }
  }
  return true;
}

DataCodec::DataCodec(int id, const std::string& name)
    : Codec(id, name, kDataCodecClockrate) {}

DataCodec::DataCodec() : Codec() {
  clockrate = kDataCodecClockrate;
}

DataCodec::DataCodec(const DataCodec& c) = default;
DataCodec::DataCodec(DataCodec&& c) = default;
DataCodec& DataCodec::operator=(const DataCodec& c) = default;
DataCodec& DataCodec::operator=(DataCodec&& c) = default;

std::string DataCodec::ToString() const {
  std::ostringstream os;
  os << "DataCodec[" << id << ":" << name << "]";
  return os.str();
}

bool HasNack(const Codec& codec) {
  return codec.HasFeedbackParam(
      FeedbackParam(kRtcpFbParamNack, kParamValueEmpty));
}

bool HasRemb(const Codec& codec) {
  return codec.HasFeedbackParam(
      FeedbackParam(kRtcpFbParamRemb, kParamValueEmpty));
}

bool HasTransportCc(const Codec& codec) {
  return codec.HasFeedbackParam(
      FeedbackParam(kRtcpFbParamTransportCc, kParamValueEmpty));
}

bool CodecNamesEq(const std::string& name1, const std::string& name2) {
  return CodecNamesEq(name1.c_str(), name2.c_str());
}

bool CodecNamesEq(const char* name1, const char* name2) {
  return _stricmp(name1, name2) == 0;
}

webrtc::VideoCodecType CodecTypeFromName(const std::string& name) {
  if (CodecNamesEq(name.c_str(), kVp8CodecName)) {
    return webrtc::kVideoCodecVP8;
  } else if (CodecNamesEq(name.c_str(), kVp9CodecName)) {
    return webrtc::kVideoCodecVP9;
  } else if (CodecNamesEq(name.c_str(), kH264CodecName)) {
    return webrtc::kVideoCodecH264;
  }
  return webrtc::kVideoCodecUnknown;
}

bool IsCodecSupported(const std::vector<VideoCodec>& supported_codecs,
                      const VideoCodec& codec) {
  return std::any_of(supported_codecs.begin(), supported_codecs.end(),
                     [&codec](const VideoCodec& supported_codec) -> bool {
                       return CodecNamesEq(codec.name, supported_codec.name);
                     });
}

}  // namespace cricket
