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

#include "chre/util/pigweed/rpc_server.h"
#include "chre/event.h"
#include "chre/re.h"
#include "chre/util/nanoapp/log.h"

#ifndef LOG_TAG
#define LOG_TAG "[RpcServer]"
#endif  // LOG_TAG

namespace chre {

bool RpcServer::registerServices(size_t numServices,
                                 RpcServer::Service *services) {
  for (size_t i = 0; i < numServices; ++i) {
    const Service &service = services[i];
    chreNanoappRpcService chreService = {
        .id = static_cast<uint64_t>(service.service.id()),
        .version = service.version,
    };

    if (!chrePublishRpcServices(&chreService, 1 /* numServices */)) {
      return false;
    }

    mServer.RegisterService(service.service);
  }

  return true;
}

bool RpcServer::handleEvent(uint32_t senderInstanceId, uint16_t eventType,
                            const void *eventData) {
  UNUSED_VAR(senderInstanceId);
  switch (eventType) {
    case CHRE_EVENT_MESSAGE_FROM_HOST:
      return handleMessageFromHost(eventData);
    default:
      return true;
  }
}

bool RpcServer::handleMessageFromHost(const void *eventData) {
  auto *hostMessage = static_cast<const chreMessageFromHostData *>(eventData);
  mOutput.setHostEndpoint(hostMessage->hostEndpoint);

  std::span packet(static_cast<const std::byte *>(hostMessage->message),
                   hostMessage->messageSize);

  pw::Result result = pw::rpc::ExtractChannelId(packet);
  if (result.status() != PW_STATUS_OK) {
    LOGE("Unable to extract channel ID from packet");
    return false;
  }

  mServer.OpenChannel(result.value(), mOutput);

  pw::Status success = mServer.ProcessPacket(packet, mOutput);
  LOGI("Parsing packet %d", success == pw::OkStatus());

  return success == pw::OkStatus();
}

}  // namespace chre