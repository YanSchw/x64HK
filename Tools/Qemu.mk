# Emulation and debugging.

QEMU      ?= qemu-system-x86_64
QEMU_CPUS ?= 4
QEMU_MEM  ?= 2048

# Loading the raw ELF through QEMU's own Multiboot loader is the fast path; the
# ISO path exercises the real GRUB/Multiboot2 handover.
QEMU_BOOT ?= -kernel $(KERNEL32)

ifeq ($(shell uname),Darwin)
	QEMU_DISPLAY ?= cocoa,zoom-to-fit=on
	QEMU_AUDIO   := -audiodev coreaudio,id=snd
	# No hardware acceleration for x86 guests on Apple silicon.
	QEMU_ACCEL   := -accel tcg,thread=multi
else
	ifeq ($(DISPLAY),)
		QEMU_DISPLAY ?= curses
	else
		QEMU_DISPLAY ?= gtk
	endif
	QEMU_AUDIO := -audiodev pipewire,id=snd
	QEMU_ACCEL := -accel tcg,thread=multi
endif

QEMU_SERIAL ?= vc:80Cx24C
QEMU_FLAGS  += -machine pcspk-audiodev=snd $(QEMU_AUDIO) -m $(QEMU_MEM) -k en-us \
               -d guest_errors -serial $(QEMU_SERIAL) -no-reboot
QEMU_EXTRA  ?=

# A unix socket under XDG_RUNTIME_DIR beats a TCP port on shared machines.
ifneq ($(XDG_RUNTIME_DIR),)
	GDB_PORT ?= $(XDG_RUNTIME_DIR)/$(PROJECT)-gdb.sock
else
	GDB_PORT ?= :$(shell echo $$(($$(id -u) + 1024)))
endif
ifneq ($(findstring /,$(GDB_PORT)),)
	QEMU_GDB := -chardev socket,path=$(GDB_PORT),server=on,wait=off,id=gdb0 -gdb chardev:gdb0
else
	QEMU_GDB := -gdb tcp:$(GDB_PORT)
endif

qemu: all
	@echo "QEMU  $(KERNEL32)"
	$(VERBOSE) $(QEMU) $(QEMU_BOOT) $(QEMU_GDB) -display $(QEMU_DISPLAY) -smp $(QEMU_CPUS) \
		$(QEMU_ACCEL) $(QEMU_FLAGS) $(QEMU_EXTRA)

# Headless run that pipes the serial console to stdout; used by `make test`.
qemu-headless: all
	$(VERBOSE) $(QEMU) $(QEMU_BOOT) -display none -serial stdio -smp $(QEMU_CPUS) \
		$(QEMU_ACCEL) $(QEMU_FLAGS) $(QEMU_EXTRA)

TEST_CORES   ?= 1 4
TEST_TIMEOUT ?= 180

test:
	$(VERBOSE) $(MAKE) BUILD_DIR="$(BUILD_ROOT)/Test" CXXFLAGS_OPT="-O1" TEST=1 run-tests

run-tests: all
	$(VERBOSE) for cores in $(TEST_CORES); do \
		./Tools/RunTests.sh "$(QEMU)" "$(KERNEL32)" "$$cores" "$(TEST_TIMEOUT)" || exit 1; \
	done

gdb: all
	$(VERBOSE) gdb "$(KERNEL64)" -ex "set arch i386:x86-64" \
		-ex "target remote | exec $(QEMU) -gdb stdio $(QEMU_BOOT) -display none -S -smp $(QEMU_CPUS) $(QEMU_FLAGS)"

connect-gdb:
	$(VERBOSE) gdb -ex "target remote $(GDB_PORT)" "$(KERNEL64)"

# Recursive helpers: `make qemu-serial`, `make qemu-gdb`, `make qemu-iso`, ...
%-serial:
	@echo "HINT: C-a x quits QEMU, C-a c opens the monitor."
	$(VERBOSE) $(MAKE) QEMU_SERIAL=mon:stdio QEMU_DISPLAY=none $*
%-gdb:
	$(VERBOSE) $(MAKE) QEMU_EXTRA="$(QEMU_EXTRA) -S" $*
%-curses:
	$(VERBOSE) $(MAKE) QEMU_DISPLAY=curses $*
%-iso: $(ISO_FILE)
	$(VERBOSE) $(MAKE) QEMU_BOOT="-cdrom $(ISO_FILE)" $*

help::
	@printf '  %-14s %s\n' "qemu"        "Boot in QEMU (Multiboot handover, GDB stub on $(GDB_PORT))"
	@printf '  %-14s %s\n' "*-serial"    "Route the serial console to stdout (e.g. qemu-serial)"
	@printf '  %-14s %s\n' "*-gdb"       "Start halted and wait for GDB (e.g. qemu-gdb)"
	@printf '  %-14s %s\n' "connect-gdb" "Attach GDB to a waiting QEMU"
	@printf '  %-14s %s\n' "test"        "Run the boot time test suites on $(TEST_CORES) cores"

.PHONY: qemu qemu-headless gdb connect-gdb test run-tests
