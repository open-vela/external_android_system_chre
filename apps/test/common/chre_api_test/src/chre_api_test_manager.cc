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

#include "chre.h"
#include "chre/util/nanoapp/log.h"
#include "chre/util/time.h"

namespace {
constexpr uint64_t kSyncFunctionTimeout = 2 * chre::kOneSecondInNanoseconds;
}  // namespace

// Start ChreApiTestService RPC generated functions

pw::Status ChreApiTestService::ChreBleGetCapabilities(
    const chre_rpc_Void &request, chre_rpc_Capabilities &response) {
  ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
      CHRE_MESSAGE_PERMISSION_NONE);
  return validateInputAndCallChreBleGetCapabilities(request, response)
             ? pw::OkStatus()
             : pw::Status::InvalidArgument();
}

pw::Status ChreApiTestService::ChreBleGetFilterCapabilities(
    const chre_rpc_Void &request, chre_rpc_Capabilities &response) {
  ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
      CHRE_MESSAGE_PERMISSION_NONE);
  return validateInputAndCallChreBleGetFilterCapabilities(request, response)
             ? pw::OkStatus()
             : pw::Status::InvalidArgument();
}

pw::Status ChreApiTestService::ChreBleStartScanAsync(
    const chre_rpc_ChreBleStartScanAsyncInput &request,
    chre_rpc_Status &response) {
  ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
      CHRE_MESSAGE_PERMISSION_NONE);
  return validateInputAndCallChreBleStartScanAsync(request, response)
             ? pw::OkStatus()
             : pw::Status::InvalidArgument();
}

pw::Status ChreApiTestService::ChreBleStopScanAsync(
    const chre_rpc_Void &request, chre_rpc_Status &response) {
  ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
      CHRE_MESSAGE_PERMISSION_NONE);
  return validateInputAndCallChreBleStopScanAsync(request, response)
             ? pw::OkStatus()
             : pw::Status::InvalidArgument();
}

pw::Status ChreApiTestService::ChreSensorFindDefault(
    const chre_rpc_ChreSensorFindDefaultInput &request,
    chre_rpc_ChreSensorFindDefaultOutput &response) {
  ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
      CHRE_MESSAGE_PERMISSION_NONE);
  return validateInputAndCallChreSensorFindDefault(request, response)
             ? pw::OkStatus()
             : pw::Status::InvalidArgument();
}

pw::Status ChreApiTestService::ChreGetSensorInfo(
    const chre_rpc_ChreHandleInput &request,
    chre_rpc_ChreGetSensorInfoOutput &response) {
  ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
      CHRE_MESSAGE_PERMISSION_NONE);
  return validateInputAndCallChreGetSensorInfo(request, response)
             ? pw::OkStatus()
             : pw::Status::InvalidArgument();
}

pw::Status ChreApiTestService::ChreGetSensorSamplingStatus(
    const chre_rpc_ChreHandleInput &request,
    chre_rpc_ChreGetSensorSamplingStatusOutput &response) {
  ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
      CHRE_MESSAGE_PERMISSION_NONE);
  return validateInputAndCallChreGetSensorSamplingStatus(request, response)
             ? pw::OkStatus()
             : pw::Status::InvalidArgument();
}

pw::Status ChreApiTestService::ChreSensorConfigureModeOnly(
    const chre_rpc_ChreSensorConfigureModeOnlyInput &request,
    chre_rpc_Status &response) {
  ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
      CHRE_MESSAGE_PERMISSION_NONE);
  return validateInputAndCallChreSensorConfigureModeOnly(request, response)
             ? pw::OkStatus()
             : pw::Status::InvalidArgument();
}

pw::Status ChreApiTestService::ChreAudioGetSource(
    const chre_rpc_ChreHandleInput &request,
    chre_rpc_ChreAudioGetSourceOutput &response) {
  ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
      CHRE_MESSAGE_PERMISSION_NONE);
  return validateInputAndCallChreAudioGetSource(request, response)
             ? pw::OkStatus()
             : pw::Status::InvalidArgument();
}

pw::Status ChreApiTestService::ChreConfigureHostEndpointNotifications(
    const chre_rpc_ChreConfigureHostEndpointNotificationsInput &request,
    chre_rpc_Status &response) {
  ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
      CHRE_MESSAGE_PERMISSION_NONE);
  return validateInputAndCallChreConfigureHostEndpointNotifications(request,
                                                                    response)
             ? pw::OkStatus()
             : pw::Status::InvalidArgument();
}

pw::Status ChreApiTestService::RetrieveLatestDisconnectedHostEndpointEvent(
    const chre_rpc_Void &request,
    chre_rpc_RetrieveLatestDisconnectedHostEndpointEventOutput &response) {
  ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
      CHRE_MESSAGE_PERMISSION_NONE);
  return validateInputAndRetrieveLatestDisconnectedHostEndpointEvent(request,
                                                                     response)
             ? pw::OkStatus()
             : pw::Status::InvalidArgument();
}

pw::Status ChreApiTestService::ChreGetHostEndpointInfo(
    const chre_rpc_ChreGetHostEndpointInfoInput &request,
    chre_rpc_ChreGetHostEndpointInfoOutput &response) {
  ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
      CHRE_MESSAGE_PERMISSION_NONE);
  return validateInputAndCallChreGetHostEndpointInfo(request, response)
             ? pw::OkStatus()
             : pw::Status::InvalidArgument();
}

// End ChreApiTestService RPC generated functions

// Start ChreApiTestService RPC sync functions

void ChreApiTestService::ChreBleStartScanSync(
    const chre_rpc_ChreBleStartScanAsyncInput &request,
    ServerWriter<chre_rpc_GeneralSyncMessage> &writer) {
  if (mWriter.has_value()) {
    LOGE("ChreBleStartScanSync: a sync message already exists");
    return;
  }

  mWriter = std::move(writer);
  mTimerHandle = CHRE_TIMER_INVALID;
  mRequestType = CHRE_BLE_REQUEST_TYPE_START_SCAN;

  chre_rpc_Status status;
  if (!validateInputAndCallChreBleStartScanAsync(request, status) ||
      !status.status || !setSyncTimer()) {
    sendFailureAndFinishSyncMessage();
    LOGD("ChreBleStartScanSync: status: false (error)");
  }
}

void ChreApiTestService::ChreBleStopScanSync(
    const chre_rpc_Void &request,
    ServerWriter<chre_rpc_GeneralSyncMessage> &writer) {
  if (mWriter.has_value()) {
    LOGE("ChreBleStopScanSync: a sync message already exists");
    return;
  }

  mWriter = std::move(writer);
  mTimerHandle = CHRE_TIMER_INVALID;
  mRequestType = CHRE_BLE_REQUEST_TYPE_STOP_SCAN;

  chre_rpc_Status status;
  if (!validateInputAndCallChreBleStopScanAsync(request, status) ||
      !status.status || !setSyncTimer()) {
    sendFailureAndFinishSyncMessage();
    LOGD("ChreBleStopScanSync: status: false (error)");
  }
}

// End ChreApiTestService RPC sync functions

void ChreApiTestService::handleBleAsyncResult(const chreAsyncResult *result) {
  if (result == nullptr || !mWriter.has_value()) {
    return;
  }

  if (result->requestType == mRequestType) {
    chreTimerCancel(mTimerHandle);

    chre_rpc_GeneralSyncMessage generalSyncMessage;
    generalSyncMessage.status = result->success;
    sendAndFinishSyncMessage(generalSyncMessage);
    LOGD("Active BLE sync function: status: %s",
         generalSyncMessage.status ? "true" : "false");
  }
}

void ChreApiTestService::handleTimerEvent(const void *cookie) {
  if (mWriter.has_value() && cookie == &mTimerHandle) {
    sendFailureAndFinishSyncMessage();
    LOGD("Active sync function: status: false (timeout)");
  }
}

void ChreApiTestService::handleHostEndpointNotificationEvent(
    const chreHostEndpointNotification *data) {
  if (data->notificationType != HOST_ENDPOINT_NOTIFICATION_TYPE_DISCONNECT) {
    LOGW("Received non disconnected event");
    return;
  }
  ++mReceivedHostEndpointDisconnectedNum;
  mLatestHostEndpointNotification = *data;
}

void ChreApiTestService::copyString(char *destination, const char *source,
                                    size_t maxChars) {
  CHRE_ASSERT_NOT_NULL(destination);
  CHRE_ASSERT_NOT_NULL(source);

  if (maxChars == 0) {
    return;
  }

  uint32_t i;
  for (i = 0; i < maxChars - 1 && source[i] != '\0'; ++i) {
    destination[i] = source[i];
  }

  memset(&destination[i], 0, maxChars - i);
}

void ChreApiTestService::sendFailureAndFinishSyncMessage() {
  if (mWriter.has_value()) {
    chre_rpc_GeneralSyncMessage generalSyncMessage;
    generalSyncMessage.status = false;
    sendAndFinishSyncMessage(generalSyncMessage);
  }
}

void ChreApiTestService::sendAndFinishSyncMessage(
    const chre_rpc_GeneralSyncMessage &message) {
  if (mWriter.has_value()) {
    ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
        CHRE_MESSAGE_PERMISSION_NONE);
    mWriter->Write(message);

    ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
        CHRE_MESSAGE_PERMISSION_NONE);
    mWriter->Finish();

    mWriter.reset();
    mTimerHandle = CHRE_TIMER_INVALID;
  }
}

bool ChreApiTestService::setSyncTimer() {
  mTimerHandle = chreTimerSet(kSyncFunctionTimeout, &mTimerHandle /* cookie */,
                              true /* oneShot */);
  return mTimerHandle != CHRE_TIMER_INVALID;
}

// Start ChreApiTestManager functions

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

void ChreApiTestManager::end() {
  // do nothing
}

void ChreApiTestManager::handleEvent(uint32_t senderInstanceId,
                                     uint16_t eventType,
                                     const void *eventData) {
  if (!mServer.handleEvent(senderInstanceId, eventType, eventData)) {
    LOGE("An RPC error occurred");
  }

  switch (eventType) {
    case CHRE_EVENT_BLE_ASYNC_RESULT:
      mChreApiTestService.handleBleAsyncResult(
          static_cast<const chreAsyncResult *>(eventData));
      break;
    case CHRE_EVENT_TIMER:
      mChreApiTestService.handleTimerEvent(eventData);
      break;
    case CHRE_EVENT_HOST_ENDPOINT_NOTIFICATION:
      mChreApiTestService.handleHostEndpointNotificationEvent(
          static_cast<const chreHostEndpointNotification *>(eventData));
      break;
    default: {
      // ignore
    }
  }
}

void ChreApiTestManager::setPermissionForNextMessage(uint32_t permission) {
  mServer.setPermissionForNextMessage(permission);
}

// End ChreApiTestManager functions
