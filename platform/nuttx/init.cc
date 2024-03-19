/*
 * Copyright (C) 2024 Xiaomi Corporation
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

#include <dlfcn.h>

#include "chre/core/event.h"
#include "chre/core/event_loop.h"
#include "chre/core/event_loop_manager.h"
#include "chre/core/nanoapp.h"
#include "chre/core/static_nanoapps.h"
#include "chre/platform/context.h"
#include "chre/platform/fatal_error.h"
#include "chre/platform/log.h"
#include "chre/platform/nuttx/platform_log.h"
#include "chre/platform/nuttx/task_util/task_manager.h"
#include "chre/platform/system_timer.h"
#include "chre/target_platform/host_link_base.h"
#include "chre/util/time.h"

using chre::EventLoopManagerSingleton;
using chre::Milliseconds;

extern const struct symtab_s g_chre_exports[];
extern const int g_chre_nexports;

namespace {

void signalHandler(int sig) {
  (void)sig;
  LOGI("Stop request received");
  EventLoopManagerSingleton::get()->getEventLoop().stop();
}

}  // namespace

extern "C" int main(int argc, char **argv) {
  std::string apps[CONFIG_CHRE_CLI_APP_MAX];
  bool no_static = false;
  int index = -1;
  int load_index;
  int c;

  while ((c = getopt(argc, argv, "Sr:")) != -1) {
    switch (c) {
      case 'S':
        no_static = true;
        break;
      case 'r':
        if ((index + 1) < CONFIG_CHRE_CLI_APP_MAX) {
          apps[++index].assign(optarg);
        }
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

  // Initialize the socket server.
  chre::HostLinkBaseSingleton::init();

  chre::HostLinkBaseSingleton::get()->startServer();

  // Register a signal handler.
  signal(SIGINT, signalHandler);

  c = dlsymtab((struct symtab_s *)g_chre_exports, g_chre_nexports);
  if (c < 0) {
    LOGE("Select symbol table failed!");
    return c;
  }

  // Load any static nanoapps and start the event loop.
  EventLoopManagerSingleton::get()->lateInit();

  // Load static nanoapps
  if (!no_static) {
    chre::loadStaticNanoapps();
  }

  // Load dynamic nanoapps
  chre::DynamicVector<chre::UniquePtr<chre::Nanoapp>> dynamicNanoapps;
  for (load_index = 0; load_index <= index; load_index++) {
    dynamicNanoapps.push_back(chre::MakeUnique<chre::Nanoapp>());
    dynamicNanoapps.back()->loadFromFile(apps[load_index]);
    EventLoopManagerSingleton::get()->getEventLoop().startNanoapp(
        dynamicNanoapps.back());
  }

  EventLoopManagerSingleton::get()->getEventLoop().run();

  chre::HostLinkBaseSingleton::get()->stopServer();

  chre::HostLinkBaseSingleton::deinit();

  chre::TaskManagerSingleton::deinit();
  chre::deinit();
  chre::PlatformLogSingleton::deinit();

  return 0;
}
