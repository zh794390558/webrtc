/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/test/run_loop.h"

#include <stdio.h>

namespace webrtc {
namespace test {

void PressEnterToContinue() {
  puts(">> Press ENTER to continue...");
  while (getc(stdin) != '\n' && !feof(stdin));
}
}  // namespace test
}  // namespace webrtc
