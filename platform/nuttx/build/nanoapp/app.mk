include $(APPDIR)/Make.defs
CXXEXT = .cc
OUT=$(shell pwd)/out
HEADER_SUFFIX = ".napp_header"

ANDROID_BUILD_TOP = $(APPDIR)/external/android
PATH_CHRE = $(ANDROID_BUILD_TOP)/system/chre/chre

NANOAPP_IS_SYSTEM_NANOAPP = 0
NANOAPP_VENDOR_STRING = "\"Xiaomi\""

include $(PATH_CHRE)/build/variant/nuttx.mk

# Nanoapp Header
#   From: build/build_template.mk
CHRE_HOST_CC ?= $(HOSTCC)
TARGET_NANOAPP_FLAGS ?= 0x00000001
BE_TO_LE_SCRIPT = $(PATH_CHRE)/build/be_to_le.sh

PRINT_CURRENT_CHRE_API_VERSION_SRCS = $(PATH_CHRE)/build/print_current_chre_api_version.c
PRINT_CURRENT_CHRE_API_VERSION_BIN = $(OUT)/print_current_chre_api_version
CURRENT_CHRE_API_VERSION_MK = $(OUT)/current_chre_api_version.mk

$(PRINT_CURRENT_CHRE_API_VERSION_BIN): $(PRINT_CURRENT_CHRE_API_VERSION_SRCS)
	$(Q)mkdir -p $(OUT)
	$(Q)$(CHRE_HOST_CC) -I$(PATH_CHRE)/chre_api/include/chre_api $^ -o $@

$(CURRENT_CHRE_API_VERSION_MK): $(PRINT_CURRENT_CHRE_API_VERSION_BIN)
	$(Q)$< > $@

# Only include default version if this is not a clean operation.
ifeq ($(filter clean, $(MAKECMDGOALS)),)
-include $(CURRENT_CHRE_API_VERSION_MK)
endif

ifeq ($(CHRE_API_VERSION_MAJOR),)
TARGET_CHRE_API_VERSION_MAJOR = $(DEFAULT_CHRE_API_VERSION_MAJOR)
endif

ifeq ($(CHRE_API_VERSION_MINOR),)
TARGET_CHRE_API_VERSION_MINOR = $(DEFAULT_CHRE_API_VERSION_MINOR)
endif

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

install::
	printf "00000000  %.8x " `$(BE_TO_LE_SCRIPT) 0x00000001` > $(NAPP_HEADER)
	printf "%.8x " `$(BE_TO_LE_SCRIPT) 0x4f4e414e` >> $(NAPP_HEADER)
	printf "%.16x\n" `$(BE_TO_LE_SCRIPT) $(NANOAPP_ID)` >> $(NAPP_HEADER)
	printf "00000010  %.8x " `$(BE_TO_LE_SCRIPT) $(NANOAPP_VERSION)` >> $(NAPP_HEADER)
	printf "%.8x " `$(BE_TO_LE_SCRIPT) $(TARGET_NANOAPP_FLAGS)` >> $(NAPP_HEADER)
	printf "%.16x\n" `$(BE_TO_LE_SCRIPT) $(TARGET_PLATFORM_ID)` >> $(NAPP_HEADER)
	printf "00000020  %.2x " \
		`$(BE_TO_LE_SCRIPT) $(TARGET_CHRE_API_VERSION_MAJOR)` >> $(NAPP_HEADER)
	printf "%.2x " \
		`$(BE_TO_LE_SCRIPT) $(TARGET_CHRE_API_VERSION_MINOR)` >> $(NAPP_HEADER)
	printf "%.12x \n" `$(BE_TO_LE_SCRIPT) 0x000000` >> $(NAPP_HEADER)
	cp $(NAPP_HEADER) $(NAPP_HEADER)@_ascii
	xxd -r $(NAPP_HEADER)@_ascii > $(NAPP_HEADER)
	cp -v $(NAPP_HEADER) $(BINDIR)

BIN = libchrenanoapp$(LIBEXT)
LDLIBS += $(BIN)

ifneq ($(NUTTX_DSO_PROGNAME),)
  PROGNAME = $(NUTTX_DSO_PROGNAME)
else
  PROGNAME = $(shell basename $(shell pwd))
endif
NAPP_HEADER = $(PROGNAME)$(HEADER_SUFFIX)

DYNLIB = y

MAINSRC = $(wildcard $(shell pwd)/*$(CXXEXT))
CXXSRCS += $(PATH_CHRE)/platform/nuttx/dynamic_nanoapp.cc

include $(APPDIR)/Application.mk
