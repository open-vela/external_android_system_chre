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

#include <cstring>

#include "pb_decode.h"
#include "pb_encode.h"
#include "sns_client.pb.h"
#include "sns_client_api_v01.h"
#include "sns_std_sensor.pb.h"
#include "sns_std_type.pb.h"
#include "sns_suid.pb.h"

#include "qsh_daemon.h"

namespace android {
namespace chre {

bool QshChreDaemon::init() {
  bool success;
  if (!(success = mQmiQshNanoappClient.connect())) {
    LOGE("Failed to connect to the QSH shim.");
  }

  return success && mQmiQshNanoappClient.enable();
}

void QshChreDaemon::deinit() {
  setShutdownRequested(true);
  mQmiQshNanoappClient.disable();
}

void QshChreDaemon::run() {
  constexpr char kChreSocketName[] = "chre";
  auto callback = [&](uint16_t clientId, void *data, size_t len) {
    sendMessageToChre(clientId, data, len);
  };

  mServer.run(kChreSocketName, true /* allowSocketCreation */, callback);
}

bool QshChreDaemon::doSendMessage(void * /*data*/, size_t /*length*/) {
  LOGE("sendMessage is currently unimplemented");
  return false;
}

}  // namespace chre
}  // namespace android
