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

#include "chre_host/file_stream.h"
#include "chre_host/log.h"

#include <cinttypes>
#include <string>
#include <vector>

#include <aidl/android/hardware/contexthub/IContextHub.h>
#include <aidl/android/hardware/contexthub/NanoappBinary.h>
#include <android/binder_manager.h>
#include <utils/String16.h>

using aidl::android::hardware::contexthub::IContextHub;
using aidl::android::hardware::contexthub::NanoappBinary;
using android::String16;
using android::chre::readFileContents;

namespace {

constexpr uint32_t kContextHubId = 0;
constexpr int32_t kTransactionId = -1;

bool loadNanoapp(const char *fileName, uint64_t nanoappId) {
  bool success = false;
  auto aidlServiceName = std::string() + IContextHub::descriptor + "/default";
  ndk::SpAIBinder binder(
      AServiceManager_waitForService(aidlServiceName.c_str()));
  if (binder.get() == nullptr) {
    LOGE("Could not find Context Hub HAL");
  } else {
    std::shared_ptr<IContextHub> contextHub = IContextHub::fromBinder(binder);
    contextHub->registerCallback(kContextHubId, nullptr);
    std::vector<uint8_t> soBuffer;
    if (!readFileContents(fileName, &soBuffer)) {
      LOGE("Failed to read contents of %s", fileName);
    } else {
      NanoappBinary binary;
      binary.nanoappId = nanoappId;
      binary.customBinary = soBuffer;
      success =
          contextHub->loadNanoapp(kContextHubId, binary, kTransactionId).isOk();
      LOGI("Load nanoapp 0x%" PRIx64 " success %d", nanoappId, success);
    }
  }
  return success;
}

bool unloadNanoapp(uint64_t nanoappId) {
  bool success = false;
  auto aidlServiceName = std::string() + IContextHub::descriptor + "/default";
  ndk::SpAIBinder binder(
      AServiceManager_waitForService(aidlServiceName.c_str()));
  if (binder.get() == nullptr) {
    LOGE("Could not find Context Hub HAL");
  } else {
    std::shared_ptr<IContextHub> contextHub = IContextHub::fromBinder(binder);
    contextHub->registerCallback(kContextHubId, nullptr);
    success =
        contextHub->unloadNanoapp(kContextHubId, nanoappId, kTransactionId)
            .isOk();
    LOGI("Unload nanoapp 0x%" PRIx64 " success %d", nanoappId, success);
  }
  return success;
}

void printUsage() {
  LOGI(
      "\n"
      "Usage:\n"
      " chre_aidl_hal_client load <path to .so file> <nanoapp ID>\n"
      " chre_aidl_hal_client unload <nanoapp ID>");
}

}  // anonymous namespace

int main(int argc, char *argv[]) {
  int argi = 1;
  const std::string cmd{argi < argc ? argv[argi++] : ""};

  std::vector<std::string> args;
  while (argi < argc) {
    args.push_back(std::string(argv[argi++]));
  }

  bool success = false;
  if (cmd == "load" && args.size() == 2) {
    std::string file = args[0];
    uint64_t nanoappId = strtoull(args[1].c_str(), NULL, 0);
    success = loadNanoapp(file.c_str(), nanoappId);
  } else if (cmd == "unload" && args.size() == 1) {
    uint64_t nanoappId = strtoull(args[0].c_str(), NULL, 0);
    success = unloadNanoapp(nanoappId);
  } else {
    printUsage();
  }

  return success ? 0 : -1;
}
