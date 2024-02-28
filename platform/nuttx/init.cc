/*
 * Copyright (C) 2023 Xiaomi Corporation
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

#include "chre/core/init.h"
#include "chre/core/event.h"
#include "chre/core/event_loop.h"
#include "chre/core/event_loop_manager.h"
#include "chre/core/nanoapp.h"
#include "chre/core/static_nanoapps.h"
#include "chre/platform/context.h"
#include "chre/platform/fatal_error.h"
#include "chre/platform/nuttx/platform_log.h"
#include "chre/platform/nuttx/task_util/task_manager.h"
#include "chre/platform/log.h"
#include "chre/platform/system_timer.h"
#include "chre/util/time.h"

using chre::EventLoopManagerSingleton;
using chre::Milliseconds;

namespace {

void signalHandler(int sig) {
  (void)sig;
  LOGI("Stop request received");
  EventLoopManagerSingleton::get()->getEventLoop().stop();
}

}  // namespace

extern "C" int main(int argc, char **argv) {
  bool no_static = false;
  int c;

  while ((c = getopt (argc, argv, "S")) != -1) {
      switch (c) {
          case 'S':
            no_static = true;
            break;
          default:
            LOGI("Unknown option 0x%x\n", optopt);
            return -EPERM;
        }
    }

  // Initialize logging.
  chre::PlatformLogSingleton::init();

  // Initialize the TaskManager.
  chre::TaskManagerSingleton::init();

  // Initialize the system.
  chre::init();

  // Register a signal handler.
  signal(SIGINT, signalHandler);

  // Load any static nanoapps and start the event loop.
  EventLoopManagerSingleton::get()->lateInit();

  // Load static nanoapps
  if (!no_static) {
    chre::loadStaticNanoapps();
  }

  EventLoopManagerSingleton::get()->getEventLoop().run();

  chre::TaskManagerSingleton::deinit();
  chre::deinit();
  chre::PlatformLogSingleton::deinit();

  return 0;
}