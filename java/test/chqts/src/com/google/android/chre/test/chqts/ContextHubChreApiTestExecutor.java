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

package com.google.android.chre.test.chqts;

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

import org.junit.Assert;

import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

import dev.pigweed.pw_rpc.Service;

/**
 * A base class for test executors that uses RPC-Based nanoapp.
 */
public class ContextHubChreApiTestExecutor extends ContextHubClientCallback {
    private final NanoAppBinary mNanoAppBinary;
    private final long mNanoAppId;
    private final ContextHubClient mContextHubClient;
    private final AtomicBoolean mChreReset = new AtomicBoolean(false);
    protected final Context mContext = InstrumentationRegistry.getTargetContext();
    protected final ContextHubManager mContextHubManager;
    protected final ContextHubInfo mContextHub;
    protected final ChreRpcClient mRpcClient;

    public ContextHubChreApiTestExecutor(NanoAppBinary nanoapp) {
        mNanoAppBinary = nanoapp;
        mNanoAppId = nanoapp.getNanoAppId();
        mContextHubManager = mContext.getSystemService(ContextHubManager.class);
        Assert.assertTrue(mContextHubManager != null);
        List<ContextHubInfo> contextHubs = mContextHubManager.getContextHubs();
        Assert.assertTrue(contextHubs.size() > 0);
        mContextHub = contextHubs.get(0);
        mContextHubClient = mContextHubManager.createClient(mContextHub, this);

        Service chreApiService = ChreApiTestUtil.getChreApiService();
        mRpcClient = new ChreRpcClient(
                mContextHubManager, mContextHub, mNanoAppId, List.of(chreApiService), this);
    }

    @Override
    public void onHubReset(ContextHubClient client) {
        mChreReset.set(true);
    }

    /** Should be invoked before run() is invoked to set up the test, e.g. in a @Before method. */
    public void init() {
        mContextHubManager.enableTestMode();
        ChreTestUtil.loadNanoAppAssertSuccess(mContextHubManager, mContextHub, mNanoAppBinary);
    }

    /** Cleans up the test, should be invoked in e.g. @After method. */
    public void deinit() {
        if (mChreReset.get()) {
            Assert.fail("CHRE reset during the test");
        }

        ChreTestUtil.unloadNanoAppAssertSuccess(mContextHubManager, mContextHub, mNanoAppId);
        mContextHubManager.disableTestMode();
        mContextHubClient.close();
    }
}
