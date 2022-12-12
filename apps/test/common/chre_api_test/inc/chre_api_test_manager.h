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

#include <chre.h>
#include <cinttypes>
#include <cstdint>

#include "chre/re.h"
#include "chre/util/pigweed/rpc_server.h"
#include "chre/util/singleton.h"
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
};

/**
 * Handles RPC requests for the CHRE API Test nanoapp
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
  // RPC server
  chre::RpcServer mServer;

  // pw_rpc service used to process the RPCs.
  ChreApiTestService mChreApiTestService;
};

typedef chre::Singleton<ChreApiTestManager> ChreApiTestManagerSingleton;

#endif  // CHRE_API_TEST_MANAGER_H_
