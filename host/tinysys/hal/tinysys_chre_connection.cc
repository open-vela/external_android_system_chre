/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include "tinysys_chre_connection.h"
#include "chre_host/file_stream.h"
#include "chre_host/generated/host_messages_generated.h"
#include "chre_host/host_protocol_host.h"

#include <errno.h>
#include <hardware_legacy/power.h>
#include <thread>

namespace aidl::android::hardware::contexthub {

using namespace ::android::chre;
namespace fbs = ::chre::fbs;

bool TinysysChreConnection::init() {
  // Make sure the payload size is large enough for nanoapp binary fragment
  static_assert(kMaxPayloadBytes > CHRE_HOST_DEFAULT_FRAGMENT_SIZE &&
                kMaxPayloadBytes - CHRE_HOST_DEFAULT_FRAGMENT_SIZE >
                    kMaxPayloadOverheadBytes);
  mChreFileDescriptor =
      TEMP_FAILURE_RETRY(open(kChreFileDescriptorPath, O_RDWR));
  if (mChreFileDescriptor < 0) {
    LOGE("open chre device failed err=%d errno=%d\n", mChreFileDescriptor,
         errno);
    return false;
  }
  mLogger.init();
  // launch the listener task
  mMessageListener = std::thread(messageListenerTask, this);
  return true;
}

void TinysysChreConnection::messageListenerTask(
    TinysysChreConnection *chreConnection) {
  auto chreFd = chreConnection->getChreFileDescriptor();
  while (true) {
    {
      ssize_t payloadSize = TEMP_FAILURE_RETRY(
          read(chreFd, chreConnection->mPayload.get(), kMaxPayloadBytes));
      if (payloadSize < 0) {
        LOGE("%s: read failed. errno=%d\n", __func__, errno);
        continue;
      }
      handleMessageFromChre(chreConnection, chreConnection->mPayload.get(),
                            payloadSize);
    }
  }
}

bool TinysysChreConnection::sendMessage(void *data, size_t length) {
  if (length <= 0 || length > kMaxPayloadBytes) {
    LOGE("length %zu is not within the accepted range.", length);
    return false;
  }
  mChreMessage->setData(data, length);
  auto size = TEMP_FAILURE_RETRY(write(mChreFileDescriptor, mChreMessage.get(),
                                       mChreMessage->getMessageSize()));
  if (size < 0) {
    LOGE("Failed to write to chre file descriptor. errno=%d\n", errno);
    return false;
  }
  return true;
}

void TinysysChreConnection::handleMessageFromChre(
    TinysysChreConnection *chreConnection, const unsigned char *messageBuffer,
    size_t messageLen) {
  // TODO(b/267188769): Move the wake lock acquisition/release to RAII pattern.
  bool isWakelockAcquired =
      acquire_wake_lock(PARTIAL_WAKE_LOCK, kWakeLock) == 0;
  if (!isWakelockAcquired) {
    LOGE("Failed to acquire the wakelock before handling a message.");
  } else {
    LOGV("Wakelock is acquired before handling a message.");
  }
  HalClientId hostClientId;
  fbs::ChreMessage messageType = fbs::ChreMessage::NONE;
  if (!HostProtocolHost::extractHostClientIdAndType(
          messageBuffer, messageLen, &hostClientId, &messageType)) {
    LOGW("Failed to extract host client ID from message - sending broadcast");
    hostClientId = ::chre::kHostClientIdUnspecified;
  }
  LOGV("Received a message (type: %hhu, len: %zu) from CHRE for client %d",
       messageType, messageLen, hostClientId);

  switch (messageType) {
    case fbs::ChreMessage::LogMessageV2: {
      std::unique_ptr<fbs::MessageContainerT> container =
          fbs::UnPackMessageContainer(messageBuffer);
      const auto *logMessage = container->message.AsLogMessageV2();
      const std::vector<int8_t> &buffer = logMessage->buffer;
      const auto *logData = reinterpret_cast<const uint8_t *>(buffer.data());
      uint32_t numLogsDropped = logMessage->num_logs_dropped;
      chreConnection->mLogger.logV2(logData, buffer.size(), numLogsDropped);
      break;
    }
    case fbs::ChreMessage::LowPowerMicAccessRequest: {
      // to be implemented
      break;
    }
    case fbs::ChreMessage::LowPowerMicAccessRelease: {
      // to be implemented
      break;
    }
    case fbs::ChreMessage::MetricLog:
    case fbs::ChreMessage::NanConfigurationRequest:
    case fbs::ChreMessage::TimeSyncRequest:
    case fbs::ChreMessage::LogMessage: {
      LOGE("Unsupported message type %hhu received from CHRE.", messageType);
      break;
    }
    default: {
      chreConnection->getCallback()->handleMessageFromChre(messageBuffer,
                                                           messageLen);
      break;
    }
  }
  if (isWakelockAcquired) {
    if (release_wake_lock(kWakeLock)) {
      LOGE("Failed to release the wake lock");
    } else {
      LOGV("The wake lock is released after handling a message.");
    }
  }
}
}  // namespace aidl::android::hardware::contexthub
