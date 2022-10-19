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

#ifndef CHRE_UTIL_INTRUSIVE_LINKED_LIST_IMPL_H_
#define CHRE_UTIL_INTRUSIVE_LINKED_LIST_IMPL_H_

#include "chre/util/intrusive_linked_list.h"

#include "chre/util/container_support.h"

namespace chre {

template <typename TypedNode>
void IntrusiveLinkedList<TypedNode>::link_back(TypedNode &element) {
  return IntrusiveLinkedListBase::doLinkBack(element.mNode);
}

template <typename TypedNode>
TypedNode &IntrusiveLinkedList<TypedNode>::front() {
  CHRE_ASSERT(mSize > 0);
  return *reinterpret_cast<TypedNode *>(mSentinelNode.next);
}

template <typename TypedNode>
const TypedNode &IntrusiveLinkedList<TypedNode>::front() const {
  CHRE_ASSERT(mSize > 0);
  return *reinterpret_cast<const TypedNode *>(mSentinelNode.next);
}

template <typename TypedNode>
void IntrusiveLinkedList<TypedNode>::unlink_front() {
  CHRE_ASSERT(mSize > 0);
  IntrusiveLinkedListBase::doUnlinkNode(*mSentinelNode.next);
}

template <typename TypedNode>
TypedNode &IntrusiveLinkedList<TypedNode>::back() {
  CHRE_ASSERT(mSize > 0);
  return *reinterpret_cast<TypedNode *>(mSentinelNode.prev);
}

template <typename TypedNode>
const TypedNode &IntrusiveLinkedList<TypedNode>::back() const {
  CHRE_ASSERT(mSize > 0);
  return *reinterpret_cast<const TypedNode *>(mSentinelNode.prev);
}

template <typename TypedNode>
void IntrusiveLinkedList<TypedNode>::unlink_back() {
  CHRE_ASSERT(mSize > 0);
  IntrusiveLinkedListBase::doUnlinkNode(*mSentinelNode.prev);
}

}  // namespace chre

#endif  // CHRE_UTIL_INTRUSIVE_LINKED_LIST_IMPL_H_
