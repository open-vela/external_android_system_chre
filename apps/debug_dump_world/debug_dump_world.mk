#
# Debug Dump World Makefile
#

# Common Compiler Flags ########################################################

# Include paths.
COMMON_CFLAGS += -I$(CHRE_PREFIX)/apps/debug_dump_world/include

# Common Source Files ##########################################################

COMMON_SRCS += $(CHRE_PREFIX)/apps/debug_dump_world/debug_dump_world.cc
