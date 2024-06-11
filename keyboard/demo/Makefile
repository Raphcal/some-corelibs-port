HEAP_SIZE      = 8388208
STACK_SIZE     = 61800

PRODUCT = pd-keyboard.pdx

# Locate the SDK
SDK = ${PLAYDATE_SDK_PATH}
ifeq ($(SDK),)
	SDK = $(shell egrep '^\s*SDKRoot' ~/.Playdate/config | head -n 1 | cut -c9-)
endif

ifeq ($(SDK),)
$(error SDK path not found; set ENV value PLAYDATE_SDK_PATH)
endif

######
# IMPORTANT: You must add your source folders to VPATH for make to find them
# ex: VPATH += src1:src2
######

VPATH += src:../src

# List C source files here
SRC = $(wildcard src/*.c) $(wildcard ../src/*.c)

# List all user directories here
UINCDIR =

# List user asm files
UASRC =

# List all user C define here, like -D_DEBUG=1
UDEFS =

# Define ASM defines here
UADEFS =

# List the user directory to look for the libraries here
ULIBDIR =

# List all user libraries here
ULIBS =

copy_assets: all
	mkdir -p Source/CoreLibs/assets/keyboard
	mkdir -p Source/CoreLibs/assets/sfx
	cp $(SDK)/CoreLibs/assets/keyboard/* Source/CoreLibs/assets/keyboard
	cp $(SDK)/CoreLibs/assets/sfx/* Source/CoreLibs/assets/sfx
	cp $(SDK)/Resources/Fonts/Asheville/Asheville\ Sans\ 14\ Bold/* Source

include $(SDK)/C_API/buildsupport/common.mk
