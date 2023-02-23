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
#include "chre/util/nanoapp/ble.h"
#include "chre/util/nanoapp/log.h"
#include "chre/util/time.h"

using ::chre::createBleGenericFilter;

namespace {
constexpr uint32_t kMaxBleScanFilters = 10;   // chre_api_test.options
constexpr uint32_t kMaxNameStringSize = 100;  // chre_api_test.options
constexpr uint64_t kSyncFunctionTimeout = 2 * chre::kOneSecondInNanoseconds;
}  // namespace

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

pw::Status ChreApiTestService::ChreBleStartScanAsync(
    const chre_rpc_ChreBleStartScanAsyncInput &request,
    chre_rpc_Status &response) {
  ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
      CHRE_MESSAGE_PERMISSION_NONE);
  return validateInputAndCallChreBleStartScanAsync(request, response)
             ? pw::OkStatus()
             : pw::Status::InvalidArgument();
}

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

pw::Status ChreApiTestService::ChreBleStopScanAsync(
    const chre_rpc_Void &request, chre_rpc_Status &response) {
  ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
      CHRE_MESSAGE_PERMISSION_NONE);
  return validateInputAndCallChreBleStopScanAsync(request, response)
             ? pw::OkStatus()
             : pw::Status::InvalidArgument();
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

pw::Status ChreApiTestService::ChreSensorFindDefault(
    const chre_rpc_ChreSensorFindDefaultInput &request,
    chre_rpc_ChreSensorFindDefaultOutput &response) {
  ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
      CHRE_MESSAGE_PERMISSION_NONE);

  if (request.sensorType > std::numeric_limits<uint8_t>::max()) {
    return pw::Status::InvalidArgument();
  }

  uint8_t sensorType = (uint8_t)request.sensorType;
  response.foundSensor =
      chreSensorFindDefault(sensorType, &response.sensorHandle);

  LOGD("ChreSensorFindDefault: foundSensor: %s, sensorHandle: %" PRIu32,
       response.foundSensor ? "true" : "false", response.sensorHandle);
  return pw::OkStatus();
}

pw::Status ChreApiTestService::ChreGetSensorInfo(
    const chre_rpc_ChreHandleInput &request,
    chre_rpc_ChreGetSensorInfoOutput &response) {
  ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
      CHRE_MESSAGE_PERMISSION_NONE);

  struct chreSensorInfo sensorInfo;
  memset(&sensorInfo, 0, sizeof(sensorInfo));

  response.status = chreGetSensorInfo(request.handle, &sensorInfo);

  if (response.status) {
    copyString(response.sensorName, sensorInfo.sensorName, kMaxNameStringSize);
  } else {
    response.sensorName[0] = '\0';
  }

  response.sensorType = sensorInfo.sensorType;
  response.isOnChange = sensorInfo.isOnChange;
  response.isOneShot = sensorInfo.isOneShot;
  response.reportsBiasEvents = sensorInfo.reportsBiasEvents;
  response.supportsPassiveMode = sensorInfo.supportsPassiveMode;
  response.unusedFlags = sensorInfo.unusedFlags;
  response.minInterval = sensorInfo.minInterval;
  response.sensorIndex = sensorInfo.sensorIndex;

  LOGD("ChreGetSensorInfo: status: %s, sensorType: %" PRIu32
       ", isOnChange: %" PRIu32
       ", "
       "isOneShot: %" PRIu32 ", reportsBiasEvents: %" PRIu32
       ", supportsPassiveMode: %" PRIu32 ", unusedFlags: %" PRIu32
       ", minInterval: %" PRIu64 ", sensorIndex: %" PRIu32,
       response.status ? "true" : "false", response.sensorType,
       response.isOnChange, response.isOneShot, response.reportsBiasEvents,
       response.supportsPassiveMode, response.unusedFlags, response.minInterval,
       response.sensorIndex);
  return pw::OkStatus();
}

pw::Status ChreApiTestService::ChreGetSensorSamplingStatus(
    const chre_rpc_ChreHandleInput &request,
    chre_rpc_ChreGetSensorSamplingStatusOutput &response) {
  ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
      CHRE_MESSAGE_PERMISSION_NONE);

  struct chreSensorSamplingStatus samplingStatus;
  memset(&samplingStatus, 0, sizeof(samplingStatus));

  response.status =
      chreGetSensorSamplingStatus(request.handle, &samplingStatus);
  response.interval = samplingStatus.interval;
  response.latency = samplingStatus.latency;
  response.enabled = samplingStatus.enabled;

  LOGD("ChreGetSensorSamplingStatus: status: %s, interval: %" PRIu64
       ", latency: %" PRIu64 ", enabled: %s",
       response.status ? "true" : "false", response.interval, response.latency,
       response.enabled ? "true" : "false");
  return pw::OkStatus();
}

pw::Status ChreApiTestService::ChreSensorConfigureModeOnly(
    const chre_rpc_ChreSensorConfigureModeOnlyInput &request,
    chre_rpc_Status &response) {
  ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
      CHRE_MESSAGE_PERMISSION_NONE);

  chreSensorConfigureMode mode =
      static_cast<chreSensorConfigureMode>(request.mode);
  response.status = chreSensorConfigureModeOnly(request.sensorHandle, mode);

  LOGD("ChreSensorConfigureModeOnly: status: %s",
       response.status ? "true" : "false");
  return pw::OkStatus();
}

pw::Status ChreApiTestService::ChreAudioGetSource(
    const chre_rpc_ChreHandleInput &request,
    chre_rpc_ChreAudioGetSourceOutput &response) {
  ChreApiTestManagerSingleton::get()->setPermissionForNextMessage(
      CHRE_MESSAGE_PERMISSION_NONE);

  struct chreAudioSource audioSource;
  memset(&audioSource, 0, sizeof(audioSource));

  response.status = chreAudioGetSource(request.handle, &audioSource);

  if (response.status) {
    copyString(response.name, audioSource.name, kMaxNameStringSize);
  } else {
    response.name[0] = '\0';
  }

  response.sampleRate = audioSource.sampleRate;
  response.minBufferDuration = audioSource.minBufferDuration;
  response.maxBufferDuration = audioSource.maxBufferDuration;
  response.format = audioSource.format;

  LOGD("ChreAudioGetSource: status: %s, name: %s, sampleRate %" PRIu32
       ", "
       "minBufferDuration: %" PRIu64 ", maxBufferDuration %" PRIu64
       ", format: %" PRIu32,
       response.status ? "true" : "false", response.name, response.sampleRate,
       response.minBufferDuration, response.maxBufferDuration, response.format);
  return pw::OkStatus();
}

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

bool ChreApiTestService::validateInputAndCallChreBleStartScanAsync(
    const chre_rpc_ChreBleStartScanAsyncInput &request,
    chre_rpc_Status &response) {
  bool success = false;
  if (request.mode < _chre_rpc_ChreBleScanMode_MIN ||
      request.mode > _chre_rpc_ChreBleScanMode_MAX ||
      request.mode == chre_rpc_ChreBleScanMode_INVALID) {
    LOGE("ChreBleStartScanAsync: invalid mode");
  } else if (!request.hasFilter) {
    chreBleScanMode mode = static_cast<chreBleScanMode>(request.mode);
    response.status =
        chreBleStartScanAsync(mode, request.reportDelayMs, nullptr);

    LOGD("ChreBleStartScanAsync: mode: %s, reportDelayMs: %" PRIu32
         ", filter: nullptr, status: %s",
         mode == CHRE_BLE_SCAN_MODE_BACKGROUND
             ? "background"
             : (mode == CHRE_BLE_SCAN_MODE_FOREGROUND ? "foreground"
                                                      : "aggressive"),
         request.reportDelayMs, response.status ? "true" : "false");
    success = true;
  } else if (request.filter.rssiThreshold <
                 std::numeric_limits<int8_t>::min() ||
             request.filter.rssiThreshold >
                 std::numeric_limits<int8_t>::max()) {
    LOGE("ChreBleStartScanAsync: invalid filter.rssiThreshold");
  } else if (request.filter.scanFilterCount == 0 ||
             request.filter.scanFilterCount > kMaxBleScanFilters) {
    LOGE("ChreBleStartScanAsync: invalid filter.scanFilterCount");
  } else {
    chreBleGenericFilter genericFilters[request.filter.scanFilterCount];
    bool validateFiltersSuccess = true;
    for (uint32_t i = 0;
         validateFiltersSuccess && i < request.filter.scanFilterCount; ++i) {
      const chre_rpc_ChreBleGenericFilter &scanFilter =
          request.filter.scanFilters[i];
      if (scanFilter.type > std::numeric_limits<uint8_t>::max() ||
          scanFilter.length > std::numeric_limits<uint8_t>::max()) {
        LOGE(
            "ChreBleStartScanAsync: invalid request.filter.scanFilters member: "
            "type: %" PRIu32 " or length: %" PRIu32,
            scanFilter.type, scanFilter.length);
        validateFiltersSuccess = false;
      } else if (scanFilter.data.size < scanFilter.length ||
                 scanFilter.mask.size < scanFilter.length) {
        LOGE(
            "ChreBleStartScanAsync: invalid request.filter.scanFilters member: "
            "data or mask size");
        validateFiltersSuccess = false;
      } else {
        genericFilters[i] = createBleGenericFilter(
            scanFilter.type, scanFilter.length, scanFilter.data.bytes,
            scanFilter.mask.bytes);
      }
    }

    if (validateFiltersSuccess) {
      struct chreBleScanFilter filter;
      filter.rssiThreshold = request.filter.rssiThreshold;
      filter.scanFilterCount = request.filter.scanFilterCount;
      filter.scanFilters = genericFilters;

      chreBleScanMode mode = static_cast<chreBleScanMode>(request.mode);
      response.status =
          chreBleStartScanAsync(mode, request.reportDelayMs, &filter);

      LOGD("ChreBleStartScanAsync: mode: %s, reportDelayMs: %" PRIu32
           ", scanFilterCount: %" PRIu32 ", status: %s",
           mode == CHRE_BLE_SCAN_MODE_BACKGROUND
               ? "background"
               : (mode == CHRE_BLE_SCAN_MODE_FOREGROUND ? "foreground"
                                                        : "aggressive"),
           request.reportDelayMs, request.filter.scanFilterCount,
           response.status ? "true" : "false");
      success = true;
    }
  }
  return success;
}

bool ChreApiTestService::validateInputAndCallChreBleStopScanAsync(
    const chre_rpc_Void & /* request */, chre_rpc_Status &response) {
  response.status = chreBleStopScanAsync();
  LOGD("ChreBleStopScanAsync: status: %s", response.status ? "true" : "false");
  return true;
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
    case CHRE_EVENT_BLE_ASYNC_RESULT: {
      mChreApiTestService.handleBleAsyncResult(
          static_cast<const chreAsyncResult *>(eventData));
      break;
    }
    case CHRE_EVENT_TIMER: {
      mChreApiTestService.handleTimerEvent(eventData);
      break;
    }
    default: {
      // ignore
    }
  }
}

void ChreApiTestManager::setPermissionForNextMessage(uint32_t permission) {
  mServer.setPermissionForNextMessage(permission);
}
