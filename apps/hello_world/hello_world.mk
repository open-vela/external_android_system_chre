#
# Hello World Makefile
#

# Common Compiler Flags ########################################################

# Include paths.
COMMON_CFLAGS += -I$(CHRE_PREFIX)/apps/hello_world/include

# Common Source Files ##########################################################

COMMON_SRCS += $(CHRE_PREFIX)/apps/hello_world/hello_world.cc
