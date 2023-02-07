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

#include "chpp/link.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "chpp/log.h"
#include "chpp/macros.h"
#include "chpp/platform/platform_link.h"
#include "chpp/transport.h"

// The set of signals to use for the linkSendThread.
#define SIGNAL_EXIT UINT32_C(1 << 0)
#define SIGNAL_DATA UINT32_C(1 << 1)

/**
 * This thread is used to "send" TX data to the remote endpoint. The remote
 * endpoint is defined by the ChppTransportState pointer, so a loopback link
 * with a single CHPP instance can be supported.
 */
static void *linkSendThread(void *linkContext) {
  struct ChppLinuxLinkState *context =
      (struct ChppLinuxLinkState *)(linkContext);
  while (true) {
    uint32_t signal = chppNotifierTimedWait(&context->notifier, CHPP_TIME_MAX);

    if (signal & SIGNAL_EXIT) {
      break;
    }
    if (signal & SIGNAL_DATA) {
      enum ChppLinkErrorCode error;

      chppMutexLock(&context->mutex);

      if (context->remoteTransportContext == NULL) {
        CHPP_LOGW("remoteTransportContext is NULL");
        error = CHPP_LINK_ERROR_NONE_SENT;

      } else if (!context->linkEstablished) {
        CHPP_LOGE("No (fake) link");
        error = CHPP_LINK_ERROR_NO_LINK;

      } else if (!chppRxDataCb(context->remoteTransportContext, context->buf,
                               context->bufLen)) {
        CHPP_LOGW("chppRxDataCb return state!=preamble (packet incomplete)");
        error = CHPP_LINK_ERROR_NONE_SENT;

      } else {
        error = CHPP_LINK_ERROR_NONE_SENT;
      }

      context->bufLen = 0;
      chppLinkSendDoneCb(context->transportContext, error);

      chppMutexUnlock(&context->mutex);
    }
  }

  return NULL;
}

static void init(void *linkContext,
                 struct ChppTransportState *transportContext) {
  struct ChppLinuxLinkState *context =
      (struct ChppLinuxLinkState *)(linkContext);
  context->bufLen = 0;
  context->transportContext = transportContext;
  chppMutexInit(&context->mutex);
  chppNotifierInit(&context->notifier);
  pthread_create(&context->linkSendThread, NULL /* attr */, linkSendThread,
                 context);
  if (context->linkThreadName != NULL) {
    pthread_setname_np(context->linkSendThread, context->linkThreadName);
  }
}

static void deinit(void *linkContext) {
  struct ChppLinuxLinkState *context =
      (struct ChppLinuxLinkState *)(linkContext);
  context->bufLen = 0;
  chppNotifierSignal(&context->notifier, SIGNAL_EXIT);
  pthread_join(context->linkSendThread, NULL /* retval */);
  chppNotifierDeinit(&context->notifier);
  chppMutexDeinit(&context->mutex);
}

static enum ChppLinkErrorCode send(void *linkContext, size_t len) {
  struct ChppLinuxLinkState *context =
      (struct ChppLinuxLinkState *)(linkContext);
  bool success = false;
  chppMutexLock(&context->mutex);
  if (context->bufLen != 0) {
    CHPP_LOGE("Failed to send data - link layer busy");
  } else if (!context->isLinkActive) {
    success = false;
  } else {
    success = true;
    context->bufLen = len;
  }
  chppMutexUnlock(&context->mutex);

  if (success) {
    chppNotifierSignal(&context->notifier, SIGNAL_DATA);
  }

  return success ? CHPP_LINK_ERROR_NONE_QUEUED : CHPP_LINK_ERROR_BUSY;
}

static void doWork(void *linkContext, uint32_t signal) {
  UNUSED_VAR(linkContext);
  UNUSED_VAR(signal);
}

static void reset(void *linkContext) {
  struct ChppLinuxLinkState *context =
      (struct ChppLinuxLinkState *)(linkContext);
  deinit(context);
  init(context, context->transportContext);
}

static struct ChppLinkConfiguration getConfig(void *linkContext) {
  UNUSED_VAR(linkContext);
  const struct ChppLinkConfiguration config = {
      .txBufferLen = CHPP_LINUX_LINK_TX_MTU_BYTES,
      .rxBufferLen = CHPP_LINUX_LINK_RX_MTU_BYTES,
  };
  return config;
}

static uint8_t *getTxBuffer(void *linkContext) {
  struct ChppLinuxLinkState *context =
      (struct ChppLinuxLinkState *)(linkContext);
  return &context->buf[0];
}

const struct ChppLinkApi gLinuxLinkApi = {
    .init = &init,
    .deinit = &deinit,
    .send = &send,
    .doWork = &doWork,
    .reset = &reset,
    .getConfig = &getConfig,
    .getTxBuffer = &getTxBuffer,
};

const struct ChppLinkApi *getLinuxLinkApi(void) {
  return &gLinuxLinkApi;
}
