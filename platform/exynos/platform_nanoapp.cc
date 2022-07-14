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

namespace chre {

PlatformNanoapp::~PlatformNanoapp() {}

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

}  // namespace chre