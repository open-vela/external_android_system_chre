/*
 * Copyright (C) 2024 Xiaomi Corporation
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

#include "chre/platform/host_link.h"

#include <sys/stat.h>

#include <atomic>

#include "chre/core/event_loop_manager.h"
#include "chre/core/host_comms_manager.h"
#include "chre/platform/shared/host_protocol_chre.h"
#include "chre/platform/shared/nanoapp_load_manager.h"
#include "chre/platform/system_time.h"
#include "chre/platform/system_timer.h"
#include "chre/target_platform/host_link_base.h"
#include "chre/target_platform/rpc_socket.h"
#include "chre/util/flatbuffers/helpers.h"
#include "chre/util/nested_data_ptr.h"

namespace chre {
namespace {
struct UnloadNanoappCallbackData {
  uint64_t appId;
  uint32_t transactionId;
  uint16_t hostClientId;
  bool allowSystemNanoappUnload;
};

struct NanoappListData {
  ChreFlatBufferBuilder* builder;
  DynamicVector<NanoappListEntryOffset> nanoappEntries;
  uint16_t hostClientId;
};

enum class PendingMessageType {
  Shutdown,
  NanoappMessageToHost,
  HubInfoResponse,
  NanoappListResponse,
  LoadNanoappResponse,
  UnloadNanoappResponse,
  DebugDumpData,
  DebugDumpResponse,
  TimeSyncRequest,
  LowPowerMicAccessRequest,
  LowPowerMicAccessRelease,
  EncodedLogMessage,
  SelfTestResponse,
  MetricLog,
  NanConfigurationRequest,
};

struct PendingMessage {
  PendingMessage(PendingMessageType msgType, uint16_t hostClientId) {
    type = msgType;
    data.hostClientId = hostClientId;
  }

  PendingMessage(PendingMessageType msgType,
                 const HostMessage* msgToHost = nullptr) {
    type = msgType;
    data.msgToHost = msgToHost;
  }

  PendingMessage(PendingMessageType msgType, ChreFlatBufferBuilder* builder) {
    type = msgType;
    data.builder = builder;
  }

  PendingMessageType type;
  union {
    const HostMessage* msgToHost;
    uint16_t hostClientId;
    ChreFlatBufferBuilder* builder;
  } data;
};

constexpr size_t kOutboundQueueSize = 100;
FixedSizeBlockingQueue<PendingMessage, kOutboundQueueSize> gOutboundQueue;

typedef void(MessageBuilderFunction)(ChreFlatBufferBuilder& builder,
                                     void* cookie);

inline HostCommsManager& getHostCommsManager() {
  return EventLoopManagerSingleton::get()->getHostCommsManager();
}

bool generateMessageFromBuilder(ChreFlatBufferBuilder* builder) {
  CHRE_ASSERT(builder != nullptr);

  const chre::fbs::MessageContainer* container =
      chre::fbs::GetMessageContainer(builder->GetBufferPointer());

  uint16_t hostClientId = container->host_addr()->client_id();

  bool result =
      chre::HostLinkBaseSingleton::get()->mRpcInterface->sendToClientById(
          builder->GetBufferPointer(), builder->GetSize(), hostClientId);

  // clean up
  builder->~ChreFlatBufferBuilder();
  memoryFree(builder);
  return result;
}

bool generateMessageToHost(const HostMessage* message) {
  LOGV("%s: message size %zu", __func__, message->message.size());
  bool result = false;
  constexpr size_t kFixedReserveSize = 80;
  ChreFlatBufferBuilder builder(message->message.size() + kFixedReserveSize);
  HostProtocolChre::encodeNanoappMessage(
      builder, message->appId, message->toHostData.messageType,
      message->toHostData.hostEndpoint, message->message.data(),
      message->message.size(), message->toHostData.appPermissions,
      message->toHostData.messagePermissions, message->toHostData.wokeHost);

  if (chre::HostLinkBaseSingleton::get()->mRpcInterface->sendToClientById(
          builder.GetBufferPointer(), builder.GetSize(),
          message->toHostData.hostEndpoint)) {
    EventLoopManagerSingleton::get()
        ->getHostCommsManager()
        .onMessageToHostComplete(message);
    result = true;
  }
  return result;
}

int generateHubInfoResponse(uint16_t hostClientId) {
  constexpr size_t kInitialBufferSize = 192;

  constexpr char kHubName[] = "CHRE on Nuttx";
  constexpr char kVendor[] = "Xiaomi";

  constexpr char kToolchain[] = "GCC" STRINGIFY(__GNUC__) "." STRINGIFY(
      __GNUC_MINOR__) "." STRINGIFY(__GNUC_PATCHLEVEL__);
  constexpr uint32_t kLegacyPlatformVersion = 0;
  constexpr uint32_t kLegacyToolchainVersion =
      __GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__;
  constexpr float kPeakMips = 350;
  constexpr float kStoppedPower = 0;
  constexpr float kSleepPower = 1;
  constexpr float kPeakPower = 15;

  // Note that this may execute prior to EventLoopManager::lateInit() completing
  ChreFlatBufferBuilder builder(kInitialBufferSize);
  HostProtocolChre::encodeHubInfoResponse(
      builder, kHubName, kVendor, kToolchain, kLegacyPlatformVersion,
      kLegacyToolchainVersion, kPeakMips, kStoppedPower, kSleepPower,
      kPeakPower, CHRE_MESSAGE_TO_HOST_MAX_SIZE, chreGetPlatformId(),
      chreGetVersion(), hostClientId);
  return chre::HostLinkBaseSingleton::get()->mRpcInterface->sendToClientById(
      builder.GetBufferPointer(), builder.GetSize(), hostClientId);
}

bool dequeueMessage(PendingMessage pendingMsg) {
  bool result = false;
  switch (pendingMsg.type) {
    case PendingMessageType::NanoappMessageToHost:
      result = generateMessageToHost(pendingMsg.data.msgToHost);
      break;

    case PendingMessageType::HubInfoResponse:
      result = generateHubInfoResponse(pendingMsg.data.hostClientId);
      break;

    case PendingMessageType::NanoappListResponse:
    case PendingMessageType::LoadNanoappResponse:
    case PendingMessageType::UnloadNanoappResponse:
    case PendingMessageType::DebugDumpData:
    case PendingMessageType::DebugDumpResponse:
    case PendingMessageType::TimeSyncRequest:
    case PendingMessageType::LowPowerMicAccessRequest:
    case PendingMessageType::LowPowerMicAccessRelease:
    case PendingMessageType::EncodedLogMessage:
    case PendingMessageType::SelfTestResponse:
    case PendingMessageType::MetricLog:
    case PendingMessageType::NanConfigurationRequest:
      // LOGI("dequeueMessage generateMessageFromBuilder");
      result = generateMessageFromBuilder(pendingMsg.data.builder);
      break;

    default:
      CHRE_ASSERT_LOG(false, "Unexpected pending message type");
  }
  return result;
}

/**
 * Wrapper function to enqueue a message on the outbound message queue. All
 * outgoing message to the host must be called through this function.
 *
 * @param message The message to send to host.
 *
 * @return true if the message was successfully added to the queue.
 */
bool enqueueMessage(PendingMessage pendingMsg) {
  return gOutboundQueue.push(pendingMsg);
}

/**
 * Helper function that takes care of the boilerplate for allocating a
 * ChreFlatBufferBuilder on the heap and adding it to the outbound message
 * queue.
 *
 * @param msgType Identifies the message while in the outbound queue
 * @param initialBufferSize Number of bytes to reserve when first allocating the
 *        ChreFlatBufferBuilder
 * @param buildMsgFunc Synchronous callback used to encode the FlatBuffer
 *        message. Will not be invoked if allocation fails.
 * @param cookie Opaque pointer that will be passed through to buildMsgFunc
 *
 * @return true if the message was successfully added to the queue
 */
bool buildAndEnqueueMessage(PendingMessageType msgType,
                            size_t initialBufferSize,
                            MessageBuilderFunction* msgBuilder, void* cookie) {
  LOGV("%s: message type %d, size %zu", __func__, (int)msgType,
       initialBufferSize);
  bool pushed = false;

  auto builder = MakeUnique<ChreFlatBufferBuilder>(initialBufferSize);
  if (builder.isNull()) {
    LOGE("Couldn't allocate memory for message type %d",
         static_cast<int>(msgType));
  } else {
    msgBuilder(*builder, cookie);

    // TODO(b/263958729): if this fails, ideally we should block for some
    // timeout until there's space in the queue
    if (!enqueueMessage(PendingMessage(msgType, builder.get()))) {
      LOGE("Couldn't push message type %d to outbound queue",
           static_cast<int>(msgType));
    } else {
      builder.release();
      pushed = true;
    }
  }

  return pushed;
}

/**
 * FlatBuffer message builder callback used with handleNanoappListRequest()
 */
void buildNanoappListResponse(ChreFlatBufferBuilder& builder, void* cookie) {
  LOGV("%s", __func__);
  auto nanoappAdderCallback = [](const Nanoapp* nanoapp, void* data) {
    auto* cbData = static_cast<NanoappListData*>(data);
    HostProtocolChre::addNanoappListEntry(
        *(cbData->builder), cbData->nanoappEntries, nanoapp->getAppId(),
        nanoapp->getAppVersion(), true /*enabled*/, nanoapp->isSystemNanoapp(),
        nanoapp->getAppPermissions(), nanoapp->getRpcServices());
  };

  // Add a NanoappListEntry to the FlatBuffer for each nanoapp
  auto* cbData = static_cast<NanoappListData*>(cookie);
  cbData->builder = &builder;
  EventLoop& eventLoop = EventLoopManagerSingleton::get()->getEventLoop();
  eventLoop.forEachNanoapp(nanoappAdderCallback, cbData);
  HostProtocolChre::finishNanoappListResponse(builder, cbData->nanoappEntries,
                                              cbData->hostClientId);
}

void handleUnloadNanoappCallback(uint16_t /*type*/, void* data,
                                 void* /*extraData*/) {
  auto* cbData = static_cast<UnloadNanoappCallbackData*>(data);
  bool success = false;
  uint16_t instanceId;
  EventLoop& eventLoop = EventLoopManagerSingleton::get()->getEventLoop();
  if (!eventLoop.findNanoappInstanceIdByAppId(cbData->appId, &instanceId)) {
    LOGE("Couldn't unload app ID 0x%016" PRIx64 ": not found", cbData->appId);
  } else {
    success =
        eventLoop.unloadNanoapp(instanceId, cbData->allowSystemNanoappUnload);
  }

  constexpr size_t kInitialBufferSize = 52;
  auto builder = MakeUnique<ChreFlatBufferBuilder>(kInitialBufferSize);
  HostProtocolChre::encodeUnloadNanoappResponse(*builder, cbData->hostClientId,
                                                cbData->transactionId, success);

  if (!enqueueMessage(PendingMessage(PendingMessageType::UnloadNanoappResponse,
                                     builder.get()))) {
    LOGE("Failed to send unload response to host: %x transactionID: 0x%" PRIx32,
         cbData->hostClientId, cbData->transactionId);
  } else {
    builder.release();
  }

  memoryFree(data);
}

/**
 * Helper function that prepares a nanoapp that can be loaded into the system
 * from a file stored on disk.
 *
 * @param hostClientId the ID of client that originated this transaction
 * @param transactionId the ID of the transaction
 * @param appId the ID of the app to load
 * @param appVersion the version of the app to load
 * @param targetApiVersion the API version this nanoapp is targeted for
 * @param appFilename Null-terminated ASCII string containing the file name that
 *     contains the app binary to be loaded.
 *
 * @return A valid pointer to a nanoapp that can be loaded into the system. A
 *     nullptr if the preparation process fails.
 */
UniquePtr<Nanoapp> handleLoadNanoappFile(uint16_t hostClientId,
                                         uint32_t transactionId, uint64_t appId,
                                         uint32_t appVersion,
                                         uint32_t targetApiVersion,
                                         const char* appFilename) {
  LOGD("Load nanoapp request for app ID 0x%016" PRIx64 " ver 0x%" PRIx32
       " target API 0x%08" PRIx32 " (txnId %" PRIu32 " client %" PRIu16 ")",
       appId, appVersion, targetApiVersion, transactionId, hostClientId);

  auto nanoapp = MakeUnique<Nanoapp>();

  if (nanoapp.isNull()) {
    LOG_OOM();
  } else if (!nanoapp->setAppInfo(appId, appVersion, appFilename,
                                  targetApiVersion) ||
             !nanoapp->isLoaded()) {
    nanoapp.reset(nullptr);
  }

  return nanoapp;
}

}  // anonymous namespace

void sendDebugDumpResultToHost(uint16_t hostClientId, const char* /*debugStr*/,
                               size_t /*debugStrSize*/, bool /*complete*/,
                               uint32_t /*dataCount*/) {
  LOGV("%s: host client id %d", __func__, hostClientId);
  // TODO if need
}

// TODO(b/263958729): HostMessageHandlers member function implementations are
// expected to be (mostly) identical for any platform that uses flatbuffers
// to encode messages - refactor the host link to merge the multiple copies
// we currently have.
void HostMessageHandlers::handleNanoappMessage(uint64_t appId,
                                               uint32_t messageType,
                                               uint16_t hostEndpoint,
                                               const void* messageData,
                                               size_t messageDataLen) {
  LOGV("Parsed nanoapp message from host: app ID 0x%016" PRIx64
       ", endpoint "
       "0x%" PRIx16 ", msgType %" PRIu32 ", payload size %zu",
       appId, hostEndpoint, messageType, messageDataLen);

  getHostCommsManager().sendMessageToNanoappFromHost(
      appId, messageType, hostEndpoint, messageData, messageDataLen);
}

void HostMessageHandlers::handleHubInfoRequest(uint16_t hostClientId) {
  LOGV("%s: host client id %d", __func__, hostClientId);
  enqueueMessage(
      PendingMessage(PendingMessageType::HubInfoResponse, hostClientId));
}

void HostMessageHandlers::handleNanoappListRequest(uint16_t hostClientId) {
  auto callback = [](uint16_t /*type*/, void* data, void* /*extraData*/) {
    uint16_t cbHostClientId = NestedDataPtr<uint16_t>(data);

    NanoappListData cbData = {};
    cbData.hostClientId = cbHostClientId;

    size_t expectedNanoappCount =
        EventLoopManagerSingleton::get()->getEventLoop().getNanoappCount();
    if (!cbData.nanoappEntries.reserve(expectedNanoappCount)) {
      LOG_OOM();
    } else {
      constexpr size_t kFixedOverhead = 48;
      constexpr size_t kPerNanoappSize = 32;
      size_t initialBufferSize =
          (kFixedOverhead + expectedNanoappCount * kPerNanoappSize);

      buildAndEnqueueMessage(PendingMessageType::NanoappListResponse,
                             initialBufferSize, buildNanoappListResponse,
                             &cbData);
    }
  };

  LOGD("Nanoapp list request from client ID %" PRIu16, hostClientId);
  EventLoopManagerSingleton::get()->deferCallback(
      SystemCallbackType::NanoappListResponse,
      NestedDataPtr<uint16_t>(hostClientId), callback);
}

void HostMessageHandlers::sendFragmentResponse(uint16_t hostClientId,
                                               uint32_t transactionId,
                                               uint32_t fragmentId,
                                               bool success) {
  struct FragmentedLoadInfoResponse {
    uint16_t hostClientId;
    uint32_t transactionId;
    uint32_t fragmentId;
    bool success;
  };

  auto msgBuilder = [](ChreFlatBufferBuilder& builder, void* cookie) {
    auto* cbData = static_cast<FragmentedLoadInfoResponse*>(cookie);
    HostProtocolChre::encodeLoadNanoappResponse(
        builder, cbData->hostClientId, cbData->transactionId, cbData->success,
        cbData->fragmentId);
  };

  FragmentedLoadInfoResponse response = {
      .hostClientId = hostClientId,
      .transactionId = transactionId,
      .fragmentId = fragmentId,
      .success = success,
  };
  constexpr size_t kInitialBufferSize = 52;
  buildAndEnqueueMessage(PendingMessageType::LoadNanoappResponse,
                         kInitialBufferSize, msgBuilder, &response);
}

void HostMessageHandlers::handleLoadNanoappRequest(
    uint16_t hostClientId, uint32_t transactionId, uint64_t appId,
    uint32_t appVersion, uint32_t appFlags, uint32_t targetApiVersion,
    const void* buffer, size_t bufferLen, const char* appFileName,
    uint32_t fragmentId, size_t appBinaryLen, bool respondBeforeStart) {
  if (appFileName == nullptr) {
    loadNanoappData(hostClientId, transactionId, appId, appVersion, appFlags,
                    targetApiVersion, buffer, bufferLen, fragmentId,
                    appBinaryLen, respondBeforeStart);
  } else {
    std::string filename =
        std::string(CONFIG_CHRE_NANOAPP_SAVEPATH) + "/" + appFileName;
    if (access(filename.c_str(), F_OK) == 0) {
      UniquePtr<Nanoapp> pendingNanoapp =
          handleLoadNanoappFile(hostClientId, transactionId, appId, appVersion,
                                targetApiVersion, filename.c_str());

      if (!pendingNanoapp.isNull()) {
        auto cbData = MakeUnique<LoadNanoappCallbackData>();
        if (cbData.isNull()) {
          LOG_OOM();
        } else {
          cbData->transactionId = transactionId;
          cbData->hostClientId = hostClientId;
          cbData->appId = appId;
          cbData->fragmentId = fragmentId;
          cbData->nanoapp = std::move(pendingNanoapp);

          // Note that if this fails, we'll generate the error response in
          // the normal deferred callback
          EventLoopManagerSingleton::get()->deferCallback(
              SystemCallbackType::FinishLoadingNanoapp, std::move(cbData),
              finishLoadingNanoappCallback);
        }
      }
    } else {
      PlatformNanoappBase::setfilename(appFileName);
      sendFragmentResponse(hostClientId, transactionId, fragmentId, true);
      LOGD("appFileName=%s ", appFileName);
    }
  }
}

void HostMessageHandlers::handleUnloadNanoappRequest(
    uint16_t hostClientId, uint32_t transactionId, uint64_t appId,
    bool allowSystemNanoappUnload) {
  LOGD("Unload nanoapp request from client %" PRIu16 " (txnID %" PRIu32
       ") for appId 0x%016" PRIx64 " system %d",
       hostClientId, transactionId, appId, allowSystemNanoappUnload);
  auto* cbData = memoryAlloc<UnloadNanoappCallbackData>();
  if (cbData == nullptr) {
    LOG_OOM();
  } else {
    cbData->appId = appId;
    cbData->transactionId = transactionId;
    cbData->hostClientId = hostClientId;
    cbData->allowSystemNanoappUnload = allowSystemNanoappUnload;

    EventLoopManagerSingleton::get()->deferCallback(
        SystemCallbackType::HandleUnloadNanoapp, cbData,
        handleUnloadNanoappCallback);
  }
}

bool HostLink::sendMessage(HostMessage const* message) {
  LOGV("HostLink::%s size(%zu)", __func__, message->message.size());
  return enqueueMessage(
      PendingMessage(PendingMessageType::NanoappMessageToHost, message));
}

void HostLink::flushMessagesSentByNanoapp(uint64_t /* appId */) {
  // TODO: this is not completely safe since it's timer-based, but should work
  // well enough for the initial implementation. To be fully safe, we'd need
  // some synchronization with the thread that runs
  // generateHubInfoResponse(), e.g. a mutex that is held by that thread
  // prior to calling pop() and only released after onMessageToHostComplete
  // would've been called. If we acquire that mutex here, and hold it while
  // purging any messages sent by the nanoapp in the queue, we can be certain
  // that onMessageToHostComplete will not be called after this function returns
  // for messages sent by that nanoapp
  flushOutboundQueue();
}

void HostMessageHandlers::handleTimeSyncMessage(int64_t offset) {
  LOGE("%s unsupported.", __func__);
}

void HostMessageHandlers::handleDebugDumpRequest(uint16_t hostClientId) {
  LOGV("%s: host client id %d", __func__, hostClientId);
  // TODO(b/263958729): Implement this.
}

void HostMessageHandlers::handleSettingChangeMessage(fbs::Setting setting,
                                                     fbs::SettingState state) {
  // TODO(b/267207477): Refactor handleSettingChangeMessage to shared code
  Setting chreSetting;
  bool chreSettingEnabled;
  if (HostProtocolChre::getSettingFromFbs(setting, &chreSetting) &&
      HostProtocolChre::getSettingEnabledFromFbs(state, &chreSettingEnabled)) {
    EventLoopManagerSingleton::get()->getSettingManager().postSettingChange(
        chreSetting, chreSettingEnabled);
  }
}

void HostMessageHandlers::handleSelfTestRequest(uint16_t hostClientId) {
  LOGV("%s: host client id %d", __func__, hostClientId);
  // TODO(b/263958729): Implement this.
}

void HostMessageHandlers::handleNanConfigurationUpdate(bool /* enabled */) {
  LOGE("%s NAN unsupported.", __func__);
}

bool RpcCallbacks::onMessageReceived(const void* data, size_t length) {
  return chre::HostProtocolChre::decodeMessageFromHost(data, length);
}

bool RpcCallbacks::onConnected(uint16_t clientid) {
  return chre::HostLinkBaseSingleton::get()->generateConnectResponse(clientid);
}

void HostLinkBase::startServer() {
#if defined(ONFIG_CHRE_LOCAL_SOCKET_SERVER) || defined(CONFIG_CHRE_RPMSG_SERVER)
  mRpcInterface = new chre::RpcSocket();
#else
  mRpcInterface = new RpcNullImpl();
#endif
  mRpcCallbacks = new RpcCallbacks();
  mRpcInterface->startServer(mRpcCallbacks);
  mChreSend_tid = std::thread(&HostLinkBase::vChreSendTask, this);
}

void HostLinkBase::stopServer() { mRpcInterface->stopServer(); }

void HostLinkBase::vChreSendTask() {
  while (true) {
    auto msg = gOutboundQueue.pop();
    dequeueMessage(msg);
  }
}

bool HostLinkBase::flushOutboundQueue() {
  int waitCount = 5;

  LOGD("Draining message queue");
  while (!gOutboundQueue.empty() && waitCount-- > 0) {
    usleep(kPollingIntervalUsec);
  }

  return (waitCount >= 0);
}

void HostLinkBase::sendLogMessage(const uint8_t* logMessage,
                                  size_t logMessageSize) {
  struct LogMessageData {
    const uint8_t* logMsg;
    size_t logMsgSize;
  };

  LogMessageData logMessageData;

  logMessageData.logMsg = logMessage;
  logMessageData.logMsgSize = logMessageSize;

  auto msgBuilder = [](ChreFlatBufferBuilder& builder, void* cookie) {
    const auto* data = static_cast<const LogMessageData*>(cookie);
    HostProtocolChre::encodeLogMessages(builder, data->logMsg,
                                        data->logMsgSize);
  };

  constexpr size_t kInitialSize = 128;
  buildAndEnqueueMessage(PendingMessageType::EncodedLogMessage, kInitialSize,
                         msgBuilder, &logMessageData);
}

void HostLinkBase::sendLogMessageV2(const uint8_t* logMessage,
                                    size_t logMessageSize,
                                    uint32_t numLogsDropped) {
  struct LogMessageData {
    const uint8_t* logMsg;
    size_t logMsgSize;
    uint32_t numLogsDropped;
  };

  LogMessageData logMessageData{logMessage, logMessageSize, numLogsDropped};

  auto msgBuilder = [](ChreFlatBufferBuilder& builder, void* cookie) {
    const auto* data = static_cast<const LogMessageData*>(cookie);
    HostProtocolChre::encodeLogMessagesV2(
        builder, data->logMsg, data->logMsgSize, data->numLogsDropped);
  };

  constexpr size_t kInitialSize = 128;
  buildAndEnqueueMessage(PendingMessageType::EncodedLogMessage, kInitialSize,
                         msgBuilder, &logMessageData);
}

void HostLinkBase::sendNanConfiguration(bool enable) {
  auto msgBuilder = [](ChreFlatBufferBuilder& builder, void* cookie) {
    const auto* data = static_cast<const bool*>(cookie);
    HostProtocolChre::encodeNanConfigurationRequest(builder, *data);
  };

  constexpr size_t kInitialSize = 48;
  buildAndEnqueueMessage(PendingMessageType::NanConfigurationRequest,
                         kInitialSize, msgBuilder, &enable);
}

bool HostLinkBase::generateConnectResponse(uint16_t hostClientId) {
  ChreFlatBufferBuilder builder(64);
  std::vector<int8_t> packageNameVec(std::begin("nanoapp"),
                                     std::end("nanoapp"));
  packageNameVec.push_back('\0');

  std::vector<int8_t> attributionTagVec(std::begin(""), std::end(""));
  attributionTagVec.push_back('\0');
  auto message = fbs::CreateHostEndpointConnectedDirect(
      builder, hostClientId, CHRE_HOST_ENDPOINT_TYPE_APP, &packageNameVec,
      &attributionTagVec);
  HostProtocolCommon::finalize(builder, fbs::ChreMessage::HostEndpointConnected,
                               message.Union());
  return chre::HostLinkBaseSingleton::get()->mRpcInterface->sendToClientById(
      builder.GetBufferPointer(), builder.GetSize(), hostClientId);
}
// HostLinkBase end

}  // namespace chre
