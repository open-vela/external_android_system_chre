/*
 * Copyright (C) 2023 The Android Open Source Project
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

package com.google.android.chre.test.bleconcurrency;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.hardware.location.ContextHubClient;
import android.hardware.location.ContextHubClientCallback;
import android.hardware.location.ContextHubInfo;
import android.hardware.location.ContextHubManager;
import android.hardware.location.NanoAppBinary;

import androidx.test.InstrumentationRegistry;

import com.google.android.chre.utils.pigweed.ChreRpcClient;
import com.google.android.utils.chre.ChreApiTestUtil;
import com.google.android.utils.chre.ChreTestUtil;
import com.google.protobuf.ByteString;

import org.junit.Assert;

import java.util.HexFormat;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

import dev.chre.rpc.proto.ChreApiTest;
import dev.pigweed.pw_rpc.Service;

/**
 * A class that can execute the CHRE BLE concurrency test.
 */
public class ContextHubBleConcurrencyTestExecutor extends ContextHubClientCallback {
    private static final String TAG = "ContextHubBleConcurrencyTestExecutor";

    /**
     * The delay to report results in milliseconds.
     */
    private static final int REPORT_DELAY_MS = 0;

    /**
     * The RSSI threshold for the BLE scan filter.
     */
    private static final int RSSI_THRESHOLD = -128;

    /**
     * The advertisement type for service data.
     */
    private static final int CHRE_BLE_AD_TYPE_SERVICE_DATA_WITH_UUID_16 = 0x16;

    private final Context mContext = InstrumentationRegistry.getTargetContext();
    private final ContextHubInfo mContextHub;
    private final ContextHubClient mContextHubClient;
    private final ContextHubManager mContextHubManager;
    private final AtomicBoolean mChreReset = new AtomicBoolean(false);
    private final NanoAppBinary mNanoAppBinary;
    private final long mNanoAppId;
    private final ChreRpcClient mRpcClient;

    public ContextHubBleConcurrencyTestExecutor(NanoAppBinary nanoapp) {
        mNanoAppBinary = nanoapp;
        mNanoAppId = nanoapp.getNanoAppId();
        mContextHubManager = mContext.getSystemService(ContextHubManager.class);
        assertThat(mContextHubManager).isNotNull();
        List<ContextHubInfo> contextHubs = mContextHubManager.getContextHubs();
        assertThat(contextHubs.size() > 0).isTrue();
        mContextHub = contextHubs.get(0);
        mContextHubClient = mContextHubManager.createClient(mContextHub, /* callback= */ this);
        Service chreApiService = ChreApiTestUtil.getChreApiService();
        mRpcClient = new ChreRpcClient(mContextHubManager, mContextHub, mNanoAppId,
                List.of(chreApiService), /* callback= */ this);
    }

    @Override
    public void onHubReset(ContextHubClient client) {
        mChreReset.set(true);
    }

    /**
     * Should be invoked before run() is invoked to set up the test, e.g. in a @Before method.
     */
    public void init() {
        mContextHubManager.enableTestMode();
        ChreTestUtil.loadNanoAppAssertSuccess(mContextHubManager, mContextHub, mNanoAppBinary);
    }

    /**
     * Runs the test.
     */
    public void run() throws Exception {
        // TODO(b/266122703): Implement test
    }

    /**
     * Cleans up the test, should be invoked in e.g. @After method.
     */
    public void deinit() {
        if (mChreReset.get()) {
            Assert.fail("CHRE reset during the test");
        }

        ChreTestUtil.unloadNanoAppAssertSuccess(mContextHubManager, mContextHub, mNanoAppId);
        mContextHubManager.disableTestMode();
        mContextHubClient.close();
    }

    /**
     * Generates a BLE scan filter that filters only for the known Google beacons:
     * Google Eddystone and Nearby Fastpair.
     */
    private static ChreApiTest.ChreBleScanFilter getDefaultScanFilter() {
        ChreApiTest.ChreBleGenericFilter eddystoneFilter =
                ChreApiTest.ChreBleGenericFilter.newBuilder()
                        .setType(CHRE_BLE_AD_TYPE_SERVICE_DATA_WITH_UUID_16)
                        .setLength(2)
                        .setData(ByteString.copyFrom(HexFormat.of().parseHex("FEAA")))
                        .setMask(ByteString.copyFrom(HexFormat.of().parseHex("FFFF")))
                        .build();
        ChreApiTest.ChreBleGenericFilter nearbyFastpairFilter =
                ChreApiTest.ChreBleGenericFilter.newBuilder()
                        .setType(CHRE_BLE_AD_TYPE_SERVICE_DATA_WITH_UUID_16)
                        .setLength(2)
                        .setData(ByteString.copyFrom(HexFormat.of().parseHex("FE2C")))
                        .setMask(ByteString.copyFrom(HexFormat.of().parseHex("FFFF")))
                        .build();

        return ChreApiTest.ChreBleScanFilter.newBuilder()
                .setRssiThreshold(RSSI_THRESHOLD)
                .setScanFilterCount(2)
                .addScanFilters(eddystoneFilter)
                .addScanFilters(nearbyFastpairFilter)
                .build();
    }

    /**
     * Starts a BLE scan and asserts it was started successfully in a synchronous manner.
     * This waits for the event to be received and returns the status in the event.
     *
     * @param scanFilter                The scan filter.
     */
    private void chreBleStartScanSync(ChreApiTest.ChreBleScanFilter scanFilter) throws Exception {
        ChreApiTest.ChreBleStartScanAsyncInput.Builder inputBuilder =
                ChreApiTest.ChreBleStartScanAsyncInput.newBuilder()
                        .setMode(ChreApiTest.ChreBleScanMode.CHRE_BLE_SCAN_MODE_FOREGROUND)
                        .setReportDelayMs(REPORT_DELAY_MS)
                        .setHasFilter(scanFilter != null);
        if (scanFilter != null) {
            inputBuilder.setFilter(scanFilter);
        }

        ChreApiTestUtil util = new ChreApiTestUtil();
        List<ChreApiTest.GeneralSyncMessage> response =
                util.callServerStreamingRpcMethodSync(mRpcClient,
                        "chre.rpc.ChreApiTestService.ChreBleStartScanSync",
                        inputBuilder.build());
        assertThat(response.size() > 0).isTrue();
        for (ChreApiTest.GeneralSyncMessage status: response) {
            assertThat(status.getStatus()).isTrue();
        }
    }

    /**
     * Stops a BLE scan and asserts it was started successfully in a synchronous manner.
     * This waits for the event to be received and returns the status in the event.
     */
    private void chreBleStopScanSync() throws Exception {
        ChreApiTestUtil util = new ChreApiTestUtil();
        List<ChreApiTest.GeneralSyncMessage> response =
                util.callServerStreamingRpcMethodSync(mRpcClient,
                        "chre.rpc.ChreApiTestService.ChreBleStopScanSync");
        assertThat(response.size() > 0).isTrue();
        for (ChreApiTest.GeneralSyncMessage status: response) {
            assertThat(status.getStatus()).isTrue();
        }
    }
}
