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
#ifndef ANDROID_HARDWARE_CONTEXTHUB_COMMON_CHRE_CLIENT_CONNECTION_H_
#define ANDROID_HARDWARE_CONTEXTHUB_COMMON_CHRE_CLIENT_CONNECTION_H_

#include <aidl/android/hardware/contexthub/IContextHub.h>
#include <chre/platform/shared/host_protocol_common.h>

namespace android::hardware::contexthub::common::implementation {

using aidl::android::hardware::contexthub::ContextHubInfo;
using aidl::android::hardware::contexthub::Setting;
using ::ndk::ScopedAStatus;

enum class Result : int32_t {
  SUCCESS = 0,
  CHRE_UNAVAILABLE = 1,
  OPERATION_FAILED = 2,
};

/**
 * This interface defines the API used to send requests to CHRE.
 *
 * It extends IContextHub which is the interface between clients and the HAL.
 */
class ChreClientConnection
    : public aidl::android::hardware::contexthub::IContextHub {
 public:
  ~ChreClientConnection() override = default;

  virtual uint32_t getClientId() {
    return mClientId;
  }

  virtual bool requestDebugDump() = 0;

  // Below 4 functions are inherited from IContextHub and ICInterface that
  // require implementations. They are not used so final stub implementations
  // are provided here.
  ScopedAStatus getInterfaceVersion(int32_t *) final {
    return ScopedAStatus::fromStatus(INVALID_OPERATION);
  }
  ScopedAStatus getInterfaceHash(std::string *) final {
    return ScopedAStatus::fromStatus(INVALID_OPERATION);
  }
  ::ndk::SpAIBinder asBinder() final {
    return nullptr;
  }
  bool isRemote() final {
    return false;
  }

  // functions that help to generate ScopedAStatus from different values.
  static inline ScopedAStatus fromResult(Result result) {
    return ScopedAStatus::fromServiceSpecificError(
        static_cast<int32_t>(result));
  }
  static inline ScopedAStatus fromResult(bool result) {
    return result ? ScopedAStatus::ok() : fromResult(Result::OPERATION_FAILED);
  }

 protected:
  uint32_t mClientId = ::chre::kHostClientIdUnspecified;
};
}  // namespace android::hardware::contexthub::common::implementation

#endif  // ANDROID_HARDWARE_CONTEXTHUB_COMMON_CHRE_CLIENT_CONNECTION_H_
