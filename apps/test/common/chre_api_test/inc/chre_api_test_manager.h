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

#ifndef CHRE_API_TEST_MANAGER_H_
#define CHRE_API_TEST_MANAGER_H_

#include <cinttypes>
#include <cstdint>

#include "chre/re.h"
#include "chre/util/pigweed/rpc_server.h"
#include "chre/util/singleton.h"
#include "chre_api/chre.h"
#include "rpc/chre_api_test.rpc.pb.h"

class ChreApiTestService final
    : public chre::rpc::pw_rpc::nanopb::ChreApiTestService::Service<
          ChreApiTestService> {
 public:
  /**
   * Returns the BLE capabilities.
   *
   * @param request           the request (Void)
   * @param response          the response (Capabilities)
   * @return                  status
   */
  pw::Status ChreBleGetCapabilities(const chre_rpc_Void &request,
                                    chre_rpc_Capabilities &response);

  /**
   * Returns the BLE filter capabilities.
   *
   * @param request           the request (Void)
   * @param response          the response (Capabilities)
   * @return                  status
   */
  pw::Status ChreBleGetFilterCapabilities(const chre_rpc_Void &request,
                                          chre_rpc_Capabilities &response);

  /**
   * Finds the default sensor and returns the handle in the output.
   *
   * @param request         the request (ChreSensorFindDefaultInput)
   * @param response        the response (ChreSensorFindDefaultOutput)
   * @return                status
   */
  pw::Status ChreSensorFindDefault(
      const chre_rpc_ChreSensorFindDefaultInput &request,
      chre_rpc_ChreSensorFindDefaultOutput &response);

  /**
   * Gets the sensor information.
   *
   * @param request         the request (ChreHandleInput)
   * @param response        the response (ChreGetSensorInfoOutput)
   * @return                status
   */
  pw::Status ChreGetSensorInfo(const chre_rpc_ChreHandleInput &request,
                               chre_rpc_ChreGetSensorInfoOutput &response);

  /**
   * Gets the sensor sampling status for a given sensor.
   *
   * @param request         the request (ChreHandleInput)
   * @param response        the response (ChreGetSensorSamplingStatusOutput)
   * @return                status
   */
  pw::Status ChreGetSensorSamplingStatus(
      const chre_rpc_ChreHandleInput &request,
      chre_rpc_ChreGetSensorSamplingStatusOutput &response);

  /**
   * Configures the mode for a sensor.
   *
   * @param request         the request (ChreSensorConfigureModeOnlyInput)
   * @param response        the response (Status)
   * @return                status
   */
  pw::Status ChreSensorConfigureModeOnly(
      const chre_rpc_ChreSensorConfigureModeOnlyInput &request,
      chre_rpc_Status &response);

  /**
   * Gets the audio source information.
   *
   * @param request         the request (ChreHandleInput)
   * @param response        the response (ChreAudioGetSourceOutput)
   * @return                status
   */
  pw::Status ChreAudioGetSource(const chre_rpc_ChreHandleInput &request,
                                chre_rpc_ChreAudioGetSourceOutput &response);

 private:
  /**
   * Max size of the name string
   */
  static const uint32_t kMaxNameStringSize = 100;

  /**
   * Copies a string from source to destination up to the length of the source
   * or the max value. Pads with null characters.
   *
   * @param destination         the destination string
   * @param source              the source string
   * @param maxChars            the maximum number of chars
   */
  void copyString(char *destination, const char *source, size_t maxChars);
};

/**
 * Handles RPC requests for the CHRE API Test nanoapp.
 */
class ChreApiTestManager {
 public:
  /**
   * Allows the manager to do any init necessary as part of nanoappStart.
   */
  bool start();

  /**
   * Handle a CHRE event.
   *
   * @param senderInstanceId    the instand ID that sent the event.
   * @param eventType           the type of the event.
   * @param eventData           the data for the event.
   */
  void handleEvent(uint32_t senderInstanceId, uint16_t eventType,
                   const void *eventData);

  /**
   * Allows the manager to do any cleanup necessary as part of nanoappEnd.
   */
  void end();

  /**
   * Sets the permission for the next server message.
   *
   * @params permission Bitmasked CHRE_MESSAGE_PERMISSION_.
   */
  void setPermissionForNextMessage(uint32_t permission);

 private:
  // RPC server.
  chre::RpcServer mServer;

  // pw_rpc service used to process the RPCs.
  ChreApiTestService mChreApiTestService;
};

typedef chre::Singleton<ChreApiTestManager> ChreApiTestManagerSingleton;

#endif  // CHRE_API_TEST_MANAGER_H_
