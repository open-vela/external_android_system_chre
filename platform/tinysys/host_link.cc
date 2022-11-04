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

#include "chre/target_platform/host_link_base.h"

namespace chre {

void sendDebugDumpResultToHost(uint16_t /*hostClientId*/,
                               const char * /*debugStr*/,
                               size_t /*debugStrSize*/, bool /*complete*/,
                               uint32_t /*dataCount*/) {}

void HostLinkBase::receive(void * /*cookie*/, void * /*message*/,
                           int /*messageLen*/) {}
bool HostLinkBase::send(uint8_t * /*data*/, size_t /*dataLen*/) {
  // Implement this.
  return false;
}
void HostLinkBase::sendTimeSyncRequest() {}
void HostLinkBase::sendLogMessageV2(const uint8_t * /*logMessage*/,
                                    size_t /*logMessageSize*/,
                                    uint32_t /*num_logs_dropped*/) {}
}  // namespace chre