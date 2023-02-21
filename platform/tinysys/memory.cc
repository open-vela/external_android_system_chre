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

#include "chre/platform/memory.h"
#include "chre/platform/shared/memory.h"
#include "portable.h"

namespace chre {

void forceDramAccess() {}

void nanoappBinaryFree(void *pointer) {
  memoryFree(pointer);
}

void nanoappBinaryDramFree(void *pointer) {
  vPortDramFree(pointer);
}

void *memoryAllocDram(size_t size) {
  return memoryAlloc(size);
}

void memoryFreeDram(void *pointer) {
  memoryFree(pointer);
}

void *palSystemApiMemoryAlloc(size_t size) {
  return memoryAlloc(size);
}

void palSystemApiMemoryFree(void *pointer) {
  memoryFree(pointer);
}

void *nanoappBinaryAlloc(size_t /*size*/, size_t /*alignment*/) {
  // TODO(b/252874047): Implementation is only required for dynamic loading.
  return nullptr;
}

void *nanoappBinaryDramAlloc(size_t size, size_t /*alignment*/) {
  return pvPortDramMalloc(size);
}

}  // namespace chre
