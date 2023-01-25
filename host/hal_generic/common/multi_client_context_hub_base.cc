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

#include "multi_client_context_hub_base.h"
#include <chre/platform/shared/host_protocol_common.h>
#include <chre_host/generated/host_messages_generated.h>
#include <chre_host/log.h>
#include "chre_host/fragmented_load_transaction.h"
#include "chre_host/host_protocol_host.h"
#include "permissions_util.h"

namespace android::hardware::contexthub::common::implementation {

using ::android::chre::FragmentedLoadTransaction;
using ::android::chre::getStringFromByteVector;
using ::ndk::ScopedAStatus;
namespace fbs = ::chre::fbs;

namespace {
constexpr uint32_t kDefaultHubId = 0;

// timeout for calling getContextHubs(), which is synchronous
constexpr auto kHubInfoQueryTimeout = std::chrono::seconds(5);

enum class HalErrorCode : int32_t {
  OPERATION_FAILED = -1,
  INVALID_RESULT = -2,
};

bool isValidContextHubId(uint32_t hubId) {
  if (hubId != kDefaultHubId) {
    LOGE("Invalid context hub ID %" PRId32, hubId);
    return false;
  }
  return true;
}

// functions that extract different version numbers
inline constexpr int8_t extractChreApiMajorVersion(uint32_t chreVersion) {
  return static_cast<int8_t>(chreVersion >> 24);
}
inline constexpr int8_t extractChreApiMinorVersion(uint32_t chreVersion) {
  return static_cast<int8_t>(chreVersion >> 16);
}
inline constexpr uint16_t extractChrePatchVersion(uint32_t chreVersion) {
  return static_cast<uint16_t>(chreVersion);
}

// functions that help to generate ScopedAStatus from different values.
inline ScopedAStatus fromServiceError(HalErrorCode errorCode) {
  return ScopedAStatus::fromServiceSpecificError(
      static_cast<int32_t>(errorCode));
}
inline ScopedAStatus fromResult(bool result) {
  return result ? ScopedAStatus::ok()
                : fromServiceError(HalErrorCode::OPERATION_FAILED);
}
}  // anonymous namespace

ScopedAStatus MultiClientContextHubBase::getContextHubs(
    std::vector<ContextHubInfo> *contextHubInfos) {
  std::unique_lock<std::mutex> lock(mHubInfoMutex);
  if (mContextHubInfo == nullptr) {
    fbs::HubInfoResponseT response;
    flatbuffers::FlatBufferBuilder builder;
    HostProtocolHost::encodeHubInfoRequest(builder);
    if (!mConnection->sendMessage(builder)) {
      LOGE("Failed to send a message to CHRE to get context hub info.");
      return fromServiceError(HalErrorCode::OPERATION_FAILED);
    }
    mHubInfoCondition.wait_for(lock, kHubInfoQueryTimeout,
                               [this]() { return mContextHubInfo != nullptr; });
  }
  if (mContextHubInfo != nullptr) {
    contextHubInfos->push_back(*mContextHubInfo);
    return ScopedAStatus::ok();
  }
  LOGE("Unable to get a valid context hub info for PID %d",
       AIBinder_getCallingPid());
  return fromServiceError(HalErrorCode::INVALID_RESULT);
}

ScopedAStatus MultiClientContextHubBase::loadNanoapp(
    int32_t contextHubId, const NanoappBinary &appBinary,
    int32_t transactionId) {
  if (!isValidContextHubId(contextHubId)) {
    return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  }

  uint32_t targetApiVersion = (appBinary.targetChreApiMajorVersion << 24) |
                              (appBinary.targetChreApiMinorVersion << 16);
  auto transaction = std::make_unique<FragmentedLoadTransaction>(
      transactionId, appBinary.nanoappId, appBinary.nanoappVersion,
      appBinary.flags, targetApiVersion, appBinary.customBinary);
  if (!mHalClientManager->registerPendingLoadTransaction(
          std::move(transaction))) {
    return fromResult(false);
  }
  auto clientId = mHalClientManager->getClientId();
  auto request = mHalClientManager->getNextFragmentedLoadRequest(
      clientId, transactionId, std::nullopt);
  if (!request.has_value()) {
    LOGE("Failed to get the first load request.");
    return fromResult(false);
  }
  bool result = sendFragmentedLoadRequest(clientId, request.value());
  return fromResult(result);
}

bool MultiClientContextHubBase::sendFragmentedLoadRequest(
    HalClientId clientId, FragmentedLoadRequest &request) {
  flatbuffers::FlatBufferBuilder builder(128 + request.binary.size());
  HostProtocolHost::encodeFragmentedLoadNanoappRequest(
      builder, request, /* respondBeforeStart= */ false);
  HostProtocolHost::mutateHostClientId(builder.GetBufferPointer(),
                                       builder.GetSize(), clientId);
  return mConnection->sendMessage(builder);
}

ScopedAStatus MultiClientContextHubBase::unloadNanoapp(int32_t contextHubId,
                                                       int64_t appId,
                                                       int32_t transactionId) {
  if (!isValidContextHubId(contextHubId)) {
    return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  }
  if (!mHalClientManager->registerPendingUnloadTransaction()) {
    return fromResult(false);
  }

  flatbuffers::FlatBufferBuilder builder(64);
  HostProtocolHost::encodeUnloadNanoappRequest(
      builder, transactionId, appId, /* allowSystemNanoappUnload= */ false);
  HostProtocolHost::mutateHostClientId(builder.GetBufferPointer(),
                                       builder.GetSize(),
                                       mHalClientManager->getClientId());
  bool result = mConnection->sendMessage(builder);
  return fromResult(result);
}

ScopedAStatus MultiClientContextHubBase::disableNanoapp(
    int32_t /* contextHubId */, int64_t appId, int32_t /* transactionId */) {
  LOGW("Attempted to disable app ID 0x%016" PRIx64 ", but not supported",
       appId);
  return ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ScopedAStatus MultiClientContextHubBase::enableNanoapp(
    int32_t /* contextHubId */, int64_t appId, int32_t /* transactionId */) {
  LOGW("Attempted to enable app ID 0x%016" PRIx64 ", but not supported", appId);
  return ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ScopedAStatus MultiClientContextHubBase::onSettingChanged(Setting /*setting*/,
                                                          bool /*enabled*/) {
  // To be implemented.
  return ScopedAStatus::ok();
}

ScopedAStatus MultiClientContextHubBase::queryNanoapps(int32_t contextHubId) {
  if (!isValidContextHubId(contextHubId)) {
    return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  }
  flatbuffers::FlatBufferBuilder builder(64);
  HostProtocolHost::encodeNanoappListRequest(builder);
  HostProtocolHost::mutateHostClientId(builder.GetBufferPointer(),
                                       builder.GetSize(),
                                       mHalClientManager->getClientId());
  return fromResult(mConnection->sendMessage(builder));
}

ScopedAStatus MultiClientContextHubBase::getPreloadedNanoappIds(
    std::vector<int64_t> * /*result*/) {
  // To be implemented.
  return ScopedAStatus::ok();
}

ScopedAStatus MultiClientContextHubBase::registerCallback(
    int32_t contextHubId,
    const std::shared_ptr<IContextHubCallback> &callback) {
  if (!isValidContextHubId(contextHubId)) {
    return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  }
  if (callback == nullptr) {
    LOGE("Callback of context hub HAL must not be null.");
    return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  }
  mHalClientManager->registerCallback(callback);
  return ScopedAStatus::ok();
}

ScopedAStatus MultiClientContextHubBase::sendMessageToHub(
    int32_t contextHubId, const ContextHubMessage &message) {
  if (!isValidContextHubId(contextHubId)) {
    return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  }
  flatbuffers::FlatBufferBuilder builder(1024);
  HostProtocolHost::encodeNanoappMessage(
      builder, message.nanoappId, message.messageType, message.hostEndPoint,
      message.messageBody.data(), message.messageBody.size());
  return fromResult(mConnection->sendMessage(builder));
}

ScopedAStatus MultiClientContextHubBase::onHostEndpointConnected(
    const HostEndpointInfo & /*in_info*/) {
  // To be implemented.
  return ScopedAStatus::ok();
}

ScopedAStatus MultiClientContextHubBase::onHostEndpointDisconnected(
    char16_t /*in_hostEndpointId*/) {
  // To be implemented.
  return ScopedAStatus::ok();
}

ScopedAStatus MultiClientContextHubBase::onNanSessionStateChanged(
    bool /*in_state*/) {
  // To be implemented.
  return ScopedAStatus::ok();
}

void MultiClientContextHubBase::handleMessageFromChre(
    const unsigned char *messageBuffer, size_t messageLen) {
  if (!::chre::HostProtocolCommon::verifyMessage(messageBuffer, messageLen)) {
    LOGE("Invalid message received from CHRE.");
    return;
  }
  std::unique_ptr<fbs::MessageContainerT> container =
      fbs::UnPackMessageContainer(messageBuffer);
  fbs::ChreMessageUnion &message = container->message;
  HalClientId clientId = container->host_addr->client_id();

  switch (container->message.type) {
    case fbs::ChreMessage::HubInfoResponse: {
      handleHubInfoResponse(*message.AsHubInfoResponse());
      break;
    }
    case fbs::ChreMessage::NanoappListResponse: {
      onNanoappListResponse(*message.AsNanoappListResponse(), clientId);
      break;
    }
    case fbs::ChreMessage::LoadNanoappResponse: {
      onLoadNanoappResponse(*message.AsLoadNanoappResponse(), clientId);
      break;
    }
    case fbs::ChreMessage::UnloadNanoappResponse: {
      onUnloadNanoappResponse(*message.AsUnloadNanoappResponse(), clientId);
      break;
    }
    default:
      LOGW("Got unexpected message type %" PRIu8,
           static_cast<uint8_t>(message.type));
  }
}

void MultiClientContextHubBase::handleHubInfoResponse(
    const fbs::HubInfoResponseT &response) {
  std::unique_lock<std::mutex> lock(mHubInfoMutex);
  mContextHubInfo = std::make_unique<ContextHubInfo>();
  mContextHubInfo->name = getStringFromByteVector(response.name);
  mContextHubInfo->vendor = getStringFromByteVector(response.vendor);
  mContextHubInfo->toolchain = getStringFromByteVector(response.toolchain);
  mContextHubInfo->id = kDefaultHubId;
  mContextHubInfo->peakMips = response.peak_mips;
  mContextHubInfo->maxSupportedMessageLengthBytes = response.max_msg_len;
  mContextHubInfo->chrePlatformId = response.platform_id;
  uint32_t version = response.chre_platform_version;
  mContextHubInfo->chreApiMajorVersion = extractChreApiMajorVersion(version);
  mContextHubInfo->chreApiMinorVersion = extractChreApiMinorVersion(version);
  mContextHubInfo->chrePatchVersion = extractChrePatchVersion(version);
  mContextHubInfo->supportedPermissions = kSupportedPermissions;
  mHubInfoCondition.notify_all();
}

void MultiClientContextHubBase::onNanoappListResponse(
    const fbs::NanoappListResponseT &response, HalClientId clientId) {
  std::shared_ptr<IContextHubCallback> callback =
      mHalClientManager->getCallback(clientId);
  if (callback == nullptr) {
    return;
  }
  std::vector<NanoappInfo> appInfoList;
  for (const auto &nanoapp : response.nanoapps) {
    if (nanoapp == nullptr || nanoapp->is_system) {
      continue;
    }
    NanoappInfo appInfo;
    appInfo.nanoappId = nanoapp->app_id;
    appInfo.nanoappVersion = nanoapp->version;
    appInfo.enabled = nanoapp->enabled;
    appInfo.permissions = chreToAndroidPermissions(nanoapp->permissions);

    std::vector<NanoappRpcService> rpcServices;
    for (const auto &service : nanoapp->rpc_services) {
      NanoappRpcService aidlService;
      aidlService.id = service->id;
      aidlService.version = service->version;
      rpcServices.emplace_back(aidlService);
    }
    appInfo.rpcServices = rpcServices;
    appInfoList.push_back(appInfo);
  }
  callback->handleNanoappInfo(appInfoList);
}

void MultiClientContextHubBase::onLoadNanoappResponse(
    const fbs::LoadNanoappResponseT &response, HalClientId clientId) {
  if (mPreloadedNanoappLoader->isPreloadOngoing()) {
    mPreloadedNanoappLoader->onLoadNanoappResponse(response, clientId);
    return;
  }

  auto nextFragmentedRequest = mHalClientManager->getNextFragmentedLoadRequest(
      clientId, response.transaction_id, response.fragment_id);
  if (response.success && nextFragmentedRequest.has_value()) {
    LOGD("Sending next FragmentedLoadRequest for client %" PRIu16
         ": (transaction: %" PRIu32 ", fragment %zu)",
         clientId, nextFragmentedRequest->transactionId,
         nextFragmentedRequest->fragmentId);
    sendFragmentedLoadRequest(clientId, nextFragmentedRequest.value());
    return;
  }

  if (!response.success) {
    LOGE("Sending FragmentedLoadRequest for client %" PRIu16
         " failed: (transaction: %" PRIu32 ", fragment %" PRIu32 ")",
         clientId, response.transaction_id, response.fragment_id);
  }
  if (auto callback = mHalClientManager->getCallback(clientId);
      callback != nullptr) {
    callback->handleTransactionResult(response.transaction_id,
                                      /* in_success= */ response.success);
  }
}

void MultiClientContextHubBase::onUnloadNanoappResponse(
    const fbs::UnloadNanoappResponseT &response, HalClientId clientId) {
  mHalClientManager->finishPendingUnloadTransaction(clientId);
  if (auto callback = mHalClientManager->getCallback(clientId);
      callback != nullptr) {
    callback->handleTransactionResult(response.transaction_id,
                                      /* in_success= */ response.success);
  }
}
}  // namespace android::hardware::contexthub::common::implementation