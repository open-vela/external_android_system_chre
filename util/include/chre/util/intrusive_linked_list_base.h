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

#ifndef CHRE_UTIL_INTRUSIVE_LINKED_LIST_BASE_H_
#define CHRE_UTIL_INTRUSIVE_LINKED_LIST_BASE_H_

#include <cstddef>

#include "chre/util/non_copyable.h"

namespace chre {

struct Node {
  Node *next = nullptr;
  Node *prev = nullptr;

  bool operator==(Node const &other) {
    return &other == this;
  }

  bool operator!=(Node const &other) {
    return &other != this;
  }
};

class IntrusiveLinkedListBase : public NonCopyable {
 protected:
  /**
   * The sentinel node for easier access to the first and last element of the
   * linked list
   */
  Node mSentinelNode;

  /**
   * Number of elements currently stored in the linked list.
   */
  size_t mSize = 0;

  IntrusiveLinkedListBase() {
    mSentinelNode.next = &mSentinelNode;
    mSentinelNode.prev = &mSentinelNode;
  };

  /**
   * Link a new node to the end of the linked list.
   *
   * @param newNode: The node to push onto the linked list.
   */
  void doLinkBack(Node &newNode);

  /**
   * Unlink a node from the linked list.
   *
   * @param node: The node to remove from the linked list.
   */
  void doUnlinkNode(Node &node);
};

}  // namespace chre

#endif  // CHRE_UTIL_INTRUSIVE_LINKED_LIST_BASE_H_
