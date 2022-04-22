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

#ifndef CHRE_PLATFORM_EMBOS_MUTEX_BASE_IMPL_H_
#define CHRE_PLATFORM_EMBOS_MUTEX_BASE_IMPL_H_

#include "chre/platform/mutex.h"

namespace chre {

inline Mutex::Mutex() {
  OS_MUTEX_Create(&mMutex);
}

inline Mutex::~Mutex() {
  OS_MUTEX_Delete(&mMutex);
}

inline void Mutex::lock() {
  OS_MUTEX_LockBlocked(&mMutex);
}

inline bool Mutex::try_lock() {
  return OS_MUTEX_Lock(&mMutex) != 0;
}

inline void Mutex::unlock() {
  OS_MUTEX_Unlock(&mMutex);
}

}  // namespace chre

#endif  // CHRE_PLATFORM_EMBOS_MUTEX_BASE_IMPL_H_
