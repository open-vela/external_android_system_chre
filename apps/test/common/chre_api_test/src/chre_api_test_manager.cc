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

/**
 * The following constants are defined in chre_api_test.options.
 */
constexpr uint32_t kThreeAxisDataReadingsMaxCount = 10;

/**
 * Closes the writer and invalidates the writer.
 *
 * @param T                   the RPC message type.
 * @param writer              the RPC ServerWriter.
 */
template <typename T>
void finishAndCloseWriter(
    Optional<ChreApiTestService::ServerWriter<T>> &writer) {
  CHRE_ASSERT(writer.has_value());

  ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
      CHRE_MESSAGE_PERMISSION_NONE);
  writer->Finish();
  writer.reset();
}

/**
 * Writes a message to the writer, then closes the writer and invalidates the
 * writer.
 *
 * @param T                   the RPC message type.
 * @param writer              the RPC ServerWriter.
 * @param message             the message to write.
 */
template <typename T>
void sendFinishAndCloseWriter(
    Optional<ChreApiTestService::ServerWriter<T>> &writer, const T &message) {
  CHRE_ASSERT(writer.has_value());

  ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
      CHRE_MESSAGE_PERMISSION_NONE);
  writer->Write(message);
  finishAndCloseWriter(writer);
}

/**
 * Sends a failure message. If there is not a valid writer, this returns
 * without doing anything.
 *
 * @param T                   the RPC message type.
 * @param writer              the RPC ServerWriter.
 */
template <typename T>
void sendFailureAndFinishCloseWriter(
    Optional<ChreApiTestService::ServerWriter<T>> &writer) {
  CHRE_ASSERT(writer.has_value());

  T message;
  message.status = false;
  sendFinishAndCloseWriter(writer, message);
}
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

pw::Status ChreApiTestService::ChreSensorConfigure(
    const chre_rpc_ChreSensorConfigureInput &request,
    chre_rpc_Status &response) {
  ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
      CHRE_MESSAGE_PERMISSION_NONE);
  return validateInputAndCallChreSensorConfigure(request, response)
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
    ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
        CHRE_MESSAGE_PERMISSION_NONE);
    writer.Finish();
    LOGE("ChreBleStartScanSync: a sync message already exists");
    return;
  }

  mWriter = std::move(writer);
  CHRE_ASSERT(mSyncTimerHandle == CHRE_TIMER_INVALID);
  mRequestType = CHRE_BLE_REQUEST_TYPE_START_SCAN;

  chre_rpc_Status status;
  if (!validateInputAndCallChreBleStartScanAsync(request, status) ||
      !status.status || !startSyncTimer()) {
    sendFailureAndFinishCloseWriter(mWriter);
    mSyncTimerHandle = CHRE_TIMER_INVALID;
    LOGD("ChreBleStartScanSync: status: false (error)");
  }
}

void ChreApiTestService::ChreBleStopScanSync(
    const chre_rpc_Void &request,
    ServerWriter<chre_rpc_GeneralSyncMessage> &writer) {
  if (mWriter.has_value()) {
    ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
        CHRE_MESSAGE_PERMISSION_NONE);
    writer.Finish();
    LOGE("ChreBleStopScanSync: a sync message already exists");
    return;
  }

  mWriter = std::move(writer);
  CHRE_ASSERT(mSyncTimerHandle == CHRE_TIMER_INVALID);
  mRequestType = CHRE_BLE_REQUEST_TYPE_STOP_SCAN;

  chre_rpc_Status status;
  if (!validateInputAndCallChreBleStopScanAsync(request, status) ||
      !status.status || !startSyncTimer()) {
    sendFailureAndFinishCloseWriter(mWriter);
    mSyncTimerHandle = CHRE_TIMER_INVALID;
    LOGD("ChreBleStopScanSync: status: false (error)");
  }
}

// End ChreApiTestService RPC sync functions

// Start ChreApiTestService event functions

void ChreApiTestService::GatherEvents(
    const chre_rpc_GatherEventsInput &request,
    ServerWriter<chre_rpc_GeneralEventsMessage> &writer) {
  if (mEventWriter.has_value()) {
    ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
        CHRE_MESSAGE_PERMISSION_NONE);
    writer.Finish();
    LOGE("GatherEvents: an event gathering call already exists");
    return;
  }

  mEventWriter = std::move(writer);
  if (request.eventType < std::numeric_limits<uint16_t>::min() ||
      request.eventType > std::numeric_limits<uint16_t>::max()) {
    LOGE("GatherEvents: invalid request.eventType");
    sendFailureAndFinishCloseWriter(mEventWriter);
    mEventTimerHandle = CHRE_TIMER_INVALID;
    return;
  }

  CHRE_ASSERT(mEventTimerHandle == CHRE_TIMER_INVALID);
  mEventTimerHandle = chreTimerSet(
      request.timeoutInNs, &mEventTimerHandle /* cookie */, true /* oneShot */);
  if (mEventTimerHandle == CHRE_TIMER_INVALID) {
    sendFailureAndFinishCloseWriter(mEventWriter);
    mEventTimerHandle = CHRE_TIMER_INVALID;
  } else {
    mEventType = static_cast<uint16_t>(request.eventType);
    mEventExpectedCount = request.eventCount;
    mEventSentCount = 0;
  }
}

// End ChreApiTestService event functions

void ChreApiTestService::handleBleAsyncResult(const chreAsyncResult *result) {
  if (result == nullptr || !mWriter.has_value()) {
    return;
  }

  if (result->requestType == mRequestType) {
    chreTimerCancel(mSyncTimerHandle);
    mSyncTimerHandle = CHRE_TIMER_INVALID;

    chre_rpc_GeneralSyncMessage generalSyncMessage;
    generalSyncMessage.status = result->success;
    sendFinishAndCloseWriter(mWriter, generalSyncMessage);
    LOGD("Active BLE sync function: status: %s",
         generalSyncMessage.status ? "true" : "false");
  }
}

void ChreApiTestService::handleGatheringEvent(uint16_t eventType,
                                              const void *eventData) {
  if (!mEventWriter.has_value() || mEventType != eventType) {
    return;
  }

  chre_rpc_GeneralEventsMessage message;
  message.status = false;
  switch (eventType) {
    case CHRE_EVENT_SENSOR_ACCELEROMETER_DATA: {
      message.status = true;

      const struct chreSensorThreeAxisData *data =
          static_cast<const struct chreSensorThreeAxisData *>(eventData);
      message.chreSensorThreeAxisData.header.baseTimestamp =
          data->header.baseTimestamp;
      message.chreSensorThreeAxisData.header.sensorHandle =
          data->header.sensorHandle;
      message.chreSensorThreeAxisData.header.readingCount =
          data->header.readingCount;
      message.chreSensorThreeAxisData.header.accuracy = data->header.accuracy;
      message.chreSensorThreeAxisData.header.reserved = data->header.reserved;

      uint32_t numIter =
          MIN(data->header.readingCount, kThreeAxisDataReadingsMaxCount);
      for (uint32_t i = 0; i < numIter; ++i) {
        message.chreSensorThreeAxisData.readings[i].timestampDelta =
            data->readings[i].timestampDelta;
        message.chreSensorThreeAxisData.readings[i].x = data->readings[i].x;
        message.chreSensorThreeAxisData.readings[i].y = data->readings[i].y;
        message.chreSensorThreeAxisData.readings[i].z = data->readings[i].z;
      }
      break;
    }
    default: {
      LOGE("GatherEvents: event type: %" PRIu16 " not implemented", eventType);
    }
  }

  ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
      CHRE_MESSAGE_PERMISSION_NONE);
  mEventWriter->Write(message);

  // Only increment the sent event count if we are sending a good event.
  // If we are sending only a failure (not implemented), then that is not
  // a good event.
  if (message.status) {
    ++mEventSentCount;
  }

  if (mEventSentCount == mEventExpectedCount || !message.status) {
    chreTimerCancel(mEventTimerHandle);
    mEventTimerHandle = CHRE_TIMER_INVALID;
    finishAndCloseWriter(mEventWriter);
  }
}

void ChreApiTestService::handleTimerEvent(const void *cookie) {
  if (mWriter.has_value() && cookie == &mSyncTimerHandle) {
    sendFailureAndFinishCloseWriter(mWriter);
    mSyncTimerHandle = CHRE_TIMER_INVALID;
    LOGD("Active sync function: status: false (timeout)");
  } else if (mEventWriter.has_value() && cookie == &mEventTimerHandle) {
    finishAndCloseWriter(mEventWriter);
    mEventTimerHandle = CHRE_TIMER_INVALID;
    LOGD("Timeout for event collection with event type: %" PRIu16, mEventType);
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

bool ChreApiTestService::startSyncTimer() {
  mSyncTimerHandle = chreTimerSet(
      kSyncFunctionTimeout, &mSyncTimerHandle /* cookie */, true /* oneShot */);
  return mSyncTimerHandle != CHRE_TIMER_INVALID;
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

  mChreApiTestService.handleGatheringEvent(eventType, eventData);

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
