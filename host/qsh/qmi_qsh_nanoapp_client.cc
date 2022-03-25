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

#include "qmi_qsh_nanoapp_client.h"

namespace android {
namespace chre {

bool QmiQshNanoappClient::enable() {
  return sendSuidReq();
}

bool QmiQshNanoappClient::enableStream() {
  LOGE("Unimplemented");
  return false;
}

void QmiQshNanoappClient::disable() {
  disconnect();
}

bool QmiQshNanoappClient::handleMessageStream(uint32_t msgId,
                                              pb_istream_t *stream,
                                              const pb_field_t *field,
                                              void **arg) {
  bool success = false;
  switch (msgId) {
    case SNS_STD_MSGID_SNS_STD_ATTR_EVENT: {
      success = QmiCallbacks::decodeAttrEvent(stream, field, arg);
      break;
    }

    case SNS_STD_MSGID_SNS_STD_ERROR_EVENT: {
      LOGE("Received an error event");
      break;
    }

    default: {
      LOGE("Unknown message id %" PRIu32 " received", msgId);
    }
  }
  return success;
}

bool QmiQshNanoappClient::handleAttribute(uint32_t /*attributeId*/,
                                          pb_istream_t *stream,
                                          const pb_field_t * /*field*/,
                                          void ** /*arg*/) {
  bool success = false;
  sns_std_attr attribute = sns_std_attr_init_default;
  attribute.value.values.funcs.decode = QmiCallbacks::decodeAttrValue;
  attribute.value.values.arg = nullptr;

  if (!(success = pb_decode(stream, sns_std_attr_fields, &attribute))) {
    LOGE("Error decoding attribute: %s", PB_GET_ERROR(stream));
  }

  return success;
}

}  // namespace chre
}  // namespace android