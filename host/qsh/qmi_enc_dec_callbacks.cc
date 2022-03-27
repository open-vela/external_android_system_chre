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

#include "qmi_enc_dec_callbacks.h"

namespace android {
namespace chre {

void QmiCallbacks::onLog(qmi_client_type handle,
                         qmi_idl_type_of_message_type msgType,
                         unsigned int msgId, unsigned int txnId,
                         const void * /*rawMsg*/, unsigned int rawMsgLen,
                         qmi_client_error_type status, void * /*cookie*/) {
  LOGD(
      "QmiLogCB: hdl %p msgType %u, msgId %u (0x%x), txnid %u (0x%x), msgLen "
      "%u, status %u",
      handle, msgType, msgId, msgId, txnId, txnId, rawMsgLen, status);
}

void QmiCallbacks::onError(qmi_client_type handle, qmi_client_error_type error,
                           void * /*err_cb_data*/) {
  LOGE("QmiErrCB: hdl %p err %u", handle, error);
}

void QmiCallbacks::onResponse(qmi_client_type /* handle */, unsigned int msgId,
                              void *responseData, unsigned int responseLen,
                              void * /* onResponseData */,
                              qmi_client_error_type err) {
  auto *response = static_cast<sns_client_resp_msg_v01 *>(responseData);
  if (response == nullptr) {
    LOGE("Got null response for msg ID %s, responseLen: %u, err: %d", msgId,
         responseLen, err);
    return;
  }
  int result = response->result_valid != 0 ? response->result : -1;
  uint64_t clientId = response->client_id_valid != 0 ? response->client_id : 0;
  LOGD("Got response for msg ID %u, responseLen: %u, clientId %" PRId64
       " result: %d err: %d",
       msgId, responseLen, clientId, result, err);
  qmi_response_type_v01 &resp = response->resp;
  LOGD("Embedded response qmi_result: %u, qmi_err: %u", resp.result,
       resp.error);
  delete[] response;
}

void QmiCallbacks::onIndication(qmi_client_type /*handle*/, unsigned int msgId,
                                void *indBuf, unsigned int indBufLen,
                                void *data) {
  auto *qmiClientInstance = static_cast<QmiClientBase *>(data);
  size_t ind_len = sizeof(sns_client_report_ind_msg_v01);
  sns_client_report_ind_msg_v01 ind;

  LOGD("Received Indication; len %i", indBufLen);

  // Extract the Protocol Buffer encoded message from the outer QMI/IDL
  // message
  int32_t err = qmi_idl_message_decode(SNS_CLIENT_SVC_get_service_object_v01(),
                                       QMI_IDL_INDICATION, msgId, indBuf,
                                       indBufLen, &ind, ind_len);
  if (err != QMI_IDL_LIB_NO_ERR) {
    LOGE("QMI decode error %i", err);
  } else {
    sns_client_event_msg event = sns_client_event_msg_init_default;

    // Decode just the sns_client_event_msg in order to get the SUID
    pb_istream_t stream = pb_istream_from_buffer(ind.payload, ind.payload_len);
    if (pb_decode(&stream, sns_client_event_msg_fields, &event)) {
      DecodeCbData decodeCbData = {
          .qmiClientInstance = qmiClientInstance,
          .suid = event.suid,
      };
      qmiClientInstance->handleEvent(ind.payload, ind.payload_len,
                                     &decodeCbData);
    } else {
      LOGE("Error decoding Event Message: %s", PB_GET_ERROR(&stream));
    }
  }
}

bool QmiCallbacks::encodePayload(pb_ostream_t *stream, const pb_field_t *field,
                                 void *const *arg) {
  auto *info = static_cast<QmiClientBase::SnsArg *>(*arg);
  return pb_encode_tag_for_field(stream, field) &&
         pb_encode_string(stream, static_cast<const pb_byte_t *>(info->buf),
                          info->bufLen);
}

bool QmiCallbacks::decodePayload(pb_istream_t *stream,
                                 const pb_field_t * /*field*/, void **arg) {
  auto *data = static_cast<QmiClientBase::SnsArg *>(*arg);

  data->bufLen = stream->bytes_left;
  data->buf = stream->state;
  return pb_read(stream, nullptr, stream->bytes_left);
}

bool QmiCallbacks::decodeFloatData(pb_istream_t *stream,
                                   const pb_field_t * /*field*/, void **arg) {
  auto *data = static_cast<PbFloatArg *>(*arg);
  size_t arraySize = sizeof(data->val) / sizeof(data->val[0]);
  float value;
  float *fltPtr = &value;
  if (data->index >= arraySize) {
    LOGE("Float array length exceeds %zu", arraySize);
  } else {
    // Decode to the provided array only if it doesn't go out of bound.
    fltPtr = &(data->val[data->index]);
  }
  // Increment index whether it's gone out of bounds or not.
  (data->index)++;

  bool success = pb_decode_fixed32(stream, fltPtr);
  if (!success) {
    LOGE("Error decoding stream: %s", PB_GET_ERROR(stream));
  }
  return success;
}

bool QmiCallbacks::decodeAttr(pb_istream_t *stream, const pb_field_t *field,
                              void **arg) {
  auto *decodeCbData = static_cast<DecodeCbData *>(*arg);
  auto *qmiClientInstance = decodeCbData->qmiClientInstance;
  sns_std_attr attribute = sns_std_attr_init_default;
  pb_istream_t streamCopy = *stream;

  if (!pb_decode(&streamCopy, sns_std_attr_fields, &attribute)) {
    LOGE("event: %s", PB_GET_ERROR(stream));
    return false;
  }
  return qmiClientInstance->handleAttribute(attribute.attr_id, stream, field,
                                            arg);
}

bool QmiCallbacks::decodeAttrValue(pb_istream_t *stream,
                                   const pb_field_t * /*field*/, void **arg) {
  sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
  QmiClientBase::SnsArg strData =
      (QmiClientBase::SnsArg){.buf = nullptr, .bufLen = 0};

  value.str.funcs.decode = QmiCallbacks::decodePayload;
  value.str.arg = &strData;

  value.subtype.values.funcs.decode = QmiCallbacks::decodeAttrValue;

  if (!pb_decode(stream, sns_std_attr_value_data_fields, &value)) {
    LOGE("Error decoding attribute: %s", PB_GET_ERROR(stream));
    return false;
  }

  if (value.has_flt)
    LOGD("Attribute float: %f", value.flt);
  else if (value.has_sint) {
    if (*arg != nullptr) {
      auto *val = static_cast<int64_t *>(*arg);
      *val = value.sint;
    }
    LOGD("Attribute int: %" PRIi64, value.sint);
  } else if (value.has_boolean)
    LOGD("Attribute boolean: %i", value.boolean);
  else if (strData.buf != nullptr)
    LOGD("Attribute string: %s", static_cast<const char *>(strData.buf));
  else if (value.has_subtype)
    LOGD("Attribute nested");
  else
    LOGE("Unknown attribute type");

  return true;
}

uint32_t QmiCallbacks::getMsgId(pb_istream_t *stream) {
  sns_client_event_msg_sns_client_event event =
      sns_client_event_msg_sns_client_event_init_default;

  if (!pb_decode(stream, sns_client_event_msg_sns_client_event_fields,
                 &event)) {
    LOGE("event: %s", PB_GET_ERROR(stream));
  } else {
    LOGI("Decoding event with message ID %i, timestamp %" PRIu64, event.msg_id,
         event.timestamp);
    return event.msg_id;
  }

  return 0;
}

bool QmiCallbacks::decodeEvents(pb_istream_t *stream,
                                const pb_field_t * /*field*/, void **arg) {
  bool success = true;
  auto *decodeCbData = static_cast<DecodeCbData *>(*arg);
  auto *qmiClientInstance = decodeCbData->qmiClientInstance;
  sns_client_event_msg_sns_client_event event =
      sns_client_event_msg_sns_client_event_init_default;
  pb_istream_t streamCopy = *stream;
  decodeCbData->msgId = getMsgId(&streamCopy);

  auto &suid = decodeCbData->suid;
  sns_std_suid lookupSuid = sns_suid_sensor_init_default;
  if (suid.suid_high == lookupSuid.suid_high &&
      suid.suid_low == lookupSuid.suid_low) {
    event.payload.funcs.decode =
        reinterpret_cast<PbDecodeCallback>(QmiCallbacks::decodeSuidEvent);
  } else {
    qmiClientInstance->setSuid(suid);
    event.payload.funcs.decode = reinterpret_cast<PbDecodeCallback>(
        QmiCallbacks::decodeGenericSuidEvent);
  }
  event.payload.arg = decodeCbData;

  if (!pb_decode(stream, sns_client_event_msg_sns_client_event_fields,
                 &event)) {
    LOGE("Error decoding Event: %s", PB_GET_ERROR(stream));
    success = false;
  }
  return success;
}

bool QmiCallbacks::decodeAttrEvent(pb_istream_t *stream,
                                   const pb_field_t * /*field*/, void **arg) {
  sns_std_attr_event event = sns_std_attr_event_init_default;
  auto *decodeCbData = static_cast<DecodeCbData *>(*arg);

  event.attributes.funcs.decode = QmiCallbacks::decodeAttr;
  event.attributes.arg = decodeCbData;

  if (!pb_decode(stream, sns_std_attr_event_fields, &event)) {
    LOGE("Error decoding Attr Event: %s", PB_GET_ERROR(stream));
    return false;
  }

  return true;
}

bool QmiCallbacks::decodeSuid(pb_istream_t *stream,
                              const pb_field_t * /*field*/, void **arg) {
  sns_std_suid uid;
  auto *decodeCbData = static_cast<DecodeCbData *>(*arg);

  if (!pb_decode(stream, sns_std_suid_fields, &uid)) {
    LOGE("Error decoding SUID: %s", PB_GET_ERROR(stream));
    return false;
  }

  LOGD("send attr req after receiving SUID Event with SUID %" PRIx64
       " %" PRIx64,
       uid.suid_low, uid.suid_high);

  decodeCbData->qmiClientInstance->sendAttrReq(&uid);

  return true;
}

bool QmiCallbacks::decodeSuidEvent(pb_istream_t *stream,
                                   const pb_field_t * /*field*/, void **arg) {
  bool success = true;
  sns_suid_event event;
  QmiClientBase::SnsArg data;
  auto *cbdata = static_cast<DecodeCbData *>(*arg);

  event.suid.funcs.decode = QmiCallbacks::decodeSuid;
  event.suid.arg = cbdata;
  event.data_type.funcs.decode = QmiCallbacks::decodePayload;
  event.data_type.arg = &data;

  if (!pb_decode(stream, sns_suid_event_fields, &event)) {
    LOGE("Error decoding SUID Event: %s", PB_GET_ERROR(stream));
    success = false;
  }

  return success;
}

bool QmiCallbacks::decodeGenericSuidEvent(pb_istream_t *stream,
                                          const pb_field_t *field, void **arg) {
  auto *decodeCbData = static_cast<QmiCallbacks::DecodeCbData *>(*arg);
  auto *qmiClientInstance = decodeCbData->qmiClientInstance;
  auto msgId = decodeCbData->msgId;

  LOGD("Begin decoding generic SUID event %u", msgId);
  return qmiClientInstance->handleMessageStream(msgId, stream, field, arg);
}

}  // namespace chre
}  // namespace android
