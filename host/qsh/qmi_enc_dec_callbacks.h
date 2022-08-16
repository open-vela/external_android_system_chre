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

#ifndef CHRE_QMI_ENC_DEC_CALLBACKS_H_
#define CHRE_QMI_ENC_DEC_CALLBACKS_H_

#include "qmi_client_base.h"

namespace android {
namespace chre {

/**
 * A collection of callbacks that are used for the QMI messages.
 */
class QmiCallbacks {
 public:
  using PbDecodeCallback = bool (*)(pb_istream_t *stream,
                                    const pb_field_t *field, void **arg);
  /**
   * Structure to be used with decodeFloatData
   */
  struct PbFloatArg {
    // Decode a vectorized 3x3 array.
    static constexpr size_t kPbFloatArgValArrayLen = 9;
    size_t index;
    float val[kPbFloatArgValArrayLen];
  };
  /**
   * Data structure that is used to hold data that is required by the various
   * layers of the nanopb decoding callbacks. An instance of this structure
   * originates in the 'handleEvent' function, and is passed all the way down
   * until the nanopb message has been completely decoded, filling and/or
   * replacing member values as needed.
   */
  struct DecodeCbData {
    QmiClientBase *qmiClientInstance;
    sns_std_suid suid;     // SUID being decoded
    uint32_t msgId;        // Current message ID that's being handled.
    uint32_t attributeId;  // Current Attribute being decoded.
  };

  /**
   * QMI transaction log callback, invoked when there's any kind of message
   * flowing in either direction over a client connection.
   * FIXME: Make this optional or verbose to prevent spam?
   */
  static void onLog(qmi_client_type handle,
                    qmi_idl_type_of_message_type msgType, unsigned int msgId,
                    unsigned int txnId, const void * /*rawMsg*/,
                    unsigned int rawMsgLen, qmi_client_error_type status,
                    void *cookie);

  /**
   * QMI error callback, invoked to notify us if the QMI connection was lost.
   * was lost.
   * FIXME: reconnect and resend requests.
   */
  static void onError(qmi_client_type handle, qmi_client_error_type error,
                      void *err_cb_data);

  /**
   * QMI response callback, invoked synchronously in response to a sensor
   * request. Note that all this invocation indicates is that whether a
   * request was valid/not malformed, and if it was accepted for further
   * processing. The actual result of the request is delivered asynchronously
   * via the onIndication callback.
   */
  static void onSnsClientResponse(qmi_client_type handle, unsigned int msgId,
                                  void *responseData, unsigned int responseLen,
                                  void *onResponseData,
                                  qmi_client_error_type err);

  /**
   * QMI indication callback, invoked when the result of a successful request
   * is ready.
   */
  static void onIndication(qmi_client_type handle, unsigned int msgId,
                           void *indBuf, unsigned int indBufLen, void *data);

  /**
   * Callback that is invoked by the nanopb library to encode a byte array
   * into a message field in a data stream.
   */
  static bool encodePayload(pb_ostream_t *stream, const pb_field_t *field,
                            void *const *arg);

  /**
   * Decode an array of bytes from a field. This function is intended to be
   * used as a callback function during message decode.
   */
  static bool decodePayload(pb_istream_t *stream, const pb_field_t *field,
                            void **arg);

  /**
   * Decode an array of float values, such as the array within the sensor
   * sample data, while ensuring data doesn't go out of bounds.
   */
  static bool decodeFloatData(pb_istream_t *stream, const pb_field_t *field,
                              void **arg);

  /**
   * Decode a single attribute from the attributes array.
   */
  static bool decodeAttr(pb_istream_t *stream, const pb_field_t *field,
                         void **arg);

  /**
   * Decode an attribute value.
   * Once we find the attribute value we have been looking for (for example,
   * to differentiate two sensors with data type "accel"), we send an enable
   * request.
   */
  static bool decodeAttrValue(pb_istream_t *stream, const pb_field_t *field,
                              void **arg);

  static uint32_t getMsgId(pb_istream_t *stream);

  /*
   * Decode an element of sns_client_event_msg::events.  This function will be
   * called by nanopb once per each element in the array.
   *
   * @param arg Sensor-specific decode function
   */
  static bool decodeEvents(pb_istream_t *stream, const pb_field_t *field,
                           void **arg);

  /**
   * Decode the payload of the event message, i.e. the Attribute Event
   */
  static bool decodeAttrEvent(pb_istream_t *stream, const pb_field_t *field,
                              void **arg);

  /**
   * Each SUID event contains an array of SUIDs.  This function will be called
   * once per each SUID in the array.
   *
   * At this time we will send an attributes request to the SUID.
   */
  static bool decodeSuid(pb_istream_t *stream, const pb_field_t *field,
                         void **arg);

  /**
   * Decode the payload of the event message, i.e. the SUID Event
   */
  static bool decodeSuidEvent(pb_istream_t *stream, const pb_field_t *field,
                              void **arg);

  /**
   * Decode an SUID specific events.
   */
  static bool decodeMessageStream(pb_istream_t *stream, const pb_field_t *field,
                                  void **arg);
};

}  // namespace chre
}  // namespace android

#endif  // CHRE_QMI_ENC_DEC_CALLBACKS_H_