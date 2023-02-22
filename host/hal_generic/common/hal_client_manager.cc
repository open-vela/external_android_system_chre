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
#include <android-base/strings.h>
#include <utils/SystemClock.h>
#include <fstream>

namespace android::hardware::contexthub::common::implementation {

using aidl::android::hardware::contexthub::ContextHubMessage;
using aidl::android::hardware::contexthub::HostEndpointInfo;
using aidl::android::hardware::contexthub::IContextHubCallback;

namespace {
std::string getProcessName(pid_t pid) {
  std::ifstream processNameFile("/proc/" + internal::ToString(pid) +
                                "/cmdline");
  std::string processName;
  processNameFile >> processName;
  std::replace(processName.begin(), processName.end(), /* old_value= */ '\0',
               /* new_value= */ ' ');
  std::vector<std::string> tokens =
      base::Tokenize(processName, /* delimiters= */ " ");
  return tokens.front();
}
}  // namespace

HalClientId HalClientManager::createClientId() {
  const std::lock_guard<std::mutex> lock(mClientIdLock);
  if (mPIdsToClientIds.size() > kMaxNumOfHalClients) {
    LOGE("Too many HAL clients registered which should never happen.");
    return kDefaultHalClientId;
  }
  while (isAllocatedClientId(mNextClientId)) {
    mNextClientId = (mNextClientId + 1) % kMaxHalClientId;
  }
  return mNextClientId;
}

HalClientId HalClientManager::getClientId() {
  pid_t pid = AIBinder_getCallingPid();
  const std::lock_guard<std::mutex> lock(mLock);
  if (isKnownPId(pid)) {
    return mPIdsToClientIds[pid];
  }
  LOGE("Failed to find the client id for pid %d", pid);
  return kDefaultHalClientId;
}

std::shared_ptr<IContextHubCallback> HalClientManager::getCallback(
    HalClientId clientId) {
  const std::lock_guard<std::mutex> lock(mLock);
  if (isAllocatedClientId(clientId)) {
    return mClientIdsToClientInfo[clientId].callback;
  }
  LOGE("Failed to find the client id %" PRIu16, clientId);
  return nullptr;
}

bool HalClientManager::registerCallback(
    const std::shared_ptr<IContextHubCallback> &callback) {
  pid_t pid = AIBinder_getCallingPid();
  HalClientId clientId;
  {
    const std::lock_guard<std::mutex> lock(mLock);
    if (isKnownPId(pid)) {
      // Though this is rare, a client is allowed to override its callback
      LOGW("The pid %d is seen before. Overriding its callback.", pid);
      clientId = mPIdsToClientIds[pid];
    } else {
      clientId = createClientId();
    }
    mPIdsToClientIds[pid] = clientId;
    mClientIdsToClientInfo.emplace(clientId, HalClientInfo(callback));
    if (mFrameworkServiceClientId == kDefaultHalClientId &&
        getProcessName(pid) == kSystemServerName) {
      mFrameworkServiceClientId = clientId;
    }
  }
  return true;
}

void HalClientManager::handleClientDeath(pid_t pid) {
  const std::lock_guard<std::mutex> lock(mLock);
  if (!isKnownPId(pid)) {
    LOGE("Failed to locate the dead pid %d", pid);
    return;
  }
  HalClientId clientId = mPIdsToClientIds[pid];
  mPIdsToClientIds.erase(mPIdsToClientIds.find(pid));
  if (!isAllocatedClientId(clientId)) {
    LOGE("Failed to locate the dead client id %" PRIu16, clientId);
    return;
  }

  auto clientInfo = mClientIdsToClientInfo.at(clientId);
  clientInfo.callback.reset();
  if (mPendingLoadTransaction.has_value() &&
      mPendingLoadTransaction->clientId == clientId) {
    mPendingLoadTransaction.reset();
  }
  if (mPendingUnloadTransaction.has_value() &&
      mPendingUnloadTransaction->clientId == clientId) {
    mPendingUnloadTransaction.reset();
  }
  mClientIdsToClientInfo.erase(clientId);
  if (mFrameworkServiceClientId == clientId) {
    mFrameworkServiceClientId = kDefaultHalClientId;
  }
  LOGI("Process %" PRIu32 " is disconnected from HAL.", pid);
}

bool HalClientManager::registerPendingLoadTransaction(
    std::unique_ptr<chre::FragmentedLoadTransaction> transaction) {
  if (transaction->isComplete()) {
    LOGW("No need to register a completed load transaction.");
    return false;
  }
  pid_t pid = AIBinder_getCallingPid();

  const std::lock_guard<std::mutex> lock(mLock);
  if (!isKnownPId(pid)) {
    LOGE("Unknown HAL client when registering its pending load transaction.");
    return false;
  }
  auto clientId = mPIdsToClientIds[pid];
  if (!isNewTransactionAllowed(clientId)) {
    return false;
  }
  mPendingLoadTransaction.emplace(
      clientId, /* registeredTimeMs= */ android::elapsedRealtime(),
      /* currentFragmentId= */ std::nullopt, std::move(transaction));
  return true;
}

std::optional<chre::FragmentedLoadRequest>
HalClientManager::getNextFragmentedLoadRequest(
    HalClientId clientId, uint32_t transactionId,
    std::optional<size_t> currentFragmentId) {
  const std::lock_guard<std::mutex> lock(mLock);
  if (!isAllocatedClientId(clientId)) {
    LOGE("Unknown client id %" PRIu16
         " while getting the next fragmented request.",
         clientId);
    return std::nullopt;
  }
  if (!isPendingLoadTransactionExpected(clientId, transactionId,
                                        currentFragmentId)) {
    LOGE("The transaction %" PRIu32 " for client %" PRIu16 " is unexpected",
         transactionId, clientId);
    return std::nullopt;
  }
  if (mPendingLoadTransaction->transaction->isComplete()) {
    mPendingLoadTransaction.reset();
    return std::nullopt;
  }
  auto request = mPendingLoadTransaction->transaction->getNextRequest();
  mPendingLoadTransaction->currentFragmentId = request.fragmentId;
  return request;
}

bool HalClientManager::isPendingLoadTransactionExpected(
    HalClientId clientId, uint32_t transactionId,
    std::optional<size_t> currentFragmentId) {
  if (!mPendingLoadTransaction.has_value()) {
    return false;
  }
  return mPendingLoadTransaction->clientId == clientId &&
         mPendingLoadTransaction->currentFragmentId == currentFragmentId &&
         mPendingLoadTransaction->transaction->getTransactionId() ==
             transactionId;
}

bool HalClientManager::registerPendingUnloadTransaction() {
  pid_t pid = AIBinder_getCallingPid();
  const std::lock_guard<std::mutex> lock(mLock);
  if (!isKnownPId(pid)) {
    LOGE("Unknown HAL client when registering its pending unload transaction.");
    return false;
  }
  auto clientId = mPIdsToClientIds[pid];
  if (!isNewTransactionAllowed(clientId)) {
    return false;
  }
  mPendingUnloadTransaction.emplace(
      clientId,
      /* registeredTimeMs= */ android::elapsedRealtime());
  return true;
}

void HalClientManager::finishPendingUnloadTransaction(HalClientId clientId) {
  const std::lock_guard<std::mutex> lock(mLock);
  if (!mPendingUnloadTransaction.has_value()) {
    // This should never happen
    LOGE("No pending unload transaction to finish.");
    return;
  }
  if (mPendingUnloadTransaction->clientId != clientId) {
    // This should never happen
    LOGE("Mismatched pending unload transaction. Registered by client %" PRIu16
         " but client %" PRIu16 " requests to clear it.",
         mPendingUnloadTransaction->clientId, clientId);
    return;
  }
  mPendingUnloadTransaction.reset();
}

bool HalClientManager::isNewTransactionAllowed(HalClientId clientId) {
  if (mPendingLoadTransaction.has_value()) {
    auto timeElapsedMs =
        android::elapsedRealtime() - mPendingLoadTransaction->registeredTimeMs;
    if (timeElapsedMs < kTransactionTimeoutThresholdMs) {
      LOGE("Rejects client %" PRIu16
           "'s transaction because an active load "
           "transaction from client %" PRIu16 " exists. Try again later.",
           clientId, mPendingLoadTransaction->clientId);
      return false;
    }
    LOGE("Client %" PRIu16
         "'s pending load transaction is overridden by client %" PRIu16
         " after holding the slot for %" PRIu64 " ms",
         mPendingLoadTransaction->clientId, clientId, timeElapsedMs);
    mPendingLoadTransaction.reset();
    return true;
  }
  if (mPendingUnloadTransaction.has_value()) {
    auto timeElapsedMs = android::elapsedRealtime() -
                         mPendingUnloadTransaction->registeredTimeMs;
    if (timeElapsedMs < kTransactionTimeoutThresholdMs) {
      LOGE("Rejects client %" PRIu16
           "'s transaction because an active unload "
           "transaction from client %" PRIu16 " exists. Try again later.",
           clientId, mPendingUnloadTransaction->clientId);
      return false;
    }
    LOGE("A pending unload transaction registered by client %" PRIu16
         " is overridden by a new transaction from client %" PRIu16
         " after holding the slot for %" PRIu64 "ms",
         mPendingUnloadTransaction->clientId, clientId, timeElapsedMs);
    mPendingUnloadTransaction.reset();
    return true;
  }
  return true;
}

bool HalClientManager::registerEndpointId(const HostEndpointId &endpointId) {
  pid_t pid = AIBinder_getCallingPid();
  const std::lock_guard<std::mutex> lock(mLock);
  if (!isKnownPId(pid)) {
    LOGE(
        "Unknown HAL client (pid %d). Register the callback before registering "
        "an endpoint.",
        pid);
    return false;
  }
  HalClientId clientId = mPIdsToClientIds[pid];
  if (!isValidEndpointId(clientId, endpointId)) {
    LOGE("Endpoint id %" PRIu16 " from process %d is out of range.", endpointId,
         pid);
    return false;
  }
  if (mClientIdsToClientInfo[clientId].endpointIds.find(endpointId) !=
      mClientIdsToClientInfo[clientId].endpointIds.end()) {
    LOGW("The endpoint %" PRIu16 " is already connected.", endpointId);
    return false;
  }
  mClientIdsToClientInfo[clientId].endpointIds.insert(endpointId);
  LOGI("Endpoint id %" PRIu16 " is connected to client %" PRIu16, endpointId,
       clientId);
  return true;
}

bool HalClientManager::removeEndpointId(const HostEndpointId &endpointId) {
  pid_t pid = AIBinder_getCallingPid();
  const std::lock_guard<std::mutex> lock(mLock);
  if (!isKnownPId(pid)) {
    LOGE(
        "Unknown HAL client (pid %d). A callback should have been registered "
        "before removing an endpoint.",
        pid);
    return false;
  }
  HalClientId clientId = mPIdsToClientIds[pid];
  if (!isValidEndpointId(clientId, endpointId)) {
    LOGE("Endpoint id %" PRIu16 " from process %d is out of range.", endpointId,
         pid);
    return false;
  }
  if (mClientIdsToClientInfo[clientId].endpointIds.find(endpointId) ==
      mClientIdsToClientInfo[clientId].endpointIds.end()) {
    LOGW("The endpoint %" PRIu16 " is not connected.", endpointId);
    return false;
  }
  mClientIdsToClientInfo[clientId].endpointIds.erase(endpointId);
  LOGI("Endpoint id %" PRIu16 " is removed from client %" PRIu16, endpointId,
       clientId);
  return true;
}

std::shared_ptr<IContextHubCallback> HalClientManager::getCallbackForEndpoint(
    const HostEndpointId &endpointId) {
  const std::lock_guard<std::mutex> lock(mLock);
  HalClientId clientId = getClientIdFromEndpointId(endpointId);
  if (!isAllocatedClientId(clientId)) {
    LOGE("Unknown endpoint id %" PRIu16 ". Please register the callback first.",
         endpointId);
    return nullptr;
  }
  return mClientIdsToClientInfo[clientId].callback;
}

void HalClientManager::sendMessageForAllCallbacks(
    const ContextHubMessage &message,
    const std::vector<std::string> &messageParams) {
  const std::lock_guard<std::mutex> lock(mLock);
  for (const auto &[_, clientInfo] : mClientIdsToClientInfo) {
    if (clientInfo.callback != nullptr) {
      clientInfo.callback->handleContextHubMessage(message, messageParams);
    }
  }
}

const std::unordered_set<HostEndpointId>
    *HalClientManager::getAllConnectedEndpoints(pid_t pid) {
  const std::lock_guard<std::mutex> lock(mLock);
  if (!isKnownPId(pid)) {
    LOGE("Unknown HAL client with pid %d", pid);
    return nullptr;
  }
  HalClientId clientId = mPIdsToClientIds[pid];
  if (mClientIdsToClientInfo.find(clientId) == mClientIdsToClientInfo.end()) {
    LOGE("Can't find any information for client id %" PRIu16, clientId);
    return nullptr;
  }
  return &mClientIdsToClientInfo[clientId].endpointIds;
}

bool HalClientManager::mutateEndpointIdFromHostIfNeeded(
    const pid_t &pid, HostEndpointId &endpointId) {
  const std::lock_guard<std::mutex> lock(mLock);
  if (!isKnownPId(pid)) {
    LOGE("Unknown HAL client with pid %d", pid);
    return false;
  }
  // no need to mutate client id for framework service
  if (mPIdsToClientIds[pid] != mFrameworkServiceClientId) {
    HalClientId clientId = mPIdsToClientIds[pid];
    endpointId = kVendorEndpointIdBitMask |
                 clientId << kNumOfBitsForEndpointId | endpointId;
  }
  return true;
}

HostEndpointId HalClientManager::convertToOriginalEndpointId(
    const HostEndpointId &endpointId) {
  if (endpointId & kVendorEndpointIdBitMask) {
    return endpointId & kMaxVendorEndpointId;
  }
  return endpointId;
}
}  // namespace android::hardware::contexthub::common::implementation