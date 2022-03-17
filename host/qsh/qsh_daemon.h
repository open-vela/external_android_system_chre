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

#ifndef CHRE_QSH_DAEMON_H_
#define CHRE_QSH_DAEMON_H_

#include "chre_host/daemon_base.h"
#include "chre_host/log.h"

#include <utils/SystemClock.h>
#include <atomic>
#include <optional>
#include <thread>

#include "qmi_client.h"
#include "qmi_qsh_nanoapp_client.h"

namespace android {
namespace chre {

class QshChreDaemon : public ChreDaemonBase {
 public:
  QshChreDaemon() : mQmiQshNanoappClient("chre_qsh_nanoapp") {}

  ~QshChreDaemon() {
    deinit();
  }

  /**
   * Initializes QSH message handling then proceeds to load any preloaded
   * nanoapps.
   *
   * @return true on successful init
   */
  bool init();

  /**
   * Starts a socket server receive loop for inbound messages.
   */
  void run();

 protected:
  bool doSendMessage(void *data, size_t length) override;

  void configureLpma(bool /* enabled */) override {
    LOGE("LPMA Unsupported");
  }

  int64_t getTimeOffset(bool *success) override {
    *success = false;
    return 0;
  }

 private:
  QmiQshNanoappClient mQmiQshNanoappClient;

  /**
   * Shutsdown the daemon, stops all the worker threads created in init()
   * Since this is to be invoked at exit, it's mostly best effort, and is
   * invoked by the class destructor
   */
  void deinit();
};

}  // namespace chre
}  // namespace android

#endif  // CHRE_QSH_DAEMON_H_
