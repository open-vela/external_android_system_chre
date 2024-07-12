/*
 * Copyright (C) 2023 Xiaomi Corporation
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

/* Core */

#ifdef CONFIG_CHRE_AUDIO_SUPPORT_ENABLED
#include "core/tests/audio_util_test.cc"
#endif
#ifdef CONFIG_CHRE_BLE_SUPPORT_ENABLED
#include "core/tests/ble_request_test.cc"
#endif
#include "core/tests/memory_manager_test.cc"
#include "core/tests/request_multiplexer_test.cc"
#ifdef CONFIG_CHRE_SENSORS_SUPPORT_ENABLED
#include "core/tests/sensor_request_test.cc"
#endif
#ifdef CONFIG_CHRE_WIFI_SUPPORT_ENABLED
#include "core/tests/wifi_scan_request_test.cc"
#endif

/* Apps */

#ifdef CONFIG_CHRE_AUDIO_SUPPORT_ENABLED
#include "apps/test/pts/audio_enable_disable_test/src/audio_enable_disable_test.cc"
//    fatal error: pb_decode.h: No such file or directory
//#include
//"apps/test/common/chre_audio_concurrency_test/src/chre_audio_concurrency_test_manager.cc"
//    error: redefinition of ‘void chre::nanoappHandleEvent(uint32_t, uint16_t,
//    const void*)’
//#include
//"apps/test/common/chre_audio_concurrency_test/src/chre_audio_concurrency_test.cc"
#include "apps/test/common/shared/src/audio_validation.cc"
#endif

#ifdef CONFIG_CHRE_WIFI_SUPPORT_ENABLED
#include "apps/test/common/chre_cross_validator_wifi/src/chre_cross_validator_wifi.cc"
#include "apps/test/common/chre_cross_validator_wifi/src/chre_cross_validator_wifi_manager.cc"
#include "apps/test/common/chre_cross_validator_wifi/src/wifi_scan_result.cc"
#include "apps/test/common/ping_test/src/ping_test.cc"
#endif

#ifdef CONFIG_CHRE_SENSORS_SUPPORT_ENABLED
#include "apps/test/common/chre_cross_validator_sensor/src/chre_cross_validator_sensor.cc"
#include "apps/test/common/chre_cross_validator_sensor/src/chre_cross_validator_sensor_manager.cc"
#endif

//    fatal error: pb_decode.h: No such file or directory
//#include "apps/test/common/permission_test/src/permission_test.cc"
//#include
//"apps/test/common/chre_settings_test/src/chre_settings_test_manager.cc"
//#include "apps/test/common/chre_settings_test/src/chre_settings_test.cc"
//    fatal error: pw_rpc/channel.h: No such file or directory
//#include "apps/test/common/chre_api_test/src/chre_api_test_service.cc"
//#include "apps/test/common/chre_api_test/src/chre_api_test.cc"
//#include "apps/test/common/chre_api_test/src/chre_api_test_manager.cc"
//    fatal error: chre_stress_test.nanopb.h: No such file or directory
//#include "apps/test/common/chre_stress_test/src/chre_stress_test_manager.cc"
//#include "apps/test/common/chre_stress_test/src/chre_stress_test.cc"
//    fatal error: pw_rpc/channel.h: No such file or directory
//#include "apps/test/common/rpc_service_test/src/rpc_service_test.cc"
//    fatal error: pw_rpc/channel.h: No such file or directory
//#include "apps/test/common/rpc_service_test/src/rpc_service_manager.cc"

#ifdef CHRE_GNSS_SUPPORT_ENABLED
#include "apps/test/common/chre_cross_validator_gnss/src/chre_cross_validator_gnss.cc"
#endif

//    fatal error: pb_encode.h: No such file or directory
//#include "apps/test/common/shared/src/send_message.cc"
//#include "apps/test/chqts/src/fail_startup/fail_startup.cc"
//#include "apps/test/chqts/src/who_am_i/who_am_i.cc"
//    symbol `nanoappHandleEvent' is already defined
//#include "apps/test/chqts/src/echo_message/echo_message.cc"

/* Utils */

#if GTEST_HAS_DEATH_TEST
#include "util/tests/array_queue_test.cc"
#endif

//   hang
//#include "util/tests/atomic_spsc_queue_test.cc"
#include "util/tests/blocking_queue_test.cc"
#include "util/tests/buffer_test.cc"
#include "util/tests/copyable_fixed_size_vector_test.cc"
#include "util/tests/debug_dump_test.cc"

#if GTEST_HAS_DEATH_TEST
#include "util/tests/dynamic_vector_test.cc"
#include "util/tests/fixed_size_vector_test.cc"
#include "util/tests/heap_test.cc"
#include "util/tests/intrusive_list_test.cc"
#include "util/tests/lock_guard_test.cc"
#include "util/tests/memory_pool_test.cc"
#include "util/tests/optional_test.cc"
#include "util/tests/priority_queue_test.cc"
#include "util/tests/raw_storage_test.cc"
#include "util/tests/ref_base_test.cc"
#endif

//    error: redefinition of ‘template<class EnumType>
//#include "util/tests/segmented_queue_test.cc"
//    error: reference to ‘BigArray’ is ambiguous
//#include "util/tests/shared_ptr_test.cc"

#if GTEST_HAS_DEATH_TEST
#include "util/tests/singleton_test.cc"
#include "util/tests/stats_container_test.cc"
#include "util/tests/synchronized_expandable_memory_pool_test.cc"
#endif

#include "util/tests/time_test.cc"
#include "util/tests/unique_ptr_test.cc"

/* Sim */

#include "test/nuttx/host_endpoint_notification_test.cc"
#include "test/nuttx/info_struct_version_test.cc"
#include "test/nuttx/memory_test.cc"
#include "test/nuttx/test_base.cc"
#include "test/nuttx/test_util.cc"
#include "test/nuttx/timer_test.cc"
//    fatal error: pw_rpc/channel.h: No such file or directory
//#include "test/nuttx/rpc_test.cc"

#ifdef CONFIG_CHRE_AUDIO_SUPPORT_ENABLED
#include "test/nuttx/audio_test.cc"
#endif
#ifdef CONFIG_CHRE_BLE_SUPPORT_ENABLED
#include "test/nuttx/ble_test.cc"
#endif
#ifdef CHRE_GNSS_SUPPORT_ENABLED
#include "test/nuttx/gnss_test.cc"
#include "test/nuttx/settings_test.cc"
#endif
#ifdef CONFIG_CHRE_SENSORS_SUPPORT_ENABLED
#include "test/nuttx/sensor_test.cc"
#endif
#ifdef CONFIG_CHRE_WIFI_SUPPORT_ENABLED
#include "test/nuttx/wifi_nan_test.cc"
#include "test/nuttx/wifi_scan_test.cc"
#include "test/nuttx/wifi_test.cc"
#include "test/nuttx/wifi_timeout_test.cc"
#endif

extern "C" int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
