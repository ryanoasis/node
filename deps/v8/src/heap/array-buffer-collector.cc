// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/array-buffer-collector.h"

#include "src/heap/array-buffer-tracker.h"
#include "src/heap/heap-inl.h"

namespace v8 {
namespace internal {

void ArrayBufferCollector::AddGarbageAllocations(
    std::vector<JSArrayBuffer::Allocation>* allocations) {
  base::LockGuard<base::Mutex> guard(&allocations_mutex_);
  allocations_.push_back(allocations);
}

void ArrayBufferCollector::FreeAllocations() {
  base::LockGuard<base::Mutex> guard(&allocations_mutex_);
  for (std::vector<JSArrayBuffer::Allocation>* allocations : allocations_) {
    for (auto alloc : *allocations) {
      JSArrayBuffer::FreeBackingStore(heap_->isolate(), alloc);
    }
    delete allocations;
  }
  allocations_.clear();
}

class ArrayBufferCollector::FreeingTask final : public CancelableTask {
 public:
  explicit FreeingTask(Heap* heap)
      : CancelableTask(heap->isolate()), heap_(heap) {}

  virtual ~FreeingTask() {}

 private:
  void RunInternal() final {
    heap_->array_buffer_collector()->FreeAllocations();
  }

  Heap* heap_;
};

void ArrayBufferCollector::FreeAllocationsOnBackgroundThread() {
  heap_->account_external_memory_concurrently_freed();
  if (heap_->use_tasks() && FLAG_concurrent_array_buffer_freeing) {
    FreeingTask* task = new FreeingTask(heap_);
    V8::GetCurrentPlatform()->CallOnBackgroundThread(
        task, v8::Platform::kShortRunningTask);
  } else {
    // Fallback for when concurrency is disabled/restricted.
    FreeAllocations();
  }
}

}  // namespace internal
}  // namespace v8
