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
#include "qmi_enc_dec_callbacks.h"

namespace android {
namespace chre {

void QmiQshNanoappClient::onSuidDecodeEventComplete() {
  // TODO(b/220195756): Consider refactoring SUID request + attr related
  // functionality out for easier reuse (eg: getting attributes from the shim).
  LOGD("Sending SUID attr requests for %zu SUIDs", mReceivedSuids.size());
  mNumPendingSuidAttrRequests = mReceivedSuids.size();
  mSuidAttributesList.clear();
  while (!mReceivedSuids.empty()) {
    auto &suid = mReceivedSuids.front();
    if (!sendAttrReq(suid)) {
      LOGE("Failed to send SUID request for %" PRIx64 " %" PRIx64,
           suid.suid_high, suid.suid_low);
    }
    mReceivedSuids.pop();
  }
}

void QmiQshNanoappClient::onAttrEventDecodeComplete() {
  // TODO(b/220195756): Consider adding a timeout (or other recovery mechanism
  // like clearing stale requests if the previous request timed out) if we end
  // up never receiving attributes.
  --mNumPendingSuidAttrRequests;
  if (mNumPendingSuidAttrRequests == 0) {
    mSuidAttrCallbackData.callAndReset(mSuidAttributesList);
    mSuidRequestCompleted = true;
  }
}

}  // namespace chre
}  // namespace android
