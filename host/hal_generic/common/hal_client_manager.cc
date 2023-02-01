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
#include <utils/SystemClock.h>

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
  HalClientId clientId = createClientId();
  {
    const std::lock_guard<std::mutex> lock(mLock);
    if (isKnownPId(pid)) {
      LOGI("The pid %d is seen before. Overriding it", pid);
    }
    mPIdsToClientIds[pid] = clientId;
    mClientIdsToClientInfo[clientId] = {
        .callback = callback,
        .activeEndpoints = {},
    };
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

  mClientIdsToClientInfo[clientId].callback.reset();
  if (mPendingLoadTransaction.has_value() &&
      mPendingLoadTransaction->clientId == clientId) {
    mPendingLoadTransaction.reset();
  }
  if (mPendingUnloadTransaction.has_value() &&
      mPendingUnloadTransaction->clientId == clientId) {
    mPendingUnloadTransaction.reset();
  }
  mClientIdsToClientInfo.erase(clientId);
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
}  // namespace android::hardware::contexthub::common::implementation