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

#include "chre/util/intrusive_list.h"

#include "gtest/gtest.h"

using chre::IntrusiveList;
using chre::ListNode;

TEST(IntrusiveList, EmptyByDefault) {
  IntrusiveList<int> testLinkedList;
  EXPECT_EQ(testLinkedList.size(), 0);
  EXPECT_TRUE(testLinkedList.empty());
}

TEST(IntrusiveList, PushReadAndPop) {
  typedef ListNode<int> ListIntNode;
  IntrusiveList<int> testLinkedList;

  ListIntNode nodeA(0);
  ListIntNode nodeB(1);
  ListIntNode nodeC(2);
  testLinkedList.link_back(&nodeA);
  testLinkedList.link_back(&nodeB);
  testLinkedList.link_back(&nodeC);
  EXPECT_EQ(testLinkedList.size(), 3);

  EXPECT_EQ(testLinkedList.front().item, nodeA.item);
  EXPECT_EQ(testLinkedList.back().item, nodeC.item);

  testLinkedList.unlink_front();
  EXPECT_EQ(testLinkedList.size(), 2);
  EXPECT_EQ(testLinkedList.front().item, nodeB.item);

  testLinkedList.unlink_back();
  EXPECT_EQ(testLinkedList.size(), 1);
  EXPECT_EQ(testLinkedList.back().item, nodeB.item);

  testLinkedList.unlink_back();
  EXPECT_EQ(testLinkedList.size(), 0);
  EXPECT_TRUE(testLinkedList.empty());

  ListIntNode nodeD(4);
  testLinkedList.link_back(&nodeD);
  EXPECT_EQ(testLinkedList.size(), 1);
  EXPECT_EQ(testLinkedList.back().item, nodeD.item);
  EXPECT_EQ(testLinkedList.front().item, nodeD.item);
}

TEST(IntrusiveList, CatchInvalidCallToEmptyList) {
  IntrusiveList<int> testList;
  ASSERT_DEATH(testList.front(), "");
  ASSERT_DEATH(testList.back(), "");
  ASSERT_DEATH(testList.unlink_front(), "");
  ASSERT_DEATH(testList.unlink_back(), "");
}
