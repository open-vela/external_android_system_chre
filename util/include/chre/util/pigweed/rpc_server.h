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

#ifndef CHRE_UTIL_PIGWEED_RPC_SERVER_H_
#define CHRE_UTIL_PIGWEED_RPC_SERVER_H_

#include <span>

#include "chre/util/macros.h"
#include "chre/util/pigweed/chre_channel_output.h"
#include "pw_rpc/server.h"
#include "pw_rpc/service.h"

namespace chre {

/**
 * RPC Server wrapping a Pigweed RPC server.
 *
 * This helper class handle Pigweed RPC calls on the server side.
 *
 * The services must be registered from the `nanoappStart` function using
 * the `registerService` method.
 *
 * The `handleEvent` method must be called at the beginning of the
 * `nanoappHandleEvent` function to respond to RPC calls from the clients.
 */
class RpcServer {
 public:
  struct Service {
    /** A Pigweed service. */
    pw::rpc::Service &service;
    /**
     * The version of the service. It should be in sync with the version on the
     * client side.
     */
    uint32_t version;
  };

  RpcServer() : mServer(std::span(mChannels, ARRAY_SIZE(mChannels))) {}

  /**
   * Registers services to the server and to CHRE.
   *
   * This method must be called from `nanoappStart`. It is valid to call this
   * method more than once.
   *
   * @param numServices The number of services to register.
   * @param services Service definitions.
   * @return whether the registration was successful.
   */
  bool registerServices(size_t numServices, Service *services);

  /**
   * Handles events related to RPC services.
   *
   * Handles the following events:
   * - CHRE_EVENT_MESSAGE_FROM_HOST: respond to RPC requests.
   *
   * @param senderInstanceId  The Instance ID for the source of this event.
   * @param eventType  The event type.
   * @param eventData  The associated data, if any, for this specific type of
   *                   event.
   * @return whether any event was handled successfully.
   */
  bool handleEvent(uint32_t senderInstanceId, uint16_t eventType,
                   const void *eventData);

 private:
  /**
   * Handles messages from host client.
   *
   * This method must be called when nanoapps receive a
   * CHRE_EVENT_MESSAGE_FROM_HOST event.
   *
   * @param eventData  The associated data, if any.
   * @return whether the RPC was handled successfully.
   */
  bool handleMessageFromHost(const void *eventData);

  // TODO(b/210138227): Make # of channels dynamic
  pw::rpc::Channel mChannels[5];
  pw::rpc::Server mServer;

  ChreHostChannelOutput mOutput;
};

}  // namespace chre

#endif  // CHRE_UTIL_PIGWEED_RPC_SERVER_H_