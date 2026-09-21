# x64HK - x86-64 Hobby Kernel
# Run `make help` for a list of targets.

.DEFAULT_GOAL = all

PROJECT   := x64HK
SRC_DIR   := Kernel
LINKER_SCRIPT := $(SRC_DIR)/Boot/Sections.ld

include Tools/Build.mk
include Tools/Image.mk
include Tools/Qemu.mk

all: $(KERNEL32)

# Indentation is spaces; this puts it back that way after an editor gets it wrong.
untabify:
	$(VERBOSE) ./Tools/Untabify.sh

untabify-check:
	$(VERBOSE) ./Tools/Untabify.sh --check

help::
	@printf '\n%s\n%s\n\n' "$(PROJECT) - x86-64 Hobby Kernel" "-------------------------------"
	@printf '  %-14s %s\n' "all"    "Build the kernel ($(KERNEL32))"
	@printf '  %-14s %s\n' "clean"  "Remove all build output"
	@printf '  %-14s %s\n' "untabify" "Convert tabs in sources to 4 spaces"
	@printf '\n  Every target has -opt / -noopt / -dbg / -verbose flavours,\n'
	@printf '  e.g. `make qemu-dbg` or `make iso-opt`.\n\n'

.PHONY: all help untabify untabify-check
