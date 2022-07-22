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

#ifndef CHRE_QSH_NANOAPP_CLIENT_H_
#define CHRE_QSH_NANOAPP_CLIENT_H_

#include "qmi_client_base.h"

namespace android {
namespace chre {

class QmiQshNanoappClient : public QmiClientBase {
 public:
  static constexpr char kSensorType[] = "chre_qsh_nanoapp";

  QmiQshNanoappClient() : QmiClientBase(kSensorType) {}

  void onAttrEventDecodeComplete() override;
  void onSuidDecodeEventComplete() override;
};

}  // namespace chre
}  // namespace android

#endif  // CHRE_QSH_NANOAPP_CLIENT_H_