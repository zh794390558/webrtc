/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCDispatcher+Private.h"

static dispatch_queue_t kAudioSessionQueue = nil;
static dispatch_queue_t kCaptureSessionQueue = nil;

@implementation RTCDispatcher

+ (void)initialize {
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    kAudioSessionQueue = dispatch_queue_create(
        "org.webrtc.RTCDispatcherAudioSession",
        DISPATCH_QUEUE_SERIAL);
    kCaptureSessionQueue = dispatch_queue_create(
        "org.webrtc.RTCDispatcherCaptureSession",
        DISPATCH_QUEUE_SERIAL);
  });
}

+ (void)dispatchAsyncOnType:(RTCDispatcherQueueType)dispatchType
                      block:(dispatch_block_t)block {
  dispatch_queue_t queue = [self dispatchQueueForType:dispatchType];
  dispatch_async(queue, block);
}

#pragma mark - Private

+ (dispatch_queue_t)dispatchQueueForType:(RTCDispatcherQueueType)dispatchType {
  switch (dispatchType) {
    case RTCDispatcherTypeMain:
      return dispatch_get_main_queue();
    case RTCDispatcherTypeCaptureSession:
      return kCaptureSessionQueue;
    case RTCDispatcherTypeAudioSession:
      return kAudioSessionQueue;
  }
}

@end

