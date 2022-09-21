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

#include <aidl/android/hardware/contexthub/BnContextHubCallback.h>
#include <aidl/android/hardware/contexthub/IContextHub.h>
#include <aidl/android/hardware/contexthub/NanoappBinary.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <dirent.h>
#include <utils/String16.h>

#include <filesystem>
#include <fstream>
#include <future>
#include <map>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

#include "chre_api/chre/version.h"
#include "chre_host/file_stream.h"
#include "chre_host/napp_header.h"

using aidl::android::hardware::contexthub::AsyncEventType;
using aidl::android::hardware::contexthub::BnContextHubCallback;
using aidl::android::hardware::contexthub::ContextHubMessage;
using aidl::android::hardware::contexthub::IContextHub;
using aidl::android::hardware::contexthub::NanoappBinary;
using aidl::android::hardware::contexthub::NanoappInfo;
using android::chre::NanoAppBinaryHeader;
using android::chre::readFileContents;
using android::internal::ToString;
using ndk::ScopedAStatus;

namespace {
constexpr uint32_t kContextHubId = 0;
constexpr int32_t kLoadTransactionId = 1;
constexpr int32_t kUnloadTransactionId = 2;
constexpr auto kTimeOutThresholdInSec = std::chrono::seconds(5);
constexpr char kUsage[] = R"(
Usage: chre_aidl_hal_client COMMAND [ARGS]
COMMAND ARGS...:
  list <PATH_OF_NANOAPPS>        - list all the nanoapps' header info in the path
  load <ABSOLUTE_PATH>           - load the nanoapp specified by the absolute path
                                   of the nanoapp. For example, load /path/to/awesome.so
  query                          - show all loaded nanoapps (system apps excluded)
  unload <HEX_NANOAPP_ID | ABSOLUTE_PATH>
                                 - unload the nanoapp specified by either the nanoapp
                                   id in hex format or the absolute path.  For example,
                                   unload 0x123def or unload /path/to/awesome.so
)";

std::string parseAppVersion(uint32_t version) {
  std::ostringstream stringStream;
  stringStream << std::hex << "0x" << version << std::dec << " (v"
               << CHRE_EXTRACT_MAJOR_VERSION(version) << "."
               << CHRE_EXTRACT_MINOR_VERSION(version) << "."
               << CHRE_EXTRACT_PATCH_VERSION(version) << ")";
  return stringStream.str();
}

std::string parseTransactionId(int32_t transactionId) {
  switch (transactionId) {
    case kLoadTransactionId:
      return "Loading";
    case kUnloadTransactionId:
      return "Unloading";
    default:
      return "Unknown";
  }
}

class ContextHubCallback : public BnContextHubCallback {
 public:
  ScopedAStatus handleNanoappInfo(
      const std::vector<NanoappInfo> &appInfo) override {
    std::cout << appInfo.size() << " nanoapps loaded" << std::endl;
    for (const NanoappInfo &app : appInfo) {
      std::cout << "appId: 0x" << std::hex << app.nanoappId << std::dec << " {"
                << "\n\tappVersion: " << parseAppVersion(app.nanoappVersion)
                << "\n\tenabled: " << (app.enabled ? "true" : "false")
                << "\n\tpermissions: " << ToString(app.permissions)
                << "\n\trpcServices: " << ToString(app.rpcServices) << "\n}"
                << std::endl;
    }
    promise.set_value();
    return ScopedAStatus::ok();
  }
  ScopedAStatus handleContextHubMessage(
      const ContextHubMessage & /*message*/,
      const std::vector<std::string> & /*msgContentPerms*/) override {
    promise.set_value();
    return ScopedAStatus::ok();
  }
  ScopedAStatus handleContextHubAsyncEvent(AsyncEventType /*event*/) override {
    promise.set_value();
    return ScopedAStatus::ok();
  }
  // Called after loading/unloading a nanoapp.
  ScopedAStatus handleTransactionResult(int32_t transactionId,
                                        bool success) override {
    std::cout << parseTransactionId(transactionId) << " transaction is "
              << (success ? "successful" : "failed") << std::endl;
    promise.set_value();
    return ScopedAStatus::ok();
  }
  std::promise<void> promise;
};

inline void throwError(const std::string &message) {
  throw std::system_error{std::error_code(), message};
}

std::shared_ptr<IContextHub> getContextHub(std::future<void> &callbackSignal) {
  auto aidlServiceName = std::string() + IContextHub::descriptor + "/default";
  ndk::SpAIBinder binder(
      AServiceManager_waitForService(aidlServiceName.c_str()));
  if (binder.get() == nullptr) {
    throwError("Could not find Context Hub HAL");
  }
  std::shared_ptr<IContextHub> contextHub = IContextHub::fromBinder(binder);
  std::shared_ptr<ContextHubCallback> callback =
      ContextHubCallback::make<ContextHubCallback>();

  if (!contextHub->registerCallback(kContextHubId, callback).isOk()) {
    throwError("Failed to register the callback");
  }
  callbackSignal = callback->promise.get_future();
  return contextHub;
}

void printNanoappHeader(const NanoAppBinaryHeader &header) {
  std::cout << " {"
            << "\n\tappId: 0x" << std::hex << header.appId << std::dec
            << "\n\tappVersion: " << parseAppVersion(header.appVersion)
            << "\n\tflags: " << header.flags << "\n\ttarget CHRE API version: "
            << static_cast<int>(header.targetChreApiMajorVersion) << "."
            << static_cast<int>(header.targetChreApiMinorVersion) << "\n}"
            << std::endl;
}

void readNanoappHeaders(std::map<std::string, NanoAppBinaryHeader> &nanoapps,
                        const std::string &binaryPath) {
  DIR *dir = opendir(binaryPath.c_str());
  if (dir == nullptr) {
    throwError("Unable to access the nanoapp path");
  }
  std::regex regex("(\\w+)\\.napp_header");
  std::cmatch match;
  for (struct dirent *entry; (entry = readdir(dir)) != nullptr;) {
    if (!std::regex_match(entry->d_name, match, regex)) {
      continue;
    }
    std::ifstream input(std::string(binaryPath) + "/" + entry->d_name,
                        std::ios::binary);
    input.read(reinterpret_cast<char *>(&nanoapps[match[1]]),
               sizeof(NanoAppBinaryHeader));
  }
  closedir(dir);
}

void verifyStatusAndSignal(const std::string &operation,
                           const ScopedAStatus &status,
                           const std::future<void> &future_signal) {
  if (!status.isOk()) {
    throwError(operation + " fails with abnormal status " +
               ToString(status.getMessage()));
  }
  auto future_status = future_signal.wait_for(kTimeOutThresholdInSec);
  if (future_status != std::future_status::ready) {
    throwError(operation + " doesn't finish within " +
               ToString(kTimeOutThresholdInSec.count()) + " seconds");
  }
}

NanoAppBinaryHeader findHeaderByPath(const std::string &pathAndName) {
  std::map<std::string, NanoAppBinaryHeader> nanoapps{};
  // To match the file pattern of [path][name].so
  std::regex pathNameRegex("(.*?)(\\w+)\\.so");
  std::smatch smatch;
  if (!std::regex_match(pathAndName, smatch, pathNameRegex)) {
    throwError("Invalid nanoapp: " + pathAndName);
  }
  std::string fullPath = smatch[1];
  std::string appName = smatch[2];
  readNanoappHeaders(nanoapps, fullPath);
  if (nanoapps.find(appName) == nanoapps.end()) {
    throwError("Cannot find the nanoapp: " + appName);
  }
  return nanoapps[appName];
}

void loadNanoapp(const std::string &pathAndName) {
  NanoAppBinaryHeader header = findHeaderByPath(pathAndName);
  std::vector<uint8_t> soBuffer{};
  if (!readFileContents(pathAndName.c_str(), &soBuffer)) {
    throwError("Failed to open the content of " + pathAndName);
  }
  NanoappBinary binary;
  binary.nanoappId = static_cast<int64_t>(header.appId);
  binary.customBinary = soBuffer;
  binary.flags = static_cast<int32_t>(header.flags);
  binary.targetChreApiMajorVersion =
      static_cast<int8_t>(header.targetChreApiMajorVersion);
  binary.targetChreApiMinorVersion =
      static_cast<int8_t>(header.targetChreApiMinorVersion);
  binary.nanoappVersion = static_cast<int32_t>(header.appVersion);

  std::future<void> callbackSignal;
  auto status = getContextHub(callbackSignal)
                    ->loadNanoapp(kContextHubId, binary, kLoadTransactionId);
  verifyStatusAndSignal(/* operation= */ "loading nanoapp " + pathAndName,
                        status, callbackSignal);
}

void unloadNanoapp(const std::string &appIdentifier) {
  std::future<void> callbackSignal;
  int64_t appId;
  try {
    // check if user provided the hex appId
    appId = std::stoll(appIdentifier, nullptr, 16);
  } catch (std::invalid_argument &e) {
    // Treat the appIdentifier as the absolute path and name and try again
    appId = static_cast<int64_t>(findHeaderByPath(appIdentifier).appId);
  }
  auto status = getContextHub(callbackSignal)
                    ->unloadNanoapp(kContextHubId, appId, kUnloadTransactionId);
  verifyStatusAndSignal(/* operation= */ "unloading nanoapp " + appIdentifier,
                        status, callbackSignal);
}

void queryNanoapps() {
  std::future<void> callbackSignal;
  auto status = getContextHub(callbackSignal)->queryNanoapps(kContextHubId);
  verifyStatusAndSignal(/* operation= */ "querying nanoapps", status,
                        callbackSignal);
}

enum Command { list, load, query, unload, unsupported };

struct CommandInfo {
  Command cmd;
  u_int8_t numofArgs;  // including cmd;
};

Command parseCommand(const std::vector<std::string> &cmdLine) {
  std::map<std::string, CommandInfo> commandMap{
      {"list", {list, 2}},
      {"load", {load, 2}},
      {"query", {query, 1}},
      {"unload", {unload, 2}},
  };
  if (cmdLine.empty() || commandMap.find(cmdLine[0]) == commandMap.end()) {
    return unsupported;
  }
  auto cmdInfo = commandMap.at(cmdLine[0]);
  return cmdLine.size() == cmdInfo.numofArgs ? cmdInfo.cmd : unsupported;
}

}  // anonymous namespace

int main(int argc, char *argv[]) {
  // Start binder thread pool to enable callbacks.
  ABinderProcess_startThreadPool();

  std::vector<std::string> cmdLine{};
  for (int i = 1; i < argc; i++) {
    cmdLine.emplace_back(argv[i]);
  }
  try {
    switch (parseCommand(cmdLine)) {
      case list: {
        std::map<std::string, NanoAppBinaryHeader> nanoapps{};
        readNanoappHeaders(nanoapps, cmdLine[1]);
        for (const auto &entity : nanoapps) {
          std::cout << entity.first;
          printNanoappHeader(entity.second);
        }
        break;
      }
      case load: {
        loadNanoapp(cmdLine[1]);
        break;
      }
      case unload: {
        unloadNanoapp(cmdLine[1]);
        break;
      }
      case query: {
        queryNanoapps();
        break;
      }
      default:
        std::cout << kUsage;
    }
  } catch (std::system_error &e) {
    std::cerr << e.what() << std::endl;
    return -1;
  }
  return 0;
}