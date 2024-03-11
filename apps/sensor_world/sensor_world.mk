#
# Sensor World Makefile
#

# Common Compiler Flags ########################################################

# Include paths.
COMMON_CFLAGS += -I$(CHRE_PREFIX)/apps/sensor_world/include

# Common Source Files ##########################################################

COMMON_SRCS += $(CHRE_PREFIX)/apps/sensor_world/sensor_world.cc
