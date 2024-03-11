#
# Audio World Makefile
#

# Common Compiler Flags ########################################################

# Include paths.
COMMON_CFLAGS += -I$(CHRE_PREFIX)/apps/audio_world/include

# Common Source Files ##########################################################

COMMON_SRCS += $(CHRE_PREFIX)/apps/audio_world/audio_world.cc
