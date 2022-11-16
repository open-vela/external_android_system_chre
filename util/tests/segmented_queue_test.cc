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

#include "chre/util/segmented_queue.h"

#include "chre/util/non_copyable.h"
#include "gtest/gtest.h"
using chre::SegmentedQueue;

TEST(SegmentedQueue, InitialzedState) {
  SegmentedQueue<int, 10> segmentedQueue(10);

  EXPECT_NE(segmentedQueue.block_count(), 0);
  EXPECT_NE(segmentedQueue.capacity(), 0);
  EXPECT_EQ(segmentedQueue.size(), 0);
}

TEST(SegmentedQueue, pushAndRead) {
  constexpr uint8_t blockSize = 5;
  constexpr uint8_t maxBlockCount = 3;
  SegmentedQueue<int, blockSize> segmentedQueue(maxBlockCount);

  for (uint8_t blockIndex = 0; blockIndex < maxBlockCount; blockIndex++) {
    for (uint8_t index = 0; index < blockSize; index++) {
      int value = blockIndex * blockSize + index;
      EXPECT_TRUE(segmentedQueue.push_back(value));
      EXPECT_EQ(segmentedQueue.size(), value + 1);
      EXPECT_EQ(segmentedQueue[value], value);
      EXPECT_EQ(segmentedQueue.back(), value);
    }
  }
  EXPECT_FALSE(segmentedQueue.push_back(10000));
  EXPECT_EQ(segmentedQueue.size(), maxBlockCount * blockSize);
  EXPECT_TRUE(segmentedQueue.full());
}

constexpr int kConstructedMagic = 0xdeadbeef;
class CopyableButNonMovable {
 public:
  CopyableButNonMovable(int value) : mValue(value) {}

  CopyableButNonMovable(const CopyableButNonMovable &other) {
    mValue = other.mValue;
  }

  CopyableButNonMovable &operator=(const CopyableButNonMovable &other) {
    CHRE_ASSERT(mMagic == kConstructedMagic);
    mValue = other.mValue;
    return *this;
  }

  CopyableButNonMovable(CopyableButNonMovable &&other) = delete;
  CopyableButNonMovable &operator=(CopyableButNonMovable &&other) = delete;

  int getValue() const {
    return mValue;
  }

 private:
  int mMagic = kConstructedMagic;
  int mValue;
};

TEST(SegmentedQueue, pushAndReadCopyableButNonMovable) {
  constexpr uint8_t blockSize = 5;
  constexpr uint8_t maxBlockCount = 3;
  SegmentedQueue<CopyableButNonMovable, blockSize> segmentedQueue(
      maxBlockCount);

  for (uint8_t blockIndex = 0; blockIndex < maxBlockCount; blockIndex++) {
    for (uint8_t index = 0; index < blockSize; index++) {
      int value = blockIndex * blockSize + index;
      CopyableButNonMovable cbnm(value);
      EXPECT_TRUE(segmentedQueue.push_back(cbnm));
      EXPECT_EQ(segmentedQueue.size(), value + 1);
      EXPECT_EQ(segmentedQueue[value].getValue(), value);
      EXPECT_EQ(segmentedQueue.back().getValue(), value);
    }
  }
}

class MovableButNonCopyable : public chre::NonCopyable {
 public:
  MovableButNonCopyable(int value) : mValue(value) {}

  MovableButNonCopyable(MovableButNonCopyable &&other) {
    mValue = other.mValue;
    other.mValue = -1;
  }

  MovableButNonCopyable &operator=(MovableButNonCopyable &&other) {
    CHRE_ASSERT(mMagic == kConstructedMagic);
    mValue = other.mValue;
    other.mValue = -1;
    return *this;
  }

  int getValue() const {
    return mValue;
  }

 private:
  int mMagic = kConstructedMagic;
  int mValue;
};

TEST(SegmentedQueue, pushAndReadMovableButNonCopyable) {
  constexpr uint8_t blockSize = 5;
  constexpr uint8_t maxBlockCount = 3;
  SegmentedQueue<MovableButNonCopyable, blockSize> segmentedQueue(
      maxBlockCount);

  for (uint8_t blockIndex = 0; blockIndex < maxBlockCount; blockIndex++) {
    for (uint8_t index = 0; index < blockSize; index++) {
      int value = blockIndex * blockSize + index;
      EXPECT_TRUE(segmentedQueue.emplace_back(value));
      EXPECT_EQ(segmentedQueue.size(), value + 1);
      EXPECT_EQ(segmentedQueue[value].getValue(), value);
      EXPECT_EQ(segmentedQueue.back().getValue(), value);
    }
  }
}

TEST(SegmentedQueue, ReadAndPop) {
  constexpr uint8_t blockSize = 5;
  constexpr uint8_t maxBlockCount = 3;
  SegmentedQueue<int, blockSize> segmentedQueue(maxBlockCount);

  for (uint32_t index = 0; index < blockSize * maxBlockCount; index++) {
    EXPECT_TRUE(segmentedQueue.push_back(index));
  }

  uint8_t originalQueueSize = segmentedQueue.size();
  for (uint8_t index = 0; index < originalQueueSize; index++) {
    EXPECT_EQ(segmentedQueue[index], index);
  }

  size_t capacityBeforePop = segmentedQueue.capacity();
  while (!segmentedQueue.empty()) {
    ASSERT_EQ(segmentedQueue.front(),
              originalQueueSize - segmentedQueue.size());
    segmentedQueue.pop_front();
  }

  EXPECT_EQ(segmentedQueue.size(), 0);
  EXPECT_TRUE(segmentedQueue.empty());
  EXPECT_LT(segmentedQueue.capacity(), capacityBeforePop);
  EXPECT_GT(segmentedQueue.capacity(), 0);
}

TEST(SegmentedQueue, RemoveTest) {
  constexpr uint8_t blockSize = 2;
  constexpr uint8_t maxBlockCount = 3;
  SegmentedQueue<int, blockSize> segmentedQueue(maxBlockCount);

  for (uint32_t index = 0; index < blockSize * maxBlockCount; index++) {
    EXPECT_TRUE(segmentedQueue.push_back(index));
  }

  // segmentedQueue = [[0, 1], [2, 3], [4, 5]]
  EXPECT_FALSE(segmentedQueue.remove(segmentedQueue.size()));

  EXPECT_TRUE(segmentedQueue.remove(4));
  EXPECT_EQ(segmentedQueue[4], 5);
  EXPECT_EQ(segmentedQueue[3], 3);
  EXPECT_EQ(segmentedQueue.size(), 5);

  EXPECT_TRUE(segmentedQueue.remove(1));
  EXPECT_EQ(segmentedQueue[3], 5);
  EXPECT_EQ(segmentedQueue[1], 2);
  EXPECT_EQ(segmentedQueue[0], 0);
  EXPECT_EQ(segmentedQueue.size(), 4);

  size_t currentSize = segmentedQueue.size();
  size_t capacityBeforeRemove = segmentedQueue.capacity();
  while (currentSize--) {
    EXPECT_TRUE(segmentedQueue.remove(0));
  }

  EXPECT_EQ(segmentedQueue.size(), 0);
  EXPECT_TRUE(segmentedQueue.empty());
  EXPECT_LT(segmentedQueue.capacity(), capacityBeforeRemove);
  EXPECT_GT(segmentedQueue.capacity(), 0);
}

TEST(SegmentedQueue, MiddleBlockTest) {
  // This test tests that the SegmentedQueue will behave correctly when
  // the reference of front() and back() are not aligned to the head/back
  // of a block.

  constexpr uint8_t blockSize = 3;
  constexpr uint8_t maxBlockCount = 3;
  SegmentedQueue<int, blockSize> segmentedQueue(maxBlockCount);

  for (uint32_t index = 0; index < blockSize * (maxBlockCount - 1); index++) {
    EXPECT_TRUE(segmentedQueue.push_back(index));
  }

  segmentedQueue.pop_front();
  segmentedQueue.pop_front();
  EXPECT_TRUE(segmentedQueue.push_back(6));
  EXPECT_TRUE(segmentedQueue.push_back(7));

  // segmentedQueue = [[6, 7, 2], [3, 4, 5], [X]]
  EXPECT_EQ(segmentedQueue.front(), 2);
  EXPECT_EQ(segmentedQueue.back(), 7);

  EXPECT_TRUE(segmentedQueue.push_back(8));
  EXPECT_EQ(segmentedQueue.back(), 8);

  // segmentedQueue = [[x, x, 2], [3, 4, 5], [6, 7, 8]]
  EXPECT_TRUE(segmentedQueue.push_back(9));
  EXPECT_TRUE(segmentedQueue.push_back(10));

  for (int i = 0; i < segmentedQueue.size(); i++) {
    EXPECT_EQ(segmentedQueue[i], i + 2);
  }
}
