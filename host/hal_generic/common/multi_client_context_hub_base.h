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

#ifndef ANDROID_HARDWARE_CONTEXTHUB_COMMON_MULTICLIENTS_HAL_BASE_H_
#define ANDROID_HARDWARE_CONTEXTHUB_COMMON_MULTICLIENTS_HAL_BASE_H_

#include <aidl/android/hardware/contexthub/BnContextHub.h>

#include "chre_connection_callback.h"
#include "chre_host/preloaded_nanoapp_loader.h"
#include "hal_client_id.h"
#include "hal_client_manager.h"

namespace android::hardware::contexthub::common::implementation {

using namespace aidl::android::hardware::contexthub;
using namespace android::chre;
using ::ndk::ScopedAStatus;

/**
 * The base class of multiclients HAL.
 *
 * A subclass should initiate mConnection, mHalClientManager and
 * mPreloadedNanoappLoader in its constructor.
 *
 * TODO(b/247124878): A few things are pending:
 *   - Some APIs of IContextHub are not implemented yet;
 *   - Involve EventLogger to log API calls;
 *   - extends DebugDumpHelper to ease debugging
 */
class MultiClientContextHubBase
    : public BnContextHub,
      public ::android::hardware::contexthub::common::implementation::
          ChreConnectionCallback {
 public:
  // functions implementing IContextHub
  ScopedAStatus getContextHubs(
      std::vector<ContextHubInfo> *contextHubInfos) override;
  ScopedAStatus loadNanoapp(int32_t contextHubId,
                            const NanoappBinary &appBinary,
                            int32_t transactionId) override;
  ScopedAStatus unloadNanoapp(int32_t contextHubId, int64_t appId,
                              int32_t transactionId) override;
  ScopedAStatus disableNanoapp(int32_t contextHubId, int64_t appId,
                               int32_t transactionId) override;
  ScopedAStatus enableNanoapp(int32_t contextHubId, int64_t appId,
                              int32_t transactionId) override;
  ScopedAStatus onSettingChanged(Setting setting, bool enabled) override;
  ScopedAStatus queryNanoapps(int32_t contextHubId) override;
  ScopedAStatus registerCallback(
      int32_t contextHubId,
      const std::shared_ptr<IContextHubCallback> &callback) override;
  ScopedAStatus sendMessageToHub(int32_t contextHubId,
                                 const ContextHubMessage &message) override;
  ScopedAStatus onHostEndpointConnected(
      const HostEndpointInfo &in_info) override;
  ScopedAStatus onHostEndpointDisconnected(char16_t in_hostEndpointId) override;
  ScopedAStatus getPreloadedNanoappIds(std::vector<int64_t> *result) override;
  ScopedAStatus onNanSessionStateChanged(bool in_state) override;

  // The callback function implementing ChreConnectionCallback
  void handleMessageFromChre(const unsigned char *messageBuffer,
                             size_t messageLen) override;

 protected:
  MultiClientContextHubBase() = default;

  bool sendFragmentedLoadRequest(HalClientId clientId,
                                 FragmentedLoadRequest &fragmentedLoadRequest);

  // Functions handling various types of messages
  void handleHubInfoResponse(const ::chre::fbs::HubInfoResponseT &message);
  void onNanoappListResponse(const ::chre::fbs::NanoappListResponseT &response,
                             HalClientId clientid);
  void onLoadNanoappResponse(const ::chre::fbs::LoadNanoappResponseT &response,
                             HalClientId clientid);
  void onUnloadNanoappResponse(
      const ::chre::fbs::UnloadNanoappResponseT &response,
      HalClientId clientid);

  // HAL is the unique owner of the communication channel to CHRE.
  std::unique_ptr<ChreConnection> mConnection{};

  // HalClientManager class should be a singleton and the only owner of the
  // HalClientManager instance. HAL just needs a pointer to call its APIs.
  HalClientManager *mHalClientManager{};

  std::unique_ptr<PreloadedNanoappLoader> mPreloadedNanoappLoader{};

  std::unique_ptr<ContextHubInfo> mContextHubInfo;

  // Mutex and CV are used to get context hub info synchronously.
  std::mutex mHubInfoMutex;
  std::condition_variable mHubInfoCondition;
};
}  // namespace android::hardware::contexthub::common::implementation
#endif  // ANDROID_HARDWARE_CONTEXTHUB_COMMON_MULTICLIENTS_HAL_BASE_H_
