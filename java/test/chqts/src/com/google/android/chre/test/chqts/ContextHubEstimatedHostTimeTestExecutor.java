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
package com.google.android.chre.test.chqts;

import android.hardware.location.ContextHubInfo;
import android.hardware.location.ContextHubManager;
import android.hardware.location.NanoAppBinary;
import android.os.SystemClock;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Verify estimated host time from nanoapp.
 *
 * Protocol:
 * host to app: ESTIMATED_HOST_TIME, no data
 * app to host: CONTINUE
 * host to app: CONTINUE, 64-bit time
 * app to host: SUCCESS
 *
 */
public class ContextHubEstimatedHostTimeTestExecutor extends ContextHubGeneralTestExecutor {
    private static final long MAX_ALLOWED_TIME_DELTA_NS = 10000000;     // 10 ms.
    private long mMsgSendTimestampNs = 0;

    public ContextHubEstimatedHostTimeTestExecutor(ContextHubManager manager, ContextHubInfo info,
            NanoAppBinary binary) {
        super(manager, info, new GeneralTestNanoApp(binary,
                ContextHubTestConstants.TestNames.ESTIMATED_HOST_TIME));
    }

    @Override
    protected void handleMessageFromNanoApp(long nanoAppId,
            ContextHubTestConstants.MessageType type, byte[] data) {
        if (type != ContextHubTestConstants.MessageType.CONTINUE) {
            fail("Unexpected message type " + type);
        } else {
            if (data.length != 0 && mMsgSendTimestampNs != 0) {
                long currentTimestampNs = SystemClock.elapsedRealtimeNanos();
                long chreTimestampNs = ByteBuffer.wrap(data)
                        .order(ByteOrder.LITTLE_ENDIAN)
                        .getLong();
                long halfRtt = (currentTimestampNs - mMsgSendTimestampNs) / 2;

                // The timestamp reported by CHRE must be close to the timestamp
                // when the message was sent with half the RTT added.
                long deltaNs = chreTimestampNs - (mMsgSendTimestampNs + halfRtt);

                if (java.lang.Math.abs(deltaNs) < MAX_ALLOWED_TIME_DELTA_NS) {
                    pass();
                } else {
                    fail("Inconsistent CHRE/AP timestamps- Current TS: "
                            + currentTimestampNs + " CHRE TS: " + chreTimestampNs
                            + " start TS: " + mMsgSendTimestampNs + " Delta: "
                            + deltaNs);
                }
            } else {
                mMsgSendTimestampNs = SystemClock.elapsedRealtimeNanos();
                sendMessageToNanoAppOrFail(nanoAppId,
                        ContextHubTestConstants.MessageType.CONTINUE.asInt(),
                        new byte[0] /* data */);
            }
        }
    }
}
