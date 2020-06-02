/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "chpp/clients.h"
#include "chpp/clients/wwan.h"

/************************************************
 *  Prototypes
 ***********************************************/

/************************************************
 *  Private Functions
 ***********************************************/

/************************************************
 *  Public Functions
 ***********************************************/

void chppRegisterCommonClients(struct ChppAppState *context) {
  UNUSED_VAR(context);
#ifdef CHPP_CLIENT_ENABLED_WWAN
  chppRegisterWwanClient(context);
#endif

#ifdef CHPP_CLIENT_ENABLED_WLAN
  chppRegisterWlanClient(context);
#endif

#ifdef CHPP_CLIENT_ENABLED_GNSS
  chppRegisterGnssClient(context);
#endif
}

void chppRegisterClient(struct ChppAppState *appContext, void *clientContext,
                        const struct ChppClient *newClient) {
  CHPP_NOT_NULL(newClient);
  UNUSED_VAR(appContext);
  UNUSED_VAR(clientContext);
  UNUSED_VAR(newClient);

  // TODO
}

struct ChppAppHeader *chppAllocClientRequest(
    struct ChppClientState *clientState, size_t len) {
  CHPP_ASSERT(len >= sizeof(struct ChppAppHeader));

  struct ChppAppHeader *result = chppMalloc(len);
  if (result != NULL) {
    result->handle = clientState->handle;
    result->type = CHPP_MESSAGE_TYPE_CLIENT_REQUEST;
    result->transaction = clientState->transaction;
    result->reserved = CHPP_RESERVED;

    clientState->transaction++;
  }
  return result;
}

struct ChppAppHeader *chppAllocClientRequestCommand(
    struct ChppClientState *clientState, uint16_t command) {
  struct ChppAppHeader *result =
      chppAllocClientRequest(clientState, sizeof(struct ChppAppHeader));

  if (result != NULL) {
    result->command = command;
  }
  return result;
}

void chppClientTimestampRequest(struct ChppRequestResponseState *rRState,
                                struct ChppAppHeader *requestHeader) {
  if (rRState->responseTime == CHPP_TIME_NONE &&
      rRState->requestTime != CHPP_TIME_NONE) {
    LOGE("Sending duplicate request with transaction ID = %" PRIu64
         " while prior request with transaction ID = %" PRIu8
         " was outstanding from t = %" PRIu8,
         rRState->requestTime, requestHeader->transaction,
         rRState->transaction);
  }
  rRState->requestTime = chppGetCurrentTime();
  rRState->responseTime = CHPP_TIME_NONE;
  rRState->transaction = requestHeader->transaction;
}

bool chppClientTimestampResponse(struct ChppRequestResponseState *rRState,
                                 struct ChppAppHeader *responseHeader) {
  uint64_t previousResponseTime = rRState->responseTime;
  rRState->responseTime = chppGetCurrentTime();

  if (rRState->requestTime == CHPP_TIME_NONE) {
    LOGE("Received response at t = %" PRIu64
         " with no prior outstanding request",
         rRState->responseTime);

  } else if (previousResponseTime != CHPP_TIME_NONE) {
    rRState->responseTime = chppGetCurrentTime();
    LOGW("Received additional response at t = %" PRIu64
         " for request at t = %" PRIu64 " (RTT = %" PRIu64 ")",
         rRState->responseTime, rRState->responseTime,
         rRState->responseTime - rRState->requestTime);

  } else {
    LOGI("Received initial response at t = %" PRIu64
         " for request at t = %" PRIu64 " (RTT = %" PRIu64 ")",
         rRState->responseTime, rRState->responseTime,
         rRState->responseTime - rRState->requestTime);
  }

  // TODO: Also check for timeout
  if (responseHeader->transaction != rRState->transaction) {
    LOGE("Received a response at t = %" PRIu64 " with transaction ID = %" PRIu8
         " for a different outstanding request with transaction ID = %" PRIu8,
         rRState->responseTime, responseHeader->transaction,
         rRState->transaction);
    return false;
  } else {
    return true;
  }
}

bool chppSendTimestampedRequestOrFail(struct ChppClientState *clientState,
                                      struct ChppRequestResponseState *rRState,
                                      void *buf, size_t len) {
  CHPP_ASSERT(len >= sizeof(struct ChppAppHeader));
  chppClientTimestampRequest(rRState, buf);
  return chppEnqueueTxDatagramOrFail(clientState->appContext->transportContext,
                                     buf, len);
}