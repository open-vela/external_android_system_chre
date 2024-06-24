include $(APPDIR)/Make.defs
CXXEXT = .cc

ANDROID_BUILD_TOP = $(APPDIR)/external/android
PATH_CHRE = $(ANDROID_BUILD_TOP)/system/chre/chre

NANOAPP_IS_SYSTEM_NANOAPP = 0
NANOAPP_VENDOR_STRING = "\"Xiaomi\""

include $(PATH_CHRE)/build/variant/nuttx.mk

# Nanoapp Header
#   From: build/build_template.mk
include $(shell pwd)/out/current_chre_api_version.mk
ifeq ($(CHRE_API_VERSION_MAJOR),)
TARGET_CHRE_API_VERSION_MAJOR = $(DEFAULT_CHRE_API_VERSION_MAJOR)
endif
ifeq ($(CHRE_API_VERSION_MINOR),)
TARGET_CHRE_API_VERSION_MINOR = $(DEFAULT_CHRE_API_VERSION_MINOR)
endif
TARGET_NANOAPP_FLAGS ?= 0x00000001
BE_TO_LE_SCRIPT = $(PATH_CHRE)/build/be_to_le.sh

ifneq ($(V), 0)
CHRE_BUILD_VERBOSE = true
endif

ifeq ($(BUILD_ID),)
# If BUILD_ID is unset this must be a local build.
BUILD_ID = local
endif

COMMON_CFLAGS += -DCHRE_MINIMUM_LOG_LEVEL=CHRE_LOG_LEVEL_DEBUG
COMMON_CFLAGS += -I$(PATH_CHRE)/platform/shared/sensor_pal/include
COMMON_CFLAGS += -I$(PATH_CHRE)/chre_api/include/chre_api
COMMON_CFLAGS += -I$(PATH_CHRE)/chre_api/include
COMMON_CFLAGS += -I$(PATH_CHRE)/platform/include
COMMON_CFLAGS += -I$(PATH_CHRE)/platform/nuttx/include/
COMMON_CFLAGS += -I$(PATH_CHRE)/util/include

ifeq ($(CHRE_PATCH_VERSION),)
ifeq ($(CHRE_HOST_OS),Darwin)
DATE_CMD=gdate
else
DATE_CMD=date
endif

# Compute the patch version as the number of hours since the start of some
# arbitrary epoch. This will roll over 16 bits after ~7 years, but patch version
# is scoped to the API version, so we can adjust the offset when a new API
# version is released.
EPOCH=$(shell $(DATE_CMD) --date='2017-01-01' +%s)
CHRE_PATCH_VERSION = $(shell echo $$(( $$(((`$(DATE_CMD) +%s` - $(EPOCH)) / (60 * 60))) % 65535)))
endif

COMMON_CFLAGS += -DCHRE_PATCH_VERSION=$(CHRE_PATCH_VERSION)

ifeq ($(NANOAPP_NAME),)
$(error "The NANOAPP_NAME variable must be set to the name of the nanoapp. \
         This should be assigned by the Makefile that includes app.mk.")
endif

NANOAPP_UNSTABLE_ID = "nanoapp=$(NANOAPP_NAME)@$(BUILD_ID)"

COMMON_CFLAGS += -DNANOAPP_ID=$(NANOAPP_ID)
COMMON_CFLAGS += -DNANOAPP_VERSION=$(NANOAPP_VERSION)
COMMON_CFLAGS += -DNANOAPP_VENDOR_STRING=$(NANOAPP_VENDOR_STRING)
COMMON_CFLAGS += -DNANOAPP_NAME_STRING=$(NANOAPP_NAME_STRING)
COMMON_CFLAGS += -DNANOAPP_IS_SYSTEM_NANOAPP=$(NANOAPP_IS_SYSTEM_NANOAPP)
COMMON_CFLAGS += -DNANOAPP_UNSTABLE_ID="\"$(NANOAPP_UNSTABLE_ID)\""

COMMON_CFLAGS += -UCHRE_NANOAPP_INTERNAL
COMMON_CFLAGS += -DCHRE_IS_NANOAPP_BUILD
COMMON_CFLAGS += -I$(shell pwd)
COMMON_CFLAGS += -I$(PATH_CHRE)/platform/shared/nanoapp/include

CXXFLAGS += $(TARGET_CFLAGS)
CXXFLAGS += $(COMMON_CFLAGS)
CXXFLAGS += -DCHRE_FILENAME=__FILE__
CXXFLAGS += -DCHRE_PLATFORM_ID=$(TARGET_PLATFORM_ID)

BIN = libchrenanoapp$(LIBEXT)
LDLIBS += $(BIN)

ifneq ($(NUTTX_DSO_PROGNAME),)
  PROGNAME = $(NUTTX_DSO_PROGNAME)
else
  PROGNAME = $(shell basename $(shell pwd))
endif

DYNLIB = y

MAINSRC = $(wildcard $(shell pwd)/*$(CXXEXT))
CXXSRCS += $(PATH_CHRE)/platform/nuttx/dynamic_nanoapp.cc

include $(APPDIR)/Application.mk
