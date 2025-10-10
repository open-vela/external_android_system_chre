#
# Copyright (C) 2025 Xiaomi Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
#

if(CONFIG_CHRE)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(ANDROID_BUILD_TOP ${NUTTX_APPS_DIR}/external/android)
  set(CHRE_PREFIX ${ANDROID_BUILD_TOP}/system/chre/chre)
  set(PIGWEED_DIR ${NUTTX_APPS_DIR}/netutils/connectedhomeip/pigweed)
  set(CHRE_PIGWEED_DIR ${CHRE_PREFIX}/external/pigweed)
  set(COMMON_CFLAGS)
  set(CHRE_CXXFLAGS)

  if(NOT DEFINED PYTHON)
    set(PYTHON python3)
  endif()

  set(OUT ${CMAKE_CURRENT_BINARY_DIR}/build/out)

  if(NOT DEFINED OPT_LEVEL)
    set(OPT_LEVEL 0)
  endif()

  if(NOT DEFINED CHRE_HOST_CC)
    set(CHRE_HOST_CC g++)
  endif()

  # Compute chre patch version
  if("${CHRE_PATCH_VERSION}" STREQUAL "")
    if("${CHRE_HOST_OS}" STREQUAL "Darwin")
      set(DATE_CMD gdate)
    else()
      set(DATE_CMD date)
    endif()

    execute_process(
      COMMAND date -d "2017-01-01" +%s
      OUTPUT_VARIABLE EPOCH
      OUTPUT_STRIP_TRAILING_WHITESPACE)

    execute_process(
      COMMAND date +%s
      OUTPUT_VARIABLE CURRENT_EPOCH_STR
      OUTPUT_STRIP_TRAILING_WHITESPACE)

    string(STRIP "${CURRENT_EPOCH_STR}" CURRENT_EPOCH)
    math(EXPR CURRENT_EPOCH "${CURRENT_EPOCH}")
    math(EXPR TIME_DIFF "${CURRENT_EPOCH} - ${EPOCH}")
    math(EXPR HOURS "${TIME_DIFF} / 3600")
    math(EXPR CHRE_PATCH_VERSION "${HOURS} % 65535")
  endif()

  # Compute CHRE API Version
  set(COMMIT_HASH_COMMAND git describe --always --long --dirty)
  execute_process(
    COMMAND ${COMMIT_HASH_COMMAND}
    OUTPUT_VARIABLE COMMIT_HASH_OUTPUT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

  set(VERSION_STRING "chre=${COMMIT_HASH_OUTPUT}")
  list(APPEND CHRE_CXXFLAGS -DCHRE_VERSION_STRING="${VERSION_STRING}")

  # For details of suffix, please see nuttx-apps/tools/mksymtab.sh
  list(APPEND CHRE_CXXFLAGS
       -DCONFIG_CHRE_SYMTAB_ARRAYNAME=${CONFIG_CHRE_SYMTAB_PREFIX}_exports)

  list(APPEND CHRE_CXXFLAGS
       -DCONFIG_CHRE_NSYMBOLS_VAR=${CONFIG_CHRE_SYMTAB_PREFIX}_nexports)
  # #########################COMMON Compiler Flags##########################

  # Configure 'all' and 'extra' warnings and promote warnings to errors.
  list(APPEND CHRE_CXXFLAGS -Wall)
  list(APPEND CHRE_CXXFLAGS -Werror)
  list(APPEND CHRE_CXXFLAGS -Wextra)

  # Disabled warnings. You better have a good reason to add more here.
  list(APPEND CHRE_CXXFLAGS -Wno-unused-parameter)
  # Additional warnings. Even more! :]
  list(APPEND CHRE_CXXFLAGS -Wshadow)
  list(APPEND CHRE_CXXFLAGS -Wdouble-promotion)

  # Disable exceptions and RTTI.
  list(APPEND CHRE_CXXFLAGS -fno-exceptions)
  list(APPEND CHRE_CXXFLAGS -fno-rtti)

  # Enable the linker to garbage collect unused code and variables.
  list(APPEND CHRE_CXXFLAGS -fdata-sections)
  list(APPEND CHRE_CXXFLAGS -ffunction-sections)

  # Compile with hidden visibility by default.
  list(APPEND CHRE_CXXFLAGS -fvisibility=hidden)

  # Common Archive Flags
  # ############################################################################
  list(APPEND COMMON_ARFLAGS rsc)
  list(APPEND CHRE_CXXFLAGS -DCHRE_PATCH_VERSION=${CHRE_PATCH_VERSION})
  list(APPEND CHRE_CXXFLAGS -DCHRE_INCLUDE_DEFAULT_STATIC_NANOAPPS)
  list(APPEND CHRE_CXXFLAGS -DCHRE_NANOAPP_INTERNAL)
  list(APPEND CHRE_CXXFLAGS -Wno-error=array-compare)
  list(APPEND CHRE_CXXFLAGS -Wno-error=shadow)
  list(APPEND CHRE_CXXFLAGS -DCHRE_MESSAGE_HEAD_SIZE=256)
  list(APPEND CHRE_CXXFLAGS -DCHRE_PLATFORM_ID=0x476f6f676c000001)
  list(APPEND CHRE_CXXFLAGS -DCHRE_VARIANT_SUPPLIES_STATIC_NANOAPP_LIST)
  list(APPEND CHRE_CXXFLAGS -DNANOAPP_MINIMUM_LOG_LEVEL=CHRE_LOG_LEVEL_DEBUG)

  # Include directories
  set(CHRE_INCLUDE_DIRS
      ${CHRE_PREFIX}/chpp/include
      ${CHRE_PREFIX}/chre_api/include
      ${CHRE_PREFIX}/chre_api/include/chre_api
      ${CHRE_PREFIX}/core/include
      ${CHRE_PREFIX}/pal/include
      ${CHRE_PREFIX}/platform/include
      ${CHRE_PREFIX}/platform/shared/include
      ${CHRE_PREFIX}/util/include
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${CHRE_PREFIX}/apps/include
      ${CHRE_PREFIX}/platform/shared/sensor_pal/include
      ${CHRE_PREFIX}/platform/nuttx/sim/include
      ${CHRE_PREFIX}/platform/nuttx/include
      ${CHRE_PREFIX}/platform/nuttx/include/nanoapp/include
      ${CHRE_PREFIX}/external/flatbuffers/include
      ${CHRE_PREFIX}/host/nuttx/include
      ${PIGWEED_DIR}/pw_assert/public
      ${PIGWEED_DIR}/pw_string/public
      ${PIGWEED_DIR}/pw_bytes/public
      ${PIGWEED_DIR}/pw_base64/public
      ${PIGWEED_DIR}/pw_polyfill/public
      ${PIGWEED_DIR}/pw_polyfill/standard_library_public
      ${PIGWEED_DIR}/pw_preprocessor/public
      ${PIGWEED_DIR}/pw_tokenizer/public
      ${PIGWEED_DIR}/pw_varint/public
      ${PIGWEED_DIR}/pw_span/public
      ${PIGWEED_DIR}/pw_containers/public
      ${PIGWEED_DIR}/pw_rpc/public
      ${PIGWEED_DIR}/pw_result/public
      ${PIGWEED_DIR}/pw_status/public
      ${PIGWEED_DIR}/pw_stream/public
      ${PIGWEED_DIR}/pw_protobuf/public
      ${PIGWEED_DIR}/pw_sync/public
      ${PIGWEED_DIR}/pw_sync_stl/public
      ${PIGWEED_DIR}/pw_sync_stl/public_overrides
      ${PIGWEED_DIR}/pw_toolchain/public
      ${PIGWEED_DIR}/third_party/fuchsia/repo/sdk/lib/stdcompat/include
      ${PW_RPC_GEN_PATH}
      ${PW_RPC_GEN_PATH}/${PIGWEED_DIR}
      ${CHRE_PIGWEED_DIR}/pw_assert_nanoapp/public_overrides
      ${CHRE_PIGWEED_DIR}/pw_log_nanoapp/public_overrides
      ${NUTTX_APPS_DIR}/netutils/jsoncpp/jsoncpp/include
      ${NUTTX_APPS_DIR}/external/android/system/logging/include
      ${NUTTX_APPS_DIR}/netutils/connectedhomeip/pigweed/pw_log/public)

  # CHRE Library SRCS
  set(CHRE_LIBRARY_SRCS
      ${CHRE_PREFIX}/platform/nuttx/context.cc
      ${CHRE_PREFIX}/platform/nuttx/host_link.cc
      ${CHRE_PREFIX}/platform/nuttx/rpc_socket.cc
      ${CHRE_PREFIX}/platform/nuttx/memory.cc
      ${CHRE_PREFIX}/platform/nuttx/memory_manager.cc
      ${CHRE_PREFIX}/platform/nuttx/power_control_manager.cc
      ${CHRE_PREFIX}/platform/nuttx/system_time.cc
      ${CHRE_PREFIX}/platform/nuttx/system_timer.cc
      ${CHRE_PREFIX}/platform/nuttx/fatal_error.cc
      ${CHRE_PREFIX}/platform/nuttx/chre_api_re.cc
      ${CHRE_PREFIX}/platform/nuttx/platform_debug_dump_manager.cc
      ${CHRE_PREFIX}/platform/nuttx/platform_pal.cc
      ${CHRE_PREFIX}/platform/nuttx/task_util/task.cc
      ${CHRE_PREFIX}/platform/nuttx/task_util/task_manager.cc
      ${CHRE_PREFIX}/platform/nuttx/pal_gnss.cc
      ${CHRE_PREFIX}/platform/shared/chre_api_audio.cc
      ${CHRE_PREFIX}/platform/shared/chre_api_ble.cc
      ${CHRE_PREFIX}/platform/shared/chre_api_core.cc
      ${CHRE_PREFIX}/platform/shared/chre_api_gnss.cc
      ${CHRE_PREFIX}/platform/shared/chre_api_re.cc
      ${CHRE_PREFIX}/platform/shared/chre_api_sensor.cc
      ${CHRE_PREFIX}/platform/shared/chre_api_user_settings.cc
      ${CHRE_PREFIX}/platform/shared/chre_api_version.cc
      ${CHRE_PREFIX}/platform/shared/chre_api_wifi.cc
      ${CHRE_PREFIX}/platform/shared/chre_api_wwan.cc
      ${CHRE_PREFIX}/platform/shared/host_protocol_chre.cc
      ${CHRE_PREFIX}/platform/shared/host_protocol_common.cc
      ${CHRE_PREFIX}/platform/shared/host_link.cc
      ${CHRE_PREFIX}/platform/shared/memory_manager.cc
      ${CHRE_PREFIX}/platform/shared/nanoapp_load_manager.cc
      ${CHRE_PREFIX}/platform/shared/nanoapp/nanoapp_dso_util.cc
      ${CHRE_PREFIX}/platform/shared/pal_system_api.cc
      ${CHRE_PREFIX}/platform/shared/system_time.cc
      ${CHRE_PREFIX}/platform/shared/tracing.cc
      ${CHRE_PREFIX}/platform/shared/version.cc
      ${CHRE_PREFIX}/platform/shared/platform_gnss.cc
      ${CHRE_PREFIX}/core/debug_dump_manager.cc
      ${CHRE_PREFIX}/core/event.cc
      ${CHRE_PREFIX}/core/event_loop.cc
      ${CHRE_PREFIX}/core/event_loop_manager.cc
      ${CHRE_PREFIX}/core/event_ref_queue.cc
      ${CHRE_PREFIX}/core/host_comms_manager.cc
      ${CHRE_PREFIX}/core/init.cc
      ${CHRE_PREFIX}/core/nanoapp.cc
      ${CHRE_PREFIX}/core/settings.cc
      ${CHRE_PREFIX}/core/static_nanoapps.cc
      ${CHRE_PREFIX}/core/system_health_monitor.cc
      ${CHRE_PREFIX}/core/host_endpoint_manager.cc
      ${CHRE_PREFIX}/core/timer_pool.cc
      ${CHRE_PREFIX}/util/buffer_base.cc
      ${CHRE_PREFIX}/util/dynamic_vector_base.cc
      ${CHRE_PREFIX}/util/intrusive_list_base.cc
      ${CHRE_PREFIX}/util/nanoapp/audio.cc
      ${CHRE_PREFIX}/util/nanoapp/ble.cc
      ${CHRE_PREFIX}/util/nanoapp/callbacks.cc
      ${CHRE_PREFIX}/util/nanoapp/debug.cc
      ${CHRE_PREFIX}/util/nanoapp/wifi.cc
      ${CHRE_PREFIX}/util/system/event_callbacks.cc
      ${CHRE_PREFIX}/util/system/debug_dump.cc
      ${CHRE_PREFIX}/variant/nuttx/static_nanoapps.cc
      ${PIGWEED_DIR}/pw_tokenizer/detokenize.cc
      ${PIGWEED_DIR}/pw_tokenizer/decode.cc
      ${PIGWEED_DIR}/pw_varint/varint.cc)

  nuttx_append_source_file_properties(${PIGWEED_DIR}/pw_tokenizer/decode.cc
                                      COMPILE_FLAGS -Wno-double-promotion)

  # CHRE WASM Framework
  if(CONFIG_CHRE_WASM)
    list(APPEND CHRE_CXXFLAGS -DCHRE_WASM)
    list(APPEND CHRE_LIBRARY_SRCS
         ${CHRE_PREFIX}/platform/nuttx/wasm/chre_wasm_embed.cc)
  endif()

  # Optional audio support
  if(CONFIG_CHRE_AUDIO_SUPPORT_ENABLED)
    list(APPEND CHRE_CXXFLAGS -DCHRE_AUDIO_SUPPORT_ENABLED)
    list(APPEND CHRE_LIBRARY_SRCS CHRE_LIBRARY_SRCS
         "${CHRE_PREFIX}/core/audio_request_manager.cc")
  endif()

  # Optional BLE support.
  if(CONFIG_CHRE_BLE_SUPPORT_ENABLED)
    list(APPEND CHRE_CXXFLAGS -DCHRE_BLE_SUPPORT_ENABLED)
    list(APPEND CHRE_LIBRARY_SRCS ${CHRE_PREFIX}/core/ble_request.cc)
    list(APPEND CHRE_LIBRARY_SRCS ${CHRE_PREFIX}/core/ble_request_manager.cc)
    list(APPEND CHRE_LIBRARY_SRCS
         ${CHRE_PREFIX}/core/ble_request_multiplexer.cc)
  endif()

  # Optional GNSS support
  if(CONFIG_CHRE_GNSS_SUPPORT_ENABLED)
    list(APPEND CHRE_CXXFLAGS -DCHRE_GNSS_SUPPORT_ENABLED)
    list(APPEND CHRE_LIBRARY_SRCS "${CHRE_PREFIX}/core/gnss_manager.cc")
  endif()

  # Optional sensor support
  if(CONFIG_CHRE_SENSORS_SUPPORT_ENABLED)
    list(APPEND CHRE_CXXFLAGS -DCHRE_SENSORS_SUPPORT_ENABLED)
    list(
      APPEND
      CHRE_LIBRARY_SRCS
      "${CHRE_PREFIX}/core/sensor.cc"
      "${CHRE_PREFIX}/core/sensor_request.cc"
      "${CHRE_PREFIX}/core/sensor_request_manager.cc"
      "${CHRE_PREFIX}/core/sensor_request_multiplexer.cc"
      "${CHRE_PREFIX}/core/sensor_type.cc"
      "${CHRE_PREFIX}/core/sensor_type_helpers.cc")
  endif()

  # Optional WiFi support
  if(CONFIG_CHRE_WIFI_SUPPORT_ENABLED)
    list(APPEND CHRE_CXXFLAGS -DCHRE_WIFI_SUPPORT_ENABLED)
    list(APPEND CHRE_LIBRARY_SRCS "${CHRE_PREFIX}/core/wifi_request_manager.cc"
         "${CHRE_PREFIX}/core/wifi_scan_request.cc")
  endif()

  # Optional WWAN support
  if(CONFIG_CHRE_WWAN_SUPPORT_ENABLED)
    list(APPEND CHRE_CXXFLAGS -DCHRE_WWAN_SUPPORT_ENABLED)
    list(APPEND CHRE_LIBRARY_SRCS "${CHRE_PREFIX}/core/wwan_request_manager.cc")
  endif()

  # Optional Telemetry support.
  if(CONFIG_CHRE_TELEMETRY_SUPPORT_ENABLED)
    list(APPEND CHRE_CXXFLAGS -DCHRE_TELEMETRY_SUPPORT_ENABLED)
    list(APPEND CHRE_CXXFLAGS -DPB_FIELD_32BIT)
    list(APPEND CHRE_LIBRARY_SRCS ${CHRE_PREFIX}/core/telemetry_manager.cc)
    set(NANOPB_EXTENSION nanopb)
    list(APPEND NANOPB_SRCS ${CHRE_PREFIX}/core/chre_metrics.proto)
    # include(${CHRE_PREFIX}/build/nanopb.cmake) # TODO
  endif()

  # NanoApp SRCS
  list(
    APPEND
    CHRE_LIBRARY_SRCS
    ${CHRE_PREFIX}/apps/hello_world/hello_world.cc
    ${CHRE_PREFIX}/apps/message_world/message_world.cc
    ${CHRE_PREFIX}/apps/spammer/spammer.cc
    ${CHRE_PREFIX}/apps/timer_world/timer_world.cc
    ${CHRE_PREFIX}/apps/unload_tester/unload_tester.cc
    ${CHRE_PREFIX}/apps/debug_dump_world/debug_dump_world.cc)

  list(APPEND CHRE_CXXFLAGS
       -DCHRE_MESSAGE_TO_HOST_MAX_SIZE=${CONFIG_CHRE_MESSAGE_TO_HOST_MAX_SIZE})

  list(APPEND CHRE_CXXFLAGS -DCHRE_FILENAME=__FILE__)

  list(APPEND CHRE_CXXFLAGS -DCHRE_MINIMUM_LOG_LEVEL=CHRE_LOG_LEVEL_INFO)

  if(DEFINED CONFIG_CHRE_ASSERTIONS)
    list(APPEND CHRE_CXXFLAGS -DCHRE_ASSERTIONS_ENABLED)
  else()
    list(APPEND CHRE_CXXFLAGS -DCHRE_ASSERTIONS_DISABLED)
  endif()

  list(APPEND CHRE_CXXFLAGS
       -DCHRE_FIRST_SUPPORTED_API_VERSION=CHRE_API_VERSION_1_5)

  list(APPEND CHRE_CXXFLAGS
       -DCHRE_FIRST_SUPPORTED_API_VERSION=CHRE_API_VERSION_1_5)

  # Generate symbol table for CHRE
  add_custom_target(
    generate_exports_chre_api_list
    COMMAND
      ${CMAKE_C_COMPILER} -E -P -x c
      ${CHRE_PREFIX}/platform/nuttx/exports_chre_api.txt -o
      ${CMAKE_CURRENT_BINARY_DIR}/exports_chre_api.txt.i)

  set(SYMTABEXT ${CONFIG_CHRE_SYMTAB_PREFIX} -a
                ${CMAKE_CURRENT_BINARY_DIR}/exports_chre_api.txt.i)

  set(SYMTAB_CHRE_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/symtab_chre.c)

  add_custom_command(
    OUTPUT ${SYMTAB_CHRE_SOURCE}
    COMMAND ${NUTTX_APPS_DIR}/tools/mksymtab.sh ${CMAKE_BINARY_DIR}/bin
            ${SYMTABEXT} > ${CMAKE_CURRENT_BINARY_DIR}/symtab_chre.c
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS chre generate_exports_chre_api_list
    VERBATIM)

  list(APPEND CHRE_SYMTAB_CFLAGS ${NO_LTO})
  list(APPEND CHRE_SYMTAB_CFLAGS -fno-builtin)
  nuttx_add_library(symtab_chre)
  target_sources(symtab_chre PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/symtab_chre.c)
  target_compile_options(symtab_chre PRIVATE ${CHRE_SYMTAB_CFLAGS})

  nuttx_add_library(chre STATIC)
  target_sources(chre PRIVATE ${CHRE_LIBRARY_SRCS})
  target_include_directories(chre PUBLIC ${CHRE_INCLUDE_DIRS})
  target_compile_options(chre PRIVATE ${CHRE_CXXFLAGS})

  nuttx_add_application(
    MODULE
    ${CONFIG_CHRE}
    NAME
    ${CONFIG_CHRE_PROGNAME}
    STACKSIZE
    ${CONFIG_CHRE_STACKSIZE}
    PRIORITY
    ${CONFIG_CHRE_PRIORITY}
    SRCS
    ${CHRE_PREFIX}/platform/nuttx/init.cc
    ${CHRE_PREFIX}/platform/nuttx/assert.cc
    ${CHRE_PREFIX}/platform/nuttx/platform_nanoapp.cc
    COMPILE_FLAGS
    ${CHRE_CXXFLAGS}
    DEPENDS
    chre
    symtab_chre)

endif()
