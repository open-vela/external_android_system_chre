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

#include "chre/util/pigweed/chre_channel_output.h"

#include "chre/util/memory.h"
#include "chre/util/nanoapp/callbacks.h"
#include "chre/util/pigweed/rpc_helper.h"

namespace chre {
namespace {

void nappMessageFreeCb(uint16_t /* eventType */, void *eventData) {
  chreHeapFree(eventData);
}

}  // namespace

ChreChannelOutputBase::ChreChannelOutputBase() : ChannelOutput("CHRE") {}

void ChreChannelOutputBase::setEndpointId(uint16_t endpointId) {
  mEndpointId = endpointId;
}

size_t ChreChannelOutputBase::MaximumTransmissionUnit() {
  return CHRE_MESSAGE_TO_HOST_MAX_SIZE - sizeof(ChrePigweedNanoappMessage);
}

void ChreNanoappChannelOutput::setNanoappEndpoint(uint32_t nanoappInstanceId) {
  CHRE_ASSERT(nanoappInstanceId <= kRpcNanoappMaxId);
  if (nanoappInstanceId <= kRpcNanoappMaxId) {
    mEndpointId = static_cast<uint16_t>(nanoappInstanceId);
  } else {
    mEndpointId = CHRE_HOST_ENDPOINT_UNSPECIFIED;
  }
}

void ChreNanoappChannelOutput::setServer(uint32_t instanceId) {
  CHRE_ASSERT(instanceId <= kRpcNanoappMaxId);
  if (instanceId <= kRpcNanoappMaxId) {
    mServerInstanceId = static_cast<uint16_t>(instanceId);
  } else {
    mServerInstanceId = 0;
  }
}

pw::Status ChreNanoappChannelOutput::Send(pw::span<const std::byte> buffer) {
  CHRE_ASSERT(mEndpointId != CHRE_HOST_ENDPOINT_UNSPECIFIED);
  CHRE_ASSERT(mRole == Role::SERVER || mServerInstanceId != 0);

  if (buffer.size() > 0) {
    auto *data = static_cast<ChrePigweedNanoappMessage *>(
        chreHeapAlloc(buffer.size() + sizeof(ChrePigweedNanoappMessage)));
    if (data == nullptr) {
      return PW_STATUS_RESOURCE_EXHAUSTED;
    }

    data->msgSize = buffer.size();
    data->msg = &data[1];
    memcpy(data->msg, buffer.data(), buffer.size());

    if (!chreSendEvent(
            mRole == Role::SERVER ? PW_RPC_CHRE_NAPP_RESPONSE_EVENT_TYPE
                                  : PW_RPC_CHRE_NAPP_REQUEST_EVENT_TYPE,
            data, nappMessageFreeCb,
            mRole == Role::SERVER ? mEndpointId : mServerInstanceId)) {
      return PW_STATUS_INVALID_ARGUMENT;
    }
  }

  return PW_STATUS_OK;
}

void ChreHostChannelOutput::setHostEndpoint(uint16_t hostEndpoint) {
  setEndpointId(hostEndpoint);
}

pw::Status ChreHostChannelOutput::Send(pw::span<const std::byte> buffer) {
  CHRE_ASSERT(mEndpointId != CHRE_HOST_ENDPOINT_UNSPECIFIED);
  pw::Status returnCode = PW_STATUS_OK;

  if (buffer.size() > 0) {
    uint8_t *data = static_cast<uint8_t *>(memoryAlloc(buffer.size()));
    if (data == nullptr) {
      returnCode = PW_STATUS_RESOURCE_EXHAUSTED;
    } else {
      memcpy(data, buffer.data(), buffer.size());
      // TODO(b/210138227): Make this pass permissions too.
      if (!chreSendMessageWithPermissions(
              data, buffer.size(), PW_RPC_CHRE_HOST_MESSAGE_TYPE, mEndpointId,
              CHRE_MESSAGE_PERMISSION_NONE, heapFreeMessageCallback)) {
        returnCode = PW_STATUS_INVALID_ARGUMENT;
      }
    }
  }

  return returnCode;
}

}  // namespace chre
