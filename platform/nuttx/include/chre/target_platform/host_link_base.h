/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef CHRE_PLATFORM_NUTTX_HOST_LINK_BASE_H_
#define CHRE_PLATFORM_NUTTX_HOST_LINK_BASE_H_

#include <poll.h>

#include <thread>

#include "chre/util/singleton.h"

#define MAX_SERVER (2)

namespace chre {
class HostLinkBase {
 public:
  HostLinkBase() {}
  ~HostLinkBase() {}
  void startServer();
  void stopServer();
  /**
   * Send a message to the host.
   *
   * @param data The message to host payload.
   * @param dataLen Size of the message payload in bytes.
   * @param clientId The Host app id.
   * @return true if the operation succeeds, false otherwise.
   */
  bool sendToClientById(uint8_t* data, size_t dataLen, uint16_t clientId);

  bool sendToAllClient(uint8_t* data, size_t dataLen);
  /**
   * Blocks the current thread until the host has retrieved all messages pending
   * in the outbound queue, or a timeout occurs. For proper function, it should
   * not be possible for new messages to be added to the queue at the point when
   * this is called.
   *
   * @return true if the outbound queue was successfully emptied
   */
  static bool flushOutboundQueue();

  /**
   * Enqueues a log message to be sent to the host.
   *
   * @param logMessage Pointer to a buffer that has the log message. Note that
   * the message might be encoded
   *
   * @param logMessageSize length of the log message buffer
   */
  void sendLogMessage(const uint8_t* logMessage, size_t logMessageSize);

  /**
   * Enqueues a V2 log message to be sent to the host.
   *
   * @param logMessage Pointer to a buffer that has the log message. Note that
   * the message might be encoded
   *
   * @param logMessageSize length of the log message buffer
   *
   * @param numLogsDropped number of logs dropped since CHRE start
   */
  void sendLogMessageV2(const uint8_t* logMessage, size_t logMessageSize,
                        uint32_t numLogsDropped);

  /**
   * Enqueues a NAN configuration request to be sent to the host.
   * For Linux, the request is simply echoed back via a NAN configuration
   * update event since there's no actual host to send the request to.
   *
   * @param enable Requests that NAN be enabled or disabled based on the
   *        boolean's value.
   */
  void sendNanConfiguration(bool enable);

 private:
  static constexpr uint32_t kPollingIntervalUsec = 5000;

  // A buffer to read packets into. Allocated here to prevent a large object on
  // the stack.
  std::vector<uint8_t> mRecvBuffer =
      std::vector<uint8_t>(CHRE_MESSAGE_TO_HOST_MAX_SIZE);

  std::optional<std::thread> mChreSend_tid;
  int mRpcListentFd[MAX_SERVER];
  int mClients[CONFIG_CHRE_CLIENT_COUNT + 1];
  int mClient_count;
  int mServer_count;
  struct pollfd mPollFds[CONFIG_CHRE_CLIENT_COUNT + MAX_SERVER];
  std::optional<std::thread> mlistent_tid;

  void runRpctask();
  void vChreSendTask();
  void vChreRecvTask();
  bool rpcServiceInit();
  void rpcServiceDeinit();
  void handleClientData(int clientRpc);
  void disconnectClient(int clientRpc);
};

typedef chre::Singleton<chre::HostLinkBase> HostLinkBaseSingleton;
}  // namespace chre

#endif  // CHRE_PLATFORM_NUTTX_HOST_LINK_BASE_H_
