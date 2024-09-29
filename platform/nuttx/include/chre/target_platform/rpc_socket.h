
/*
 * Copyright (C) 224 The Xiaomi Open Source Project
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

#ifndef RPC_SOCKET_H
#define RPC_SOCKET_H
#include <poll.h>
#include <stdint.h>

#include <optional>
#include <thread>

#include "chre/target_platform/RpcInterface.h"

#define MAX_SERVER (2)

namespace chre {
class RpcSocket : public RpcInterface {
 public:
  void startServer(IRpcCallbacks* callback);
  void stopServer();
  void disconnectClient(int fd);
  bool sendToClientById(uint8_t* data, size_t dataLen, uint16_t clientId);

 private:
  IRpcCallbacks* mRpcCallback;
  int mRpcListentFd[MAX_SERVER];
  int mClients[CONFIG_CHRE_CLIENT_COUNT + 1];
  int mClient_count;
  int mServer_count;
  struct pollfd mPollFds[CONFIG_CHRE_CLIENT_COUNT + MAX_SERVER];
  std::optional<std::thread> mlistent_tid;

  bool rpcServiceInit();
  void rpcServiceDeinit();
  void runRpctask();
  void vChreRecvTask();
  bool sendToAllClient(uint8_t* data, size_t dataLen);
};
}  // namespace chre

#endif  // CHRE_HOST_HOST_PROTOCOL_HOST_H_
