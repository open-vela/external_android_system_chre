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

#ifndef CHRE_UTIL_INTRUSIVE_LINKED_LIST_H_
#define CHRE_UTIL_INTRUSIVE_LINKED_LIST_H_

#include <type_traits>

#include "chre/util/intrusive_linked_list_base.h"

namespace chre {

template <typename ElementType>
struct LinkedNode {
  /**
   * Node that allows the linked list to link data.
   * This need to be the first member of LinkedNode or the reinterpret_cast
   * between Node and LinkedNode will fail.
   */
  Node mNode;

  /**
   * The data that the user wants to store.
   */
  ElementType mItem;

  LinkedNode(ElementType item_) : mItem(item_) {
    // Check if the ElementType is appropriate. Inappropriate ElementType or
    // LinkedNode will lead to wrong behavior of the reinterpret_cast between
    // Node and LinkedNode that we use the retrieve item.
    static_assert(std::is_standard_layout<LinkedNode<ElementType>>::value,
                  "must be std layout to alias");
    static_assert(offsetof(LinkedNode, mNode) == 0,
                  "mNode must be the first element");
  };
};

/**
 * A container for storing data in a linked list. Note that this container does
 * not allocate any memory, the caller need to manage the memory of the
 * data/node that it wants to insert.
 *
 * Caller need to turn the data into nodes before using the linked list to
 * manage data.
 *
 * For example:
 *  typedef LinkedNode<int> LinkedIntNode;
 *  IntrusiveLinkedList<LinkedIntNode> myList;
 *  LinkedIntNode node(10);
 *  myList.push_back(node);
 *
 * @tparam TypedNode created by using @LinkedNode
 */
template <typename TypedNode = Node>
class IntrusiveLinkedList : private IntrusiveLinkedListBase {
 public:
  /**
   * Default construct a new Intrusive Linked List.
   */
  IntrusiveLinkedList() = default;

  /**
   * Examines if the linked list is empty.
   *
   * @return true if the linked list has no linked node.
   */
  bool empty() const {
    return mSize == 0;
  }

  /**
   * Returns the number of nodes stored in this linked list.
   *
   * @return The number of nodes in the linked list.
   */
  size_t size() const {
    return mSize;
  }

  /**
   * Link a new node to the end of the linked list.
   *
   * @param element: the node to push to the pack of the linked list.
   */
  void link_back(TypedNode &element);

  /**
   * Returns a reference to the first node of the linked list.
   * It is not allowed to call this on a empty list.
   *
   * @return The first node of the linked list
   */
  TypedNode &front();
  const TypedNode &front() const;

  /**
   * Unlink the first node from the list.
   * It is not allowed to call this on a empty list.
   * Note that this function does not free the memory of the node.
   */
  void unlink_front();

  /**
   * Returns a reference to the last node of the linked list.
   * It is not allowed to call this on a empty list.
   *
   * @return The last node of the linked list
   */
  TypedNode &back();
  const TypedNode &back() const;

  /**
   * Unlink the last node from the list.
   * It is not allowed to call this on a empty list.
   * Note that this function does not free the memory of the node.
   */
  void unlink_back();
};

}  // namespace chre

#include "chre/util/intrusive_linked_list_impl.h"

#endif  // CHRE_UTIL_INTRUSIVE_LINKED_LIST_H_
