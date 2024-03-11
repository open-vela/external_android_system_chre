#
# Rpc World Makefile
#

# Common Compiler Flags ########################################################

# Include paths.
COMMON_CFLAGS += -I$(CHRE_PREFIX)/apps/rpc_world/inc

# Common Source Files ##########################################################

COMMON_SRCS += $(CHRE_PREFIX)/apps/rpc_world/rpc_world.cc
COMMON_SRCS += $(CHRE_PREFIX)/apps/rpc_world/rpc_world_manager.cc