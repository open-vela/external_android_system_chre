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

#ifndef TINYSYS_CHRE_CONNECTION_H_
#define TINYSYS_CHRE_CONNECTION_H_

#include "chre_connection.h"
#include "chre_connection_callback.h"
#include "chre_host/fragmented_load_transaction.h"
#include "chre_host/log.h"
#include "chre_host/log_message_parser.h"

#include <unistd.h>
#include <cassert>
#include <thread>

namespace aidl::android::hardware::contexthub {

using namespace ::android::hardware::contexthub::common::implementation;

/** A class handling message transmission between context hub HAL and CHRE. */
// TODO(b/267188769): We should add comments explaining how IPI works.
class TinysysChreConnection : public ChreConnection {
 public:
  TinysysChreConnection(ChreConnectionCallback *callback)
      : mCallback(callback) {
    mPayload = std::make_unique<uint8_t[]>(kMaxPayloadBytes);
    mChreMessage = std::make_unique<ChreConnectionMessage>();
  };

  ~TinysysChreConnection() {
    // TODO(b/264308286): Need a decent way to terminate the listener thread.
    close(mChreFileDescriptor);
    if (mMessageListener.joinable()) {
      mMessageListener.join();
    }
  }

  static void handleMessageFromChre(TinysysChreConnection *chreConnection,
                                    const unsigned char *messageBuffer,
                                    size_t messageLen);

  bool init() override;

  bool sendMessage(void *data, size_t length) override;

  inline ChreConnectionCallback *getCallback() {
    return mCallback;
  }

 private:
  // The wakelock used to keep device awake while handleUsfMsgAsync() is being
  // called.
  static constexpr char kWakeLock[] = "tinysys_chre_hal_wakelock";

  // Max payload size that can be sent to CHRE
  static constexpr uint32_t kMaxPayloadBytes = 4096;

  // Max overhead of the nanoapp binary payload caused by the fbs encapsulation
  static constexpr uint32_t kMaxPayloadOverheadBytes = 1024;

  // The path to CHRE file descriptor
  static constexpr char kChreFileDescriptorPath[] = "/dev/scp_chre_manager";

  // Wrapper for a message sent to CHRE
  struct ChreConnectionMessage {
    // This magic number is the SCP_CHRE_MAGIC constant defined by kernel
    // scp_chre_manager service. The value is embedded in the payload as a
    // security check for proper use of the device node.
    uint32_t magic = 0x67728269;
    uint32_t payloadSize = 0;
    uint8_t payload[kMaxPayloadBytes];

    void setData(void *data, size_t length) {
      assert(length <= kMaxPayloadBytes);
      memcpy(payload, data, length);
      payloadSize = static_cast<uint32_t>(length);
    }

    uint32_t getMessageSize() {
      return sizeof(magic) + sizeof(payloadSize) + payloadSize;
    }
  };

  // The task receiving message from CHRE
  static void messageListenerTask(TinysysChreConnection *chreConnection);

  inline int getChreFileDescriptor() {
    return mChreFileDescriptor;
  }

  // The parser of buffered logs from CHRE
  ::android::chre::LogMessageParser mLogger{};

  // The file descriptor for communication with CHRE
  int mChreFileDescriptor;

  // The calback function that should be implemented by HAL
  ChreConnectionCallback *mCallback;

  // the message listener thread that hosts messageListenerTask
  std::thread mMessageListener;

  // Payload received from CHRE
  std::unique_ptr<uint8_t[]> mPayload;

  // message to be sent to CHRE
  std::unique_ptr<ChreConnectionMessage> mChreMessage;
};
}  // namespace aidl::android::hardware::contexthub

#endif  // TINYSYS_CHRE_CONNECTION_H_
