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

#include "tinysys_hal_client_manager.h"

namespace aidl::android::hardware::contexthub {

std::mutex TinysysHalClientManager::sInstanceLock;
std::unique_ptr<TinysysHalClientManager> TinysysHalClientManager::sInstance;

void TinysysHalClientManager::onClientDied(void *cookie) {
  auto *pid = static_cast<pid_t *>(cookie);
  getInstance()->handleClientDeath(*pid);
  delete pid;
}
}  // namespace aidl::android::hardware::contexthub
