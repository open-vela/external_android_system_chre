#
# Apps Makefile
#

# Common Compiler Flags ########################################################

COMMON_CFLAGS += -I$(CHRE_PREFIX)/apps/include

# App makefiles ################################################################

ifeq ($(CHRE_AUDIO_SUPPORT_ENABLED), true)
include $(CHRE_PREFIX)/apps/audio_world/audio_world.mk
endif

ifeq ($(CHRE_BLE_SUPPORT_ENABLED), true)
include $(CHRE_PREFIX)/apps/ble_world/ble_world.mk
endif

ifeq ($(CHRE_GNSS_SUPPORT_ENABLED), true)
include $(CHRE_PREFIX)/apps/gnss_world/gnss_world.mk
endif

ifeq ($(CHRE_WIFI_SUPPORT_ENABLED), true)
include $(CHRE_PREFIX)/apps/wifi_world/wifi_world.mk
endif

ifeq ($(CHRE_WWAN_SUPPORT_ENABLED), true)
include $(CHRE_PREFIX)/apps/wwan_world/wwan_world.mk
endif

include $(CHRE_PREFIX)/apps/debug_dump_world/debug_dump_world.mk
include $(CHRE_PREFIX)/apps/hello_world/hello_world.mk
include $(CHRE_PREFIX)/apps/host_awake_world/host_awake_world.mk
include $(CHRE_PREFIX)/apps/message_world/message_world.mk
include $(CHRE_PREFIX)/apps/sensor_world/sensor_world.mk
include $(CHRE_PREFIX)/apps/spammer/spammer.mk
include $(CHRE_PREFIX)/apps/timer_world/timer_world.mk
include $(CHRE_PREFIX)/apps/unload_tester/unload_tester.mk
