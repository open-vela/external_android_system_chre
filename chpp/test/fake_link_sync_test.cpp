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

#include <gtest/gtest.h>

#include <cstdint>
#include <iostream>
#include <thread>
#include <type_traits>

#include "chpp/app.h"
#include "chpp/crc.h"
#include "chpp/link.h"
#include "chpp/log.h"
#include "chpp/transport.h"
#include "fake_link.h"
#include "packet_util.h"

using chpp::test::FakeLink;

void chppPlatformLinkInit(struct ChppPlatformLinkParameters *params) {
  params->fake = new FakeLink();
}

void chppPlatformLinkDeinit(struct ChppPlatformLinkParameters *params) {
  auto *fake = reinterpret_cast<FakeLink *>(params->fake);
  delete fake;
}

enum ChppLinkErrorCode chppPlatformLinkSend(
    struct ChppPlatformLinkParameters *params, uint8_t *buf, size_t len) {
  auto *fake = reinterpret_cast<FakeLink *>(params->fake);
  fake->appendTxPacket(buf, len);
  return CHPP_LINK_ERROR_NONE_SENT;
}

void chppPlatformLinkDoWork(struct ChppPlatformLinkParameters * /*params*/,
                            uint32_t /*signal*/) {}

void chppPlatformLinkReset(struct ChppPlatformLinkParameters * /*params*/) {}

namespace chpp::test {

class FakeLinkSyncTests : public testing::Test {
 protected:
  void SetUp() override {
    chppTransportInit(&mTransportContext, &mAppContext);
    chppAppInitWithClientServiceSet(&mAppContext, &mTransportContext,
                                    /*clientServiceSet=*/{});
    mFakeLink = reinterpret_cast<FakeLink *>(mTransportContext.linkParams.fake);

    mWorkThread = std::thread(chppWorkThreadStart, &mTransportContext);

    // Proceed to the initialized state by performing the CHPP 3-way handshake
    ASSERT_TRUE(mFakeLink->waitForTxPacket());
    std::vector<uint8_t> resetPkt = mFakeLink->popTxPacket();
    ASSERT_TRUE(comparePacket(resetPkt, generateResetPacket()))
        << "Full packet: " << asResetPacket(resetPkt);

    ChppResetPacket resetAck = generateResetAckPacket();
    chppRxDataCb(&mTransportContext, reinterpret_cast<uint8_t *>(&resetAck),
                 sizeof(resetAck));

    ASSERT_TRUE(mFakeLink->waitForTxPacket());
    std::vector<uint8_t> ackPkt = mFakeLink->popTxPacket();
    ASSERT_TRUE(comparePacket(ackPkt, generateEmptyPacket()))
        << "Full packet: " << asChpp(ackPkt);
  }

  void TearDown() override {
    chppWorkThreadStop(&mTransportContext);
    mWorkThread.join();
    EXPECT_EQ(mFakeLink->getTxPacketCount(), 0);
  }

  void txPacket() {
    uint32_t *payload = static_cast<uint32_t *>(chppMalloc(sizeof(uint32_t)));
    *payload = 0xdeadbeef;
    bool enqueued = chppEnqueueTxDatagramOrFail(&mTransportContext, payload,
                                                sizeof(uint32_t));
    EXPECT_TRUE(enqueued);
  }

  ChppTransportState mTransportContext = {};
  ChppAppState mAppContext = {};
  ChppPlatformLinkParameters mLinkContext = {};
  FakeLink *mFakeLink;
  std::thread mWorkThread;
};

TEST_F(FakeLinkSyncTests, CheckRetryOnTimeout) {
  txPacket();
  ASSERT_TRUE(mFakeLink->waitForTxPacket());
  EXPECT_EQ(mFakeLink->getTxPacketCount(), 1);

  std::vector<uint8_t> pkt1 = mFakeLink->popTxPacket();

  // Ideally, to speed up the test, we'd have a mechanism to trigger
  // chppNotifierWait() to return immediately, to simulate timeout
  ASSERT_TRUE(mFakeLink->waitForTxPacket());
  EXPECT_EQ(mFakeLink->getTxPacketCount(), 1);
  std::vector<uint8_t> pkt2 = mFakeLink->popTxPacket();

  // The retry packet should be an exact match of the first one
  EXPECT_EQ(pkt1, pkt2);
}

TEST_F(FakeLinkSyncTests, NoRetryAfterAck) {
  txPacket();
  ASSERT_TRUE(mFakeLink->waitForTxPacket());
  EXPECT_EQ(mFakeLink->getTxPacketCount(), 1);

  // Generate and reply back with an ACK
  std::vector<uint8_t> pkt = mFakeLink->popTxPacket();
  ChppEmptyPacket ack = generateAck(pkt);
  chppRxDataCb(&mTransportContext, reinterpret_cast<uint8_t *>(&ack),
               sizeof(ack));

  // We shouldn't get that packet again
  EXPECT_FALSE(mFakeLink->waitForTxPacket());
}

TEST_F(FakeLinkSyncTests, MultipleNotifications) {
  constexpr int kNumPackets = 5;
  for (int i = 0; i < kNumPackets; i++) {
    txPacket();
  }

  for (int i = 0; i < kNumPackets; i++) {
    ASSERT_TRUE(mFakeLink->waitForTxPacket());

    // Generate and reply back with an ACK
    std::vector<uint8_t> pkt = mFakeLink->popTxPacket();
    ChppEmptyPacket ack = generateAck(pkt);
    chppRxDataCb(&mTransportContext, reinterpret_cast<uint8_t *>(&ack),
                 sizeof(ack));
  }

  EXPECT_FALSE(mFakeLink->waitForTxPacket());
}

}  // namespace chpp::test
