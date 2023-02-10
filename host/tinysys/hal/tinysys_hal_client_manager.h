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
#ifndef TINYSYS_HAL_CLIENT_MANAGER_H_
#define TINYSYS_HAL_CLIENT_MANAGER_H_

#include <sys/types.h>
#include "chre_host/log.h"
#include "hal_client_manager.h"

namespace aidl::android::hardware::contexthub {

using namespace ::android::hardware::contexthub::common::implementation;

/**
 * A singleton class manages HAL clients for Tinysys.
 *
 * See the document of HalClientManager for details.
 *
 */
class TinysysHalClientManager : public HalClientManager {
 public:
  ~TinysysHalClientManager() override = default;

  /** The entry point of death recipient for a disconnected client. */
  static void onClientDied(void *cookie);

  static TinysysHalClientManager *getInstance() {
    const std::lock_guard<std::mutex> lock(sInstanceLock);
    // TODO(b/247124878): Given we expect only one HAL to use the client
    //   manager, we don't have to make it a singleton.
    if (sInstance == nullptr) {
      struct _TinysysHalClientManager : public TinysysHalClientManager {};
      sInstance = std::make_unique<_TinysysHalClientManager>();
    }
    return sInstance.get();
  }

 private:
  TinysysHalClientManager() {
    mDeathRecipient = ndk::ScopedAIBinder_DeathRecipient(
        AIBinder_DeathRecipient_new(TinysysHalClientManager::onClientDied));
  }
  static std::mutex sInstanceLock;
  static std::unique_ptr<TinysysHalClientManager> sInstance;
};
}  // namespace aidl::android::hardware::contexthub

#endif  // TINYSYS_HAL_CLIENT_MANAGER_H_
