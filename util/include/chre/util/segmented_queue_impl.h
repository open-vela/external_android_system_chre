/*
 * Copyright (C) 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CHRE_UTIL_SEGMENTED_QUEUE_IMPL_H
#define CHRE_UTIL_SEGMENTED_QUEUE_IMPL_H

#include <type_traits>
#include <utility>

#include "chre/util/segmented_queue.h"

#include "chre/util/container_support.h"

namespace chre {

template <typename ElementType, size_t kBlockSize>
SegmentedQueue<ElementType, kBlockSize>::SegmentedQueue(size_t maxBlockCount,
                                                        size_t staticBlockCount)
    : kMaxBlockCount(maxBlockCount), kStaticBlockCount(staticBlockCount) {
  CHRE_ASSERT(kMaxBlockCount >= kStaticBlockCount);
  CHRE_ASSERT(kStaticBlockCount > 0);
  CHRE_ASSERT(kMaxBlockCount * kBlockSize < SIZE_MAX);
  mRawStoragePtrs.reserve(kMaxBlockCount);
  for (size_t i = 0; i < kStaticBlockCount; i++) {
    pushOneBlock();
  }
}

template <typename ElementType, size_t kBlockSize>
SegmentedQueue<ElementType, kBlockSize>::~SegmentedQueue() {
  clear();
}

template <typename ElementType, size_t kBlockSize>
bool SegmentedQueue<ElementType, kBlockSize>::push_back(
    const ElementType &element) {
  if (!prepareForPush()) {
    return false;
  }
  new (&locateData(mTail)) ElementType(element);
  mSize++;

  return true;
}

template <typename ElementType, size_t kBlockSize>
bool SegmentedQueue<ElementType, kBlockSize>::push_back(ElementType &&element) {
  if (!prepareForPush()) {
    return false;
  }
  new (&locateData(mTail)) ElementType(std::move(element));
  mSize++;

  return true;
}

template <typename ElementType, size_t kBlockSize>
template <typename... Args>
bool SegmentedQueue<ElementType, kBlockSize>::emplace_back(Args &&...args) {
  if (!prepareForPush()) {
    return false;
  }
  new (&locateData(mTail)) ElementType(std::forward<Args>(args)...);
  mSize++;

  return true;
}

template <typename ElementType, size_t kBlockSize>
ElementType &SegmentedQueue<ElementType, kBlockSize>::operator[](size_t index) {
  CHRE_ASSERT(index < mSize);

  return locateData(relativeIndexToAbsolute(index));
}

template <typename ElementType, size_t kBlockSize>
const ElementType &SegmentedQueue<ElementType, kBlockSize>::operator[](
    size_t index) const {
  CHRE_ASSERT(index < mSize);

  return locateData(relativeIndexToAbsolute(index));
}

template <typename ElementType, size_t kBlockSize>
ElementType &SegmentedQueue<ElementType, kBlockSize>::back() {
  CHRE_ASSERT(!empty());

  return locateData(mTail);
}

template <typename ElementType, size_t kBlockSize>
const ElementType &SegmentedQueue<ElementType, kBlockSize>::back() const {
  CHRE_ASSERT(!empty());

  return locateData(mTail);
}

template <typename ElementType, size_t kBlockSize>
ElementType &SegmentedQueue<ElementType, kBlockSize>::front() {
  CHRE_ASSERT(!empty());

  return locateData(mHead);
}

template <typename ElementType, size_t kBlockSize>
const ElementType &SegmentedQueue<ElementType, kBlockSize>::front() const {
  CHRE_ASSERT(!empty());

  return locateData(mHead);
}

template <typename ElementType, size_t kBlockSize>
void SegmentedQueue<ElementType, kBlockSize>::pop_front() {
  CHRE_ASSERT(!empty());

  doRemove(mHead);

  if (mSize == 0) {
    // TODO(b/258860898), Define a more proactive way to deallocate unused
    // block.
    resetEmptyQueue();
  } else {
    mHead = advanceOrWrapAround(mHead);
  }
}

template <typename ElementType, size_t kBlockSize>
bool SegmentedQueue<ElementType, kBlockSize>::remove(size_t index) {
  if (index >= mSize) {
    return false;
  }
  size_t absoluteIndex = relativeIndexToAbsolute(index);
  doRemove(absoluteIndex);
  if (mSize == 0) {
    resetEmptyQueue();
  } else {
    // TODO(b/258557394): optimize by adding check to see if pull from head
    // to tail is more efficient.
    moveElements(advanceOrWrapAround(absoluteIndex), absoluteIndex,
                 absoluteIndexToRelative(mTail) - index);
    mTail = mTail == 0 ? capacity() - 1 : mTail - 1;
  }
  return true;
}

template <typename ElementType, size_t kBlockSize>
bool SegmentedQueue<ElementType, kBlockSize>::pushOneBlock() {
  return insertBlock(mRawStoragePtrs.size());
}

template <typename ElementType, size_t kBlockSize>
bool SegmentedQueue<ElementType, kBlockSize>::insertBlock(size_t blockIndex) {
  // Supporting inserting at any index since we started this data structure as
  // std::deque and would like to support push_front() in the future. This
  // function should not be needed once b/258771255 is implemented.
  CHRE_ASSERT(mRawStoragePtrs.size() != kMaxBlockCount);
  bool success = false;

  Block *newBlockPtr = static_cast<Block *>(memoryAlloc(sizeof(Block)));
  if (newBlockPtr != nullptr) {
    success = mRawStoragePtrs.insert(blockIndex, UniquePtr(newBlockPtr));
    if (success) {
      if (!empty() && mHead >= blockIndex * kBlockSize) {
        // If we are inserting a block before the current mHead, we need to
        // increase the offset since we now have a new gap from mHead to the
        // head of the container.
        mHead += kBlockSize;
      }

      // If mTail is after the new block, we want to move mTail to make sure
      // that the queue is continuous.
      if (mTail >= blockIndex * kBlockSize) {
        moveElements((blockIndex + 1) * kBlockSize, blockIndex * kBlockSize,
                     (mTail + 1) % kBlockSize);
      }
    }
  }
  if (!success) {
    LOG_OOM();
  }
  return success;
}

template <typename ElementType, size_t kBlockSize>
void SegmentedQueue<ElementType, kBlockSize>::moveElements(size_t srcIndex,
                                                           size_t destIndex,
                                                           size_t count) {
  // TODO(b/259281024): Make sure SegmentedQueue::moveElement() does not
  // incorrectly overwrites elements.
  while (count--) {
    doMove(srcIndex, destIndex,
           typename std::is_move_constructible<ElementType>::type());
    srcIndex = advanceOrWrapAround(srcIndex);
    destIndex = advanceOrWrapAround(destIndex);
  }
}

template <typename ElementType, size_t kBlockSize>
void SegmentedQueue<ElementType, kBlockSize>::doMove(size_t srcIndex,
                                                     size_t destIndex,
                                                     std::true_type) {
  new (&locateData(destIndex)) ElementType(std::move(locateData(srcIndex)));
}

template <typename ElementType, size_t kBlockSize>
void SegmentedQueue<ElementType, kBlockSize>::doMove(size_t srcIndex,
                                                     size_t destIndex,
                                                     std::false_type) {
  new (&locateData(destIndex)) ElementType(locateData(srcIndex));
}

template <typename ElementType, size_t kBlockSize>
size_t SegmentedQueue<ElementType, kBlockSize>::relativeIndexToAbsolute(
    size_t index) {
  size_t absoluteIndex = mHead + index;
  if (absoluteIndex >= capacity()) {
    absoluteIndex -= capacity();
  }
  return absoluteIndex;
}

template <typename ElementType, size_t kBlockSize>
size_t SegmentedQueue<ElementType, kBlockSize>::absoluteIndexToRelative(
    size_t index) {
  if (mHead > index) {
    index += capacity();
  }
  return index - mHead;
}

template <typename ElementType, size_t kBlockSize>
bool SegmentedQueue<ElementType, kBlockSize>::prepareForPush() {
  bool success = false;
  if (full()) {
    LOG_OOM();
  } else {
    if (mSize == capacity()) {
      // TODO(b/258771255): index-based insert block should go away when we
      // have a ArrayQueue based container.
      size_t insertBlockIndex = (mTail + 1) / kBlockSize;
      success = insertBlock(insertBlockIndex);
    } else {
      success = true;
    }
    if (success) {
      mTail = advanceOrWrapAround(mTail);
    }
  }
  return success;
}

template <typename ElementType, size_t kBlockSize>
void SegmentedQueue<ElementType, kBlockSize>::clear() {
  if (!std::is_trivially_destructible<ElementType>::value) {
    while (!empty()) {
      pop_front();
    }
  } else {
    mSize = 0;
    mHead = 0;
    mTail = capacity() - 1;
  }
}

template <typename ElementType, size_t kBlockSize>
ElementType &SegmentedQueue<ElementType, kBlockSize>::locateData(size_t index) {
  return mRawStoragePtrs[index / kBlockSize].get()->data()[index % kBlockSize];
}

template <typename ElementType, size_t kBlockSize>
size_t SegmentedQueue<ElementType, kBlockSize>::advanceOrWrapAround(
    size_t index) {
  return index >= capacity() - 1 ? 0 : index + 1;
}

template <typename ElementType, size_t kBlockSize>
void SegmentedQueue<ElementType, kBlockSize>::doRemove(size_t index) {
  mSize--;
  locateData(index).~ElementType();
}

template <typename ElementType, size_t kBlockSize>
void SegmentedQueue<ElementType, kBlockSize>::resetEmptyQueue() {
  CHRE_ASSERT(empty());

  while (mRawStoragePtrs.size() != kStaticBlockCount) {
    mRawStoragePtrs.pop_back();
  }
  mHead = 0;
  mTail = capacity() - 1;
}

}  // namespace chre

#endif  // CHRE_UTIL_SEGMENTED_QUEUE_IMPL_H