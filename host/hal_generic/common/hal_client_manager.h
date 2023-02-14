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
#ifndef ANDROID_HARDWARE_CONTEXTHUB_COMMON_HAL_CLIENT_MANAGER_H_
#define ANDROID_HARDWARE_CONTEXTHUB_COMMON_HAL_CLIENT_MANAGER_H_

#include <aidl/android/hardware/contexthub/ContextHubMessage.h>
#include <aidl/android/hardware/contexthub/IContextHub.h>
#include <aidl/android/hardware/contexthub/IContextHubCallback.h>
#include <chre_host/fragmented_load_transaction.h>
#include <chre_host/preloaded_nanoapp_loader.h>
#include <sys/types.h>
#include <cstddef>
#include <unordered_map>
#include <unordered_set>
#include "chre_host/log.h"
#include "hal_client_id.h"

using aidl::android::hardware::contexthub::ContextHubMessage;
using aidl::android::hardware::contexthub::HostEndpointInfo;
using aidl::android::hardware::contexthub::IContextHubCallback;
using HostEndpointId = uint16_t;

namespace android::hardware::contexthub::common::implementation {

/**
 * A class managing clients for Context Hub HAL.
 *
 * A HAL client is defined as a user calling the IContextHub API. The main
 * purpose of this class are:
 *   - to assign a unique HalClientId identifying each client;
 *   - to maintain a mapping between client ids and HalClientInfos;
 *   - to maintain a mapping between client ids and their endpoint ids.
 *
 * There are two types of ids HalClientManager will track, host endpoint id and
 * client id. A host endpoint id, which is defined at
 * hardware/interfaces/contexthub/aidl/android/hardware/contexthub/ContextHubMessage.aidl,
 * identifies a host app that communicates with a HAL client. A client id
 * identifies a HAL client, which is the layer beneath the host apps, such as
 * ContextHubService. Multiple apps with different host endpoint IDs can have
 * the same client ID.
 *
 * Note that HalClientManager is not responsible for generating endpoint ids,
 * which should be managed by HAL clients themselves.
 */
class HalClientManager {
 public:
  HalClientManager() = default;
  virtual ~HalClientManager() = default;

  /** Disable copy constructor and copy assignment to avoid duplicates. */
  HalClientManager(HalClientManager &) = delete;
  void operator=(const HalClientManager &) = delete;

  /**
   * Gets the client id allocated to the current HAL client.
   *
   * The current HAL client is identified by its process id, which is retrieved
   * by calling AIBinder_getCallingPid(). If the process doesn't have any client
   * id assigned, HalClientManager will create one mapped to its process id.
   *
   * @return client id assigned to the calling process, or kDefaultHalClientId
   * if the process id is not found.
   */
  HalClientId getClientId();

  /**
   * Gets the callback for the current HAL client identified by the clientId.
   *
   * @return callback previously registered. nullptr is returned if the clientId
   * is not found.
   */
  std::shared_ptr<IContextHubCallback> getCallback(HalClientId clientId);

  /**
   * Registers a IContextHubCallback function mapped to the current client's
   * client id.
   *
   * @return true if success, otherwise false.
   */
  bool registerCallback(const std::shared_ptr<IContextHubCallback> &callback);

  /**
   * Registers a FragmentedLoadTransaction for the current HAL client.
   *
   * At this moment only one active transaction, either load or unload, is
   * supported.
   *
   * @return true if success, otherwise false.
   */
  bool registerPendingLoadTransaction(
      std::unique_ptr<chre::FragmentedLoadTransaction> transaction);

  /**
   * Gets the next FragmentedLoadRequest from PendingLoadTransaction if it's
   * available.
   *
   * @param clientId the client id of the caller.
   * @param transactionId unique id of the load transaction.
   * @param currentFragmentId the fragment id that was previously sent out. Use
   * std::nullopt to indicate no fragment was sent out before.
   *
   * TODO(b/247124878): It would be better to differentiate between
   *   no-more-fragment and fail-to-get-fragment
   * @return an optional FragmentedLoadRequest, std::nullopt if unavailable.
   */

  std::optional<chre::FragmentedLoadRequest> getNextFragmentedLoadRequest(
      HalClientId clientId, uint32_t transactionId,
      std::optional<size_t> currentFragmentId);

  /**
   * Registers the current HAL client as having a pending unload transaction.
   *
   * At this moment only one active transaction, either load or unload, is
   * supported.
   *
   * @return true if success, otherwise false.
   */
  bool registerPendingUnloadTransaction();

  /**
   * Clears the PendingUnloadTransaction registered by clientId after the
   * operation is finished.
   */
  void finishPendingUnloadTransaction(HalClientId clientId);

  /**
   * Registers an endpoint id when it is connected to HAL.
   *
   * @return true if success, otherwise false.
   */
  bool registerEndpointId(const HostEndpointId &endpointId);

  /**
   * Removes an endpoint id when it is disconnected to HAL.
   *
   * @return true if success, otherwise false.
   */
  bool removeEndpointId(const HostEndpointId &endpointId);

  /**
   * Gets all the connected endpoints for the client identified by the pid.
   *
   * @return the pointer to the endpoint id set if the client is identifiable,
   * otherwise nullptr.
   */
  const std::unordered_set<HostEndpointId> *getAllConnectedEndpoints(pid_t pid);

  /** Sends a message to every connected endpoints. */
  void sendMessageForAllCallbacks(
      const ContextHubMessage &message,
      const std::vector<std::string> &messageParams);

  std::shared_ptr<IContextHubCallback> getCallbackForEndpoint(
      const HostEndpointId &endpointId);

  /**
   * Handles the client death event.
   *
   * @param pid of the client that loses the binder connection to the HAL.
   */
  void handleClientDeath(pid_t pid);

 protected:
  static constexpr int64_t kTransactionTimeoutThresholdMs = 5000;  // 5 seconds

  struct HalClientInfo {
    explicit HalClientInfo(
        const std::shared_ptr<IContextHubCallback> &callback) {
      this->callback = callback;
    }
    HalClientInfo() = default;
    std::shared_ptr<IContextHubCallback> callback;
    std::unordered_set<HostEndpointId> endpointIds{};
  };

  struct PendingTransaction {
    PendingTransaction(HalClientId clientId, int64_t registeredTimeMs) {
      this->clientId = clientId;
      this->registeredTimeMs = registeredTimeMs;
    }
    HalClientId clientId;
    int64_t registeredTimeMs;
  };

  /**
   * PendingLoadTransaction tracks ongoing load transactions.
   */
  struct PendingLoadTransaction : public PendingTransaction {
    PendingLoadTransaction(
        HalClientId clientId, int64_t registeredTimeMs,
        std::optional<size_t> currentFragmentId,
        std::unique_ptr<chre::FragmentedLoadTransaction> transaction)
        : PendingTransaction(clientId, registeredTimeMs) {
      this->currentFragmentId = currentFragmentId;
      this->transaction = std::move(transaction);
    }

    std::optional<size_t> currentFragmentId;  // the fragment id being sent out.
    std::unique_ptr<chre::FragmentedLoadTransaction> transaction;

    [[nodiscard]] std::string toString() const {
      using android::internal::ToString;
      return "[Load transaction: client id " + ToString(clientId) +
             ", Transaction id " + ToString(transaction->getTransactionId()) +
             ", fragment id " + ToString(currentFragmentId) + "]";
    }
  };

  /** Returns a newly created client id to uniquely identify a HAL client. */
  virtual HalClientId createClientId();

  /**
   * Returns true if the load transaction is expected.
   *
   * mLock must be held when this function is called.
   */
  bool isPendingLoadTransactionExpected(
      HalClientId clientId, uint32_t transactionId,
      std::optional<size_t> currentFragmentId);

  /**
   * Checks if the transaction registration is allowed and clears out any stale
   * pending transaction if possible.
   *
   * This function is called when registering a new transaction. The reason that
   * we still proceed when there is already a pending transaction is because we
   * don't want a stale one, for whatever reason, to block future transactions.
   * However, every transaction is guaranteed to have up to
   * kTransactionTimeoutThresholdMs to finish.
   *
   * mLock must be held when this function is called.
   *
   * @param clientId id of the client trying to register the transaction
   * @return true if registration is allowed, otherwise false.
   */
  bool isNewTransactionAllowed(HalClientId clientId);

  /**
   * Returns true if the clientId is being used.
   *
   * mLock must be held when this function is called.
   */
  inline bool isAllocatedClientId(HalClientId clientId) {
    return mClientIdsToClientInfo.find(clientId) !=
               mClientIdsToClientInfo.end() ||
           clientId == kDefaultHalClientId || clientId == kHalId;
  }

  /**
   * Returns true if the pid is being used to identify a client.
   *
   * mLock must be held when this function is called.
   */
  inline bool isKnownPId(pid_t pid) {
    return mPIdsToClientIds.find(pid) != mPIdsToClientIds.end();
  }

  // next available client id
  HalClientId mNextClientId = 1;

  // The lock guarding the access to clients' states and pending transactions
  std::mutex mLock;
  // The lock guarding the creation of client Ids
  std::mutex mClientIdLock;

  // Map from pids to client ids
  std::unordered_map<pid_t, HalClientId> mPIdsToClientIds{};
  // Map from client ids to ClientInfos
  std::unordered_map<HalClientId, HalClientInfo> mClientIdsToClientInfo{};
  // Map from endpoint ids to client ids
  std::unordered_map<HostEndpointId, HalClientId> mEndpointIdsToClientIds{};

  // States tracking pending transactions
  std::optional<PendingLoadTransaction> mPendingLoadTransaction = std::nullopt;
  std::optional<PendingTransaction> mPendingUnloadTransaction = std::nullopt;
};
}  // namespace android::hardware::contexthub::common::implementation

#endif  // ANDROID_HARDWARE_CONTEXTHUB_COMMON_HAL_CLIENT_MANAGER_H_
