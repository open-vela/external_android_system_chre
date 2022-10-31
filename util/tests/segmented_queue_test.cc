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