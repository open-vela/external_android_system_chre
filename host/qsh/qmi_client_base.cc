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

#include "qmi_client_base.h"
#include "qmi_enc_dec_callbacks.h"

namespace android {
namespace chre {

bool QmiClientBase::sendSnsClientReq(sns_client_req_msg_v01 &reqMsg) {
  bool success = false;
  auto resp = std::make_unique<sns_client_resp_msg_v01>();

  LOGD("Sending request payload len %i hdl %p", reqMsg.payload_len, mQmiHandle);

  if (resp == nullptr) {
    LOGE("Unable to allocate sns_client_resp_msg_v01");
  } else {
    qmi_txn_handle txnHandle;
    qmi_client_error_type qmi_err = qmi_client_send_msg_async(
        mQmiHandle, SNS_CLIENT_REQ_V01, &reqMsg, sizeof(reqMsg), resp.get(),
        sizeof(*resp), QmiCallbacks::onSnsClientResponse,
        nullptr /* responseCbData */, &txnHandle);

    if (qmi_err != QMI_NO_ERR) {
      LOGE("qmi_client_send_msg_async error %i", qmi_err);
    } else {
      resp.release();
      success = true;
    }
  }

  return success;
}

bool QmiClientBase::sendReq(SnsArg *payload, sns_std_suid suid,
                            uint32_t msgId) {
  bool success = false;
  sns_client_request_msg pbReqMsg = sns_client_request_msg_init_default;
  sns_client_req_msg_v01 reqMsg = {
      .payload_len = 0, .use_jumbo_report_valid = 0, .use_jumbo_report = 0};
  pb_ostream_t stream =
      pb_ostream_from_buffer(reqMsg.payload, SNS_CLIENT_REQ_LEN_MAX_V01);

  pbReqMsg.suid = suid;
  pbReqMsg.msg_id = msgId;
  pbReqMsg.request.has_batching = false;
  pbReqMsg.request.payload.funcs.encode = QmiCallbacks::encodePayload;
  pbReqMsg.request.payload.arg = payload;

  if (!pb_encode(&stream, sns_client_request_msg_fields, &pbReqMsg)) {
    LOGE("Error Encoding request: %s", PB_GET_ERROR(&stream));
  } else {
    reqMsg.payload_len = stream.bytes_written;
    success = sendSnsClientReq(reqMsg);
  }
  return success;
}

bool QmiClientBase::getEncodedAttrReq(std::vector<pb_byte_t> &encodedReq) {
  bool success = false;
  size_t encodedReqSize;
  sns_std_attr_req attrReq = sns_std_attr_req_init_default;

  if (!pb_get_encoded_size(&encodedReqSize, sns_std_attr_req_fields,
                           &attrReq)) {
    LOGE("pb_get_encoded_size error");
  } else {
    encodedReq.resize(encodedReqSize, 0);
    pb_ostream_t stream =
        pb_ostream_from_buffer(encodedReq.data(), encodedReqSize);

    if (!pb_encode(&stream, sns_std_attr_req_fields, &attrReq)) {
      LOGE("Error Encoding attribute request: %s", PB_GET_ERROR(&stream));
    } else {
      success = true;
    }
  }
  return success;
}

bool QmiClientBase::sendAttrReq(sns_std_suid &suid) {
  bool success = false;
  std::vector<pb_byte_t> encodedReq;
  if (getEncodedAttrReq(encodedReq)) {
    SnsArg payload = {.buf = encodedReq.data(), .bufLen = encodedReq.size()};
    LOGD("sending attr req for suid %" PRIu64 " %" PRIu64, suid.suid_high,
         suid.suid_low);
    success = sendReq(&payload, suid, SNS_STD_MSGID_SNS_STD_ATTR_REQ);
  }

  return success;
}

bool QmiClientBase::getEncodedSuidReq(std::vector<pb_byte_t> &encodedReq) {
  bool success = false;
  size_t encodedReqSize;
  sns_suid_req suidReq = sns_suid_req_init_default;

  suidReq.data_type.funcs.encode = QmiCallbacks::encodePayload;
  SnsArg arg = {.buf = mSensorType, .bufLen = strlen(mSensorType)};
  suidReq.data_type.arg = &arg;
  suidReq.has_register_updates = true;
  suidReq.register_updates = false;

  if (!pb_get_encoded_size(&encodedReqSize, sns_suid_req_fields, &suidReq)) {
    LOGE("pb_get_encoded_size error");
  } else {
    encodedReq.resize(encodedReqSize, 0);
    pb_ostream_t stream =
        pb_ostream_from_buffer(encodedReq.data(), encodedReqSize);

    if (!pb_encode(&stream, sns_suid_req_fields, &suidReq)) {
      LOGE("Error Encoding attribute request: %s", PB_GET_ERROR(&stream));
    } else {
      success = true;
    }
  }

  return success;
}

bool QmiClientBase::sendSuidReq(SuidAttrCallback callback, void *callbackData) {
  bool success = true;

  if (!isConnected()) {
    success = connect();
  } else if (!mSuidRequestCompleted) {
    LOGE("An SUID request is already in process, rejecting this request.");
    success = false;
  } else if (success) {
    std::vector<pb_byte_t> encodedReq;
    if (getEncodedSuidReq(encodedReq)) {
      SnsArg payload =
          (SnsArg){.buf = encodedReq.data(), .bufLen = encodedReq.size()};
      success = sendReq(&payload, mLookupSuid, SNS_SUID_MSGID_SNS_SUID_REQ);
      if (!success) {
        LOGE("Failed to send SUID request");
      } else {
        mSuidAttrCallbackData.callback = callback;
        mSuidAttrCallbackData.data = callbackData;
        mSuidRequestCompleted = false;
      }
    }
  }

  return success;
}

bool QmiClientBase::handleEvent(void const *eventMsg, size_t eventMsgLen,
                                void *data) {
  pb_istream_t stream;
  auto *decodeCbData = static_cast<QmiCallbacks::DecodeCbData *>(data);
  sns_client_event_msg event = sns_client_event_msg_init_default;
  LOGD("Processing events from SUID %" PRIx64 " %" PRIx64,
       decodeCbData->suid.suid_low, decodeCbData->suid.suid_high);

  event.events.funcs.decode = QmiCallbacks::decodeEvents;
  event.events.arg = decodeCbData;

  stream = pb_istream_from_buffer(static_cast<const pb_byte_t *>(eventMsg),
                                  eventMsgLen);
  if (!pb_decode(&stream, sns_client_event_msg_fields, &event)) {
    LOGE("Error decoding event list: %s", PB_GET_ERROR(&stream));
  }
  return true;
}

bool QmiClientBase::handleMessageStream(uint32_t msgId, pb_istream_t *stream,
                                        const pb_field_t *field, void **arg) {
  bool success = false;
  switch (msgId) {
    case SNS_STD_MSGID_SNS_STD_ATTR_EVENT: {
      LOGD("SNS_STD_MSGID_SNS_STD_ATTR_EVENT");
      success = QmiCallbacks::decodeAttrEvent(stream, field, arg);
      break;
    }

    case SNS_STD_MSGID_SNS_STD_ERROR_EVENT: {
      LOGE("Received an error event");
      break;
    }

    default: {
      success = handleMessageStreamExtended(msgId, stream, field, arg);
    }
  }
  return success;
}

bool QmiClientBase::handleMessageStreamExtended(uint32_t msgId,
                                                pb_istream_t * /* stream */,
                                                const pb_field_t * /* field */,
                                                void ** /* arg */) {
  LOGE("Unknown message id %" PRIu32 " received", msgId);
  return false;
}

bool QmiClientBase::handleAttribute(uint32_t attributeId, const char *value) {
  bool success = true;
  switch (attributeId) {
    case SNS_STD_SENSOR_ATTRID_NAME: {
      // The name attribute signifies the start of a new set of attributes:
      // push the current one to our attribute list before proceeding to
      // process the new set of attributes.
      updateAttributes();
      mCurrentSuidAttribute.name = std::string(value);
      LOGD("%s SNS_STD_SENSOR_ATTRID_NAME %s", __FUNCTION__,
           mCurrentSuidAttribute.name.c_str());
      break;
    }

    case SNS_STD_SENSOR_ATTRID_VENDOR: {
      mCurrentSuidAttribute.vendor = std::string(value);
      LOGD("%s SNS_STD_SENSOR_ATTRID_VENDOR %s", __FUNCTION__,
           mCurrentSuidAttribute.vendor.c_str());
      break;
    }

    case SNS_STD_SENSOR_ATTRID_TYPE: {
      mCurrentSuidAttribute.type = std::string(value);
      LOGD("%s SNS_STD_SENSOR_ATTRID_TYPE %s", __FUNCTION__,
           mCurrentSuidAttribute.vendor.c_str());
      break;
    }

    case SNS_STD_SENSOR_ATTRID_API: {
      mCurrentSuidAttribute.api = std::string(value);
      LOGD("%s SNS_STD_SENSOR_ATTRID_API %s", __FUNCTION__,
           mCurrentSuidAttribute.api.c_str());
      break;
    }

    default: {
      LOGE("Unknown string attribute ID: %" PRIu32, attributeId);
      success = false;
    }
  }
  return success;
}

void QmiClientBase::updateAttributes() {
  if (!mCurrentSuidAttribute.name.empty()) {
    const auto &currentName = mCurrentSuidAttribute.name;
    mSuidAttributesList.erase(
        std::remove_if(mSuidAttributesList.begin(), mSuidAttributesList.end(),
                       [&currentName](const SuidAttributes &x) {
                         return x.name == currentName;
                       }),
        mSuidAttributesList.end());
    mSuidAttributesList.push_back(mCurrentSuidAttribute);
    mCurrentSuidAttribute.reset();
  }
}

bool QmiClientBase::handleAttribute(uint32_t attributeId, float value) {
  bool success = true;
  switch (attributeId) {
    default: {
      LOGE("Unknown float attribute %f for ID: %" PRIu32, value, attributeId);
    }
  }
  return success;
}

bool QmiClientBase::handleAttribute(uint32_t attributeId, bool value) {
  bool success = true;
  switch (attributeId) {
    case SNS_STD_SENSOR_ATTRID_AVAILABLE: {
      mCurrentSuidAttribute.isAvailable = value;
      LOGD("%s SNS_STD_SENSOR_ATTRID_AVAILABLE %d", __FUNCTION__,
           mCurrentSuidAttribute.isAvailable);
      break;
    }

    default: {
      LOGE("Unknown uint32 attribute ID: %" PRIu32, attributeId);
      success = false;
    }
  }
  return success;
}

bool QmiClientBase::handleAttribute(uint32_t attributeId, int64_t value) {
  bool success = false;
  switch (attributeId) {
    case SNS_STD_SENSOR_ATTRID_STREAM_TYPE: {
      mCurrentSuidAttribute.streamType =
          static_cast<sns_std_sensor_stream_type>(value);
      LOGD("%s SNS_STD_SENSOR_ATTRID_STREAM_TYPE %" PRIu32, __FUNCTION__,
           mCurrentSuidAttribute.streamType);
      break;
    }

    case SNS_STD_SENSOR_ATTRID_VERSION: {
      mCurrentSuidAttribute.version = value;
      LOGD("%s SNS_STD_SENSOR_ATTRID_VERSION %" PRIu32, __FUNCTION__,
           mCurrentSuidAttribute.version);
      break;
    }

    default: {
      LOGE("Unknown int64 attribute ID: %" PRIu32, attributeId);
      success = false;
    }
  }
  return success;
}

bool QmiClientBase::handleAttribute(uint32_t attributeId, uint32_t value) {
  LOGE("Unknown uint32 attribute ID %" PRIu32 " with value 0x%" PRIx32,
       attributeId, value);
  return false;
}

bool QmiClientBase::connect() {
  bool success = true;

  if (!isConnected()) {
    success = waitForService();

    if (!success) {
      LOGE("Failed to wait on SNS client service");
    } else {
      success = false;
      qmi_idl_service_object_type service =
          SNS_CLIENT_SVC_get_service_object_v01();
      qmi_service_instance serviceInstance = 0;
      qmi_client_error_type err;
      qmi_cci_os_signal_type osParams;

      LOGD("Creating client connection from inst %p", this);

      err = qmi_client_init_instance(
          service, serviceInstance, QmiCallbacks::onIndication,
          this /*indCbData*/, &osParams, kQmiTimeoutMs, &mQmiHandle);

      if (err != QMI_NO_ERR) {
        LOGE("qmi_client_init_instance error %i", err);
      } else {
        LOGD("qmi client instance init done hdl: %p", mQmiHandle);
        err = qmi_client_register_error_cb(mQmiHandle, QmiCallbacks::onError,
                                           nullptr /*errCbData*/);

        if (err != QMI_NO_ERR) {
          LOGE("qmi_client_register_error_cb error %d", err);
        } else {
          err = qmi_client_register_log_cb(mQmiHandle, QmiCallbacks::onLog,
                                           nullptr /*cookie*/);
          LOGD("register cb done: %d", err);
          success = true;
        }
      }

      if (!success) {
        if (mQmiHandle != nullptr) {
          qmi_client_release(mQmiHandle);
          mQmiHandle = nullptr;
        }
      }
    }
  }
  return success;
}

void QmiClientBase::disconnect() {
  if (isConnected()) {
    qmi_client_error_type err = qmi_client_release(mQmiHandle);
    if (err != QMI_NO_ERR) {
      LOGE("Disconnection failed: %d", err);
    }
    mQmiHandle = nullptr;
  }
}

bool QmiClientBase::waitForService() {
  qmi_idl_service_object_type service = SNS_CLIENT_SVC_get_service_object_v01();
  qmi_client_type notifierHandle;
  qmi_cci_os_signal_type osParams;
  bool success = true;

  LOGD("Waiting for service");

  qmi_client_error_type err =
      qmi_client_notifier_init(service, &osParams, &notifierHandle);
  if (err != QMI_NO_ERR) {
    LOGE("qmi_client_notifier_init error %i", err);
    success = false;
  } else {
    QMI_CCI_OS_SIGNAL_WAIT(&osParams, kQmiTimeoutMs);
    if (osParams.timed_out) {
      LOGE("service is not available after %i timeout", kQmiTimeoutMs);
      success = false;
    }
  }

  qmi_client_release(notifierHandle);
  return success;
}

uint64_t QmiClientBase::getNanoappId(const sns_std_suid &suid) {
  uint64_t nanoappId = 0;
  uint64_t suidHigh = suid.suid_high;
  for (size_t i = 0; i < 8; i++) {
    nanoappId += suidHigh & 0xff;
    suidHigh >>= 8;
    if (i < 7) {
      nanoappId <<= 8;
    }
  }

  return nanoappId;
}

}  // namespace chre
}  // namespace android