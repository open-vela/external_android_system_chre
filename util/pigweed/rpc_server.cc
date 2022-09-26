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

#include <cinttypes>

#include "chre/event.h"
#include "chre/re.h"
#include "chre/util/nanoapp/log.h"

#ifndef LOG_TAG
#define LOG_TAG "[RpcServer]"
#endif  // LOG_TAG

namespace chre {
namespace {

constexpr uint32_t kChannelIdHostClient = 1 << 16;

// Mask to extract the host ID / nanoapp ID from a channel ID.
constexpr uint32_t kClientIdMask = 0xffff;

// Returns whether the host / nanoapp IDs match.
bool endpointsMatch(uint32_t expectedId, uint32_t actualId) {
  if ((expectedId & kClientIdMask) != (actualId & kClientIdMask)) {
    LOGE("Invalid endpoint 0x%04" PRIx32 " expected 0x%04" PRIx32, actualId,
         expectedId);
    return false;
  }

  return true;
}

bool isChannelIdHostClient(uint32_t id) {
  return id >> 16 == 1;
}

bool isChannelIdNanoappClient(uint32_t id) {
  return id >> 16 == 0;
}

}  // namespace

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

  if (!validateHostChannelId(hostMessage, result.value())) {
    return false;
  }

  mServer.OpenChannel(result.value(), mOutput);

  pw::Status success = mServer.ProcessPacket(packet, mOutput);

  if (success != pw::OkStatus()) {
    LOGE("Failed to process the packet");
    return false;
  }

  return true;
}

bool RpcServer::validateHostChannelId(const chreMessageFromHostData *msg,
                                      uint32_t channelId) {
  struct chreHostEndpointInfo info;

  if (!isChannelIdHostClient(channelId) ||
      !chreGetHostEndpointInfo(msg->hostEndpoint, &info)) {
    LOGE("Invalid channelId for a host client 0x%08" PRIx32, channelId);
    return false;
  }

  return endpointsMatch(channelId, static_cast<uint32_t>(msg->hostEndpoint));
}

bool RpcServer::validateNanoappChannelId(uint32_t nappId, uint32_t channelId) {
  if (nappId > 0xffff) {
    LOGE("Invalid nanoapp Id 0x%08" PRIx32, nappId);
    return false;
  }

  if (!isChannelIdNanoappClient(channelId)) {
    LOGE("Invalid channelId for a nanoapp client 0x%08" PRIx32, channelId);
    return false;
  }

  return endpointsMatch(channelId, nappId);
}

}  // namespace chre