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

#include "qmi_context_hub_aidl.h"

#include "chre_api/chre/event.h"
#include "chre_host/fragmented_load_transaction.h"
#include "permissions_util.h"

namespace aidl {
namespace android {
namespace hardware {
namespace contexthub {

using ::android::chre::FragmentedLoadTransaction;
using ::android::hardware::contexthub::common::implementation::
    chreToAndroidPermissions;
using ::android::hardware::contexthub::common::implementation::
    kSupportedPermissions;
using ::ndk::ScopedAStatus;

namespace {
constexpr uint32_t kDefaultHubId = 0;

}  // anonymous namespace

ContextHub::ContextHub()
    : mDeathRecipient(AIBinder_DeathRecipient_new(ContextHub::onServiceDied)) {}

ScopedAStatus ContextHub::getContextHubs(
    std::vector<ContextHubInfo> *out_contextHubInfos) {
  // TODO(b/220195756): Currently has hardcoded info for testing.
  ContextHubInfo info;
  uint32_t chreVersion = 0x01060000;

  info.name = "CHRE on QSH";
  info.vendor = "Google";
  info.toolchain = "Hexagon Clang";
  info.id = kDefaultHubId;
  info.chrePlatformId = 0x476f6f676c000005;
  info.chreApiMajorVersion = static_cast<uint8_t>(chreVersion >> 24);
  info.chreApiMinorVersion = static_cast<uint8_t>(chreVersion >> 16);
  info.chrePatchVersion = static_cast<char16_t>(chreVersion);
  info.supportedPermissions = kSupportedPermissions;

  out_contextHubInfos->push_back(info);

  return ScopedAStatus::ok();
}

ScopedAStatus ContextHub::loadNanoapp(int32_t /*contextHubId*/,
                                      const NanoappBinary & /*appBinary*/,
                                      int32_t /*transactionId*/) {
  // TODO(b/220195756): Implement this.
  return ScopedAStatus::ok();
}

ScopedAStatus ContextHub::unloadNanoapp(int32_t /*contextHubId*/,
                                        int64_t /*appId*/,
                                        int32_t /*transactionId*/) {
  // TODO(b/220195756): Implement this.
  return ScopedAStatus::ok();
}

ScopedAStatus ContextHub::disableNanoapp(int32_t /* contextHubId */,
                                         int64_t appId,
                                         int32_t /* transactionId */) {
  ALOGW("Attempted to disable app ID 0x%016" PRIx64 ", but not supported",
        appId);
  return ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ScopedAStatus ContextHub::enableNanoapp(int32_t /* contextHubId */,
                                        int64_t appId,
                                        int32_t /* transactionId */) {
  ALOGW("Attempted to enable app ID 0x%016" PRIx64 ", but not supported",
        appId);
  return ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ScopedAStatus ContextHub::onSettingChanged(Setting /*setting*/,
                                           bool /*enabled*/) {
  // TODO(b/220195756): Implement this.
  return ScopedAStatus::ok();
}

ScopedAStatus ContextHub::queryNanoapps(int32_t /*contextHubId*/) {
  bool success =
      mQmiQshNanoappClient.sendSuidReq(onSuidAttributesReceived, this);
  return success ? ScopedAStatus::ok()
                 : ScopedAStatus::fromServiceSpecificErrorWithMessage(
                       BnContextHub::EX_CONTEXT_HUB_UNSPECIFIED,
                       "Failed to send SUID request");
}

ScopedAStatus ContextHub::registerCallback(
    int32_t contextHubId, const std::shared_ptr<IContextHubCallback> &cb) {
  if (contextHubId != kDefaultHubId) {
    ALOGE("Invalid ID %" PRId32, contextHubId);
    return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  } else {
    std::lock_guard<std::mutex> lock(mCallbackMutex);
    if (mCallback != nullptr) {
      binder_status_t binder_status = AIBinder_unlinkToDeath(
          mCallback->asBinder().get(), mDeathRecipient.get(), this);
      if (binder_status != STATUS_OK) {
        ALOGE("Failed to unlink to death");
      }
    }

    mCallback = cb;

    if (cb != nullptr) {
      binder_status_t binder_status = AIBinder_linkToDeath(
          cb->asBinder().get(), mDeathRecipient.get(), this);

      if (binder_status != STATUS_OK) {
        ALOGE("Failed to link to death");
      }
    }

    return ScopedAStatus::ok();
  }
}

ScopedAStatus ContextHub::sendMessageToHub(
    int32_t /*contextHubId*/, const ContextHubMessage & /*message*/) {
  // TODO(b/220195756): Implement this.
  return ScopedAStatus::ok();
}

ScopedAStatus ContextHub::onHostEndpointConnected(
    const HostEndpointInfo & /*in_info*/) {
  // TODO(b/220195756): Implement this.
  return ScopedAStatus::ok();
}

ScopedAStatus ContextHub::onHostEndpointDisconnected(
    char16_t /*in_hostEndpointId*/) {
  // TODO(b/220195756): Implement this.
  return ScopedAStatus::ok();
}

void ContextHub::handleServiceDeath() {
  ALOGI("Context Hub Service died ...");
  {
    std::lock_guard<std::mutex> lock(mCallbackMutex);
    mCallback.reset();
  }
  {
    std::lock_guard<std::mutex> lock(mConnectedHostEndpointsMutex);
    mConnectedHostEndpoints.clear();
  }
}

void ContextHub::onServiceDied(void *cookie) {
  auto *contexthub = static_cast<ContextHub *>(cookie);
  contexthub->handleServiceDeath();
}

binder_status_t ContextHub::dump(int /*fd*/, const char ** /* args */,
                                 uint32_t /* numArgs */) {
  // TODO(b/220195756): Implement this.
  return STATUS_OK;
}

void ContextHub::onSuidAttributesReceived(
    const SuidAttributeList &attributeList, void *ctx) {
  std::vector<NanoappInfo> appInfoList;
  auto *instance = static_cast<ContextHub *>(ctx);
  for (const auto &attr : attributeList) {
    NanoappInfo info;
    ALOGV("processing attribute with name %s, nappID %" PRIx64,
          attr.name.c_str(), attr.nanoappId);
    info.nanoappId = attr.nanoappId;
    info.nanoappVersion = attr.version;
    info.enabled = attr.isAvailable;

    appInfoList.push_back(info);
  }

  instance->getCallback()->handleNanoappInfo(appInfoList);
}

}  // namespace contexthub
}  // namespace hardware
}  // namespace android
}  // namespace aidl
