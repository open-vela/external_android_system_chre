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

#ifndef CHRE_EXYNOS_DAEMON_H_
#define CHRE_EXYNOS_DAEMON_H_

#include <atomic>
#include <thread>

#include "chre_host/fbs_daemon_base.h"
#include "chre_host/st_hal_lpma_handler.h"

namespace android {
namespace chre {

class ExynosDaemon : public FbsDaemonBase {
 public:
  ExynosDaemon();
  ~ExynosDaemon() {
    deinit();
  }

  //! EXYNOS's shared memory size for CHRE <-> AP is 4KB.
  static constexpr size_t kIpcMsgSizeMax = 4096;

  /**
   * Initializes the CHRE daemon.
   *
   * @return true on successful init
   */
  bool init();

  /**
   * Starts a socket server receive loop for inbound messages.
   */
  void run();

  void processIncomingMsgs();

 protected:
  bool doSendMessage(void *data, size_t length) override;

  void configureLpma(bool enabled) override {
    mLpmaHandler.enable(enabled);
  }

 private:
  static constexpr char kCommsDeviceFilename[] = "/dev/nanohub_comms";
  static constexpr int kInvalidFd = -1;

  int mCommsReadFd = kInvalidFd;
  int mCommsWriteFd = kInvalidFd;
  std::thread mIncomingMsgProcessThread;
  std::thread::native_handle_type mNativeThreadHandle;
  std::atomic<bool> mProcessThreadRunning = false;

  StHalLpmaHandler mLpmaHandler;

  /**
   * Perform a graceful shutdown of the daemon
   */
  void deinit();

  /**
   * Platform specific getTimeOffset.
   *
   * @return clock drift offset in nanoseconds
   */
  int64_t getTimeOffset(bool *success);

  /**
   * Stops the inbound message processing thread (forcibly).
   * Since the message read mechanism uses blocking system calls (poll, read),
   * and since there's no timeout on the system calls (to avoid waking the AP
   * up to handle timeouts), we forcibly terminate the thread on a daemon
   * deinit. POSIX semantics are used since the C++20 threading interface does
   * not provide an API to accomplish this.
   */
  void stopMsgProcessingThread();

  /**
   * Empty signal handler to handle SIGINT
   */
  static void signalHandler(int /*signal*/) {}
};

}  // namespace chre
}  // namespace android

#endif  // CHRE_EXYNOS_DAEMON_H_
