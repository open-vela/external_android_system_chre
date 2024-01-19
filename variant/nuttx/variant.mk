#
# Vela-Specific CHRE Makefile
#

VARIANT_PREFIX = $(CHRE_PREFIX)/variant

# Version String ###############################################################

COMMIT_HASH_COMMAND = git describe --always --long --dirty

VERSION_STRING = chre=$(shell $(COMMIT_HASH_COMMAND))

COMMON_CFLAGS += -DCHRE_VERSION_STRING='"$(VERSION_STRING)"'

# Common Compiler Flags ########################################################

# Supply a symbol to indicate that the build variant supplies the static
# nanoapp list.
COMMON_CFLAGS += -DCHRE_VARIANT_SUPPLIES_STATIC_NANOAPP_LIST

# Optional Features ############################################################

OPT_LEVEL ?= 0
CHRE_AUDIO_SUPPORT_ENABLED = false
CHRE_BLE_SUPPORT_ENABLED = false
CHRE_GNSS_SUPPORT_ENABLED = false
CHRE_SENSORS_SUPPORT_ENABLED = false
CHRE_WIFI_SUPPORT_ENABLED = false
CHRE_WIFI_NAN_SUPPORT_ENABLED = false
CHRE_WWAN_SUPPORT_ENABLED = false

ifneq ($(CONFIG_CHRE_AUDIO_SUPPORT_ENABLED),)
CHRE_AUDIO_SUPPORT_ENABLED = true
endif
ifneq ($(CONFIG_CHRE_BLE_SUPPORT_ENABLED),)
CHRE_BLE_SUPPORT_ENABLED = true
endif
ifneq ($(CONFIG_CHRE_GNSS_SUPPORT_ENABLED),)
CHRE_GNSS_SUPPORT_ENABLED = true
endif
ifneq ($(CONFIG_CHRE_SENSORS_SUPPORT_ENABLED),)
CHRE_SENSORS_SUPPORT_ENABLED = true
endif
ifneq ($(CONFIG_CHRE_WIFI_SUPPORT_ENABLED),)
CHRE_WIFI_SUPPORT_ENABLED = true
endif
ifneq ($(CONFIG_CHRE_WIFI_NAN_SUPPORT_ENABLED),)
CHRE_WIFI_NAN_SUPPORT_ENABLED = true
endif
ifneq ($(CONFIG_CHRE_WWAN_SUPPORT_ENABLED),)
CHRE_WWAN_SUPPORT_ENABLED = true
endif

# Common Source Files ##########################################################

COMMON_SRCS += $(VARIANT_PREFIX)/nuttx/static_nanoapps.cc
