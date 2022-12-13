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

#include "chre_api_test_manager.h"

#include <limits>

#include "chre.h"
#include "chre/util/nanoapp/log.h"

pw::Status ChreApiTestService::ChreBleGetCapabilities(
    const chre_rpc_Void & /* request */, chre_rpc_Capabilities &response) {
  ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
      CHRE_MESSAGE_PERMISSION_NONE);
  response.capabilities = chreBleGetCapabilities();

  LOGD("ChreBleGetCapabilities: capabilities: %" PRIu32, response.capabilities);
  return pw::OkStatus();
}

pw::Status ChreApiTestService::ChreBleGetFilterCapabilities(
    const chre_rpc_Void & /* request */, chre_rpc_Capabilities &response) {
  ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
      CHRE_MESSAGE_PERMISSION_NONE);
  response.capabilities = chreBleGetFilterCapabilities();

  LOGD("ChreBleGetFilterCapabilities: capabilities: %" PRIu32,
       response.capabilities);
  return pw::OkStatus();
}

bool ChreApiTestManager::start() {
  chre::RpcServer::Service service = {.service = mChreApiTestService,
                                      .id = 0x61002d392de8430a,
                                      .version = 0x01000000};
  if (!mServer.registerServices(1, &service)) {
    LOGE("Error while registering the service");
    return false;
  }

  return true;
}

void ChreApiTestManager::setPermissionForNextMessage(uint32_t permission) {
  mServer.setPermissionForNextMessage(permission);
}

void ChreApiTestManager::handleEvent(uint32_t senderInstanceId,
                                     uint16_t eventType,
                                     const void *eventData) {
  if (!mServer.handleEvent(senderInstanceId, eventType, eventData)) {
    LOGE("An RPC error occurred");
  }
}

void ChreApiTestManager::end() {
  // do nothing
}
