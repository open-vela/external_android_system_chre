/*
 * Copyright (C) 2024 Xiaomi Corporation
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
#include "chre/target_platform/rpc_socket.h"

#include <cutils/sockets.h>
#include <netpacket/rpmsg.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include "chre/platform/log.h"

namespace chre {
void RpcSocket::startServer(IRpcCallbacks* callback) {
  for (size_t i = 0; i < CONFIG_CHRE_CLIENT_COUNT + MAX_SERVER; i++) {
    mPollFds[i].fd = -1;
  }

  for (size_t i = 0; i <= CONFIG_CHRE_CLIENT_COUNT; i++) {
    mClients[i] = -1;
  }

  mClient_count = 1;
  mServer_count = 0;

  mRpcListentFd[0] = -1;
  mRpcListentFd[1] = -1;

  if (true == rpcServiceInit()) {
    mRpcCallback = callback;
    runRpctask();
  }
}

bool RpcSocket::rpcServiceInit() {
#ifdef CONFIG_CHRE_LOCAL_SOCKET_SERVER
  struct sockaddr_un uadd = {
      .sun_family = AF_UNIX,
      .sun_path = "chre",
  };
  mRpcListentFd[0] = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (mRpcListentFd[0] < 0) {
    LOGE("socket create failed errno=%d\n", errno);
    return false;
  }
  if (bind(mRpcListentFd[0], (struct sockaddr*)&uadd,
           sizeof(struct sockaddr_un)) < 0) {
    LOGE("bind failed errno=%d", errno);
    close(mRpcListentFd[0]);
    mRpcListentFd[0] = -1;
    return false;
  }
#endif

#ifdef CONFIG_CHRE_RPMSG_SERVER
  struct sockaddr_rpmsg raddr = {
      .rp_family = AF_RPMSG,
      .rp_cpu = "",
      .rp_name = "chre",
  };
  mRpcListentFd[1] = socket(AF_RPMSG, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (mRpcListentFd[1] < 0) {
    LOGE("socket create failed errno=%d\n", errno);
    return false;
  }

  if (bind(mRpcListentFd[1], (struct sockaddr*)&raddr,
           sizeof(struct sockaddr_rpmsg)) < 0) {
    LOGE("bind failed errno=%d", errno);
    return false;
  }
#endif

  return true;
}

void RpcSocket::rpcServiceDeinit() {
  for (int i = 0; i <= CONFIG_CHRE_CLIENT_COUNT; i++) {
    if (mClients[i] > 0) {
      close(mClients[i]);
    }
  }

  for (int i = 0; i < MAX_SERVER; i++) {
    if (mRpcListentFd[i]) {
      close(mRpcListentFd[i]);
    }
  }
}

void RpcSocket::disconnectClient(int clientRpc) { close(clientRpc); }

void RpcSocket::stopServer() {
  if (mlistent_tid.has_value()) {
    mlistent_tid->join();
  }
  rpcServiceDeinit();
}

void RpcSocket::runRpctask() {
  mlistent_tid = std::thread(&RpcSocket::vChreRecvTask, this);
}

void RpcSocket::vChreRecvTask() {
  struct sockaddr_rpmsg remotaddr;
  socklen_t addrlen = sizeof(remotaddr);
  const int MAX_NFDS = CONFIG_CHRE_CLIENT_COUNT + MAX_SERVER;
  nfds_t nfds = 0;
  int timeout = 3000;
  ssize_t messageLen = 0;
  ssize_t max_package_size =
      CHRE_MESSAGE_TO_HOST_MAX_SIZE + CHRE_MESSAGE_HEAD_SIZE;
  uint8_t* messageBuffer = (uint8_t*)malloc(max_package_size);
  if (!messageBuffer) {
    LOGE("out of memorry");
    return;
  }

  LOGV("[chre server] listen ...\n");
  for (nfds_t i = 0; i < 2; i++) {
    if (mRpcListentFd[i] < 0) continue;
    if (listen(mRpcListentFd[i], CONFIG_CHRE_CLIENT_COUNT) < 0) {
      LOGE("[chre server] listen failure %d\n", errno);
      return;
    } else {
      mPollFds[nfds].fd = mRpcListentFd[i];
      mPollFds[nfds].events = POLLIN;
      nfds++;
      mServer_count++;
    }
  }

  while (1) {
    int ret = poll(mPollFds, nfds, timeout);
    if (ret < 0) {
      if (errno == EINTR || errno == EBUSY) continue;
      LOGE("[chre server] poll failure: %d\n", errno);
      break;
    } else if (ret == 0) {
      continue;
    }

    for (nfds_t i = 0; i < nfds; i++) {
      if ((mPollFds[i].fd == mRpcListentFd[0] ||
           mPollFds[i].fd == mRpcListentFd[1]) &&
          mClient_count <= CONFIG_CHRE_CLIENT_COUNT) {
        if (mPollFds[i].revents & POLLIN) {
          int newfd =
              accept(mPollFds[i].fd, (struct sockaddr*)&remotaddr, &addrlen);
          if (newfd < 0) continue;
          mClients[mClient_count++] = newfd;
          for (int j = mServer_count; j < MAX_NFDS; j++) {
            if (mPollFds[j].fd == -1) {
              mPollFds[j].fd = newfd;
              mPollFds[j].events = POLLIN;
              nfds++;
            }
          }
        }
      } else if (mPollFds[i].revents & POLLIN) {
        ssize_t n = recv(mPollFds[i].fd, messageBuffer + messageLen,
                         max_package_size - messageLen, MSG_DONTWAIT);
        if (n == 0) {
          disconnectClient(mPollFds[i].fd);
          messageLen = 0;
        } else if (n > 0) {
          messageLen += n;
          if (mRpcCallback &&
              mRpcCallback->onMessageReceived(messageBuffer, messageLen)) {
            messageLen = 0;
          } else {
            if (messageLen >= max_package_size) {
              LOGE("Failed to decode message from host");
              messageLen = 0;
            }
          }
        }
      } else if (mPollFds[i].revents & (POLLHUP | POLLERR)) {
        mPollFds[i].revents &= ~(POLLHUP | POLLERR);
        mPollFds[i].fd = -1;
        nfds--;
      }
    }
  }
}

bool RpcSocket::sendToAllClient(uint8_t* data, size_t dataLen) {
  for (int i = 0; i <= mClient_count; i++) {
    if (mClients[i] > 0) send(mClients[i], data, dataLen, MSG_WAITALL);
  }
  return true;
}

bool RpcSocket::sendToClientById(uint8_t* data, size_t dataLen,
                                 uint16_t clientId) {
  if (mServer_count > 1 &&
      clientId >= (UINT16_MAX - CONFIG_CHRE_CLIENT_COUNT)) {
    return sendToAllClient(data, dataLen);
  } else {
    uint16_t id =
        clientId >= (UINT16_MAX - CONFIG_CHRE_CLIENT_COUNT) ? 1 : clientId;
    if (id <= CONFIG_CHRE_CLIENT_COUNT && mClients[id] > 0 &&
        send(mClients[id], data, dataLen, MSG_WAITALL) > 0)
      return true;
  }
  return false;
}

// HostLinkBase end

}  // namespace chre
