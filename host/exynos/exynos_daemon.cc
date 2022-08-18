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

#include "exynos_daemon.h"
#include <sys/epoll.h>
#include <array>
#include <csignal>

namespace android {
namespace chre {

namespace {

int createEpollFd(int fdToEpoll) {
  struct epoll_event event;
  event.data.fd = fdToEpoll;
  event.events = EPOLLIN | EPOLLWAKEUP;
  int epollFd = epoll_create1(EPOLL_CLOEXEC);
  if (epoll_ctl(epollFd, EPOLL_CTL_ADD, event.data.fd, &event) != 0) {
    LOGE("Failed to add control interface to msg read fd errno: %s",
         strerror(errno));
    epollFd = -1;
  }
  return epollFd;
}

}  // anonymous namespace

ExynosDaemon::ExynosDaemon() : mLpmaHandler(true /* LPMA enabled */) {
  // TODO(b/235631242): Implement this.
}

bool ExynosDaemon::init() {
  bool success = false;
  mNativeThreadHandle = 0;
  siginterrupt(SIGINT, true);
  std::signal(SIGINT, signalHandler);
  if ((mCommsReadFd = open(kCommsDeviceFilename, O_RDONLY | O_CLOEXEC)) < 0) {
    LOGE("Read FD open failed: %s", strerror(errno));
  } else if ((mCommsWriteFd =
                  open(kCommsDeviceFilename, O_WRONLY | O_CLOEXEC)) < 0) {
    LOGE("Write FD open failed: %s", strerror(errno));
  } else {
    success = true;
    mProcessThreadRunning = true;
    mIncomingMsgProcessThread =
        std::thread([&] { this->processIncomingMsgs(); });
    mNativeThreadHandle = mIncomingMsgProcessThread.native_handle();
  }
  return success;
}

void ExynosDaemon::deinit() {
  stopMsgProcessingThread();

  close(mCommsWriteFd);
  mCommsWriteFd = kInvalidFd;

  close(mCommsReadFd);
  mCommsReadFd = kInvalidFd;
}

void ExynosDaemon::run() {
  constexpr char kChreSocketName[] = "chre";
  auto serverCb = [&](uint16_t clientId, void *data, size_t len) {
    sendMessageToChre(clientId, data, len);
  };

  mServer.run(kChreSocketName, true /* allowSocketCreation */, serverCb);
}

void ExynosDaemon::stopMsgProcessingThread() {
  if (mProcessThreadRunning) {
    mProcessThreadRunning = false;
    pthread_kill(mNativeThreadHandle, SIGINT);
    if (mIncomingMsgProcessThread.joinable()) {
      mIncomingMsgProcessThread.join();
    }
  }
}

void ExynosDaemon::processIncomingMsgs() {
  std::array<uint8_t, kIpcMsgSizeMax> message;
  int epollFd = createEpollFd(mCommsReadFd);

  while (mProcessThreadRunning) {
    struct epoll_event retEvent;
    int nEvents = epoll_wait(epollFd, &retEvent, 1 /* maxEvents */,
                             -1 /* infinite timeout */);
    if (nEvents < 0) {
      // epoll_wait will get interrupted if the CHRE daemon is shutting down,
      // check this condition before logging an error.
      if (mProcessThreadRunning) {
        LOGE("Epolling failed: %s", strerror(errno));
      }
    } else if (nEvents == 0) {
      LOGW("Epoll returned with 0 FDs ready despite no timeout (errno: %s)",
           strerror(errno));
    } else {
      int bytesRead = read(mCommsReadFd, message.data(), message.size());
      if (bytesRead < 0) {
        LOGE("Failed to read from fd: %s", strerror(errno));
      } else if (bytesRead == 0) {
        LOGE("Read 0 bytes from fd");
      } else {
        onMessageReceived(message.data(), bytesRead);
      }
    }
  }
}

bool ExynosDaemon::doSendMessage(void *data, size_t length) {
  bool success = false;
  if (length > kIpcMsgSizeMax) {
    LOGE("Msg size %zu larger than max msg size %zu", length, kIpcMsgSizeMax);
  } else {
    ssize_t rv = write(mCommsWriteFd, data, length);

    if (rv < 0) {
      LOGE("Failed to send message: %s", strerror(errno));
    } else if (rv != length) {
      LOGW("Msg send data loss: %zd of %zu bytes were written", rv, length);
    } else {
      success = true;
    }
  }
  return success;
}

int64_t ExynosDaemon::getTimeOffset(bool *success) {
  // TODO(b/235631242): Implement this.
  *success = false;
  return 0;
}

}  // namespace chre
}  // namespace android
