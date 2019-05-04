/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "stdinc.h"

#include "rtc_base/synchronization/rw_lock_wrapper.h"

#if defined(_WIN32)
#include "rtc_base/synchronization/rw_lock_win.h"
#include "rtc_base/synchronization/rw_lock_winxp_win.h"
#else
#include "rtc_base/synchronization/rw_lock_posix.h"
#endif

namespace webrtc {

RWLockWrapper* RWLockWrapper::CreateRWLock() {
#ifdef _WIN32
    // Native implementation is faster, so use that if available.
    RWLockWrapper* lock = RWLockWin::Create();
    if (lock) {
        return lock;
}
    return new RWLockWinXP();

#else
  return RWLockPosix::Create();
#endif
}

}  // namespace webrtc
