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

#include "chre/platform/platform_nanoapp.h"
#include "chre/util/system/napp_permissions.h"

namespace chre {

PlatformNanoapp::~PlatformNanoapp() {}

bool PlatformNanoappBase::reserveBuffer(uint64_t appId, uint32_t appVersion,
                                        uint32_t appFlags, size_t appBinaryLen,
                                        uint32_t targetApiVersion) {
  CHRE_ASSERT(!isLoaded());

  bool success = false;
  mAppBinary = memoryAlloc(appBinaryLen);

  // TODO(b/237819962): Check binary signature when authentication is
  // implemented.
  if (mAppBinary == nullptr) {
    LOG_OOM();
  } else {
    mExpectedAppId = appId;
    mExpectedAppVersion = appVersion;
    mExpectedTargetApiVersion = targetApiVersion;
    mAppBinaryLen = appBinaryLen;
    success = true;
  }

  return success;
}

bool PlatformNanoappBase::copyNanoappFragment(const void *buffer,
                                              size_t bufferLen) {
  CHRE_ASSERT(!isLoaded());

  bool success = true;

  if ((mBytesLoaded + bufferLen) > mAppBinaryLen) {
    LOGE("Overflow: cannot load %zu bytes to %zu/%zu nanoapp binary buffer",
         bufferLen, mBytesLoaded, mAppBinaryLen);
    success = false;
  } else {
    uint8_t *binaryBuffer = static_cast<uint8_t *>(mAppBinary) + mBytesLoaded;
    memcpy(binaryBuffer, buffer, bufferLen);
    mBytesLoaded += bufferLen;
  }

  return success;
}

bool PlatformNanoapp::start() {
  bool success = false;

  if (mAppInfo != nullptr) {
    success = mAppInfo->entryPoints.start();
  }

  return success;
}

void PlatformNanoapp::handleEvent(uint32_t senderInstanceId, uint16_t eventType,
                                  const void *eventData) {
  // TODO(b/18494851): Remove assert after dynamic loading has been ported.
  CHRE_ASSERT(mAppInfo != nullptr);
  mAppInfo->entryPoints.handleEvent(senderInstanceId, eventType, eventData);
}

void PlatformNanoapp::end() {
  // TODO(b/18494851): Remove assert after dynamic loading has been ported.
  CHRE_ASSERT(mAppInfo != nullptr);
  mAppInfo->entryPoints.end();
}

uint64_t PlatformNanoapp::getAppId() const {
  // TODO(b/18494851): Remove assert and modify implementation after dynamic
  // loading has been ported.
  CHRE_ASSERT(mAppInfo != nullptr);
  return mAppInfo->appId;
}

uint32_t PlatformNanoapp::getAppVersion() const {
  // TODO(b/18494851): Remove assert and modify implementation after dynamic
  // loading has been ported.
  CHRE_ASSERT(mAppInfo != nullptr);
  return mAppInfo->appVersion;
}

uint32_t PlatformNanoapp::getTargetApiVersion() const {
  // TODO(b/18494851): Remove assert and modify implementation after dynamic
  // loading has been ported.
  CHRE_ASSERT(mAppInfo != nullptr);
  return mAppInfo->targetApiVersion;
}
bool PlatformNanoapp::isSystemNanoapp() const {
  // TODO(b/18494851): Remove assert and modify implementation after dynamic
  // loading has been ported.
  CHRE_ASSERT(mAppInfo != nullptr);
  return mAppInfo->isSystemNanoapp;
}

bool PlatformNanoappBase::isLoaded() const {
  return mIsStatic;
}

void PlatformNanoappBase::loadStatic(const struct chreNslNanoappInfo *appInfo) {
  CHRE_ASSERT(!isLoaded());
  mIsStatic = true;
  mAppInfo = appInfo;
}

bool PlatformNanoapp::supportsAppPermissions() const {
  return (mAppInfo != nullptr) ? (mAppInfo->structMinorVersion >=
                                  CHRE_NSL_NANOAPP_INFO_STRUCT_MINOR_VERSION)
                               : false;
}

uint32_t PlatformNanoapp::getAppPermissions() const {
  return (supportsAppPermissions())
             ? mAppInfo->appPermissions
             : static_cast<uint32_t>(chre::NanoappPermissions::CHRE_PERMS_NONE);
}

}  // namespace chre
