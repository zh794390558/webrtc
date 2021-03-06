/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <list>
#include <memory>

#include "webrtc/api/call/audio_state.h"
#include "webrtc/call.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log.h"
#include "webrtc/modules/audio_coding/codecs/mock/mock_audio_decoder_factory.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/mock_voice_engine.h"

namespace {

struct CallHelper {
  explicit CallHelper(
      rtc::scoped_refptr<webrtc::AudioDecoderFactory> decoder_factory = nullptr)
      : voice_engine_(decoder_factory) {
    webrtc::AudioState::Config audio_state_config;
    audio_state_config.voice_engine = &voice_engine_;
    webrtc::Call::Config config(&event_log_);
    config.audio_state = webrtc::AudioState::Create(audio_state_config);
    call_.reset(webrtc::Call::Create(config));
  }

  webrtc::Call* operator->() { return call_.get(); }

 private:
  testing::NiceMock<webrtc::test::MockVoiceEngine> voice_engine_;
  webrtc::RtcEventLogNullImpl event_log_;
  std::unique_ptr<webrtc::Call> call_;
};
}  // namespace

namespace webrtc {

TEST(CallTest, ConstructDestruct) {
  CallHelper call;
}

TEST(CallTest, CreateDestroy_AudioSendStream) {
  CallHelper call;
  AudioSendStream::Config config(nullptr);
  config.rtp.ssrc = 42;
  config.voe_channel_id = 123;
  AudioSendStream* stream = call->CreateAudioSendStream(config);
  EXPECT_NE(stream, nullptr);
  call->DestroyAudioSendStream(stream);
}

TEST(CallTest, CreateDestroy_AudioReceiveStream) {
  rtc::scoped_refptr<webrtc::AudioDecoderFactory> decoder_factory(
      new rtc::RefCountedObject<webrtc::MockAudioDecoderFactory>);
  CallHelper call(decoder_factory);
  AudioReceiveStream::Config config;
  config.rtp.remote_ssrc = 42;
  config.voe_channel_id = 123;
  config.decoder_factory = decoder_factory;
  AudioReceiveStream* stream = call->CreateAudioReceiveStream(config);
  EXPECT_NE(stream, nullptr);
  call->DestroyAudioReceiveStream(stream);
}

TEST(CallTest, CreateDestroy_AudioSendStreams) {
  CallHelper call;
  AudioSendStream::Config config(nullptr);
  config.voe_channel_id = 123;
  std::list<AudioSendStream*> streams;
  for (int i = 0; i < 2; ++i) {
    for (uint32_t ssrc = 0; ssrc < 1234567; ssrc += 34567) {
      config.rtp.ssrc = ssrc;
      AudioSendStream* stream = call->CreateAudioSendStream(config);
      EXPECT_NE(stream, nullptr);
      if (ssrc & 1) {
        streams.push_back(stream);
      } else {
        streams.push_front(stream);
      }
    }
    for (auto s : streams) {
      call->DestroyAudioSendStream(s);
    }
    streams.clear();
  }
}

TEST(CallTest, CreateDestroy_AudioReceiveStreams) {
  rtc::scoped_refptr<webrtc::AudioDecoderFactory> decoder_factory(
      new rtc::RefCountedObject<webrtc::MockAudioDecoderFactory>);
  CallHelper call(decoder_factory);
  AudioReceiveStream::Config config;
  config.voe_channel_id = 123;
  config.decoder_factory = decoder_factory;
  std::list<AudioReceiveStream*> streams;
  for (int i = 0; i < 2; ++i) {
    for (uint32_t ssrc = 0; ssrc < 1234567; ssrc += 34567) {
      config.rtp.remote_ssrc = ssrc;
      AudioReceiveStream* stream = call->CreateAudioReceiveStream(config);
      EXPECT_NE(stream, nullptr);
      if (ssrc & 1) {
        streams.push_back(stream);
      } else {
        streams.push_front(stream);
      }
    }
    for (auto s : streams) {
      call->DestroyAudioReceiveStream(s);
    }
    streams.clear();
  }
}

TEST(CallTest, CreateDestroy_FlexfecReceiveStream) {
  CallHelper call;
  FlexfecReceiveStream::Config config;
  config.flexfec_payload_type = 118;
  config.flexfec_ssrc = 38837212;
  config.protected_media_ssrcs = {27273};

  FlexfecReceiveStream* stream = call->CreateFlexfecReceiveStream(config);
  EXPECT_NE(stream, nullptr);
  call->DestroyFlexfecReceiveStream(stream);
}

TEST(CallTest, CreateDestroy_FlexfecReceiveStreams) {
  CallHelper call;
  FlexfecReceiveStream::Config config;
  config.flexfec_payload_type = 118;
  std::list<FlexfecReceiveStream*> streams;

  for (int i = 0; i < 2; ++i) {
    for (uint32_t ssrc = 0; ssrc < 1234567; ssrc += 34567) {
      config.flexfec_ssrc = ssrc;
      config.protected_media_ssrcs = {ssrc + 1};
      FlexfecReceiveStream* stream = call->CreateFlexfecReceiveStream(config);
      EXPECT_NE(stream, nullptr);
      if (ssrc & 1) {
        streams.push_back(stream);
      } else {
        streams.push_front(stream);
      }
    }
    for (auto s : streams) {
      call->DestroyFlexfecReceiveStream(s);
    }
    streams.clear();
  }
}

TEST(CallTest, MultipleFlexfecReceiveStreamsProtectingSingleVideoStream) {
  CallHelper call;
  FlexfecReceiveStream::Config config;
  config.flexfec_payload_type = 118;
  config.protected_media_ssrcs = {1324234};
  FlexfecReceiveStream* stream;
  std::list<FlexfecReceiveStream*> streams;

  config.flexfec_ssrc = 838383;
  stream = call->CreateFlexfecReceiveStream(config);
  EXPECT_NE(stream, nullptr);
  streams.push_back(stream);

  config.flexfec_ssrc = 424993;
  stream = call->CreateFlexfecReceiveStream(config);
  EXPECT_NE(stream, nullptr);
  streams.push_back(stream);

  config.flexfec_ssrc = 99383;
  stream = call->CreateFlexfecReceiveStream(config);
  EXPECT_NE(stream, nullptr);
  streams.push_back(stream);

  config.flexfec_ssrc = 5548;
  stream = call->CreateFlexfecReceiveStream(config);
  EXPECT_NE(stream, nullptr);
  streams.push_back(stream);

  for (auto s : streams) {
    call->DestroyFlexfecReceiveStream(s);
  }
}

}  // namespace webrtc
