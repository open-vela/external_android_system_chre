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

#include "gtest/gtest.h"

#include "chre/util/intrusive_linked_list.h"

using chre::IntrusiveLinkedList;
using chre::LinkedNode;

TEST(IntrusiveLinkedList, EmptyByDefault) {
  IntrusiveLinkedList testLinkedList;
  EXPECT_EQ(testLinkedList.size(), 0);
  EXPECT_TRUE(testLinkedList.empty());
}

TEST(IntrusiveLinkedList, PushReadAndPop) {
  typedef LinkedNode<int> LinkedIntNode;
  IntrusiveLinkedList<LinkedIntNode> testLinkedList;

  LinkedIntNode nodeA(0);
  LinkedIntNode nodeB(1);
  LinkedIntNode nodeC(2);
  testLinkedList.link_back(nodeA);
  testLinkedList.link_back(nodeB);
  testLinkedList.link_back(nodeC);
  EXPECT_EQ(testLinkedList.size(), 3);

  EXPECT_EQ(testLinkedList.front().mItem, nodeA.mItem);
  EXPECT_EQ(testLinkedList.back().mItem, nodeC.mItem);

  testLinkedList.unlink_front();
  EXPECT_EQ(testLinkedList.size(), 2);
  EXPECT_EQ(testLinkedList.front().mItem, nodeB.mItem);

  testLinkedList.unlink_back();
  EXPECT_EQ(testLinkedList.size(), 1);
  EXPECT_EQ(testLinkedList.back().mItem, nodeB.mItem);

  testLinkedList.unlink_back();
  EXPECT_EQ(testLinkedList.size(), 0);
  EXPECT_TRUE(testLinkedList.empty());
}

TEST(IntrusiveLinkedList, CatchInvalidCallToEmptyList) {
  IntrusiveLinkedList testList;
  ASSERT_DEATH(testList.front(), "");
  ASSERT_DEATH(testList.back(), "");
  ASSERT_DEATH(testList.unlink_front(), "");
  ASSERT_DEATH(testList.unlink_back(), "");
}