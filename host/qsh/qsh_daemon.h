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

  /**
   * Send a message to CHRE
   *
   * @param clientId The client ID that this message originates from.
   * @param data The data to pass down.
   * @param length The size of the data to send.
   * @return true if successful, false otherwise.
   */
  bool sendMessageToChre(uint16_t clientId, void *data,
                         size_t dataLen) override;

 protected:
  void configureLpma(bool /* enabled */) override {
    LOGE("LPMA Unsupported");
  }

  int64_t getTimeOffset(bool *success) override {
    *success = false;
    return 0;
  }

  /**
   * Interface to a callback that is called when the Daemon receives a message.
   *
   * @param message A buffer containing the message
   * @param messageLen size of the message buffer in bytes
   */
  void onMessageReceived(const unsigned char *message,
                         size_t messageLen) override;

  /**
   * Handles a message that is directed towards the daemon.
   *
   * @param message The message sent to the daemon.
   */
  void handleDaemonMessage(const uint8_t *message) override;

  /**
   * Loads a nanoapp by sending the nanoapp filename to the CHRE framework. This
   * method will return after sending the request so no guarantee is made that
   * the nanoapp is loaded until after the response is received.
   *
   * @param appId The ID of the nanoapp to load.
   * @param appVersion The version of the nanoapp to load.
   * @param appTargetApiVersion The version of the CHRE API that the app
   * targets.
   * @param appBinaryName The name of the binary as stored in the filesystem.
   * This will be used to load the nanoapp into CHRE.
   * @param transactionId The transaction ID to use when loading.
   * @return true if a request was successfully sent, false otherwise.
   */
  bool sendNanoappLoad(uint64_t appId, uint32_t appVersion,
                       uint32_t appTargetApiVersion,
                       const std::string &appBinaryName,
                       uint32_t transactionId) override;

  /**
   * Send a time sync message to CHRE
   *
   * @param logOnError If true, logs an error message on failure.
   *
   * @return true if the time sync message was successfully sent to CHRE.
   */
  bool sendTimeSync(bool logOnError) override;

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
