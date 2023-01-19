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

package com.google.android.chre.test.chqts;

import android.content.Context;
import android.hardware.location.ContextHubClient;
import android.hardware.location.ContextHubClientCallback;
import android.hardware.location.ContextHubInfo;
import android.hardware.location.ContextHubManager;
import android.hardware.location.NanoAppBinary;

import androidx.test.InstrumentationRegistry;

import com.google.android.chre.utils.pigweed.ChreRpcClient;
import com.google.android.utils.chre.ChreTestUtil;
import com.google.protobuf.MessageLite;

import org.junit.Assert;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

import dev.chre.rpc.proto.ChreApiTest;
import dev.pigweed.pw_rpc.Call.UnaryFuture;
import dev.pigweed.pw_rpc.MethodClient;
import dev.pigweed.pw_rpc.Service;
import dev.pigweed.pw_rpc.UnaryResult;

public class ContextHubNanoAppRequirementsTestExecutor extends ContextHubClientCallback {
    private final Context mContext = InstrumentationRegistry.getTargetContext();
    private final NanoAppBinary mNanoAppBinary;
    private final long mNanoAppId;
    private final ContextHubClient mContextHubClient;
    private final AtomicBoolean mChreReset = new AtomicBoolean(false);
    private final ContextHubManager mContextHubManager;
    private final ContextHubInfo mContextHub;
    private final ChreRpcClient mRpcClient;

    private static final int RPC_TIMEOUT_IN_SECONDS = 2;
    private static final int MAX_AUDIO_SOURCES_TO_TRY = 10;

    /**
    * Formats for audio that can be provided to a nanoapp. See enum chreAudioDataFormat in the
    * CHRE API.
    */
    public enum ChreAudioDataFormat {
        /**
         * Unsigned, 8-bit u-Law encoded data as specified by ITU-T G.711.
         */
        CHRE_AUDIO_DATA_FORMAT_8_BIT_U_LAW(0),

        /**
         * Signed, 16-bit linear PCM data. Endianness must be native to the local
         * processor.
         */
        CHRE_AUDIO_DATA_FORMAT_16_BIT_SIGNED_PCM(1);

        private final int mId;

        ChreAudioDataFormat(int id) {
            mId = id;
        }

        /**
         * Returns the ID.
         *
         * @return int      the ID
         */
        public int getId() {
            return mId;
        }
    }

    public ContextHubNanoAppRequirementsTestExecutor(NanoAppBinary nanoapp) {
        mNanoAppBinary = nanoapp;
        mNanoAppId = nanoapp.getNanoAppId();
        mContextHubManager = mContext.getSystemService(ContextHubManager.class);
        Assert.assertTrue(mContextHubManager != null);
        List<ContextHubInfo> contextHubs = mContextHubManager.getContextHubs();
        Assert.assertTrue(contextHubs.size() > 0);
        mContextHub = contextHubs.get(0);
        mContextHubClient = mContextHubManager.createClient(mContextHub, this);

        Service chreApiService = new Service("chre.rpc.ChreApiTestService",
                Service.unaryMethod("ChreBleGetCapabilities",
                        ChreApiTest.Void.class,
                        ChreApiTest.Capabilities.class),
                Service.unaryMethod("ChreBleGetFilterCapabilities",
                        ChreApiTest.Void.class,
                        ChreApiTest.Capabilities.class),
                Service.unaryMethod("ChreSensorFindDefault",
                        ChreApiTest.ChreSensorFindDefaultInput.class,
                        ChreApiTest.ChreSensorFindDefaultOutput.class),
                Service.unaryMethod("ChreGetSensorInfo",
                        ChreApiTest.ChreHandleInput.class,
                        ChreApiTest.ChreGetSensorInfoOutput.class),
                Service.unaryMethod("ChreGetSensorSamplingStatus",
                        ChreApiTest.ChreHandleInput.class,
                        ChreApiTest.ChreGetSensorSamplingStatusOutput.class),
                Service.unaryMethod("ChreSensorConfigureModeOnly",
                        ChreApiTest.ChreSensorConfigureModeOnlyInput.class,
                        ChreApiTest.Status.class),
                Service.unaryMethod("ChreAudioGetSource",
                        ChreApiTest.ChreHandleInput.class,
                        ChreApiTest.ChreAudioGetSourceOutput.class));
        mRpcClient = new ChreRpcClient(mContextHubManager, mContextHub, mNanoAppId,
                List.of(chreApiService), this);
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
     * Gets the preloaded nanoapp IDs
     *
     * @return List<Long>       the list of nanoapp IDs
     */
    public List<Long> getPreloadedNanoappIds() {
        List<Long> preloadedNanoappIds = new ArrayList<Long>();
        for (long nanoappId: mContextHubManager.getPreloadedNanoAppIds(mContextHub)) {
            preloadedNanoappIds.add(nanoappId);
        }
        return preloadedNanoappIds;
    }

    /**
     * Finds the default sensor for the given type and asserts that it exists.
     *
     * @param sensorType        the type of the sensor (constant)
     *
     * @return                  the handle of the sensor
     */
    public int findDefaultSensorAndAssertItExists(int sensorType) throws Exception {
        ChreApiTest.ChreSensorFindDefaultInput input = ChreApiTest.ChreSensorFindDefaultInput
                .newBuilder().setSensorType(sensorType).build();
        ChreApiTest.ChreSensorFindDefaultOutput response =
                callRpcMethod("chre.rpc.ChreApiTestService.ChreSensorFindDefault", input);
        Assert.assertTrue("Did not find sensor with type: " + sensorType,
                response.getFoundSensor());
        return response.getSensorHandle();
    }

    /**
     * Gets the sensor samping status and verifies the minimum interval from chreGetSensorInfo
     * is less than or equal to the expected interval -> the sensor is at least as fast at sampling
     * as is required.
     *
     * @param sensorHandle          the handle to the sensor
     * @param expectedInterval      the true sampling interval
     */
    public void getSensorInfoAndVerifyInterval(int sensorHandle, long expectedInterval)
            throws Exception {
        ChreApiTest.ChreHandleInput input =
                ChreApiTest.ChreHandleInput.newBuilder()
                .setHandle(sensorHandle).build();
        ChreApiTest.ChreGetSensorInfoOutput response =
                callRpcMethod("chre.rpc.ChreApiTestService.ChreGetSensorInfo", input);
        Assert.assertTrue("Failed to get sensor info for sensor with handle: " + sensorHandle,
                response.getStatus());
        Assert.assertTrue("The sensor with handle: " + sensorHandle
                + " does not sample at a fast enough rate.",
                response.getMinInterval() <= expectedInterval);
    }

    /**
     * Iterates through possible audio sources to find a source that has a minimum buffer
     * size in ns of expectedMinBufferSizeNs and a format of format.
     *
     * @param expectedMinBufferSizeInNs         the minimum buffer size in nanoseconds (ns)
     * @param format                            the audio format enum
     */
    public void findAudioSourceAndAssertItExists(long expectedMinBufferSizeNs,
            ChreAudioDataFormat format) throws Exception {
        boolean foundAcceptableAudioSource = false;
        for (int i = 0; i < MAX_AUDIO_SOURCES_TO_TRY; ++i) {
            ChreApiTest.ChreHandleInput input =
                    ChreApiTest.ChreHandleInput.newBuilder()
                    .setHandle(i).build();
            ChreApiTest.ChreAudioGetSourceOutput response =
                    callRpcMethod("chre.rpc.ChreApiTestService.ChreAudioGetSource", input);
            if (response.getStatus()
                    && response.getMinBufferDuration() >= expectedMinBufferSizeNs
                    && response.getFormat() == format.getId()) {
                foundAcceptableAudioSource = true;
                break;
            }
        }
        Assert.assertTrue("Did not find an acceptable audio source with a minimum buffer "
                + "size of " + expectedMinBufferSizeNs
                + " ns and format: " + format.name(),
                foundAcceptableAudioSource);
    }

    // TODO(b/262043286): Enable this once BLE is available
    /*
    /**
     * Gets the BLE capabilities and asserts the capability exists.
     *
     * @param capability        the capability to assert exists
     *
    public void getBleCapabilitiesAndAssertCapabilityExists(int capability) throws Exception {
        getCapabilitiesAndAssertCapabilityExists(
                "chre.rpc.ChreApiTestService.ChreBleGetCapabilities",
                capability,
                "Did not find the BLE capabilities");
    }

    /**
     * Gets the BLE filter capabilities and asserts the capability exists.
     *
     * @param capability        the capability to assert exists
     *
    public void getBleFilterCapabilitiesAndAssertCapabilityExists(int capability) throws Exception {
        getCapabilitiesAndAssertCapabilityExists(
                "chre.rpc.ChreApiTestService.ChreBleGetFilterCapabilities",
                capability,
                "Did not find the BLE filter capabilities");
    }
    */

    // TODO(b/262043286): Enable this once BLE is available
    /*
    /**
     * Gets the capabilities returned by RPC function: function and asserts that
     * capability exists with a failure message: errorMessage.
     *
     * @param function          the function to call
     * @param capability        the capability to assert exists
     * @param errorMessage      the error message to show when there is an assertion failure
     *
    private void getCapabilitiesAndAssertCapabilityExists(String function,
            int capability, String errorMessage) throws Exception {
        ChreApiTest.Capabilities capabilitiesResponse = callRpcMethod(function);
        int capabilities = capabilitiesResponse.getCapabilities();
        Assert.assertTrue(errorMessage + ": " + capability,
                (capabilities & capability) != 0);
    }
    */

    /**
     * Calls an RPC method with RPC_TIMEOUT_IN_SECONDS seconds of timeout
     *
     * @param <InputType>   the type of the input (proto generated type)
     * @param <OutputType>  the type of the output (proto generated type)
     * @param method        the fully-qualified method name
     * @param input         the input object
     *
     * @return              the proto output
     */
    private <InputType extends MessageLite, OutputType extends MessageLite> OutputType
            callRpcMethod(String method, InputType input) throws Exception {
        MethodClient methodClient = mRpcClient.getMethodClient(method);
        UnaryFuture<OutputType> responseFuture = methodClient.invokeUnaryFuture(input);
        UnaryResult<OutputType> responseResult = responseFuture.get(RPC_TIMEOUT_IN_SECONDS,
                TimeUnit.SECONDS);
        return responseResult.response();
    }

    // TODO(b/262043286): Enable this once BLE is available
    /*
    /**
     * Calls an RPC method with RPC_TIMEOUT_IN_SECONDS seconds of timeout with a void input
     *
     * @param <OutputType>  the type of the output (proto generated type)
     * @param method        the fully-qualified method name
     *
     * @return              the proto output
     *
    private <OutputType extends MessageLite> OutputType callRpcMethod(String method)
            throws Exception {
        ChreApiTest.Void input = ChreApiTest.Void.newBuilder().build();
        return callRpcMethod(method, input);
    }
    */
}
