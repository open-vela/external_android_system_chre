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

#include <cstdint>

#include "chre/platform/system_time.h"

extern "C" {
#include "xgpt.h"
}

extern int64_t ap_sync_time_offset;

namespace chre {

Nanoseconds SystemTime::getMonotonicTime() {
  return Nanoseconds(get_boot_time_ns());
}

int64_t SystemTime::getEstimatedHostTimeOffset() {
  // TODO(b/264541538): switch to use timesync_get_host_offset_time()
  // API when we go to the U build
  return ap_sync_time_offset;
}

void SystemTime::setEstimatedHostTimeOffset(int64_t offset) {
  // not implemented for Tinysys
}
}  // namespace chre
