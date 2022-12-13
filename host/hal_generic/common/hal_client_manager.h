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

#include <aidl/android/hardware/contexthub/IContextHub.h>
#include <aidl/android/hardware/contexthub/IContextHubCallback.h>
#include <sys/types.h>
#include <cstddef>
#include <unordered_map>
#include "chre_host/log.h"
#include "hal_client_id.h"

using aidl::android::hardware::contexthub::HostEndpointInfo;
using aidl::android::hardware::contexthub::IContextHubCallback;

namespace android::hardware::contexthub::common::implementation {

/**
 * A singleton class managing clients for Context Hub HAL.
 *
 * A HAL client is defined as a user calling the IContextHub API. The main
 * purpose of this class are:
 *   - to assign a unique HalClientId identifying each client;
 *   - to maintain a mapping between client ids and the IContextHubCallback
 * functions;
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
 * A subclass extending this class should make itself a singleton and initialize
 * the mDeathRecipient.
 *
 * Note that HalClientManager is not responsible for generating endpoint ids,
 * which should be managed by HAL clients themselves.
 *
 * TODO(b/247124878): Add functions related to endpoints mapping.
 */
class HalClientManager {
 public:
  virtual ~HalClientManager() = default;

  /** Disable copy and assignment constructors as this class should be a
   * singleton.*/
  HalClientManager(HalClientManager &) = delete;
  void operator=(const HalClientManager &) = delete;

  /** Returns a newly created client id to uniquely identify a HAL client. */
  virtual HalClientId createClientId();

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
   * Handles the client death event.
   *
   * @param pid of the client that loses the binder connection to the HAL.
   */
  void handleClientDeath(pid_t pid);

 protected:
  HalClientManager() = default;
  // next available client id
  HalClientId mNextClientId = 1;
  // The lock guarding the access to mPIdsToClientIds and mClientIdsToCallbacks
  // below.
  std::mutex mMapLock;
  // The lock guarding the creation of client Ids
  std::mutex mClientIdLock;
  // Map from pids to client ids
  std::unordered_map<pid_t, HalClientId> mPIdsToClientIds{};
  // Map from client ids to callback functions
  std::unordered_map<HalClientId, std::shared_ptr<IContextHubCallback>>
      mClientIdsToCallbacks{};
  ndk::ScopedAIBinder_DeathRecipient mDeathRecipient;

  /** Returns true if the clientId is being used. */
  inline bool isAllocatedClientId(HalClientId clientId) {
    return mClientIdsToCallbacks.find(clientId) !=
               mClientIdsToCallbacks.end() ||
           clientId == kDefaultHalClientId || clientId == kHalId;
  }

  /** Returns true if the pid is being used to identify a client. */
  inline bool isKnownPId(pid_t pid) {
    return mPIdsToClientIds.find(pid) != mPIdsToClientIds.end();
  }
};

}  // namespace android::hardware::contexthub::common::implementation

#endif  // ANDROID_HARDWARE_CONTEXTHUB_COMMON_HAL_CLIENT_MANAGER_H_