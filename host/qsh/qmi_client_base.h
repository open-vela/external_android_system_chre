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

#ifndef CHRE_QMI_CLIENT_BASE_H_
#define CHRE_QMI_CLIENT_BASE_H_

#include <array>
#include <cassert>
#include <cinttypes>
#include <cstring>
#include <vector>

#include "chre_host/log.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include "qmi_client.h"
#include "sns_client.pb.h"
#include "sns_client_api_v01.h"
#include "sns_std_sensor.pb.h"
#include "sns_std_type.pb.h"
#include "sns_suid.pb.h"

namespace android {
namespace chre {

class QmiClientBase {
 public:
  /**
   * A generic buffer-length data structure used by the SNS encoding callbacks.
   */
  struct SnsArg {
    const void *buf;
    size_t bufLen;
  };

  QmiClientBase(const char *sensorType) : mSensorType(sensorType) {}
  virtual ~QmiClientBase() {
    disconnect();
  }

  /**
   * Create a QMI connection to Sensors.
   *
   * @param handle QMI Handle populated on a successful connection.
   * @param timeout Time-out in milliseconds. 0 = no timeout
   *
   * @return True on success; false otherwise
   */
  bool connect();

  /**
   * Disconnect from the SNS client service.
   */
  void disconnect();

  /**
   * Enables the client by initiating a connection to the SNS client service.
   *
   * @return true if the service was discovered and successfully connected to,
   *         false otherwise.
   */
  virtual bool enable() = 0;

  /**
   * Request constant rate data from a sensor.
   *
   * @return true on successfully requesting a stream, false otherwise.
   */
  virtual bool enableStream() = 0;

  /**
   * Disconnect the client from the SNS client service.
   */
  virtual void disable() = 0;

  /**
   * Process the data in a PB input data stream for the received message ID.
   *
   * @param msgId Message ID.
   * @param stream Input PB data stream.
   * @param field Message field being processed.
   * @param arg Opaque pointer to a structure that holds data necessary for
   *        downstream decoding.
   * @return true if the stream was successfully decoded, false otherwise.
   */
  virtual bool handleMessageStream(uint32_t msgId, pb_istream_t *stream,
                                   const pb_field_t *field, void **arg) = 0;

  /**
   * Process the data in a PB input stream for the received attribute ID.
   *
   * @param attributeId Attribute ID.
   * @param stream Input PB data stream.
   * @param field Attribute field being processed.
   * @param arg Opaque pointer to a structure that holds data necessary for
   *        downstream decoding.
   * @return true if the stream was successfully decoded, false otherwise.
   */
  virtual bool handleAttribute(uint32_t attributeId, pb_istream_t *stream,
                               const pb_field_t *field, void **arg) = 0;

  /**
   * Send a request to the specified service. Does not specify any batching
   * options.
   *
   * @param payload Encoded Sensor-specific request message
   * @param suid Destination SUID
   * @param msgId Sensor-specific message ID
   */
  bool sendReq(SnsArg *payload, sns_std_suid suid, uint32_t msgId);

  /**
   * Send a request to the SUID Lookup Sensor for the complete list of SUIDs
   *
   * @return true if request was sent successfully, false otherwise
   */
  bool sendSuidReq();

  /**
   * Send an attribute request for the specified SUID.
   *
   * @param suid Destination SUID
   */
  bool sendAttrReq(sns_std_suid *suid);

  /**
   * Handle a received event message from the Sensor Service.
   *
   * @param eventMsg PB-encoded message of type sns_client_event_msg
   * @param eventMsgLen Length (in bytes) of the encoded event_msg
   * @param data Pointer to a DecodeCbData structure, holding the necessary
   *        metadata required to decode a message.
   */
  bool handleEvent(void const *eventMsg, size_t eventMsgLen, void *data);

  void setSuid(sns_std_suid suid) {
    mSuid = suid;
  }

  const sns_std_suid &getSuid() {
    return mSuid;
  }

 protected:
  static constexpr uint32_t kQmiTimeoutMs = 500;
  const char *mSensorType;
  sns_std_suid mLookupSuid = sns_suid_sensor_init_default;
  sns_std_suid mSuid;
  qmi_client_type mQmiHandle = nullptr;

  bool isConnected() {
    return mQmiHandle != nullptr;
  }

  /**
   * Check whether the Sensor service is available.
   *
   * @return true if the service is available, false upon timeout
   */
  bool waitForService();

  /**
   * Create an encoded Attribute request message.
   *
   * @param encodedReq Vector whose data is populated with the encoded request
   *        as bytes.
   *
   * @return true if encoding succeeded, false otherwise.
   */
  bool getEncodedAttrReq(std::vector<pb_byte_t> &encodedReq);

  /**
   * Create a SUID lookup request for the specified data type.
   *
   * @param encodedReq Vector whose data is populated with the encoded request
   *        as bytes.
   *
   * @return true if encoding succeeded, false otherwise.
   */
  bool getEncodedSuidReq(std::vector<pb_byte_t> &encodedReq);

  /**
   * Send a QMI request message.
   *
   * @param reqMsg Completed request message to be sent
   *
   * @return true on success, false otherwise
   */
  bool sendQmiReq(sns_client_req_msg_v01 *reqMsg);
};

}  // namespace chre
}  // namespace android

#endif  // CHRE_QMI_CLIENT_BASE_H_