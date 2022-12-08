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
#include <chre.h>
#include <inttypes.h>

#include "chre/util/nanoapp/log.h"
#include "chre/util/time.h"

/**
 * @file
 *
 * This nanoapp is designed to continually start and stop BLE scans and verify
 * that the expected data is delivered. BLE_WORLD_ENABLE_BATCHING can be enabled
 * to test batching and flushing if the nanoapp has the
 * CHRE_BLE_CAPABILITIES_SCAN_RESULT_BATCHING capability. This will configure
 * the BLE scans with a batch window and periodically make flush requests to get
 * batched BLE scan result events.
 */

#ifdef CHRE_NANOAPP_INTERNAL
namespace chre {
namespace {
#endif  // CHRE_NANOAPP_INTERNAL

constexpr int8_t kDataTypeServiceData = 0x16;

//! Set this environment variable to true to test BLE scan batching.
#define BLE_WORLD_ENABLE_BATCHING false

#ifdef BLE_WORLD_ENABLE_BATCHING
//! A timer handle to request the BLE flush.
uint32_t gFlushTimerHandle = 0;
//! The period to which to make the BLE flush request.
uint64_t gFlushPeriodNs = 7 * chre::kOneSecondInNanoseconds;
#endif  // BLE_WORLD_ENABLE_BATCHING

//! Report delay for BLE scans.
uint32_t gBleBatchDurationMs = 0;
//! A timer handle to toggle enable/disable BLE scans.
uint32_t gEnableDisableTimerHandle = 0;
//! The period at which to enable/disable BLE scans.
uint64_t gEnableDisablePeriodNs = 10 * chre::kOneSecondInNanoseconds;
//! True if BLE scans are currently enabled
bool gBleEnabled = false;

chreBleGenericFilter createBleGenericFilter(uint8_t type, uint8_t len,
                                            uint8_t *data, uint8_t *mask) {
  chreBleGenericFilter filter;
  memset(&filter, 0, sizeof(filter));
  filter.type = type;
  filter.len = len;
  memcpy(filter.data, data, sizeof(uint8_t) * len);
  memcpy(filter.dataMask, mask, sizeof(uint8_t) * len);
  return filter;
}

bool enableBleScans() {
  chreBleGenericFilter scanFilters[2];
  uint8_t mask[2] = {0xFF, 0xFF};
  // Google eddystone UUID.
  uint8_t uuid1[2] = {0xFE, 0xAA};
  scanFilters[0] = createBleGenericFilter(
      CHRE_BLE_AD_TYPE_SERVICE_DATA_WITH_UUID_16, 2, uuid1, mask);
  // Google nearby fastpair UUID.
  uint8_t uuid2[2] = {0xFE, 0x2C};
  scanFilters[1] = createBleGenericFilter(
      CHRE_BLE_AD_TYPE_SERVICE_DATA_WITH_UUID_16, 2, uuid2, mask);
  const struct chreBleScanFilter filter = {
      .rssiThreshold = -128, .scanFilterCount = 2, .scanFilters = scanFilters};
  return chreBleStartScanAsync(CHRE_BLE_SCAN_MODE_BACKGROUND,
                               gBleBatchDurationMs, &filter);
}

bool disableBleScans() {
  return chreBleStopScanAsync();
}

bool nanoappStart() {
  LOGI("BLE world from version 0x%08" PRIx32, chreGetVersion());
  uint32_t capabilities = chreBleGetCapabilities();
  LOGI("Got BLE capabilities 0x%" PRIx32, capabilities);
#ifdef BLE_WORLD_ENABLE_BATCHING
  bool batchingAvailable =
      ((capabilities & CHRE_BLE_CAPABILITIES_SCAN_RESULT_BATCHING) != 0);
  if (!batchingAvailable) {
    LOGE("BLE scan result batching is unavailable");
  } else {
    gBleBatchDurationMs = 5000;
  }
#endif  // BLE_WORLD_ENABLE_BATCHING
  bool success = enableBleScans();
  if (!success) {
    LOGE("Failed to send BLE start scan request");
  } else {
    gEnableDisableTimerHandle =
        chreTimerSet(gEnableDisablePeriodNs, &gEnableDisableTimerHandle,
                     false /* oneShot */);
    if (gEnableDisableTimerHandle == CHRE_TIMER_INVALID) {
      LOGE("Could not set enable/disable timer");
    }

#ifdef BLE_WORLD_ENABLE_BATCHING
    if (batchingAvailable) {
      gFlushTimerHandle =
          chreTimerSet(gFlushPeriodNs, &gFlushTimerHandle, false /* oneShot */);
      if (gFlushTimerHandle == CHRE_TIMER_INVALID) {
        LOGE("Could not set flush timer");
      }
    }
#endif  // BLE_WORLD_ENABLE_BATCHING
  }
  return true;
}

void parseAdData(const uint8_t *data, uint16_t size) {
  for (uint16_t i = 0; i < size;) {
    // First byte has the dvertisement data length.
    uint16_t ad_data_length = data[i];
    // Early termination with zero length advertisement.
    if (ad_data_length == 0) break;
    // Second byte has advertisement data type.
    // Only retrieves service data for Nearby.
    if (data[++i] == kDataTypeServiceData) {
      // First two bytes of the service data are service data UUID in little
      // endian.
      uint16_t uuid = static_cast<uint16_t>(data[i + 1] + (data[i + 2] << 8));
      LOGD("Service Data UUID: %" PRIx16, uuid);
    }
    // Moves to next advertisement.
    i += ad_data_length;
  }
}

void handleAsyncResultEvent(const chreAsyncResult *result) {
  const char *requestType =
      result->requestType == CHRE_BLE_REQUEST_TYPE_START_SCAN ? "start"
                                                              : "stop";
  if (result->success) {
    LOGI("BLE %s scan success", requestType);
    gBleEnabled = (result->requestType == CHRE_BLE_REQUEST_TYPE_START_SCAN);
  } else {
    LOGE("BLE %s scan failure: %" PRIu8, requestType, result->errorCode);
  }
}

void handleAdvertismentEvent(const chreBleAdvertisementEvent *event) {
  for (uint8_t i = 0; i < event->numReports; i++) {
    LOGD("BLE Report %" PRIu32, static_cast<uint32_t>(i + 1));
    LOGD("Event type and data status: 0x%" PRIx8,
         event->reports[i].eventTypeAndDataStatus);
    LOGD("Timestamp: %" PRIu64 " ms",
         event->reports[i].timestamp / chre::kOneMillisecondInNanoseconds);
    parseAdData(event->reports[i].data, event->reports[i].dataLength);
  }
}

void handleTimerEvent(const void *cookie) {
  if (cookie == &gEnableDisableTimerHandle) {
    bool success = false;
    if (!gBleEnabled) {
      success = enableBleScans();
    } else {
      success = disableBleScans();
    }
    if (!success) {
      LOGE("Failed to send BLE %s scan request",
           !gBleEnabled ? "start" : "stop");
    }
#ifdef BLE_WORLD_ENABLE_BATCHING
  } else if (cookie == &gFlushTimerHandle) {
    if (gBleEnabled) {
      if (!chreBleFlushAsync(nullptr /* cookie */)) {
        LOGE("Could not send flush request");
      } else {
        LOGI("Successfully sent flush request at time %" PRIu64 " ms",
             chreGetTime() / chre::kOneMillisecondInNanoseconds);
      }
    }
#endif  // BLE_WORLD_ENABLE_BATCHING
  } else {
    LOGE("Received unknown timer cookie %p", cookie);
  }
}

void nanoappHandleEvent(uint32_t senderInstanceId, uint16_t eventType,
                        const void *eventData) {
  LOGI("Received event 0x%" PRIx16 " from 0x%" PRIx32 " at time %" PRIu64 " ms",
       eventType, senderInstanceId,
       chreGetTime() / chre::kOneMillisecondInNanoseconds);
  switch (eventType) {
    case CHRE_EVENT_BLE_ADVERTISEMENT:
      handleAdvertismentEvent(
          static_cast<const chreBleAdvertisementEvent *>(eventData));
      break;
    case CHRE_EVENT_BLE_ASYNC_RESULT:
      handleAsyncResultEvent(static_cast<const chreAsyncResult *>(eventData));
      break;
    case CHRE_EVENT_TIMER:
      handleTimerEvent(eventData);
      break;
    case CHRE_EVENT_BLE_FLUSH_COMPLETE:
      LOGI("Received flush complete");
      break;
    default:
      LOGW("Unhandled event type %" PRIu16, eventType);
      break;
  }
}

void nanoappEnd() {
  if (gBleEnabled && !chreBleStopScanAsync()) {
    LOGE("Error sending BLE stop scan request sent to PAL");
  }
  if (!chreTimerCancel(gEnableDisableTimerHandle)) {
    LOGE("Error canceling BLE scan timer");
  }
  if (!chreTimerCancel(gFlushTimerHandle)) {
    LOGE("Error canceling BLE flush timer");
  }
  LOGI("nanoapp stopped");
}

#ifdef CHRE_NANOAPP_INTERNAL
}  // anonymous namespace
}  // namespace chre

#include "chre/platform/static_nanoapp_init.h"
#include "chre/util/nanoapp/app_id.h"
#include "chre/util/system/napp_permissions.h"

CHRE_STATIC_NANOAPP_INIT(BleWorld, kBleWorldAppId, 0,
                         NanoappPermissions::CHRE_PERMS_BLE);
#endif  // CHRE_NANOAPP_INTERNAL
