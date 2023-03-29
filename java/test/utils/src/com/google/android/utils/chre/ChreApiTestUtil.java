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

import androidx.annotation.NonNull;

import com.google.android.chre.utils.pigweed.ChreRpcClient;
import com.google.protobuf.MessageLite;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.TimeUnit;

import dev.chre.rpc.proto.ChreApiTest;
import dev.pigweed.pw_rpc.Call.ServerStreamingFuture;
import dev.pigweed.pw_rpc.Call.UnaryFuture;
import dev.pigweed.pw_rpc.MethodClient;
import dev.pigweed.pw_rpc.Service;
import dev.pigweed.pw_rpc.UnaryResult;

/**
 * A set of helper functions for tests that use the CHRE API Test nanoapp.
 */
public class ChreApiTestUtil {
    /**
     * The default timeout for an RPC call in seconds.
     */
    public static final int RPC_TIMEOUT_IN_SECONDS = 5;

    /**
     * The default timeout for an RPC call in milliseconds.
     */
    public static final int RPC_TIMEOUT_IN_MS = RPC_TIMEOUT_IN_SECONDS * 1000;

    /**
     * Storage for nanoapp streaming messages. This is a map from each RPC client to the
     * list of messages received.
     */
    private final Map<ChreRpcClient, List<MessageLite>> mNanoappStreamingMessages =
            new HashMap<ChreRpcClient, List<MessageLite>>();

    /**
     * If true, there is an active server streaming RPC ongoing.
     */
    private boolean mActiveServerStreamingRpc = false;

    /**
     * Calls a server streaming RPC method on multiple RPC clients. The RPC will be initiated for
     * each client, then we will give each client a maximum of RPC_TIMEOUT_IN_SECONDS seconds of
     * timeout, getting the futures in sequential order.
     *
     * @param <RequestType>   the type of the request (proto generated type).
     * @param <ResponseType>  the type of the response (proto generated type).
     * @param rpcClients      the RPC clients.
     * @param method          the fully-qualified method name.
     * @param request         the request object.
     *
     * @return                the proto responses or null if there was an error.
     */
    public <RequestType extends MessageLite, ResponseType extends MessageLite>
            List<List<ResponseType>> callConcurrentServerStreamingRpcMethodSync(
                    @NonNull List<ChreRpcClient> rpcClients,
                    @NonNull String method,
                    @NonNull RequestType request) throws Exception {
        Objects.requireNonNull(rpcClients);
        Objects.requireNonNull(method);
        Objects.requireNonNull(request);

        List<ServerStreamingFuture> responseFutures = new ArrayList<ServerStreamingFuture>();
        synchronized (mNanoappStreamingMessages) {
            if (mActiveServerStreamingRpc) {
                return null;
            }

            for (ChreRpcClient rpcClient: rpcClients) {
                MethodClient methodClient = rpcClient.getMethodClient(method);
                ServerStreamingFuture responseFuture = methodClient.invokeServerStreamingFuture(
                        request,
                        (ResponseType response) -> {
                            synchronized (mNanoappStreamingMessages) {
                                mNanoappStreamingMessages.putIfAbsent(rpcClient,
                                        new ArrayList<MessageLite>());
                                mNanoappStreamingMessages.get(rpcClient).add(response);
                            }
                        });
                responseFutures.add(responseFuture);
            }
            mActiveServerStreamingRpc = true;
        }

        boolean success = true;
        long endTimeInMs = System.currentTimeMillis() + RPC_TIMEOUT_IN_MS;
        for (ServerStreamingFuture responseFuture: responseFutures) {
            try {
                responseFuture.get(Math.max(0, endTimeInMs - System.currentTimeMillis()),
                        TimeUnit.MILLISECONDS);
            } catch (Exception exception) {
                success = false;
            }
        }

        synchronized (mNanoappStreamingMessages) {
            List<List<ResponseType>> responses = null;
            if (success) {
                responses = new ArrayList<List<ResponseType>>();
                for (ChreRpcClient rpcClient: rpcClients) {
                    List<MessageLite> messages = mNanoappStreamingMessages.get(rpcClient);
                    List<ResponseType> responseList = new ArrayList<ResponseType>();
                    if (messages != null) {
                        // Only needed to cast the type.
                        for (MessageLite message: messages) {
                            responseList.add((ResponseType) message);
                        }
                    }

                    responses.add(responseList);
                }
            }

            mNanoappStreamingMessages.clear();
            mActiveServerStreamingRpc = false;
            return responses;
        }
    }

    /**
     * Calls a server streaming RPC method with RPC_TIMEOUT_IN_SECONDS seconds of timeout.
     *
     * @param <RequestType>   the type of the request (proto generated type).
     * @param <ResponseType>  the type of the response (proto generated type).
     * @param rpcClient       the RPC client.
     * @param method          the fully-qualified method name.
     * @param request         the request object.
     *
     * @return                the proto response or null if there was an error.
     */
    public <RequestType extends MessageLite, ResponseType extends MessageLite> List<ResponseType>
            callServerStreamingRpcMethodSync(
                    @NonNull ChreRpcClient rpcClient,
                    @NonNull String method,
                    @NonNull RequestType request) throws Exception {
        Objects.requireNonNull(rpcClient);
        Objects.requireNonNull(method);
        Objects.requireNonNull(request);

        List<List<ResponseType>> responses = callConcurrentServerStreamingRpcMethodSync(
                Arrays.asList(rpcClient),
                method,
                request);
        return responses == null || responses.isEmpty() ? null : responses.get(0);
    }

    /**
     * Calls a server streaming RPC method with RPC_TIMEOUT_IN_SECONDS seconds of
     * timeout with a void request.
     *
     * @param <ResponseType>  the type of the response (proto generated type).
     * @param rpcClient       the RPC client.
     * @param method          the fully-qualified method name.
     *
     * @return                the proto response or null if there was an error.
     */
    public <ResponseType extends MessageLite> List<ResponseType>
            callServerStreamingRpcMethodSync(
                    @NonNull ChreRpcClient rpcClient,
                    @NonNull String method) throws Exception {
        Objects.requireNonNull(rpcClient);
        Objects.requireNonNull(method);

        ChreApiTest.Void request = ChreApiTest.Void.newBuilder().build();
        return callServerStreamingRpcMethodSync(rpcClient, method, request);
    }

    /**
     * Calls an RPC method with RPC_TIMEOUT_IN_SECONDS seconds of timeout for concurrent
     * instances of the ChreApiTest nanoapp.
     *
     * @param <RequestType>   the type of the request (proto generated type).
     * @param <ResponseType>  the type of the response (proto generated type).
     * @param rpcClients      the RPC clients corresponding to the instances of the
     *                        ChreApiTest nanoapp.
     * @param method          the fully-qualified method name.
     * @param request         the request object.
     *
     * @return                the proto response or null if there was an error.
     */
    public static <RequestType extends MessageLite, ResponseType extends MessageLite>
            List<ResponseType> callConcurrentUnaryRpcMethodSync(
                    @NonNull List<ChreRpcClient> rpcClients,
                    @NonNull String method,
                    @NonNull RequestType request) throws Exception {
        Objects.requireNonNull(rpcClients);
        Objects.requireNonNull(method);
        Objects.requireNonNull(request);

        List<UnaryFuture<ResponseType>> responseFutures =
                new ArrayList<UnaryFuture<ResponseType>>();
        for (ChreRpcClient rpcClient: rpcClients) {
            MethodClient methodClient = rpcClient.getMethodClient(method);
            responseFutures.add(methodClient.invokeUnaryFuture(request));
        }

        List<ResponseType> responses = new ArrayList<ResponseType>();
        boolean success = true;
        long endTimeInMs = System.currentTimeMillis() + RPC_TIMEOUT_IN_MS;
        for (UnaryFuture<ResponseType> responseFuture: responseFutures) {
            try {
                UnaryResult<ResponseType> responseResult = responseFuture.get(
                        Math.max(0, endTimeInMs - System.currentTimeMillis()),
                                TimeUnit.MILLISECONDS);
                responses.add(responseResult.response());
            } catch (Exception exception) {
                success = false;
            }
        }
        return success ? responses : null;
    }

    /**
     * Calls an RPC method with RPC_TIMEOUT_IN_SECONDS seconds of timeout.
     *
     * @param <RequestType>   the type of the request (proto generated type).
     * @param <ResponseType>  the type of the response (proto generated type).
     * @param rpcClient       the RPC client.
     * @param method          the fully-qualified method name.
     * @param request         the request object.
     *
     * @return                the proto response or null if there was an error.
     */
    public static <RequestType extends MessageLite, ResponseType extends MessageLite> ResponseType
            callUnaryRpcMethodSync(
                    @NonNull ChreRpcClient rpcClient,
                    @NonNull String method,
                    @NonNull RequestType request) throws Exception {
        Objects.requireNonNull(rpcClient);
        Objects.requireNonNull(method);
        Objects.requireNonNull(request);

        List<ResponseType> responses = callConcurrentUnaryRpcMethodSync(Arrays.asList(rpcClient),
                method, request);
        return responses == null || responses.isEmpty() ? null : responses.get(0);
    }

    /**
     * Calls an RPC method with RPC_TIMEOUT_IN_SECONDS seconds of timeout with a void request.
     *
     * @param <ResponseType>  the type of the response (proto generated type).
     * @param rpcClient       the RPC client.
     * @param method          the fully-qualified method name.
     *
     * @return                the proto response or null if there was an error.
     */
    public static <ResponseType extends MessageLite> ResponseType
            callUnaryRpcMethodSync(@NonNull ChreRpcClient rpcClient, @NonNull String method)
            throws Exception {
        Objects.requireNonNull(rpcClient);
        Objects.requireNonNull(method);

        ChreApiTest.Void request = ChreApiTest.Void.newBuilder().build();
        return callUnaryRpcMethodSync(rpcClient, method, request);
    }

    /**
     * Gets the RPC service for the CHRE API Test nanoapp.
     */
    public static Service getChreApiService() {
        Service chreApiService = new Service("chre.rpc.ChreApiTestService",
                Service.unaryMethod(
                        "ChreBleGetCapabilities",
                        ChreApiTest.Void.class,
                        ChreApiTest.Capabilities.class),
                Service.unaryMethod(
                        "ChreBleGetFilterCapabilities",
                        ChreApiTest.Void.class,
                        ChreApiTest.Capabilities.class),
                Service.unaryMethod(
                        "ChreBleStartScanAsync",
                        ChreApiTest.ChreBleStartScanAsyncInput.class,
                        ChreApiTest.Status.class),
                Service.serverStreamingMethod(
                        "ChreBleStartScanSync",
                        ChreApiTest.ChreBleStartScanAsyncInput.class,
                        ChreApiTest.GeneralSyncMessage.class),
                Service.unaryMethod(
                        "ChreBleStopScanAsync",
                        ChreApiTest.Void.class,
                        ChreApiTest.Status.class),
                Service.serverStreamingMethod(
                        "ChreBleStopScanSync",
                        ChreApiTest.Void.class,
                        ChreApiTest.GeneralSyncMessage.class),
                Service.unaryMethod(
                        "ChreSensorFindDefault",
                        ChreApiTest.ChreSensorFindDefaultInput.class,
                        ChreApiTest.ChreSensorFindDefaultOutput.class),
                Service.unaryMethod(
                        "ChreGetSensorInfo",
                        ChreApiTest.ChreHandleInput.class,
                        ChreApiTest.ChreGetSensorInfoOutput.class),
                Service.unaryMethod(
                        "ChreGetSensorSamplingStatus",
                        ChreApiTest.ChreHandleInput.class,
                        ChreApiTest.ChreGetSensorSamplingStatusOutput.class),
                Service.unaryMethod(
                        "ChreSensorConfigureModeOnly",
                        ChreApiTest.ChreSensorConfigureModeOnlyInput.class,
                        ChreApiTest.Status.class),
                Service.unaryMethod(
                        "ChreAudioGetSource",
                        ChreApiTest.ChreHandleInput.class,
                        ChreApiTest.ChreAudioGetSourceOutput.class),
                Service.unaryMethod(
                        "ChreConfigureHostEndpointNotifications",
                        ChreApiTest.ChreConfigureHostEndpointNotificationsInput.class,
                        ChreApiTest.Status.class),
                Service.unaryMethod(
                        "RetrieveLatestDisconnectedHostEndpointEvent",
                        ChreApiTest.Void.class,
                        ChreApiTest.RetrieveLatestDisconnectedHostEndpointEventOutput
                                .class),
                Service.unaryMethod(
                        "ChreGetHostEndpointInfo",
                        ChreApiTest.ChreGetHostEndpointInfoInput.class,
                        ChreApiTest.ChreGetHostEndpointInfoOutput.class));
        return chreApiService;
    }
}
