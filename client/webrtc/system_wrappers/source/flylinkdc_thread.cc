/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "stdinc.h" // [+]FlylinkDC++

#include "webrtc/system_wrappers/interface/thread_wrapper.h"

#if defined(_WIN32)
#include "webrtc/system_wrappers/source/thread_win.h"
#else
#include "webrtc/system_wrappers/source/thread_posix.h"
#endif

namespace webrtc {
ThreadWrapper* ThreadWrapper::CreateThread(ThreadRunFunction func,
                                           ThreadObj obj, ThreadPriority prio,
                                           const char* threadName)
{
#if defined(_WIN32)
    return new ThreadWindows(func, obj, prio, threadName);
#else
    return ThreadPosix::Create(func, obj, prio, threadName);
#endif
}
} // namespace webrtc
