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

package com.google.android.chre.test.setting;

import android.app.Instrumentation;
import android.bluetooth.BluetoothAdapter;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.location.NanoAppBinary;
import android.util.Log;

import com.google.android.chre.nanoapp.proto.ChreSettingsTest;
import com.google.android.utils.chre.SettingsUtil;

import org.junit.Assert;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * A test to check for behavior when Bluetooth settings are changed.
 */
public class ContextHubBleSettingsTestExecutor {
    private static final String TAG = "ContextHubBleSettingsTestExecutor";
    private final ContextHubSettingsTestExecutor mExecutor;

    private final Instrumentation mInstrumentation =
            androidx.test.platform.app.InstrumentationRegistry.getInstrumentation();

    private final Context mContext = mInstrumentation.getTargetContext();

    private final SettingsUtil mSettingsUtil;

    private boolean mInitialBluetoothEnabled;

    private boolean mInitialBluetoothScanningEnabled;

    public static class BluetoothUpdateListener {
        public BluetoothUpdateListener(boolean enable) {
            mEnable = enable;
        }

        // True if Bluetooth is expected to become available
        private final boolean mEnable;

        public CountDownLatch mBluetoothLatch = new CountDownLatch(1);

        public BroadcastReceiver mBluetoothUpdateReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                if (BluetoothAdapter.ACTION_STATE_CHANGED.equals(intent.getAction())) {
                    int state = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, -1);
                    if ((mEnable && state == BluetoothAdapter.STATE_ON)
                            || (!mEnable && state == BluetoothAdapter.STATE_OFF)) {
                        mBluetoothLatch.countDown();
                    }
                }
            }
        };
    }

    public ContextHubBleSettingsTestExecutor(NanoAppBinary binary) {
        mExecutor = new ContextHubSettingsTestExecutor(binary);
        mSettingsUtil = new SettingsUtil(mContext);
    }

    /**
     * Should be called in a @Before method.
     */
    public void setUp() {
        mInitialBluetoothEnabled = mSettingsUtil.isBluetoothEnabled();
        mInitialBluetoothScanningEnabled = mSettingsUtil.isBluetoothScanningAlwaysEnabled();
        Log.d(TAG, "isBluetoothEnabled=" + mInitialBluetoothEnabled
                    + "; isBluetoothScanningEnabled=" + mInitialBluetoothScanningEnabled);
        mExecutor.init();
    }

    public void runBleScanningTest() {
        runBleScanningTest(false /* enableBluetooth */, false /* enableBluetoothScanning */);
        runBleScanningTest(true /* enableBluetooth */, true /* enableBluetoothScanning */);

        // TODO(b/258203997): Enable these tests at a later date
        // runBleScanningTest(true /* enableBluetooth */, false /* enableBluetoothScanning */);
        // runBleScanningTest(false /* enableBluetooth */, true /* enableBluetoothScanning */);
    }

    /**
     * Should be called in an @After method.
     */
    public void tearDown() {
        mExecutor.deinit();
        mSettingsUtil.setBluetooth(mInitialBluetoothEnabled);
        mSettingsUtil.setBluetoothScanningSettings(mInitialBluetoothScanningEnabled);
    }

    /**
     * Sets the BLE scanning settings on the device.
     * @param enable                    true to enable Bluetooth settings, false to disable it.
     * @param enableBluetoothScanning   if true, enable BLE scanning; false, otherwise
     */
    private void setBluetoothSettings(boolean enable, boolean enableBluetoothScanning) {
        BluetoothUpdateListener bluetoothUpdateListener = new BluetoothUpdateListener(enable);
        mContext.registerReceiver(
                bluetoothUpdateListener.mBluetoothUpdateReceiver,
                new IntentFilter(BluetoothAdapter.EXTRA_STATE));

        mSettingsUtil.setBluetooth(enable);
        mSettingsUtil.setBluetoothScanningSettings(enableBluetoothScanning);

        try {
            bluetoothUpdateListener.mBluetoothLatch.await(30, TimeUnit.SECONDS);

            // Wait a few seconds to ensure setting is propagated to CHRE path
            Thread.sleep(10000);
        } catch (InterruptedException e) {
            Assert.fail(e.getMessage());
        }

        mContext.unregisterReceiver(bluetoothUpdateListener.mBluetoothUpdateReceiver);
    }

    /**
     * Helper function to run the test
     *
     * @param enableBluetooth               if bluetooth is enabled
     * @param enableBluetoothScanning       if bluetooth scanning is always enabled
     */
    private void runBleScanningTest(boolean enableBluetooth,
            boolean enableBluetoothScanning) {
        setBluetoothSettings(enableBluetooth, enableBluetoothScanning);

        boolean enableFeature = enableBluetooth || enableBluetoothScanning;
        ChreSettingsTest.TestCommand.State state = enableFeature
                ? ChreSettingsTest.TestCommand.State.ENABLED
                : ChreSettingsTest.TestCommand.State.DISABLED;
        mExecutor.startTestAssertSuccess(
                ChreSettingsTest.TestCommand.Feature.BLE_SCANNING, state);
    }
}
