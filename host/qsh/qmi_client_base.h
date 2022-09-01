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
#include <queue>
#include <string>
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

  /**
   * A structure that holds information (attributes) published by a sensor.
   */
  struct SuidAttributes {
    std::string name;
    std::string vendor;
    std::string type;
    std::string api;
    bool isAvailable;
    uint32_t version;
    uint64_t nanoappId;
    sns_std_sensor_stream_type streamType;

    void reset() {
      name.clear();
      vendor.clear();
      type.clear();
      api.clear();
      isAvailable = false;
    }
  };

  using SuidAttrCallback =
      std::function<void(const std::vector<SuidAttributes> &, void *)>;

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
   * Process the data in a PB input data stream for the received message ID.
   * This function is expected to handle message IDs exposed by the QMI library
   * framework (eg: SNS_STD_*). Messages that are specific to entities owned by
   * the CHRE are passed on to the handleMessageStreamExtended function.
   *
   * @param msgId Message ID.
   * @param stream Input PB data stream.
   * @param field Message field being processed.
   * @param arg Opaque pointer to a structure that holds data necessary for
   *        downstream decoding.
   * @return true if the stream was successfully decoded, false otherwise.
   */
  bool handleMessageStream(uint32_t msgId, pb_istream_t *stream,
                           const pb_field_t *field, void **arg);

  /**
   * Process msg IDs that are specific to a CHRE owned entity (eg: the QSH
   * nanoapp client, Shim, etc.). Classes that inherit from this one are
   * expected to provide a specific implementation - the default version here
   * logs an error message and always returns false if invoked. All parameters
   * in the default implementation are unused.
   *
   * @return false when invoked after logging an error message - the
   *         inheriting class is expected to override this function if
   *         instance specific message streams are expected to be handled.
   */
  virtual bool handleMessageStreamExtended(uint32_t msgId, pb_istream_t *stream,
                                           const pb_field_t *field, void **arg);

  /**
   * A set of overloaded methods that handle various types of attributes.
   *
   * @param attributeId Published attribute identifier.
   * @param value Published attribute's value.
   * @return true if the attribute ID was valid and the attribute value was
   *         processed, false otherwise.
   */
  virtual bool handleAttribute(uint32_t attributeId, const char *value);
  virtual bool handleAttribute(uint32_t attributeId, float value);
  virtual bool handleAttribute(uint32_t attributeId, bool value);
  virtual bool handleAttribute(uint32_t attributeId, uint32_t value);
  virtual bool handleAttribute(uint32_t attributeId, int64_t value);

  /**
   * Invoked when all attributes for an SUID attribute request have been
   * accumulated.
   */
  virtual void onAttrEventDecodeComplete() = 0;

  /**
   * Invoked when all the received SUIDs on an SUID lookup request have been
   * decoded.
   */
  virtual void onSuidDecodeEventComplete() = 0;

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
   * @param callback A function that is invoked when all the SUIDs from an SUID
   *        lookup have been accumulated, attribute requests have been sent for
   *        each of these SUIDs, and attribute responses collected.
   * @param callbackData Data that the calling function desires to be passed to
   *        the callback when it is invoked.
   * @return true if request was sent successfully, false otherwise
   */
  bool sendSuidReq(SuidAttrCallback callback, void *callbackData);

  /**
   * Send an attribute request for the specified SUID.
   *
   * @param suid Destination SUID
   */
  bool sendAttrReq(sns_std_suid &suid);

  /**
   * Handle a received event message from the Sensor Service.
   *
   * @param eventMsg PB-encoded message of type sns_client_event_msg
   * @param eventMsgLen Length (in bytes) of the encoded event_msg
   * @param data Pointer to a DecodeCbData structure, holding the necessary
   *        metadata required to decode a message.
   */
  bool handleEvent(void const *eventMsg, size_t eventMsgLen, void *data);

  /**
   * Add a decoded SUID to a queue to later request attributes on.
   * @param suid Decoded SUID.
   */
  inline void addReceivedSuid(const sns_std_suid &suid) {
    mReceivedSuids.emplace(suid);
  }

  inline void setCurrentAttributeNanoappId(const sns_std_suid &suid) {
    mCurrentSuidAttribute.nanoappId = getNanoappId(suid);
  }

 protected:
  /**
   * Structure that holds the callback function to be invoked when all
   * attributes for an SUID attribute request have been collected, and the
   * caller specified callback data to be passed to the callback.
   */
  struct SuidAttrCallbackData {
    SuidAttrCallback callback;
    void *data;

    /**
     * Helper method to invoke the callback once, and then reset the structure
     * members
     *
     * @param attributes List of attributes for all the SUIDs returned from an
     *        SUID lookup.
     */
    void callAndReset(const std::vector<SuidAttributes> &attributes) {
      if (callback != nullptr) {
        callback(attributes, data);
      }
      callback = nullptr;
      data = nullptr;
    }
  };

  //! Default timeout for QMI calls.
  static constexpr uint32_t kQmiTimeoutMs = 500;

  //! A NULL-terminated string indicating the sensor type being looked up.
  const char *mSensorType;
  sns_std_suid mLookupSuid = sns_suid_sensor_init_default;
  qmi_client_type mQmiHandle = nullptr;

  //! Indicates if the current SUID request has completed. Note that only one
  //! SUID request can be outstanding at any given time.
  bool mSuidRequestCompleted = true;

  //! On performing an SUID lookup, a list of SUIDs are returned in response.
  //! This variable indicates the number of SUID attr requests pending.
  uint32_t mNumPendingSuidAttrRequests;

  //! List of SUIDs received from an SUID lookup request.
  std::queue<sns_std_suid> mReceivedSuids;

  //! List of SUID attributes accumulated from SUID attribute requests.
  std::vector<SuidAttributes> mSuidAttributesList;

  //! The current set of SUID attributes being decoded.
  SuidAttributes mCurrentSuidAttribute;

  //! Caller specified callback function to be invoked when all SUID attributes
  //! have been accumulated.
  SuidAttrCallbackData mSuidAttrCallbackData;

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
   * Send a client QMI request message.
   *
   * @param reqMsg Completed request message to be sent
   *
   * @return true on success, false otherwise
   */
  bool sendSnsClientReq(sns_client_req_msg_v01 &reqMsg);

  /**
   * Function that's called when the current set of attributes have been
   * accumulated, and need to be stored/updated before decoding the next
   * set of published attributes.
   */
  void updateAttributes();

  /**
   * Constructs and returns the nanoappID from the SUID.
   *
   * @param suid The SUID containing the embedded SUID
   * @return The nanoappID extracted from the SUID.
   */
  uint64_t getNanoappId(const sns_std_suid &suid);
};

}  // namespace chre
}  // namespace android

#endif  // CHRE_QMI_CLIENT_BASE_H_