// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_MEMORY_PROFILER_ALLOCATION_CONTEXT_H_
#define BASE_TRACE_EVENT_MEMORY_PROFILER_ALLOCATION_CONTEXT_H_

#include <vector>

#include "base/atomicops.h"
#include "base/base_export.h"
#include "base/containers/small_map.h"

namespace base {
namespace trace_event {

// When heap profiling is enabled, tracing keeps track of the allocation
// context for each allocation intercepted. It is generated by the
// |AllocationContextTracker| which keeps stacks of context in TLS.
// The tracker is initialized lazily.

using StackFrame = const char*;

// A simple stack of |StackFrame| that unlike |std::stack| allows iterating
// the stack and guards for underflow.
class BASE_EXPORT AllocationStack {
 public:
  // Incrementing the iterator iterates down the stack.
  using ConstIterator = std::vector<StackFrame>::const_reverse_iterator;

  AllocationStack();
  ~AllocationStack();

  inline ConstIterator top() const { return stack_.rbegin(); }
  inline ConstIterator bottom() const { return stack_.rend(); }

  inline void push(StackFrame frame) {
    // Impose a limit on the height to verify that every push is popped, because
    // in practice the pseudo stack never grows higher than ~20 frames.
    DCHECK_LT(stack_.size(), 128u);
    stack_.push_back(frame);
  }

  inline void pop() {
    if (!stack_.empty())
      stack_.pop_back();
  }

 private:
  std::vector<StackFrame> stack_;

  DISALLOW_COPY_AND_ASSIGN(AllocationStack);
};

class BASE_EXPORT AllocationContext {
  // TODO(ruuda): Fill this in a follow-up CL.
};

// The allocation context tracker keeps track of thread-local context for heap
// profiling. It includes a pseudo stack of trace events, and it might contain
// arbitrary (key, value) context. On every allocation the tracker provides a
// snapshot of its context in the form of an |AllocationContext| that is to be
// stored together with the allocation details.
class BASE_EXPORT AllocationContextTracker {
 public:
  // Globally enables capturing allocation context.
  // TODO(ruuda): Should this be replaced by |EnableCapturing| in the future?
  // Or at least have something that guards agains enable -> disable -> enable?
  static void SetCaptureEnabled(bool enabled);

  // Returns whether capturing allocation context is enabled globally.
  inline static bool capture_enabled() {
    // A little lag after heap profiling is enabled or disabled is fine, it is
    // more important that the check is as cheap as possible when capturing is
    // not enabled, so do not issue a memory barrier in the fast path.
    if (subtle::NoBarrier_Load(&capture_enabled_) == 0)
        return false;

    // In the slow path, an acquire load is required to pair with the release
    // store in |SetCaptureEnabled|. This is to ensure that the TLS slot for
    // the thread-local allocation context tracker has been initialized if
    // |capture_enabled| returns true.
    return subtle::Acquire_Load(&capture_enabled_) != 0;
  }

  // Pushes a frame onto the thread-local pseudo stack.
  static void PushPseudoStackFrame(StackFrame frame);

  // Pops a frame from the thread-local pseudo stack.
  static void PopPseudoStackFrame(StackFrame frame);

  // Sets a thread-local (key, value) pair.
  static void SetContextField(const char* key, const char* value);

  // Removes the (key, value) pair with the specified key from the thread-local
  // context.
  static void UnsetContextField(const char* key);

  // Returns a snapshot of the current thread-local context.
  static AllocationContext GetContext();

  // TODO(ruuda): Remove in a follow-up CL, this is only used for testing now.
  static AllocationStack* GetPseudoStackForTesting();

  ~AllocationContextTracker();

 private:
  AllocationContextTracker();

  static AllocationContextTracker* GetThreadLocalTracker();

  static subtle::Atomic32 capture_enabled_;

  // The pseudo stack where frames are |TRACE_EVENT| names.
  AllocationStack pseudo_stack_;

  // A dictionary of arbitrary context.
  SmallMap<std::map<const char*, const char*>> context_;

  DISALLOW_COPY_AND_ASSIGN(AllocationContextTracker);
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_MEMORY_PROFILER_ALLOCATION_CONTEXT_H_
