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

#include <cstdint>

#include "chre/core/event_loop.h"
#include "chre/core/event_loop_manager.h"
#include "chre/core/settings.h"
#include "chre/util/nanoapp/log.h"
#include "chre/util/time.h"
#include "chre_api/chre/event.h"
#include "chre_api/chre/re.h"

#include "gtest/gtest.h"
#include "inc/test_util.h"
#include "test_base.h"
#include "test_event.h"
#include "test_event_queue.h"
#include "test_util.h"

namespace chre {

namespace {

TEST_F(TestBase, PwRpcCanPublishServicesInNanoappStart) {
  struct App : public TestNanoapp {
    bool (*start)() = []() -> bool {
      struct chreNanoappRpcService servicesA[] = {
          {.id = 1, .version = 0},
          {.id = 2, .version = 0},
      };

      struct chreNanoappRpcService servicesB[] = {
          {.id = 3, .version = 0},
          {.id = 4, .version = 0},
      };

      return chrePublishRpcServices(servicesA, 2 /* numServices */) &&
             chrePublishRpcServices(servicesB, 2 /* numServices */);
    };
  };

  auto app = loadNanoapp<App>();

  uint16_t instanceId;
  EXPECT_TRUE(EventLoopManagerSingleton::get()
                  ->getEventLoop()
                  .findNanoappInstanceIdByAppId(app.id, &instanceId));

  Nanoapp *napp =
      EventLoopManagerSingleton::get()->getEventLoop().findNanoappByInstanceId(
          instanceId);

  ASSERT_FALSE(napp == nullptr);

  EXPECT_EQ(napp->getRpcServices().size(), 4);
  EXPECT_EQ(napp->getRpcServices()[0].id, 1);
  EXPECT_EQ(napp->getRpcServices()[1].id, 2);
  EXPECT_EQ(napp->getRpcServices()[2].id, 3);
  EXPECT_EQ(napp->getRpcServices()[3].id, 4);
}

TEST_F(TestBase, PwRpcCanNotPublishDuplicateServices) {
  struct App : public TestNanoapp {
    bool (*start)() = []() -> bool {
      struct chreNanoappRpcService servicesA[] = {
          {.id = 1, .version = 0},
          {.id = 2, .version = 0},
      };

      bool success = chrePublishRpcServices(servicesA, 2 /* numServices */);

      EXPECT_FALSE(chrePublishRpcServices(servicesA, 2 /* numServices */));

      struct chreNanoappRpcService servicesB[] = {
          {.id = 5, .version = 0},
          {.id = 5, .version = 0},
      };

      EXPECT_FALSE(chrePublishRpcServices(servicesB, 2 /* numServices */));

      return success;
    };
  };

  auto app = loadNanoapp<App>();

  uint16_t instanceId;
  EXPECT_TRUE(EventLoopManagerSingleton::get()
                  ->getEventLoop()
                  .findNanoappInstanceIdByAppId(app.id, &instanceId));

  Nanoapp *napp =
      EventLoopManagerSingleton::get()->getEventLoop().findNanoappByInstanceId(
          instanceId);

  ASSERT_FALSE(napp == nullptr);

  EXPECT_EQ(napp->getRpcServices().size(), 2);
  EXPECT_EQ(napp->getRpcServices()[0].id, 1);
  EXPECT_EQ(napp->getRpcServices()[1].id, 2);
}

TEST_F(TestBase, PwRpcDifferentAppCanPublishSameServices) {
  struct App1 : public TestNanoapp {
    uint64_t id = 0x01;

    bool (*start)() = []() -> bool {
      struct chreNanoappRpcService services[] = {
          {.id = 1, .version = 0},
          {.id = 2, .version = 0},
      };

      return chrePublishRpcServices(services, 2 /* numServices */);
    };
  };

  struct App2 : public App1 {
    uint64_t id = 0x02;
  };

  auto app1 = loadNanoapp<App1>();
  auto app2 = loadNanoapp<App2>();

  uint16_t instanceId1;
  EXPECT_TRUE(EventLoopManagerSingleton::get()
                  ->getEventLoop()
                  .findNanoappInstanceIdByAppId(app1.id, &instanceId1));

  Nanoapp *napp1 =
      EventLoopManagerSingleton::get()->getEventLoop().findNanoappByInstanceId(
          instanceId1);

  ASSERT_FALSE(napp1 == nullptr);

  EXPECT_EQ(napp1->getRpcServices().size(), 2);
  EXPECT_EQ(napp1->getRpcServices()[0].id, 1);
  EXPECT_EQ(napp1->getRpcServices()[1].id, 2);

  uint16_t instanceId2;
  EXPECT_TRUE(EventLoopManagerSingleton::get()
                  ->getEventLoop()
                  .findNanoappInstanceIdByAppId(app2.id, &instanceId2));

  Nanoapp *napp2 =
      EventLoopManagerSingleton::get()->getEventLoop().findNanoappByInstanceId(
          instanceId2);

  ASSERT_FALSE(napp2 == nullptr);

  EXPECT_EQ(napp2->getRpcServices().size(), 2);
  EXPECT_EQ(napp2->getRpcServices()[0].id, 1);
  EXPECT_EQ(napp2->getRpcServices()[1].id, 2);
}

TEST_F(TestBase, PwRpcCanNotPublishServicesOutsideOfNanoappStart) {
  CREATE_CHRE_TEST_EVENT(PUBLISH_SERVICES, 0);

  struct App : public TestNanoapp {
    void (*handleEvent)(uint32_t, uint16_t, const void *) =
        [](uint32_t, uint16_t eventType, const void *eventData) {
          switch (eventType) {
            case CHRE_EVENT_TEST_EVENT: {
              auto event = static_cast<const TestEvent *>(eventData);
              switch (event->type) {
                case PUBLISH_SERVICES: {
                  struct chreNanoappRpcService services[] = {
                      {.id = 1, .version = 0},
                      {.id = 2, .version = 0},
                  };

                  bool success =
                      chrePublishRpcServices(services, 2 /* numServices */);
                  TestEventQueueSingleton::get()->pushEvent(PUBLISH_SERVICES,
                                                            success);
                  break;
                }
              }
            }
          }
        };
  };

  auto app = loadNanoapp<App>();

  bool success = true;
  sendEventToNanoapp(app, PUBLISH_SERVICES);
  waitForEvent(PUBLISH_SERVICES, &success);
  EXPECT_FALSE(success);

  uint16_t instanceId;
  EXPECT_TRUE(EventLoopManagerSingleton::get()
                  ->getEventLoop()
                  .findNanoappInstanceIdByAppId(app.id, &instanceId));

  Nanoapp *napp =
      EventLoopManagerSingleton::get()->getEventLoop().findNanoappByInstanceId(
          instanceId);

  ASSERT_FALSE(napp == nullptr);

  EXPECT_EQ(napp->getRpcServices().size(), 0);
}

}  // namespace

}  // namespace chre
