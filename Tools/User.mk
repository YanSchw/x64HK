# The ring 3 demo program.
#
# Built for the low half as a static executable and embedded in the kernel by
# Kernel/User/Program.asm. It shares ABI/C with the kernel, so the two cannot
# disagree about the system call numbers.

USER_SRC_DIR := User
USER_ABI_DIR := ABI/C
USER_OUT_DIR := $(BUILD_DIR)/User
USER_PROGRAM := $(USER_OUT_DIR)/Hello.elf

USER_SOURCES := $(USER_SRC_DIR)/Hello.c $(USER_ABI_DIR)/HK.c

# The link address is part of the ABI, so it is read from the header rather than
# written down a second time.
USER_IMAGE_BASE := $(shell sed -n 's/^\#define HK_USER_IMAGE_BASE *\(0x[0-9A-Fa-f]*\)ULL.*/\1/p' \
                     $(USER_ABI_DIR)/HK.h)

ifeq ($(origin CC),default)
	CC := $(if $(shell command -v clang 2>/dev/null),clang,gcc)
endif

# No -mno-red-zone, unlike the kernel: interrupts taken in ring 3 switch to the
# stack in the TSS, so nothing the kernel does ever lands on the user stack.
USER_CFLAGS := -std=c11 -m64 -ffreestanding -nostdinc -fno-pic -fno-stack-protector \
               -mno-mmx -mno-sse -mgeneral-regs-only -mcmodel=small \
               -I$(USER_ABI_DIR) -Wall -Wextra -Werror -O2 -g -gdwarf-4
USER_LDFLAGS := -no-pie -nostdlib -nostartfiles -nodefaultlibs $(LDFLAGS_EXTRA) \
                -Wl,-e,_start -Wl,--image-base=$(USER_IMAGE_BASE) -Wl,--build-id=none \
                -Wl,-z,noexecstack -Wl,-z,separate-code

ifeq ($(shell uname),Darwin)
	USER_CFLAGS += --target=x86_64-pc-linux-gnu
endif

$(USER_PROGRAM): $(USER_SOURCES) $(USER_ABI_DIR)/HK.h
	@echo "CC    $@"
	@mkdir -p $(@D)
	$(VERBOSE) $(CC) $(USER_CFLAGS) $(USER_LDFLAGS) -o $@ $(USER_SOURCES)

# incbin resolves the name against the assembler's include path.
ASMFLAGS += -I$(USER_OUT_DIR)/
$(BUILD_DIR)/$(SRC_DIR)/User/Program.asm.o: $(USER_PROGRAM)

user: $(USER_PROGRAM)

help::
	@printf '  %-14s %s\n' "user" "Build the ring 3 demo program only ($(USER_PROGRAM))"

.PHONY: user
