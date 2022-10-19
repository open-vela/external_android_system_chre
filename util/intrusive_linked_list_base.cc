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

#include "chre/util/intrusive_linked_list_base.h"

#include "chre/util/container_support.h"

namespace chre {

void IntrusiveLinkedListBase::doLinkBack(Node &newNode) {
  Node *prevNode = mSentinelNode.prev;
  prevNode->next = &newNode;
  newNode.prev = prevNode;
  newNode.next = &mSentinelNode;
  mSentinelNode.prev = &newNode;
  mSize++;
}

void IntrusiveLinkedListBase::doUnlinkNode(Node &node) {
  CHRE_ASSERT(node != mSentinelNode);

  node.prev->next = node.next;
  node.next->prev = node.prev;
  node.next = nullptr;
  node.prev = nullptr;
  mSize--;
}

}  // namespace chre