# Compilation and linking.

VERBOSE ?= @

BUILD_ROOT := Build
BUILD_DIR  ?= $(BUILD_ROOT)

KERNEL64 := $(BUILD_DIR)/Kernel.elf64
KERNEL32 := $(BUILD_DIR)/Kernel.elf

ifeq ($(origin CXX),default)
	CXX := $(if $(shell command -v clang++ 2>/dev/null),clang++,g++)
endif
ifeq ($(CXX),c++)
	CXX := clang++
endif
ASM := nasm

IS_CLANG := $(findstring clang,$(shell $(CXX) --version 2>/dev/null))

# Prefer the LLVM binutils when they are on PATH (they cross-target out of the box,
# unlike Apple's cctools).
ifneq (,$(shell command -v llvm-objcopy 2>/dev/null))
	OBJCOPY := llvm-objcopy
	STRIP   := llvm-strip
else
	OBJCOPY := objcopy
	STRIP   := strip
endif

# Freestanding, no runtime, no red zone (interrupts would clobber it), and no
# SSE/MMX because we never save the FPU state across a context switch.
CXXFLAGS_BASE  := -std=c++23 -m64 -I. -I$(SRC_DIR) -ffreestanding -nostdinc -nostdlib -nodefaultlibs \
                  -nostartfiles -fno-pic -no-pie -fno-rtti -fno-exceptions \
                  -fno-stack-protector -fno-use-cxa-atexit -fno-threadsafe-statics -fno-strict-aliasing \
                  -mno-red-zone -mno-mmx -mno-sse -mgeneral-regs-only -mcx16 \
                  -g -gdwarf-4 -MMD -MP
CXXFLAGS_WARN  := -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable \
                  -Wno-non-virtual-dtor
ifeq (,$(IS_CLANG))
	# Hardware register layouts deliberately name a bitfield after the enum that
	# types it (`Granularity Granularity : 1`), which GCC rejects by default.
	CXXFLAGS_WARN += -Wno-changes-meaning -Wno-unused-const-variable
else
	CXXFLAGS_WARN += -Wno-unused-const-variable \
	                 -Wno-unused-private-field -Wno-implicit-exception-spec-mismatch \
	                 -Wno-unused-command-line-argument
endif
CXXFLAGS_OPT   ?= -O2 -fomit-frame-pointer

ifeq ($(shell uname),Darwin)
	CXXFLAGS_BASE += --target=x86_64-pc-linux-gnu
	LDFLAGS_EXTRA += -fuse-ld=lld
else ifneq (,$(shell command -v ld.lld 2>/dev/null))
	LDFLAGS_EXTRA += -fuse-ld=lld
endif

CXXFLAGS := $(CXXFLAGS_BASE) $(CXXFLAGS_WARN) $(CXXFLAGS_OPT)
ASMFLAGS := -f elf64 -g -F dwarf -I$(SRC_DIR)/
LDFLAGS  := $(LDFLAGS_EXTRA) -Wl,-T,$(LINKER_SCRIPT) -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-dynamic-linker

CPP_SOURCES := $(shell find $(SRC_DIR) -name '*.cpp' -not -name '.*')
ASM_SOURCES := $(shell find $(SRC_DIR) -name '*.asm' -not -name '.*')

ifeq ($(TEST),1)
	CXXFLAGS += -DTEST
else
	CPP_SOURCES := $(filter-out $(SRC_DIR)/Test/%,$(CPP_SOURCES))
endif

CPP_OBJECTS := $(addprefix $(BUILD_DIR)/,$(CPP_SOURCES:.cpp=.o))
ASM_OBJECTS := $(addprefix $(BUILD_DIR)/,$(ASM_SOURCES:.asm=.asm.o))
OBJECTS     := $(ASM_OBJECTS) $(CPP_OBJECTS)
DEP_FILES   := $(CPP_OBJECTS:.o=.d) $(ASM_OBJECTS:.o=.d)

$(BUILD_DIR)/%.o: %.cpp
	@echo "CXX   $<"
	@mkdir -p $(@D)
	$(VERBOSE) $(CXX) -c $(CXXFLAGS) -o $@ $<

$(BUILD_DIR)/%.asm.o: %.asm
	@echo "ASM   $<"
	@mkdir -p $(@D)
	$(VERBOSE) $(ASM) $(ASMFLAGS) -MD $(@:.o=.d) -MT $@ -o $@ $<

$(KERNEL64): $(OBJECTS) $(LINKER_SCRIPT)
	@echo "LD    $@"
	@mkdir -p $(@D)
	$(VERBOSE) $(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJECTS)

# Boot loaders locate the Multiboot header in a 32-bit ELF; the 64-bit image is
# kept around because that is what GDB wants for the long mode symbols.
$(KERNEL32): $(KERNEL64)
	@echo "OBJCP $@"
	$(VERBOSE) $(OBJCOPY) -I elf64-x86-64 -O elf32-i386 $< $@

clean::
	@echo "RM    $(BUILD_ROOT)"
	$(VERBOSE) rm -rf "$(BUILD_ROOT)"

# Build flavours: `make <target>-opt`, `-noopt`, `-dbg`, `-verbose`.
%-opt:
	$(VERBOSE) $(MAKE) BUILD_DIR="$(BUILD_ROOT)/Opt" CXXFLAGS_OPT="-O3 -flto -march=x86-64-v2 -DNDEBUG" $*
%-noopt:
	$(VERBOSE) $(MAKE) BUILD_DIR="$(BUILD_ROOT)/NoOpt" CXXFLAGS_OPT="-O0" $*
%-dbg:
	$(VERBOSE) $(MAKE) BUILD_DIR="$(BUILD_ROOT)/Dbg" CXXFLAGS_OPT="-Og -fno-omit-frame-pointer" $*
%-verbose:
	$(VERBOSE) $(MAKE) BUILD_DIR="$(BUILD_ROOT)/Verbose" CXXFLAGS_OPT="-O1 -DVERBOSE" $*
%-test:
	$(VERBOSE) $(MAKE) BUILD_DIR="$(BUILD_ROOT)/Test" CXXFLAGS_OPT="-O1" TEST=1 $*

MAKEFLAGS += --no-builtin-rules --no-print-directory
.SUFFIXES:

ifneq ($(MAKECMDGOALS),clean)
-include $(DEP_FILES)
endif

.PHONY: clean
