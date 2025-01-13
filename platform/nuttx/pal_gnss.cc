/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "chre/platform/nuttx/pal_gnss.h"

#include <debug.h>
#include <poll.h>
#include <pthread.h>
#include <sensor/gnss.h>
#include <uORB/uORB.h>

#include <chrono>
#include <cinttypes>
#include <mutex>
#include <optional>

#include "chre/pal/gnss.h"
#include "chre/platform/log.h"
#include "chre/platform/nuttx/task_util/task_manager.h"
#include "chre/util/memory.h"
#include "chre/util/unique_ptr.h"

/**
 * A simulated implementation of the GNSS PAL for the nuttx platform.
 */
namespace {

using chre::TaskManagerSingleton;

struct gnssInfoContext {
  orb_handle_s handle;
  orb_id_t meta;
  int fd;
  void *buffer;
};

struct gnss_pal_context {
  const struct chrePalSystemApi *systemApi;
  const struct chrePalGnssCallbacks *callbacks;
  struct gnssInfoContext location;
  struct gnssInfoContext measurement;
  orb_loop_s loop;
  pthread_t thread;
};

static struct gnss_pal_context gGnssContext;
// Task to deliver asynchronous location data after a CHRE request.
std::mutex gLocationEventsMutex;
std::optional<uint32_t> gLocationEventsTaskId;
std::optional<uint32_t> gLocationEventsChangeCallbackTaskId;
bool gDelaySendingLocationEvents = false;
bool gIsLocationEnabled = false;

// Task to use when delivering a location status update.
std::optional<uint32_t> gLocationStatusTaskId;

// Task to deliver asynchronous measurement data after a CHRE request.
std::optional<uint32_t> gMeasurementEventsChangeCallbackTaskId;
std::optional<uint32_t> gMeasurementEventsTaskId;
bool gIsMeasurementEnabled = false;

// Task to use when delivering a measurement status update.
std::optional<uint32_t> gMeasurementStatusTaskId;

// Passive listener flag.
bool gIsPassiveListenerEnabled = false;

uint32_t chrePalGnssGetCapabilities() {
  int ret;
  uint32_t capabilities = 0;
  orb_info_t info;
  const orb_metadata *locationMeta = gGnssContext.location.meta;
  const orb_metadata *measurementMeta = gGnssContext.measurement.meta;
  ret = orb_subscribe(locationMeta);
  if (ret > 0) {
    if (orb_get_info(ret, &info) == OK) {
      capabilities |= CHRE_GNSS_CAPABILITIES_LOCATION;
    }

    orb_unsubscribe(ret);
  }

  ret = orb_subscribe(measurementMeta);
  if (ret > 0) {
    if (orb_get_info(ret, &info) == OK) {
      capabilities |= CHRE_GNSS_CAPABILITIES_MEASUREMENTS;
    }

    orb_unsubscribe(ret);
  }

  return capabilities;
}

static int orb_datain_cb(struct orb_handle_s *handle, void *arg) {
  int ret;
  orb_state state;
  gnssInfoContext *gnss = static_cast<gnssInfoContext *>(arg);
  const orb_metadata *meta = gnss->meta;
  ret = orb_get_state(gnss->fd, &state);
  if (ret < 0) {
    snerr("orb_get_state failed, ret: %d", ret);
    return ret;
  }

  ret = orb_copy_multi(gnss->fd, gnss->buffer, meta->o_size * state.queue_size);
  if (ret < 0) {
    snerr("orb_copy_multi failed, ret: %d", ret);
    return ret;
  }

  if (meta == ORB_ID(sensor_gnss)) {
    for (int i = 0; i < ret / meta->o_size; i++) {
      sensor_gnss *buffer = static_cast<sensor_gnss *>(gnss->buffer) + i;
      auto event = chre::MakeUniqueZeroFill<struct chreGnssLocationEvent>();
      event->timestamp = buffer->timestamp;
      event->latitude_deg_e7 = buffer->latitude;
      event->longitude_deg_e7 = buffer->longitude;
      event->altitude = buffer->altitude;
      event->accuracy = buffer->epv;
      gGnssContext.callbacks->locationEventCallback(event.release());
    }
  } else if (meta == ORB_ID(sensor_gnss_measurement)) {
    for (int i = 0; i < ret / meta->o_size; i++) {
      sensor_gnss_measurement *buffer =
          static_cast<sensor_gnss_measurement *>(gnss->buffer) + i;
      auto event = chre::MakeUniqueZeroFill<struct chreGnssDataEvent>();
      auto measurement = chre::MakeUniqueZeroFill<struct chreGnssMeasurement>();
      measurement->time_offset_ns = buffer->time_offset_ns;
      measurement->accumulated_delta_range_um =
          buffer->accumulated_delta_range_m;
      measurement->received_sv_time_in_ns = buffer->received_sv_time_in_ns;
      measurement->received_sv_time_uncertainty_in_ns =
          buffer->received_sv_time_uncertainty_in_ns;
      measurement->pseudorange_rate_mps = buffer->pseudorange_rate_mps;
      measurement->pseudorange_rate_uncertainty_mps =
          buffer->pseudorange_rate_uncertainty_mps;
      measurement->accumulated_delta_range_uncertainty_m =
          buffer->accumulated_delta_range_uncertainty_m;
      measurement->c_n0_dbhz = buffer->c_n0_dbhz;
      measurement->snr_db = buffer->snr;
      measurement->state = buffer->state;
      measurement->accumulated_delta_range_state =
          buffer->accumulated_delta_range_state;
      measurement->svid = buffer->svid;
      measurement->constellation = buffer->constellation;
      measurement->multipath_indicator = buffer->multipath_indicator;
      measurement->carrier_frequency_hz = buffer->carrier_frequency_hz;

      event->measurement_count = 1;
      event->clock.time_ns =
          static_cast<int64_t>(gGnssContext.systemApi->getCurrentTime());
      event->measurements = measurement.release();
      gGnssContext.callbacks->measurementEventCallback(event.release());
    }
  }

  return 0;
}

static int gnssUnsubscribe(gnssInfoContext *infoContext) {
  int ret = CHRE_ERROR_NONE;
  if (infoContext->fd) {
    ret = orb_handle_stop(&gGnssContext.loop, &infoContext->handle);
    if (ret < 0) {
      snerr("gnss handle stop failed, ret: %d", ret);
      ret = CHRE_ERROR;
    }

    ret = orb_unsubscribe(infoContext->fd);
    if (ret < 0) {
      snerr("gnss unsubscribe failed, ret: %d", ret);
      ret = CHRE_ERROR_INVALID_ARGUMENT;
    }

    infoContext->fd = 0;
  }

  return ret;
}

static int gnssSubscribe(uint32_t minIntervalMs, gnssInfoContext *infoContext) {
  int ret;
  if (infoContext->fd == 0) {
    ret = orb_subscribe(infoContext->meta);
    if (ret < 0) {
      snerr("gnss subscribe failed, ret: %d", infoContext->fd);
      return CHRE_ERROR_NOT_SUPPORTED;
    }

    infoContext->fd = ret;
    struct orb_state state;
    ret = orb_get_state(infoContext->fd, &state);
    if (infoContext->buffer == NULL) {
      infoContext->buffer = malloc(state.queue_size * infoContext->meta->o_size);
    }

    ret = orb_handle_init(&infoContext->handle, infoContext->fd, POLLIN,
                          infoContext, orb_datain_cb, NULL, NULL, NULL);
    if (ret < 0) {
      snerr("orb_handle_init failed, ret: %d", ret);
      goto errout;
    }

    ret = orb_handle_start(&gGnssContext.loop, &infoContext->handle);
    if (ret < 0) {
      snerr("orb_handle_start failed, ret: %d", ret);
      goto errout;
    }
  }

  ret = orb_set_interval(infoContext->fd, minIntervalMs / 1000);
  if (ret < 0) {
    snerr("gnss set interval failed, ret: %d", ret);
    return CHRE_ERROR_INVALID_ARGUMENT;
  }

  return CHRE_ERROR_NONE;

errout:
  free(infoContext->buffer);
  infoContext->buffer = NULL;
  orb_unsubscribe(infoContext->fd);
  infoContext->fd = 0;
  return CHRE_ERROR;
}

bool chrePalControlLocationSession(bool enable, uint32_t minIntervalMs,
                                   uint32_t minTimeToNextFixMs) {
  int ret;
  if (enable) {
    ret = gnssSubscribe(minIntervalMs, &gGnssContext.location);
    if (ret != CHRE_ERROR_NONE) {
      snerr("gnss subscribe failed, ret: %d", ret);
      goto errout;
    }

    ret = orb_set_batch_interval(gGnssContext.location.fd, minTimeToNextFixMs);
    if (ret < 0) {
      snerr("gnss set batch interval failed, ret: %d", ret);
      ret = CHRE_ERROR_INVALID_ARGUMENT;
      goto errout;
    }

    gGnssContext.callbacks->locationStatusChangeCallback(true, CHRE_ERROR_NONE);
    gIsLocationEnabled = true;
    return true;

errout:
    orb_unsubscribe(gGnssContext.location.fd);
    gGnssContext.location.fd = 0;
    gGnssContext.callbacks->locationStatusChangeCallback(true, ret);
    return false;
  }

  ret = gnssUnsubscribe(&gGnssContext.location);
  if (ret != CHRE_ERROR_NONE) {
    snerr("gnss unsubscribe failed, ret: %d", ret);
    ret = CHRE_ERROR_INVALID_ARGUMENT;
  }

  gGnssContext.callbacks->locationStatusChangeCallback(false, ret);
  gIsLocationEnabled = !ret;
  return ret == CHRE_ERROR_NONE;
}

void chrePalGnssReleaseLocationEvent(struct chreGnssLocationEvent *event) {
  chre::memoryFree(event);
}

bool chrePalControlMeasurementSession(bool enable, uint32_t minIntervalMs) {
  int ret;
  if (enable) {
    ret = gnssSubscribe(minIntervalMs, &gGnssContext.measurement);
    if (ret != CHRE_ERROR_NONE) {
      snerr("gnss subscribe failed, ret: %d", ret);
      orb_unsubscribe(gGnssContext.measurement.fd);
      gGnssContext.location.fd = 0;
      gGnssContext.callbacks->measurementStatusChangeCallback(true, ret);
      return false;
    }

    gGnssContext.callbacks->measurementStatusChangeCallback(true,
                                                            CHRE_ERROR_NONE);
    gIsMeasurementEnabled = true;
    return true;
  }

  ret = gnssUnsubscribe(&gGnssContext.measurement);
  if (ret != CHRE_ERROR_NONE) {
    snerr("gnss unsubscribe failed, ret: %d", ret);
    ret = CHRE_ERROR_INVALID_ARGUMENT;
  }

  gGnssContext.callbacks->measurementStatusChangeCallback(false, ret);
  gIsMeasurementEnabled = !ret;
  return ret == CHRE_ERROR_NONE;
}

void chrePalGnssReleaseMeasurementDataEvent(struct chreGnssDataEvent *event) {
  chre::memoryFree(
      const_cast<struct chreGnssMeasurement *>(event->measurements));
  chre::memoryFree(event);
}

void chrePalGnssApiClose() {
  sninfo("chre pal gnss close");
  if (gGnssContext.location.fd) {
    orb_unsubscribe(gGnssContext.location.fd);
    gGnssContext.location.fd = 0;
  }

  free(gGnssContext.location.buffer);
  gGnssContext.location.buffer = nullptr;
  if (gGnssContext.measurement.fd) {
    orb_unsubscribe(gGnssContext.measurement.fd);
    gGnssContext.measurement.fd = 0;
  }

  free(gGnssContext.measurement.buffer);
  gGnssContext.measurement.buffer = nullptr;

  if (gGnssContext.loop.fd) {
    orb_loop_deinit(&gGnssContext.loop);
    gGnssContext.loop.fd = 0;
  }
}

static void *orb_loop_run_wrapper(void *arg) {
  int ret;
  orb_loop_s *loop = static_cast<orb_loop_s *>(arg);
  ret = orb_loop_init(loop, ORB_EPOLL_TYPE);
  if (ret < 0) {
    snerr("orb_loop_init failed, ret: %d", ret);
    return nullptr;
  }

  ret = orb_loop_run(loop);
  if (ret < 0) {
    snerr("orb_loop_run failed, ret: %d", ret);
  }

  return nullptr;
}

bool chrePalGnssApiOpen(const struct chrePalSystemApi *systemApi,
                        const struct chrePalGnssCallbacks *callbacks) {
  chrePalGnssApiClose();

  if (systemApi != nullptr && callbacks != nullptr) {
    pthread_attr_t thread_attr;
    int ret;

    gGnssContext.systemApi = systemApi;
    gGnssContext.callbacks = callbacks;
    gGnssContext.location.meta = ORB_ID(sensor_gnss);
    gGnssContext.measurement.meta = ORB_ID(sensor_gnss_measurement);

    pthread_attr_init(&thread_attr);

    thread_attr.priority = CONFIG_CHRE_PRIORITY + 1;

    ret = pthread_create(&gGnssContext.thread, &thread_attr,
                         orb_loop_run_wrapper, &gGnssContext.loop);
    if (ret < 0) {
      snerr("pthread_create failed, ret: %d", ret);
      return false;
    }

    sninfo("pal sensor open success");
    return true;
  }

  return false;
}

bool chrePalGnssconfigurePassiveLocationListener(bool enable) {
  gIsPassiveListenerEnabled = enable;
  return true;
}

}  // anonymous namespace

bool chrePalGnssIsLocationEnabled() { return gIsLocationEnabled; }

bool chrePalGnssIsMeasurementEnabled() { return gIsMeasurementEnabled; }

bool chrePalGnssIsPassiveLocationListenerEnabled() {
  return gIsPassiveListenerEnabled;
}

void chrePalGnssDelaySendingLocationEvents(bool enabled) {
  gDelaySendingLocationEvents = enabled;
}

void chrePalGnssStartSendingLocationEvents() {
  CHRE_ASSERT(gDelaySendingLocationEvents);
}

const struct chrePalGnssApi *chrePalGnssGetApi(uint32_t requestedApiVersion) {
  static const struct chrePalGnssApi kApi = {
      .moduleVersion = CHRE_PAL_GNSS_API_CURRENT_VERSION,
      .open = chrePalGnssApiOpen,
      .close = chrePalGnssApiClose,
      .getCapabilities = chrePalGnssGetCapabilities,
      .controlLocationSession = chrePalControlLocationSession,
      .releaseLocationEvent = chrePalGnssReleaseLocationEvent,
      .controlMeasurementSession = chrePalControlMeasurementSession,
      .releaseMeasurementDataEvent = chrePalGnssReleaseMeasurementDataEvent,
      .configurePassiveLocationListener =
          chrePalGnssconfigurePassiveLocationListener,
  };

  if (!CHRE_PAL_VERSIONS_ARE_COMPATIBLE(kApi.moduleVersion,
                                        requestedApiVersion)) {
    return nullptr;
  } else {
    return &kApi;
  }
}
