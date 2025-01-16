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

#include <debug.h>
#include <poll.h>
#include <pthread.h>
#include <sensor/accel.h>
#include <sensor/angle.h>
#include <sensor/baro.h>
#include <sensor/gyro.h>
#include <sensor/light.h>
#include <sensor/mag.h>
#include <sensor/prox.h>
#include <sensor/step_counter.h>
#include <uORB/uORB.h>

#include <chrono>
#include <cinttypes>
#include <cstdint>

#include "chre/core/event_loop_manager.h"
#include "chre/pal/sensor.h"
#include "chre/platform/memory.h"
#include "chre/platform/nuttx/task_util/task_manager.h"
#include "chre/util/array_queue.h"
#include "chre/util/macros.h"
#include "chre/util/memory.h"
#include "chre/util/unique_ptr.h"

#define QUEUE_SIZE 8

/**
 * A simulated implementation of the Sensor PAL for the nuttx platform.
 */
namespace {

using chre::EventLoopManagerSingleton;
using chre::TaskManagerSingleton;

struct sensorFlushElement {
  uint32_t requestId;
  chre::TimerHandle timer;
};

struct sensorInfoContext {
  orb_handle_s handle;
  orb_id_t meta;
  int fd;
  uint8_t type;
  uint32_t index;
  bool passive;
  bool oneshot;
  void *buffer;
  chre::ArrayQueue<sensorFlushElement, QUEUE_SIZE> flush_requests;
};

#define SENSOR_ENTRY(name, type)                                             \
  {                                                                          \
    .sensorName = name, .sensorType = type, .isOnChange = 0, .isOneShot = 0, \
    .reportsBiasEvents = 0, .supportsPassiveMode = 0, .unusedFlags = 0,      \
    .minInterval = 0, .sensorIndex = CHRE_SENSOR_INDEX_DEFAULT,              \
  }

static const struct chreSensorInfo gSensors[] = {
    SENSOR_ENTRY("sensor_accel", CHRE_SENSOR_TYPE_ACCELEROMETER),
    SENSOR_ENTRY("sensor_gyro", CHRE_SENSOR_TYPE_GYROSCOPE),
    SENSOR_ENTRY("sensor_gyro_uncal", CHRE_SENSOR_TYPE_UNCALIBRATED_GYROSCOPE),
    SENSOR_ENTRY("sensor_mag", CHRE_SENSOR_TYPE_GEOMAGNETIC_FIELD),
    SENSOR_ENTRY("sensor_mag_uncal",
                 CHRE_SENSOR_TYPE_UNCALIBRATED_GEOMAGNETIC_FIELD),
    SENSOR_ENTRY("sensor_baro", CHRE_SENSOR_TYPE_PRESSURE),
    SENSOR_ENTRY("sensor_light", CHRE_SENSOR_TYPE_LIGHT),
    SENSOR_ENTRY("sensor_prox", CHRE_SENSOR_TYPE_PROXIMITY),
    SENSOR_ENTRY("sensor_step_counter", CHRE_SENSOR_TYPE_STEP_COUNTER),
    SENSOR_ENTRY("sensor_hinge_angle", CHRE_SENSOR_TYPE_HINGE_ANGLE),
    SENSOR_ENTRY("sensor_accel_uncal",
                 CHRE_SENSOR_TYPE_UNCALIBRATED_ACCELEROMETER),
    SENSOR_ENTRY("sensor_accel", CHRE_SENSOR_TYPE_ACCELEROMETER_TEMPERATURE),
    SENSOR_ENTRY("sensor_gyro", CHRE_SENSOR_TYPE_GYROSCOPE_TEMPERATURE),
    SENSOR_ENTRY("sensor_mag", CHRE_SENSOR_TYPE_GEOMAGNETIC_FIELD_TEMPERATURE),
};

struct sensor_pal_context {
  const struct chrePalSystemApi *systemApi;
  const struct chrePalSensorCallbacks *callbacks;
  struct sensorInfoContext infoContext[ARRAY_SIZE(gSensors)];
  orb_loop_s loop;
  pthread_t thread;
};

bool gIsSensor0Enabled = false;
static struct sensor_pal_context gSensorContext;

void chrePalSensorApiClose(void) {
  sninfo("chre pal sensor close");
  for (size_t i = 0; i < ARRAY_SIZE(gSensors); i++) {
    if (gSensorContext.infoContext[i].fd) {
      orb_unsubscribe(gSensorContext.infoContext[i].fd);
      gSensorContext.infoContext[i].fd = 0;
    }

    free(gSensorContext.infoContext[i].buffer);
    gSensorContext.infoContext[i].buffer = nullptr;
  }

  if (gSensorContext.loop.fd) {
    orb_loop_deinit(&gSensorContext.loop);
    gSensorContext.loop.fd = 0;
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

bool chrePalSensorApiOpen(const struct chrePalSystemApi *systemApi,
                          const struct chrePalSensorCallbacks *callbacks) {
  int ret;
  chrePalSensorApiClose();

  if (systemApi != nullptr && callbacks != nullptr) {
    pthread_attr_t thread_attr;

    gSensorContext.systemApi = systemApi;
    gSensorContext.callbacks = callbacks;

    pthread_attr_init(&thread_attr);

    thread_attr.priority = CONFIG_CHRE_PRIORITY + 1;

    ret = pthread_create(&gSensorContext.thread, &thread_attr,
                         orb_loop_run_wrapper, &gSensorContext.loop);
    if (ret < 0) {
      snerr("pthread_create failed, ret: %d", ret);
      return false;
    }

    sninfo("pal sensor open success");
  }

  return true;
}

bool chrePalSensorApiGetSensors(const struct chreSensorInfo **sensors,
                                uint32_t *arraySize) {
  if (sensors != nullptr) {
    *sensors = gSensors;
  }
  if (arraySize != nullptr) {
    *arraySize = ARRAY_SIZE(gSensors);
  }
  return true;
}

void sendSensorStatusUpdate(uint32_t sensorInfoIndex, uint64_t intervalNs,
                            uint64_t latencyNs, bool enabled) {
  auto status = chre::MakeUniqueZeroFill<struct chreSensorSamplingStatus>();
  status->interval = intervalNs;
  status->latency = latencyNs;
  status->enabled = enabled;
  gSensorContext.callbacks->samplingStatusUpdateCallback(sensorInfoIndex,
                                                         status.release());
}

static bool chreConfigureModeDone(sensorInfoContext *sensor) {
  if (sensor->fd) {
    int ret = orb_handle_stop(&gSensorContext.loop, &sensor->handle);
    if (ret < 0) {
      snerr("orb_handle_stop failed, ret: %d", ret);
    }

    ret = orb_unsubscribe(sensor->fd);
    if (ret < 0) {
      snerr("orb_unsubscribe failed, ret: %d", ret);
    }

    sensor->fd = 0;
    sendSensorStatusUpdate(sensor->index, CHRE_SENSOR_INTERVAL_DEFAULT,
                           CHRE_SENSOR_LATENCY_DEFAULT, false);
  }

  return true;
}

static int orb_datain_cb(struct orb_handle_s *handle, void *arg) {
  int ret;
  orb_state state;
  sensorInfoContext *sensor = static_cast<sensorInfoContext *>(arg);
  ret = orb_get_state(sensor->fd, &state);
  if (ret < 0) {
    snerr("orb_get_state failed, ret: %d", ret);
    return ret;
  }

  ret = orb_copy_multi(sensor->fd, sensor->buffer, state.queue_size);
  if (ret < 0) {
    snerr("orb_copy_multi failed, ret: %d", ret);
    return ret;
  }

  switch (sensor->type) {
    case CHRE_SENSOR_TYPE_ACCELEROMETER:
    case CHRE_SENSOR_TYPE_UNCALIBRATED_ACCELEROMETER:
    case CHRE_SENSOR_TYPE_GYROSCOPE:
    case CHRE_SENSOR_TYPE_UNCALIBRATED_GYROSCOPE:
    case CHRE_SENSOR_TYPE_GEOMAGNETIC_FIELD:
    case CHRE_SENSOR_TYPE_UNCALIBRATED_GEOMAGNETIC_FIELD: {
      for (int i = 0; i < ret / sensor->meta->o_size; i++) {
        sensor_accel *buffer = static_cast<sensor_accel *>(sensor->buffer) + i;
        auto data = chre::MakeUniqueZeroFill<struct chreSensorThreeAxisData>();
        data->header.baseTimestamp = buffer->timestamp;
        data->header.sensorHandle = sensor->index;
        data->header.readingCount = 1;
        data->header.accuracy = CHRE_SENSOR_ACCURACY_HIGH;
        data->header.reserved = 0;
        data->readings[0].x = buffer->x;
        data->readings[0].y = buffer->y;
        data->readings[0].z = buffer->z;
        gSensorContext.callbacks->dataEventCallback(sensor->index,
                                                    data.release());
      }

      break;
    }
    case CHRE_SENSOR_TYPE_PRESSURE:
    case CHRE_SENSOR_TYPE_LIGHT:
    case CHRE_SENSOR_TYPE_PROXIMITY:
    case CHRE_SENSOR_TYPE_STEP_COUNTER:
    case CHRE_SENSOR_TYPE_HINGE_ANGLE: {
      for (int i = 0; i < ret / sensor->meta->o_size; i++) {
        sensor_baro *buffer = static_cast<sensor_baro *>(sensor->buffer) + i;
        auto data = chre::MakeUniqueZeroFill<struct chreSensorFloatData>();
        data->header.baseTimestamp = buffer->timestamp;
        data->header.sensorHandle = sensor->index;
        data->header.readingCount = 1;
        data->header.accuracy = CHRE_SENSOR_ACCURACY_HIGH;
        data->header.reserved = 0;
        data->readings[0].value = buffer->pressure;
        gSensorContext.callbacks->dataEventCallback(sensor->index,
                                                    data.release());
      }

      break;
    }
    case CHRE_SENSOR_TYPE_ACCELEROMETER_TEMPERATURE:
    case CHRE_SENSOR_TYPE_GYROSCOPE_TEMPERATURE:
    case CHRE_SENSOR_TYPE_GEOMAGNETIC_FIELD_TEMPERATURE: {
      for (int i = 0; i < ret / sensor->meta->o_size; i++) {
        sensor_accel buffer =
            *(static_cast<sensor_accel *>(sensor->buffer) + i);
        auto data = chre::MakeUniqueZeroFill<struct chreSensorFloatData>();
        data->header.baseTimestamp = buffer.timestamp;
        data->header.sensorHandle = sensor->index;
        data->header.readingCount = 1;
        data->header.accuracy = CHRE_SENSOR_ACCURACY_HIGH;
        data->header.reserved = 0;
        data->readings[0].temperature = buffer.temperature;
        gSensorContext.callbacks->dataEventCallback(sensor->index,
                                                    data.release());
      }

      break;
    }
    default:
      snerr("sensor type: %d is not support", sensor->type);
      return -EINVAL;
  }
  if (sensor->oneshot && !chreConfigureModeDone(sensor)) {
    snerr("sensor type: %d close failed after one shot, errno: %d",
          sensor->type, errno);
    return -errno;
  }

  return 0;
}

static int orb_flush_complete_cb(FAR struct orb_handle_s *handle,
                                 FAR void *arg) {
  int ret;
  unsigned int events;
  sensorInfoContext *sensor = static_cast<sensorInfoContext *>(arg);

  ret = orb_get_events(sensor->fd, &events);
  if (ret < 0 || events != SENSOR_EVENT_FLUSH_COMPLETE) {
    return -errno;
  }

  struct sensorFlushElement flush_request = sensor->flush_requests.front();
  chre::EventLoopManagerSingleton::get()->cancelDelayedCallback(
      flush_request.timer);
  gSensorContext.callbacks->flushCompleteCallback(
      sensor->index, flush_request.requestId, CHRE_ERROR_NONE);
  sensor->flush_requests.pop();
  return ret;
}

bool chrePalSensorApiConfigureSensor(uint32_t sensorInfoIndex,
                                     enum chreSensorConfigureMode mode,
                                     uint64_t intervalNs, uint64_t latencyNs) {
  int ret;
  UNUSED_VAR(latencyNs);
  if (sensorInfoIndex > ARRAY_SIZE(gSensors) - 1) {
    return false;
  }

  sensorInfoContext *sensor = &gSensorContext.infoContext[sensorInfoIndex];
  if (mode == CHRE_SENSOR_CONFIGURE_MODE_DONE) {
    return chreConfigureModeDone(sensor);
  }

  if (mode == CHRE_SENSOR_CONFIGURE_MODE_CONTINUOUS) {
    sensor->passive = false;
    sensor->oneshot = false;
  } else if (mode == CHRE_SENSOR_CONFIGURE_MODE_ONE_SHOT) {
    sensor->passive = false;
    sensor->oneshot = true;
  } else if (mode == CHRE_SENSOR_CONFIGURE_MODE_PASSIVE_CONTINUOUS) {
    sensor->passive = true;
    sensor->oneshot = false;
  } else if (mode == CHRE_SENSOR_CONFIGURE_MODE_PASSIVE_ONE_SHOT) {
    sensor->passive = true;
    sensor->oneshot = true;
  }

  if (sensor->passive && !sensor->fd) {
    sninfo("passive, device is not open no need to configure");
    return true;
  }

  if (!sensor->fd) {
    const orb_metadata *meta =
        orb_get_meta(gSensors[sensorInfoIndex].sensorName);
    sensor->fd = orb_subscribe(meta);
    if (sensor->fd < 0) {
      snerr("sensor type: %d subscribe failed", sensorInfoIndex);
      return false;
    }

    sensor->meta = meta;
    struct orb_state state;
    ret = orb_get_state(sensor->fd, &state);
    if (sensor->buffer == NULL) {
      sensor->buffer = malloc(state.queue_size * meta->o_size);
    }

    sensor->type = gSensors[sensorInfoIndex].sensorType;
    sensor->index = sensorInfoIndex;
    ret = orb_handle_init(&sensor->handle, sensor->fd, POLLIN | POLLPRI, sensor,
                          &orb_datain_cb, nullptr, &orb_flush_complete_cb,
                          nullptr);
    if (ret < 0) {
      snerr("sensor type: %d handle init failed", sensorInfoIndex);
      goto errout;
    }

    ret = orb_handle_start(&gSensorContext.loop, &sensor->handle);
    if (ret < 0) {
      snerr("sensor type: %d handle start failed", sensorInfoIndex);
      goto errout;
    }
  }

  if (!sensor->oneshot) {
    ret = orb_set_interval(sensor->fd, intervalNs / 1000);
    if (ret < 0) {
      snerr("sensor type: %d set interval failed", sensorInfoIndex);
      goto errout;
    }

    ret = orb_set_batch_interval(sensor->fd, latencyNs / 1000);
    if (ret < 0) {
      snerr("sensor type: %d set batch interval failed", sensorInfoIndex);
      goto errout;
    }

    sendSensorStatusUpdate(sensorInfoIndex, intervalNs, latencyNs, true);
  }

  return true;

errout:
  orb_unsubscribe(sensor->fd);
  sensor->fd = 0;
  return false;
}

static void timeoutCallback(uint16_t type, void *data, void *extraData) {
  sensorInfoContext *sensor = static_cast<sensorInfoContext *>(data);
  sensorFlushElement flush_request = sensor->flush_requests.front();
  gSensorContext.callbacks->flushCompleteCallback(
      sensor->index, flush_request.requestId, CHRE_ERROR_TIMEOUT);
  sensor->flush_requests.pop();
}

bool chrePalSensorApiFlush(uint32_t sensorInfoIndex, uint32_t *flushRequestId) {
  int ret;
  UNUSED_VAR(sensorInfoIndex);
  UNUSED_VAR(flushRequestId);
  sensorInfoContext *sensor = &gSensorContext.infoContext[sensorInfoIndex];
  if (!sensor->fd) {
    snerr("sensor is not open, sensorInfoIndex: %d", sensorInfoIndex);
    return false;
  }

  chre::TimerHandle flush_timer =
      chre::EventLoopManagerSingleton::get()->setDelayedCallback(
          chre::SystemCallbackType::SensorFlushTimeout, sensor,
          &timeoutCallback,
          (chre::Nanoseconds)CHRE_SENSOR_FLUSH_COMPLETE_TIMEOUT_NS);

  struct sensorFlushElement flush_request;
  flush_request.requestId = *flushRequestId;
  flush_request.timer = flush_timer;
  sensor->flush_requests.push(flush_request);
  ret = orb_flush(sensor->fd);
  if (ret < 0) {
    snerr("ERROR: orb_flush failed, ret:%d", errno);
    chre::EventLoopManagerSingleton::get()->cancelDelayedCallback(flush_timer);
    sensor->flush_requests.pop();
    return false;
  }

  return true;
}

bool chrePalSensorApiConfigureBiasEvents(uint32_t sensorInfoIndex, bool enable,
                                         uint64_t latencyNs) {
  UNUSED_VAR(sensorInfoIndex);
  UNUSED_VAR(enable);
  UNUSED_VAR(latencyNs);
  return true;
}

bool chrePalSensorApiGetThreeAxisBias(uint32_t sensorInfoIndex,
                                      struct chreSensorThreeAxisData *bias) {
  UNUSED_VAR(sensorInfoIndex);
  UNUSED_VAR(bias);
  return false;
}

void chrePalSensorApiReleaseSensorDataEvent(void *data) {
  chre::memoryFree(data);
}

void chrePalSensorApiReleaseSamplingStatusEvent(
    struct chreSensorSamplingStatus *status) {
  chre::memoryFree(status);
}

void chrePalSensorApiReleaseBiasEvent(void *bias) { chre::memoryFree(bias); }

}  // namespace

bool chrePalSensorIsSensor0Enabled() { return gIsSensor0Enabled; }

const chrePalSensorApi *chrePalSensorGetApi(uint32_t requestedApiVersion) {
  static const struct chrePalSensorApi kApi = {
      .moduleVersion = CHRE_PAL_SENSOR_API_CURRENT_VERSION,
      .open = chrePalSensorApiOpen,
      .close = chrePalSensorApiClose,
      .getSensors = chrePalSensorApiGetSensors,
      .configureSensor = chrePalSensorApiConfigureSensor,
      .flush = chrePalSensorApiFlush,
      .configureBiasEvents = chrePalSensorApiConfigureBiasEvents,
      .getThreeAxisBias = chrePalSensorApiGetThreeAxisBias,
      .releaseSensorDataEvent = chrePalSensorApiReleaseSensorDataEvent,
      .releaseSamplingStatusEvent = chrePalSensorApiReleaseSamplingStatusEvent,
      .releaseBiasEvent = chrePalSensorApiReleaseBiasEvent,

  };

  if (!CHRE_PAL_VERSIONS_ARE_COMPATIBLE(kApi.moduleVersion,
                                        requestedApiVersion)) {
    return nullptr;
  } else {
    return &kApi;
  }
}