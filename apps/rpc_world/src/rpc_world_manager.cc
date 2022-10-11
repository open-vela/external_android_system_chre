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

#define LOG_TAG "[RpcWorld]"

pw::Status RpcWorldService::Increment(const chre_rpc_NumberMessage &request,
                                      chre_rpc_NumberMessage &response) {
  response.number = request.number + 1;
  return pw::OkStatus();
}

void incrementResponse(const chre_rpc_NumberMessage &response,
                       pw::Status status) {
  if (status.ok()) {
    LOGI("Increment response: %d", response.number);
  } else {
    LOGE("Increment failed with status %d", static_cast<int>(status.code()));
  }
}

void RpcWorldService::Timer(
    const chre_rpc_TimerRequest &request,
    pw::rpc::ServerWriter<chre_rpc_TimerResponse> &writer) {
  RpcWorldManagerSingleton::get()->timerStart(request.num_ticks, writer);
}

void timerResponse(const chre_rpc_TimerResponse &response) {
  LOGI("Tick response: %d", response.tick_number);
}

void timerEnd(pw::Status status) {
  LOGI("Tick stream end: %d", static_cast<int>(status.code()));
}

bool RpcWorldManager::start() {
  chre::RpcServer::Service service = {
      mRpcWorldService, 0xca8f7150a3f05847 /* id */, 0x01020034 /* version */};
  if (!mServer.registerServices(1, &service)) {
    LOGE("Error while registering the service");
  }

  auto client =
      mClient.get<chre::rpc::pw_rpc::nanopb::RpcWorldService::Client>();

  if (client.has_value()) {
    chre_rpc_NumberMessage incrementRequest;
    incrementRequest.number = 101;
    mIncrementCall = client->Increment(incrementRequest, incrementResponse);
    CHRE_ASSERT(mIncrementCall.active());

    chre_rpc_TimerRequest timerRequest;
    timerRequest.num_ticks = 5;
    mTimerCall = client->Timer(timerRequest, timerResponse, timerEnd);
    CHRE_ASSERT(mTimerCall.active());
  } else {
    LOGE("Error while retrieving the client");
  }

  return true;
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
      chre_rpc_TimerResponse response;
      response.tick_number = mTimerCurrentTick;
      mTimerWriter.Write(response);
      if (mTimerCurrentTick == mTimerTotalTicks) {
        mTimerWriter.Finish(pw::OkStatus());
        if (chreTimerCancel(mTimerId)) {
          mTimerId = CHRE_TIMER_INVALID;
        } else {
          LOGE("Error while cancelling the timer");
        }
      }
      mTimerCurrentTick++;
  }
}

void RpcWorldManager::end() {
  if (mTimerId != CHRE_TIMER_INVALID) {
    chreTimerCancel(mTimerId);
  }
}

void RpcWorldManager::timerStart(
    uint32_t numTicks, pw::rpc::ServerWriter<chre_rpc_TimerResponse> &writer) {
  mTimerCurrentTick = 1;
  mTimerTotalTicks = numTicks;
  mTimerWriter = std::move(writer);
  mTimerId = chreTimerSet(chre::kOneSecondInNanoseconds, nullptr /*cookie*/,
                          false /*oneShot*/);
}
