#
# Host Awake World Makefile
#

# Common Compiler Flags ########################################################

# Include paths.
COMMON_CFLAGS += -I$(CHRE_PREFIX)/apps/host_awake_world/include

# Common Source Files ##########################################################

COMMON_SRCS += $(CHRE_PREFIX)/apps/host_awake_world/host_awake_world.cc
