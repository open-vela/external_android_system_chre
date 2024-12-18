/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <algorithm>

#include "gtest/gtest.h"

#include "chre/core/request_multiplexer.h"

using chre::RequestMultiplexer;

class FakeRequest {
 public:
  FakeRequest() : FakeRequest(0) {}

  FakeRequest(int priority) : mPriority(priority) {}

  bool isEquivalentTo(const FakeRequest &request) const {
    return (mPriority == request.mPriority);
  }

  bool mergeWith(const FakeRequest &request) {
    bool newMaximal = false;
    if (request.mPriority > mPriority) {
      mPriority = request.mPriority;
      newMaximal = true;
    }

    return newMaximal;
  }

  int getPriority() const {
    return mPriority;
  }

 private:
  int mPriority;
};

TEST(RequestMultiplexer, DefaultRequestDoesNotCauseNewMaximal) {
  RequestMultiplexer<FakeRequest> multiplexer;
  FakeRequest request;
  size_t index;
  bool maximalRequestChanged;
  ASSERT_TRUE(multiplexer.addRequest(request, &index, &maximalRequestChanged));
  EXPECT_FALSE(maximalRequestChanged);
  EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 0);
}

TEST(RequestMultiplexer, FirstHighPriorityRequestCausesNewMaximal) {
  RequestMultiplexer<FakeRequest> multiplexer;
  FakeRequest request(10);
  size_t index;
  bool maximalRequestChanged;
  ASSERT_TRUE(multiplexer.addRequest(request, &index, &maximalRequestChanged));
  EXPECT_TRUE(maximalRequestChanged);
  EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
}

TEST(RequestMultiplexer, NewLowerPriorityRequestDoesNotCauseNewMaximal) {
  RequestMultiplexer<FakeRequest> multiplexer;
  size_t index;

  {
    FakeRequest request(10);
    bool maximalRequestChanged;
    ASSERT_TRUE(
        multiplexer.addRequest(request, &index, &maximalRequestChanged));
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
  }

  {
    FakeRequest request(5);
    bool maximalRequestChanged;
    ASSERT_TRUE(
        multiplexer.addRequest(request, &index, &maximalRequestChanged));
    EXPECT_FALSE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
  }
}

TEST(RequestMultiplexer, AddOneRemoveMaximal) {
  RequestMultiplexer<FakeRequest> multiplexer;
  FakeRequest request(10);
  size_t index;
  bool maximalRequestChanged;
  ASSERT_TRUE(multiplexer.addRequest(request, &index, &maximalRequestChanged));
  EXPECT_TRUE(maximalRequestChanged);
  EXPECT_EQ(multiplexer.getRequests()[index].getPriority(), 10);
  EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);

  FakeRequest defaultRequest;
  multiplexer.removeRequest(0, &maximalRequestChanged);
  EXPECT_TRUE(maximalRequestChanged);
  EXPECT_TRUE(
      multiplexer.getCurrentMaximalRequest().isEquivalentTo(defaultRequest));
  EXPECT_TRUE(multiplexer.getRequests().empty());
}

TEST(RequestMultiplexer, AddManyRemoveMaximal) {
  RequestMultiplexer<FakeRequest> multiplexer;
  size_t index;

  {
    FakeRequest request(10);
    bool maximalRequestChanged;
    ASSERT_TRUE(
        multiplexer.addRequest(request, &index, &maximalRequestChanged));
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[index].getPriority(), 10);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
  }

  {
    FakeRequest request(5);
    bool maximalRequestChanged;
    ASSERT_TRUE(
        multiplexer.addRequest(request, &index, &maximalRequestChanged));
    EXPECT_FALSE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[index].getPriority(), 5);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
  }

  {
    FakeRequest request(10);
    bool maximalRequestChanged;
    ASSERT_TRUE(
        multiplexer.addRequest(request, &index, &maximalRequestChanged));
    EXPECT_FALSE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[index].getPriority(), 10);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
  }

  bool maximalRequestChanged;
  multiplexer.removeRequest(0, &maximalRequestChanged);
  EXPECT_FALSE(maximalRequestChanged);
  EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
  EXPECT_EQ(multiplexer.getRequests()[0].getPriority(), 5);
  EXPECT_EQ(multiplexer.getRequests()[1].getPriority(), 10);
}

TEST(RequestMultiplexer, AddManyRemoveBeforeMaximalThenRemoveMaximal) {
  RequestMultiplexer<FakeRequest> multiplexer;
  size_t index;

  {
    FakeRequest request(1);
    bool maximalRequestChanged;
    ASSERT_TRUE(
        multiplexer.addRequest(request, &index, &maximalRequestChanged));
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[index].getPriority(), 1);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 1);
  }

  {
    FakeRequest request(5);
    bool maximalRequestChanged;
    ASSERT_TRUE(
        multiplexer.addRequest(request, &index, &maximalRequestChanged));
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[index].getPriority(), 5);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 5);
  }

  {
    FakeRequest request(10);
    bool maximalRequestChanged;
    ASSERT_TRUE(
        multiplexer.addRequest(request, &index, &maximalRequestChanged));
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[index].getPriority(), 10);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
  }

  bool maximalRequestChanged;
  multiplexer.removeRequest(0, &maximalRequestChanged);
  EXPECT_FALSE(maximalRequestChanged);
  EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
  EXPECT_EQ(multiplexer.getRequests()[0].getPriority(), 5);
  EXPECT_EQ(multiplexer.getRequests()[1].getPriority(), 10);

  multiplexer.removeRequest(1, &maximalRequestChanged);
  EXPECT_TRUE(maximalRequestChanged);
  EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 5);
  EXPECT_EQ(multiplexer.getRequests()[0].getPriority(), 5);
}

TEST(RequestMultiplexer, AddManyRemoveAfterMaximalThenRemoveMaximal) {
  RequestMultiplexer<FakeRequest> multiplexer;
  size_t index;

  {
    FakeRequest request(1);
    bool maximalRequestChanged;
    ASSERT_TRUE(
        multiplexer.addRequest(request, &index, &maximalRequestChanged));
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[index].getPriority(), 1);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 1);
  }

  {
    FakeRequest request(5);
    bool maximalRequestChanged;
    ASSERT_TRUE(
        multiplexer.addRequest(request, &index, &maximalRequestChanged));
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[index].getPriority(), 5);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 5);
  }

  {
    FakeRequest request(10);
    bool maximalRequestChanged;
    ASSERT_TRUE(
        multiplexer.addRequest(request, &index, &maximalRequestChanged));
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[index].getPriority(), 10);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
  }

  {
    FakeRequest request(5);
    bool maximalRequestChanged;
    ASSERT_TRUE(
        multiplexer.addRequest(request, &index, &maximalRequestChanged));
    EXPECT_FALSE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[index].getPriority(), 5);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
  }

  bool maximalRequestChanged;
  multiplexer.removeRequest(3, &maximalRequestChanged);
  EXPECT_FALSE(maximalRequestChanged);
  EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
  EXPECT_EQ(multiplexer.getRequests()[0].getPriority(), 1);
  EXPECT_EQ(multiplexer.getRequests()[1].getPriority(), 5);
  EXPECT_EQ(multiplexer.getRequests()[2].getPriority(), 10);

  multiplexer.removeRequest(2, &maximalRequestChanged);
  EXPECT_TRUE(maximalRequestChanged);
  EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 5);
  EXPECT_EQ(multiplexer.getRequests()[0].getPriority(), 1);
  EXPECT_EQ(multiplexer.getRequests()[1].getPriority(), 5);
}

TEST(RequestMultiplexer, AddManyUpdateWithLowerPriority) {
  RequestMultiplexer<FakeRequest> multiplexer;
  size_t index;

  {
    FakeRequest request(1);
    bool maximalRequestChanged;
    ASSERT_TRUE(
        multiplexer.addRequest(request, &index, &maximalRequestChanged));
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[index].getPriority(), 1);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 1);
  }

  {
    FakeRequest request(5);
    bool maximalRequestChanged;
    ASSERT_TRUE(
        multiplexer.addRequest(request, &index, &maximalRequestChanged));
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[index].getPriority(), 5);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 5);
  }

  {
    FakeRequest request(10);
    bool maximalRequestChanged;
    ASSERT_TRUE(
        multiplexer.addRequest(request, &index, &maximalRequestChanged));
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[index].getPriority(), 10);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
  }

  {
    FakeRequest request(8);
    bool maximalRequestChanged;
    multiplexer.updateRequest(1, request, &maximalRequestChanged);
    EXPECT_FALSE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[1].getPriority(), 8);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
  }
}

TEST(RequestMultiplexer, AddManyUpdateWithLowerPriorityMoveSemantics) {
  RequestMultiplexer<FakeRequest> multiplexer;
  size_t index;

  {
    FakeRequest request(1);
    bool maximalRequestChanged;
    ASSERT_TRUE(multiplexer.addRequest(std::move(request), &index,
                                       &maximalRequestChanged));
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[index].getPriority(), 1);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 1);
  }

  {
    FakeRequest request(5);
    bool maximalRequestChanged;
    ASSERT_TRUE(multiplexer.addRequest(std::move(request), &index,
                                       &maximalRequestChanged));
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[index].getPriority(), 5);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 5);
  }

  {
    FakeRequest request(10);
    bool maximalRequestChanged;
    ASSERT_TRUE(multiplexer.addRequest(std::move(request), &index,
                                       &maximalRequestChanged));
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[index].getPriority(), 10);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
  }

  {
    FakeRequest request(8);
    bool maximalRequestChanged;
    multiplexer.updateRequest(1, std::move(request), &maximalRequestChanged);
    EXPECT_FALSE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[1].getPriority(), 8);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
  }
}

TEST(RequestMultiplexer, AddManyUpdateWithNewMaximalLowerPriority) {
  RequestMultiplexer<FakeRequest> multiplexer;
  size_t index;

  {
    FakeRequest request(1);
    bool maximalRequestChanged;
    ASSERT_TRUE(
        multiplexer.addRequest(request, &index, &maximalRequestChanged));
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[index].getPriority(), 1);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 1);
  }

  {
    FakeRequest request(5);
    bool maximalRequestChanged;
    ASSERT_TRUE(
        multiplexer.addRequest(request, &index, &maximalRequestChanged));
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[index].getPriority(), 5);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 5);
  }

  {
    FakeRequest request(10);
    bool maximalRequestChanged;
    ASSERT_TRUE(
        multiplexer.addRequest(request, &index, &maximalRequestChanged));
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[index].getPriority(), 10);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
  }

  {
    FakeRequest request(8);
    bool maximalRequestChanged = false;
    multiplexer.updateRequest(2, request, &maximalRequestChanged);
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[2].getPriority(), 8);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 8);
  }
}

TEST(RequestMultiplexer, AddManyUpdateNewMaximal) {
  RequestMultiplexer<FakeRequest> multiplexer;
  size_t index;

  {
    FakeRequest request(1);
    bool maximalRequestChanged;
    ASSERT_TRUE(
        multiplexer.addRequest(request, &index, &maximalRequestChanged));
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[index].getPriority(), 1);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 1);
  }

  {
    FakeRequest request(5);
    bool maximalRequestChanged;
    ASSERT_TRUE(
        multiplexer.addRequest(request, &index, &maximalRequestChanged));
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[index].getPriority(), 5);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 5);
  }

  {
    FakeRequest request(10);
    bool maximalRequestChanged;
    ASSERT_TRUE(
        multiplexer.addRequest(request, &index, &maximalRequestChanged));
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[index].getPriority(), 10);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
  }

  {
    FakeRequest request(20);
    bool maximalRequestChanged = false;
    multiplexer.updateRequest(1, request, &maximalRequestChanged);
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[1].getPriority(), 20);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 20);
  }
}

TEST(RequestMultiplexer, RemoveAllRequestsEmpty) {
  RequestMultiplexer<FakeRequest> multiplexer;

  bool maximalRequestChanged;
  multiplexer.removeAllRequests(&maximalRequestChanged);

  EXPECT_FALSE(maximalRequestChanged);
  EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 0);
}

TEST(RequestMultiplexer, RemoveAllRequestsNonEmpty) {
  RequestMultiplexer<FakeRequest> multiplexer;
  size_t index;

  {
    FakeRequest request(1);
    bool maximalRequestChanged;
    ASSERT_TRUE(
        multiplexer.addRequest(request, &index, &maximalRequestChanged));
  }

  bool maximalRequestChanged;
  multiplexer.removeAllRequests(&maximalRequestChanged);

  EXPECT_TRUE(maximalRequestChanged);
  EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 0);
}
