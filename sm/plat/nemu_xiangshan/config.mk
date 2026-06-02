#
# SPDX-License-Identifier: BSD-2-Clause
#
# Xiangshan (NEMU) platform build configuration
#

# Compiler flags
platform-cppflags-y = -I$(src_dir)/include -I$(src)/../src
platform-cflags-y = -I../src
platform-asflags-y =
platform-ldflags-y =

# Blobs to build
FW_TEXT_START=0x80000000
FW_DYNAMIC=y
FW_JUMP=y
ifeq ($(PLATFORM_RISCV_XLEN), 32)
  FW_JUMP_ADDR=$(shell printf "0x%X" $$(($(FW_TEXT_START) + 0x400000)))
else
  FW_JUMP_ADDR=$(shell printf "0x%X" $$(($(FW_TEXT_START) + 0x200000)))
endif
FW_JUMP_FDT_ADDR=$(shell printf "0x%X" $$(($(FW_TEXT_START) + 0x2200000)))
FW_PAYLOAD=y
ifeq ($(PLATFORM_RISCV_XLEN), 32)
  FW_PAYLOAD_OFFSET=0x400000
else
  FW_PAYLOAD_OFFSET=0x200000
endif
FW_PAYLOAD_FDT_ADDR=$(FW_JUMP_FDT_ADDR)
