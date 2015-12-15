/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_INTERFACE_RW_LOCK_WRAPPER_H_
#define WEBRTC_SYSTEM_WRAPPERS_INTERFACE_RW_LOCK_WRAPPER_H_

#include "webrtc/system_wrappers/interface/thread_annotations.h"

#include "CFlyLockProfiler.h" // FlylinkDC++

// Note, Windows pre-Vista version of RW locks are not supported natively. For
// these OSs regular critical sections have been used to approximate RW lock
// functionality and will therefore have worse performance.

namespace webrtc {

class LOCKABLE RWLockWrapper {
 public:
  static RWLockWrapper* CreateRWLock();
  virtual ~RWLockWrapper() {}

  virtual void AcquireLockExclusive() EXCLUSIVE_LOCK_FUNCTION() = 0;
  virtual void ReleaseLockExclusive() UNLOCK_FUNCTION() = 0;

  virtual void AcquireLockShared() SHARED_LOCK_FUNCTION() = 0;
  virtual void ReleaseLockShared() UNLOCK_FUNCTION() = 0;
};

// RAII extensions of the RW lock. Prevents Acquire/Release missmatches and
// provides more compact locking syntax.
class SCOPED_LOCKABLE ReadLockScoped
#ifdef FLYLINKDC_USE_PROFILER_CS
	: public CFlyLockProfiler
#endif
{
 public:
  explicit ReadLockScoped(RWLockWrapper& rw_lock
#ifdef FLYLINKDC_USE_PROFILER_CS
	  , const char* p_function = nullptr
#endif
	  ) SHARED_LOCK_FUNCTION(rw_lock)
      : rw_lock_(rw_lock)
#ifdef FLYLINKDC_USE_PROFILER_CS
	  , CFlyLockProfiler(p_function)
#endif
  {
    rw_lock_.AcquireLockShared();
#ifdef FLYLINKDC_USE_PROFILER_CS
	log("D:\\ReadSectionLog-lock.txt", 0);
#endif
  }

  ~ReadLockScoped() UNLOCK_FUNCTION() {
    rw_lock_.ReleaseLockShared();
#ifdef FLYLINKDC_USE_PROFILER_CS
	log("D:\\ReadSectionLog-unlock.txt", 0);
#endif
  }

 private:
  RWLockWrapper& rw_lock_;
};

class SCOPED_LOCKABLE WriteLockScoped 
#ifdef FLYLINKDC_USE_PROFILER_CS
	: public CFlyLockProfiler
#endif
{
 public:
  explicit WriteLockScoped(RWLockWrapper& rw_lock
#ifdef FLYLINKDC_USE_PROFILER_CS
	  , const char* p_function = nullptr
#endif
	  ) EXCLUSIVE_LOCK_FUNCTION(rw_lock)
      : rw_lock_(rw_lock) 
#ifdef FLYLINKDC_USE_PROFILER_CS
	  , CFlyLockProfiler(p_function)
#endif
  {
    rw_lock_.AcquireLockExclusive();
#ifdef FLYLINKDC_USE_PROFILER_CS
	log("D:\\WriteSectionLog-lock.txt", 0);
#endif

  }

  ~WriteLockScoped() UNLOCK_FUNCTION() {
    rw_lock_.ReleaseLockExclusive();
#ifdef FLYLINKDC_USE_PROFILER_CS
	log("D:\\WriteSectionLog-unlock.txt", 0);
#endif
  }

 private:
  RWLockWrapper& rw_lock_;
};

#ifdef FLYLINKDC_USE_PROFILER_CS
#define CFlyReadLock(cs) webrtc::ReadLockScoped l_lock(cs,__FUNCTION__);
#define CFlyWriteLock(cs) webrtc::WriteLockScoped l_lock(cs,__FUNCTION__);
#else
#define CFlyReadLock(cs) webrtc::ReadLockScoped l_lock(cs);
#define CFlyWriteLock(cs) webrtc::WriteLockScoped l_lock(cs);
#endif

}  // namespace webrtc

#endif  // WEBRTC_SYSTEM_WRAPPERS_INTERFACE_RW_LOCK_WRAPPER_H_
