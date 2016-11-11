/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/modules/desktop_capture/screen_capturer.h"

#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/desktop_region.h"
#include "webrtc/modules/desktop_capture/screen_capturer_mock_objects.h"

#if defined(WEBRTC_WIN)
#include "webrtc/modules/desktop_capture/win/screen_capturer_win_directx.h"
#endif  // defined(WEBRTC_WIN)

using ::testing::_;

const int kTestSharedMemoryId = 123;

namespace webrtc {

class ScreenCapturerTest : public testing::Test {
 public:
  void SetUp() override {
    capturer_ = DesktopCapturer::CreateScreenCapturer(
        DesktopCaptureOptions::CreateDefault());
  }

 protected:
#if defined(WEBRTC_WIN)
  // Enable allow_directx_capturer in DesktopCaptureOptions, but let
  // DesktopCapturer::CreateScreenCapturer to decide whether a DirectX capturer
  // should be used.
  void MaybeCreateDirectxCapturer() {
    DesktopCaptureOptions options(DesktopCaptureOptions::CreateDefault());
    options.set_allow_directx_capturer(true);
    capturer_ = DesktopCapturer::CreateScreenCapturer(options);
  }

  bool CreateDirectxCapturer() {
    if (!ScreenCapturerWinDirectx::IsSupported()) {
      LOG(LS_WARNING) << "Directx capturer is not supported";
      return false;
    }

    MaybeCreateDirectxCapturer();
    return true;
  }

  void CreateMagnifierCapturer() {
    DesktopCaptureOptions options(DesktopCaptureOptions::CreateDefault());
    options.set_allow_use_magnification_api(true);
    capturer_ = DesktopCapturer::CreateScreenCapturer(options);
  }
#endif  // defined(WEBRTC_WIN)

  std::unique_ptr<DesktopCapturer> capturer_;
  MockScreenCapturerCallback callback_;
};

class FakeSharedMemory : public SharedMemory {
 public:
  FakeSharedMemory(char* buffer, size_t size)
    : SharedMemory(buffer, size, 0, kTestSharedMemoryId),
      buffer_(buffer) {
  }
  virtual ~FakeSharedMemory() {
    delete[] buffer_;
  }
 private:
  char* buffer_;
  RTC_DISALLOW_COPY_AND_ASSIGN(FakeSharedMemory);
};

class FakeSharedMemoryFactory : public SharedMemoryFactory {
 public:
  FakeSharedMemoryFactory() {}
  ~FakeSharedMemoryFactory() override {}

  std::unique_ptr<SharedMemory> CreateSharedMemory(size_t size) override {
    return std::unique_ptr<SharedMemory>(
        new FakeSharedMemory(new char[size], size));
  }

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(FakeSharedMemoryFactory);
};

ACTION_P(SaveUniquePtrArg, dest) {
  *dest = std::move(*arg1);
}

TEST_F(ScreenCapturerTest, GetScreenListAndSelectScreen) {
  webrtc::DesktopCapturer::SourceList screens;
  EXPECT_TRUE(capturer_->GetSourceList(&screens));
  for (const auto& screen : screens) {
    EXPECT_TRUE(capturer_->SelectSource(screen.id));
  }
}

TEST_F(ScreenCapturerTest, StartCapturer) {
  capturer_->Start(&callback_);
}

TEST_F(ScreenCapturerTest, Capture) {
  // Assume that Start() treats the screen as invalid initially.
  std::unique_ptr<DesktopFrame> frame;
  EXPECT_CALL(callback_,
              OnCaptureResultPtr(DesktopCapturer::Result::SUCCESS, _))
      .WillOnce(SaveUniquePtrArg(&frame));

  capturer_->Start(&callback_);
  capturer_->CaptureFrame();

  ASSERT_TRUE(frame);
  EXPECT_GT(frame->size().width(), 0);
  EXPECT_GT(frame->size().height(), 0);
  EXPECT_GE(frame->stride(),
            frame->size().width() * DesktopFrame::kBytesPerPixel);
  EXPECT_TRUE(frame->shared_memory() == NULL);

  // Verify that the region contains whole screen.
  EXPECT_FALSE(frame->updated_region().is_empty());
  DesktopRegion::Iterator it(frame->updated_region());
  ASSERT_TRUE(!it.IsAtEnd());
  EXPECT_TRUE(it.rect().equals(DesktopRect::MakeSize(frame->size())));
  it.Advance();
  EXPECT_TRUE(it.IsAtEnd());
}

#if defined(WEBRTC_WIN)

TEST_F(ScreenCapturerTest, UseSharedBuffers) {
  std::unique_ptr<DesktopFrame> frame;
  EXPECT_CALL(callback_,
              OnCaptureResultPtr(DesktopCapturer::Result::SUCCESS, _))
      .WillOnce(SaveUniquePtrArg(&frame));

  capturer_->Start(&callback_);
  capturer_->SetSharedMemoryFactory(
      std::unique_ptr<SharedMemoryFactory>(new FakeSharedMemoryFactory()));
  capturer_->CaptureFrame();

  ASSERT_TRUE(frame);
  ASSERT_TRUE(frame->shared_memory());
  EXPECT_EQ(frame->shared_memory()->id(), kTestSharedMemoryId);
}

TEST_F(ScreenCapturerTest, UseMagnifier) {
  CreateMagnifierCapturer();
  std::unique_ptr<DesktopFrame> frame;
  EXPECT_CALL(callback_,
              OnCaptureResultPtr(DesktopCapturer::Result::SUCCESS, _))
      .WillOnce(SaveUniquePtrArg(&frame));

  capturer_->Start(&callback_);
  capturer_->CaptureFrame();
  ASSERT_TRUE(frame);
}

TEST_F(ScreenCapturerTest, UseDirectxCapturer) {
  if (!CreateDirectxCapturer()) {
    return;
  }

  std::unique_ptr<DesktopFrame> frame;
  EXPECT_CALL(callback_,
              OnCaptureResultPtr(DesktopCapturer::Result::SUCCESS, _))
      .WillOnce(SaveUniquePtrArg(&frame));

  capturer_->Start(&callback_);
  capturer_->CaptureFrame();
  ASSERT_TRUE(frame);
}

TEST_F(ScreenCapturerTest, UseDirectxCapturerWithSharedBuffers) {
  if (!CreateDirectxCapturer()) {
    return;
  }

  std::unique_ptr<DesktopFrame> frame;
  EXPECT_CALL(callback_,
              OnCaptureResultPtr(DesktopCapturer::Result::SUCCESS, _))
      .WillOnce(SaveUniquePtrArg(&frame));

  capturer_->Start(&callback_);
  capturer_->SetSharedMemoryFactory(
      std::unique_ptr<SharedMemoryFactory>(new FakeSharedMemoryFactory()));
  capturer_->CaptureFrame();
  ASSERT_TRUE(frame);
  ASSERT_TRUE(frame->shared_memory());
  EXPECT_EQ(frame->shared_memory()->id(), kTestSharedMemoryId);
}

#endif  // defined(WEBRTC_WIN)

}  // namespace webrtc
