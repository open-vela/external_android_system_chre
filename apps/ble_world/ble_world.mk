#
# BLE World Makefile
#

# Common Compiler Flags ########################################################

# Include paths.
COMMON_CFLAGS += -I$(CHRE_PREFIX)/apps/ble_world/include
COMMON_CFLAGS += -Wno-error=implicit-fallthrough=
# Common Source Files ##########################################################

COMMON_SRCS += $(CHRE_PREFIX)/apps/ble_world/ble_world.cc
