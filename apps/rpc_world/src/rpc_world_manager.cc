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

#include "rpc_world_manager.h"

#include "chre/util/macros.h"
#include "chre/util/nanoapp/log.h"
#include "chre/util/time.h"
#include "pw_rpc/echo.rpc.pb.h"

#define LOG_TAG "[RpcWorld]"

pw::Status EchoService::Echo(const pw_rpc_EchoMessage &request,
                             pw_rpc_EchoMessage &response) {
  memcpy(response.msg, request.msg,
         MIN(ARRAY_SIZE(response.msg), ARRAY_SIZE(request.msg)));
  return pw::OkStatus();
}

void echoResponse(const pw_rpc_EchoMessage &response, pw::Status status) {
  if (status.ok()) {
    LOGI("Received echo response: %s", response.msg);
  } else {
    LOGE("Echo failed with status %d", static_cast<int>(status.code()));
  }
}

bool RpcWorldManager::start() {
  chre::RpcServer::Service service = {mEchoService, 0x01020034 /* version */};
  chreTimerSet(chre::kOneSecondInNanoseconds, nullptr /*cookie*/,
               true /*oneShot*/);
  return mServer.registerServices(1 /*numServices*/, &service);
}

void RpcWorldManager::handleEvent(uint32_t senderInstanceId, uint16_t eventType,
                                  const void *eventData) {
  if (!mServer.handleEvent(senderInstanceId, eventType, eventData)) {
    LOGE("[Server] An RPC error occurred");
  }

  if (!mClient.handleEvent(senderInstanceId, eventType, eventData)) {
    LOGE("[Client] An RPC error occurred");
  }

  switch (eventType) {
    case CHRE_EVENT_TIMER:
      auto client = mClient.get<pw::rpc::pw_rpc::nanopb::EchoService::Client>();
      if (client.has_value()) {
        const char kMsg[] = "RPC";
        pw_rpc_EchoMessage requestParams;
        memcpy(&requestParams.msg, kMsg, ARRAY_SIZE(kMsg) + 1);

        mCall = client->Echo(requestParams, echoResponse);
        CHRE_ASSERT(mCall.active());
      } else {
        LOGE("Error");
      }
  }
}
