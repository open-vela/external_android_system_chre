
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

#ifndef CHRE_RPCINTERFACE_H_
#define CHRE_RPCINTERFACE_H_

#include <stdint.h>

namespace chre {
class IRpcCallbacks {
 public:
  virtual bool onMessageReceived(const void *data, size_t length) = 0;
  virtual bool onConnected(uint16_t clientid) {
    (void)clientid;
    return true;
  }
  virtual void onConnectionAborted() {}
  virtual void onDisconnected(uint16_t clientid) {}
  virtual ~IRpcCallbacks() {}
};

class RpcInterface {
 public:
  virtual void startServer(IRpcCallbacks *callback) = 0;
  virtual void stopServer() = 0;
  virtual void disconnectClient(int fd) = 0;
  virtual bool sendToClientById(uint8_t *data, size_t dataLen,
                                uint16_t clientId) = 0;
};

class RpcNullImpl : public RpcInterface {
 public:
  void startServer(IRpcCallbacks *callback) override { (void)callback; }
  void stopServer() override {}
  void disconnectClient(int fd) override { (void)fd; }
  bool sendToClientById(uint8_t *data, size_t dataLen,
                        uint16_t clientId) override {
    (void)data;
    (void)dataLen;
    (void)clientId;
    return true;
  }
};
}  // namespace chre

#endif  // CHRE_HOST_HOST_PROTOCOL_HOST_H_
