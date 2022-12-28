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
#include "hal_client_manager.h"

namespace android::hardware::contexthub::common::implementation {

using aidl::android::hardware::contexthub::HostEndpointInfo;
using aidl::android::hardware::contexthub::IContextHubCallback;

HalClientId HalClientManager::createClientId() {
  const std::lock_guard<std::mutex> lock(mClientIdLock);
  if (mPIdsToClientIds.size() > kMaxNumOfHalClients) {
    LOGE("Too many HAL clients registered which should never happen.");
    return kDefaultHalClientId;
  }
  while (isAllocatedClientId(mNextClientId)) {
    mNextClientId++;
  }
  return mNextClientId++;
}

HalClientId HalClientManager::getClientId() {
  pid_t pid = AIBinder_getCallingPid();
  const std::lock_guard<std::mutex> lock(mMapLock);
  if (isKnownPId(pid)) {
    return mPIdsToClientIds[pid];
  }
  LOGE("Failed to find the client id for pid %d", pid);
  return kDefaultHalClientId;
}

std::shared_ptr<IContextHubCallback> HalClientManager::getCallback(
    HalClientId clientId) {
  const std::lock_guard<std::mutex> lock(mMapLock);
  if (isAllocatedClientId(clientId)) {
    return mClientIdsToCallbacks[clientId];
  }
  LOGE("Failed to find the client id %d", clientId);
  return nullptr;
}

bool HalClientManager::registerCallback(
    const std::shared_ptr<IContextHubCallback> &callback) {
  pid_t pid = AIBinder_getCallingPid();
  HalClientId clientId = createClientId();
  {
    const std::lock_guard<std::mutex> lock(mMapLock);
    if (isKnownPId(pid)) {
      LOGI("The pid %d is seen before. Overriding it", pid);
    }
    mPIdsToClientIds[pid] = clientId;
    mClientIdsToCallbacks[clientId] = callback;
  }

  // once the call to AIBinder_linkToDeath() is successful, the cookie is
  // supposed to be release by the death recipient later.
  auto *cookie = new pid_t(pid);
  if (AIBinder_linkToDeath(callback->asBinder().get(), mDeathRecipient.get(),
                           cookie) != STATUS_OK) {
    LOGE("Failed to link client binder to death recipient");
    delete cookie;
    return false;
  }
  return true;
}

void HalClientManager::handleClientDeath(pid_t pid) {
  const std::lock_guard<std::mutex> lock(mMapLock);
  if (!isKnownPId(pid)) {
    LOGE("Failed to locate the dead pid %d", pid);
    return;
  }
  HalClientId clientId = mPIdsToClientIds[pid];
  mPIdsToClientIds.erase(mPIdsToClientIds.find(pid));
  if (!isAllocatedClientId(clientId)) {
    LOGE("Failed to locate the dead client id %d", clientId);
    return;
  }
  mClientIdsToCallbacks[clientId].reset();
  mClientIdsToCallbacks.erase(mClientIdsToCallbacks.find(clientId));
  // TODO(b/247124878): Remove clients endpoints too.
}

}  // namespace android::hardware::contexthub::common::implementation