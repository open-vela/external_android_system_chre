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

#include "chre/platform/platform_nanoapp.h"

#include <dlfcn.h>
#include <sys/stat.h>

#include <cinttypes>

#include "chre/platform/assert.h"
#include "chre/platform/log.h"
#include "chre/platform/shared/nanoapp_dso_util.h"
#include "chre/util/system/napp_permissions.h"
#include "chre_api/chre/version.h"

namespace chre {
std::string PlatformNanoappBase::mSavefilename;

PlatformNanoapp::~PlatformNanoapp() { closeNanoapp(); }

bool PlatformNanoapp::start() {
  return openNanoapp() && mAppInfo->entryPoints.start();
}

void PlatformNanoapp::handleEvent(uint32_t senderInstanceId, uint16_t eventType,
                                  const void *eventData) {
  mAppInfo->entryPoints.handleEvent(senderInstanceId, eventType, eventData);
}

void PlatformNanoapp::end() {
  mAppInfo->entryPoints.end();
  closeNanoapp();
}

uint64_t PlatformNanoapp::getAppId() const {
  return (mAppInfo == nullptr) ? 0 : mAppInfo->appId;
}

uint32_t PlatformNanoapp::getAppVersion() const { return mAppInfo->appVersion; }

uint32_t PlatformNanoapp::getTargetApiVersion() const {
  return CHRE_API_VERSION;
}

const char *PlatformNanoapp::getAppName() const {
  return (mAppInfo != nullptr) ? mAppInfo->name : "Unknown";
}

bool PlatformNanoapp::supportsAppPermissions() const {
  return (mAppInfo != nullptr) ? (mAppInfo->structMinorVersion >=
                                  CHRE_NSL_NANOAPP_INFO_STRUCT_MINOR_VERSION_3)
                               : false;
}

uint32_t PlatformNanoapp::getAppPermissions() const {
  return (supportsAppPermissions())
             ? mAppInfo->appPermissions
             : static_cast<uint32_t>(chre::NanoappPermissions::CHRE_PERMS_NONE);
}

bool PlatformNanoapp::isSystemNanoapp() const {
  return (mAppInfo != nullptr && mAppInfo->isSystemNanoapp);
}

void PlatformNanoapp::logStateToBuffer(
    DebugDumpWrapper & /* debugDump */) const {}

void PlatformNanoappBase::loadFromFile(const std::string &filename) {
  CHRE_ASSERT(!isLoaded());
  mFilename = filename;
}

bool PlatformNanoappBase::setfilename(const std::string &filename) {
  if (access(CONFIG_CHRE_NANOAPP_SAVEPATH, F_OK) != 0) {
    if (mkdir(CONFIG_CHRE_NANOAPP_SAVEPATH, 0755) < 0) {
      return false;
    }
  }
  mSavefilename = std::string(CONFIG_CHRE_NANOAPP_SAVEPATH) + "/" + filename;
  return true;
}

bool PlatformNanoappBase::setAppInfo(uint64_t appId, uint32_t appVersion,
                                     const std::string &appFilename,
                                     uint32_t targetApiVersion) {
  CHRE_ASSERT(!isLoaded());
  if (access(appFilename.c_str(), F_OK) == 0) {
    mFilename = appFilename;
    mExpectedAppId = appId;
    mExpectedAppVersion = appVersion;
    mExpectedTargetApiVersion = targetApiVersion;
    return true;
  }
  return false;
}

void PlatformNanoappBase::loadStatic(const struct chreNslNanoappInfo *appInfo) {
  CHRE_ASSERT(!isLoaded());
  mIsStatic = true;
  mAppInfo = appInfo;
}

bool PlatformNanoappBase::isLoaded() const {
  return (mIsStatic || mDsoHandle != nullptr || !mFilename.empty() ||
          receiveComplete());
}

bool PlatformNanoappBase::openNanoapp() {
  bool success = false;

  if (mIsStatic) {
    success = true;
  } else if (!mFilename.empty()) {
    success = openNanoappFromFile();
  } else {
    CHRE_ASSERT(false);
  }

  return success;
}

bool PlatformNanoappBase::reserveBuffer(uint64_t appId, uint32_t appVersion,
                                        uint32_t appFlags, size_t appBinaryLen,
                                        uint32_t targetApiVersion) {
  CHRE_ASSERT(!isLoaded());
  if (mAppfd != NULL) {
    fclose(mAppfd);
    mAppfd = NULL;
  }
  if (mSavefilename.empty()) {
    mSavefilename =
        std::string(CONFIG_CHRE_NANOAPP_SAVEPATH) + "/" + std::to_string(appId);
  }
  mAppfd = fopen(mSavefilename.c_str(), "wb");
  if (mAppfd == NULL) {
    LOGE("open %s failed errno=%d", mSavefilename.c_str(), errno);
    return false;
  }
  mExpectedAppId = appId;
  mExpectedAppVersion = appVersion;
  mExpectedTargetApiVersion = targetApiVersion;
  mAppBinaryLen = appBinaryLen;
  return true;
}

bool PlatformNanoappBase::copyNanoappFragment(const void *buffer,
                                              size_t bufferLen) {
  CHRE_ASSERT(!isLoaded());

  if ((mBytesLoaded + bufferLen) > mAppBinaryLen) {
    LOGE("Overflow: cannot load %zu bytes to %zu/%zu nanoapp binary buffer",
         bufferLen, mBytesLoaded, mAppBinaryLen);
    return false;
  } else if (mAppfd) {
    size_t n = fwrite(buffer, 1, (int)bufferLen, mAppfd);
    if (n != bufferLen) {
      fclose(mAppfd);
      mAppfd = NULL;
      remove(mSavefilename.c_str());
      LOGE("fwrite size %zu != %zu errno=%d", n, bufferLen, errno);
      return false;
    } else {
      mBytesLoaded += bufferLen;
    }
    if (mAppBinaryLen == mBytesLoaded) {
      fclose(mAppfd);
      mAppfd = NULL;
      mFilename = mSavefilename;
      mSavefilename.clear();
    }
  }
  return true;
}

bool PlatformNanoappBase::openNanoappFromFile() {
  CHRE_ASSERT(!mFilename.empty());
  CHRE_ASSERT_LOG(mDsoHandle == nullptr, "Re-opening nanoapp");
  bool success = false;

  mDsoHandle = dlopen(mFilename.c_str(), RTLD_NOW | RTLD_GLOBAL);
  if (mDsoHandle == nullptr) {
    LOGE("Failed to load nanoapp from file %s: %s", mFilename.c_str(),
         dlerror());
  } else {
    mAppInfo = static_cast<const struct chreNslNanoappInfo *>(
        dlsym(mDsoHandle, CHRE_NSL_DSO_NANOAPP_INFO_SYMBOL_NAME));
    if (mAppInfo == nullptr) {
      LOGE("Failed to find app info symbol in %s: %s", mFilename.c_str(),
           dlerror());
    } else {
      // TODO(b/120778991): reenable this check after adding support for passing
      // in the .napp_header to the simulator
      // success = validateAppInfo(0 /* skip ID validation */, 0, mAppInfo);
      success = true;
      if (!success) {
        mAppInfo = nullptr;
      } else {
        LOGI("Nanoapp loaded: %s (0x%016" PRIx64 ") version 0x%" PRIx32
             " uimg %d system %d from file %s",
             mAppInfo->name, mAppInfo->appId, mAppInfo->appVersion,
             mAppInfo->isTcmNanoapp, mAppInfo->isSystemNanoapp,
             mFilename.c_str());
        if (mAppInfo->structMinorVersion >=
            CHRE_NSL_NANOAPP_INFO_STRUCT_MINOR_VERSION_3) {
          LOGI("Nanoapp permissions: 0x%" PRIx32, mAppInfo->appPermissions);
        }
      }
    }
  }

  return success;
}

void PlatformNanoappBase::closeNanoapp() {
  if (mDsoHandle != nullptr) {
    mAppInfo = nullptr;
    if (dlclose(mDsoHandle) != 0) {
      LOGE("dlclose failed: %s", dlerror());
    }
    mDsoHandle = nullptr;
  }
}

}  // namespace chre
