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

package com.google.android.utils.chre;

import dev.chre.rpc.proto.ChreApiTest;
import dev.pigweed.pw_rpc.Service;

/**
 * A set of helper functions for tests that use the CHRE API Test nanoapp.
 */
public class ChreApiTestUtil {
    /**
     * Gets the RPC service for the CHRE API Test nanoapp.
     */
    public static Service getChreApiService() {
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
        return chreApiService;
    }
}
