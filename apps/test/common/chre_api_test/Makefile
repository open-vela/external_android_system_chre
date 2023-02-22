#
# CHRE API Test Nanoapp Makefile
#
# Environment Checks ###########################################################
ifeq ($(CHRE_PREFIX),)
  ifneq ($(ANDROID_BUILD_TOP),)
    CHRE_PREFIX = $(ANDROID_BUILD_TOP)/system/chre
  else
    $(error "You must run 'lunch' to setup ANDROID_BUILD_TOP, or explicitly \
    define the CHRE_PREFIX environment variable to point to the CHRE root \
    directory.")
  endif
endif

# Nanoapp Configuration ########################################################

NANOAPP_NAME = chre_api_test
NANOAPP_ID = 0x476f6f675400000d
NANOAPP_NAME_STRING = \"CHRE\ API\ Test\"
NANOAPP_VERSION = 0x00000001

NANOAPP_PATH = $(CHRE_PREFIX)/apps/test/common/chre_api_test


# Source Code ##################################################################

COMMON_SRCS += $(NANOAPP_PATH)/src/chre_api_test_manager.cc
COMMON_SRCS += $(NANOAPP_PATH)/src/chre_api_test.cc

# Compiler Flags ###############################################################

# Defines
COMMON_CFLAGS += -DNANOAPP_MINIMUM_LOG_LEVEL=CHRE_LOG_LEVEL_DEBUG
COMMON_CFLAGS += -DLOG_TAG=\"[ChreApiTest]\"
COMMON_CFLAGS += -DCHRE_ASSERTIONS_ENABLED

# Includes
COMMON_CFLAGS += -I$(NANOAPP_PATH)/inc

# Permission declarations ######################################################

CHRE_NANOAPP_USES_BLE = true

# PW RPC protos ################################################################

PW_RPC_SRCS = $(NANOAPP_PATH)/rpc/chre_api_test.proto

# Makefile Includes ############################################################

include $(CHRE_PREFIX)/build/nanoapp/app.mk
