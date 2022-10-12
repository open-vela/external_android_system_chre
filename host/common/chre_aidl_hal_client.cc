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

#include <cctype>
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
// Locations should be searched in the sequence defined below:
const char *kPredefinedNanoappPaths[] = {
    "/vendor/etc/chre/",
    "/vendor/dsp/adsp/",
    "/vendor/dsp/sdsp/",
    "/vendor/lib/rfsa/adsp/",
};
constexpr char kUsage[] = R"(
Usage: chre_aidl_hal_client COMMAND [ARGS]
COMMAND ARGS...:
  list <PATH_OF_NANOAPPS>     - list all the nanoapps' header info in the path.
  load <APP_NAME>             - load the nanoapp specified by the name.
                                If an absolute path like /path/to/awesome.so,
                                which is optional, is not provided then default
                                locations are searched.
  query                       - show all loaded nanoapps (system apps excluded)
  unload <HEX_NANOAPP_ID | APP_NAME>
                              - unload the nanoapp specified by either the
                                nanoapp id in hex format or the app name.
                                If an absolute path like /path/to/awesome.so,
                                which is optional, is not provided then default
                                locations are searched.
)";

inline void throwError(const std::string &message) {
  throw std::system_error{std::error_code(), message};
}

bool isValidNanoappHexId(const std::string &number) {
  if (number.empty() ||
      (number.substr(0, 2) != "0x" && number.substr(0, 2) != "0X")) {
    return false;
  }
  // Once the input has the hex prefix, an exception will be thrown if it is
  // malformed because it shouldn't be treated as an app name anymore.
  if (number.size() > 18 || number.size() < 3) {
    throwError("Hex app id must has a length of [3, 18] including the prefix.");
  }
  for (int i = 2; i < number.size(); i++) {
    if (!isxdigit(number[i])) {
      throwError("Hex app id " + number + " contains invalid character.");
    }
  }
  return true;
}

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

std::unique_ptr<NanoAppBinaryHeader> findHeaderByName(
    const std::string &appName, const std::string &binaryPath) {
  DIR *dir = opendir(binaryPath.c_str());
  if (dir == nullptr) {
    return nullptr;
  }
  std::regex regex(appName + ".napp_header");
  std::cmatch match;

  std::unique_ptr<NanoAppBinaryHeader> result = nullptr;
  for (struct dirent *entry; (entry = readdir(dir)) != nullptr;) {
    if (!std::regex_match(entry->d_name, match, regex)) {
      continue;
    }
    std::ifstream input(std::string(binaryPath) + "/" + entry->d_name,
                        std::ios::binary);
    result = std::make_unique<NanoAppBinaryHeader>();
    input.read(reinterpret_cast<char *>(result.get()),
               sizeof(NanoAppBinaryHeader));
    break;
  }
  closedir(dir);
  return result;
}

void readNanoappHeaders(std::map<std::string, NanoAppBinaryHeader> &nanoapps,
                        const std::string &binaryPath) {
  DIR *dir = opendir(binaryPath.c_str());
  if (dir == nullptr) {
    return;
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
/** Finds the .napp_header file associated to the nanoapp.
 *
 * This function guarantees to return a non-null {@link NanoAppBinaryHeader}
 * pointer. In case a .napp_header file cannot be found an exception will be
 * raised.
 *
 * @param pathAndName name of the nanoapp that might be prefixed with it path.
 * It will be normalized to the format of <absolute-path><name>.so at the end.
 * For example, "abc" will be changed to "/path/to/abc.so".
 * @return a unique pointer to the {@link NanoAppBinaryHeader} found
 */
std::unique_ptr<NanoAppBinaryHeader> findHeaderAndNormalizePath(
    std::string &pathAndName) {
  // To match the file pattern of [path]<name>[.so]
  std::regex pathNameRegex("(.*?)(\\w+)(\\.so)?");
  std::smatch smatch;
  if (!std::regex_match(pathAndName, smatch, pathNameRegex)) {
    throwError("Invalid nanoapp: " + pathAndName);
  }
  std::string fullPath = smatch[1];
  std::string appName = smatch[2];
  // absolute path is provided:
  if (!fullPath.empty() && fullPath[0] == '/') {
    auto result = findHeaderByName(appName, fullPath);
    if (result == nullptr) {
      throwError("Unable to find the nanoapp header for " + pathAndName);
    }
    pathAndName = fullPath + appName + ".so";
    return result;
  }
  // relative path is searched form predefined locations:
  for (const std::string &predefinedPath : kPredefinedNanoappPaths) {
    auto result = findHeaderByName(appName, predefinedPath);
    if (result == nullptr) {
      continue;
    }
    pathAndName = predefinedPath + appName + ".so";
    std::cout << "Found the nanoapp header for " << pathAndName << std::endl;
    return result;
  }
  throwError("Unable to find the nanoapp header for " + pathAndName);
  return nullptr;
}

void loadNanoapp(std::string &pathAndName) {
  auto header = findHeaderAndNormalizePath(pathAndName);
  std::vector<uint8_t> soBuffer{};
  if (!readFileContents(pathAndName.c_str(), &soBuffer)) {
    throwError("Failed to open the content of " + pathAndName);
  }
  NanoappBinary binary;
  binary.nanoappId = static_cast<int64_t>(header->appId);
  binary.customBinary = soBuffer;
  binary.flags = static_cast<int32_t>(header->flags);
  binary.targetChreApiMajorVersion =
      static_cast<int8_t>(header->targetChreApiMajorVersion);
  binary.targetChreApiMinorVersion =
      static_cast<int8_t>(header->targetChreApiMinorVersion);
  binary.nanoappVersion = static_cast<int32_t>(header->appVersion);

  std::future<void> callbackSignal;
  auto status = getContextHub(callbackSignal)
                    ->loadNanoapp(kContextHubId, binary, kLoadTransactionId);
  verifyStatusAndSignal(/* operation= */ "loading nanoapp " + pathAndName,
                        status, callbackSignal);
}

void unloadNanoapp(std::string &appIdOrName) {
  std::future<void> callbackSignal;
  int64_t appId;
  if (isValidNanoappHexId(appIdOrName)) {
    appId = std::stoll(appIdOrName, nullptr, 16);
  } else {
    // Treat the appIdOrName as the app name and try again
    appId =
        static_cast<int64_t>(findHeaderAndNormalizePath(appIdOrName)->appId);
  }
  auto status = getContextHub(callbackSignal)
                    ->unloadNanoapp(kContextHubId, appId, kUnloadTransactionId);
  verifyStatusAndSignal(/* operation= */ "unloading nanoapp " + appIdOrName,
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