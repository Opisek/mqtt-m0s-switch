# Original code by Bouffalo Lab
# Modified by Opisek

BL_SDK_BASE ?= $(BOUFFALO_SDK_PATH)

export BL_SDK_BASE

CHIP ?= bl616
BOARD ?= bl616dk
CROSS_COMPILE ?= riscv64-unknown-elf-

include $(BL_SDK_BASE)/project.build