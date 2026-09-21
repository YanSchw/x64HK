# Bootable image generation.
#
# `grub-mkrescue` emits an isohybrid image: the same file boots from a virtual
# CD in QEMU and, byte-for-byte dd'ed onto a USB stick, from real hardware in
# legacy BIOS mode.

ISO_DIR  := $(BUILD_DIR)/Iso
ISO_FILE := $(BUILD_DIR)/$(PROJECT).iso

GRUB_TITLE   ?= $(PROJECT)
GRUB_TIMEOUT ?= 1
GRUB_CMDLINE ?=

# Homebrew ships GRUB under a target prefix; plain distros do not.
GRUB_MKRESCUE := $(firstword $(foreach c,grub-mkrescue grub2-mkrescue i686-elf-grub-mkrescue \
                   x86_64-elf-grub-mkrescue,$(shell command -v $(c) 2>/dev/null)))

iso: $(ISO_FILE)

$(ISO_DIR)/boot/grub/grub.cfg: $(MAKEFILE_LIST)
	@echo "GEN   $@"
	@mkdir -p $(@D)
	@printf 'set timeout=%s\nset default=0\n\nmenuentry "%s" {\n\tmultiboot2 /boot/Kernel.elf %s\n\tboot\n}\n' \
		'$(GRUB_TIMEOUT)' '$(GRUB_TITLE)' '$(GRUB_CMDLINE)' > $@

$(ISO_DIR)/boot/Kernel.elf: $(KERNEL32)
	@echo "STRIP $@"
	@mkdir -p $(@D)
	$(VERBOSE) $(STRIP) --strip-debug --strip-unneeded -o $@ $<

$(ISO_FILE): $(ISO_DIR)/boot/Kernel.elf $(ISO_DIR)/boot/grub/grub.cfg
ifeq (,$(GRUB_MKRESCUE))
	@echo "grub-mkrescue was not found." >&2
	@echo "  macOS:  brew install xorriso i686-elf-grub" >&2
	@echo "  Debian: apt install grub-pc-bin grub-common xorriso" >&2
	@exit 1
else
	@echo "ISO   $@"
	$(VERBOSE) $(GRUB_MKRESCUE) -o $@ $(ISO_DIR)
endif

# Write the isohybrid image to a USB mass storage device.
#   make usb USB_DEVICE=/dev/disk4      (macOS)
#   make usb USB_DEVICE=/dev/sdb        (Linux)
usb: $(ISO_FILE)
ifeq (,$(USB_DEVICE))
	@echo "Set USB_DEVICE to the target device, e.g. 'make usb USB_DEVICE=/dev/disk4'." >&2
	@echo "Available devices:" >&2
ifeq ($(shell uname),Darwin)
	@diskutil list external physical >&2 || true
else
	@lsblk -o KNAME,SIZE,TYPE,MODEL -p -d >&2 || true
endif
	@exit 1
else
	@echo "This ERASES $(USB_DEVICE). Press Ctrl-C within 5 seconds to abort."
	@sleep 5
ifeq ($(shell uname),Darwin)
	$(VERBOSE) diskutil unmountDisk $(USB_DEVICE)
	$(VERBOSE) sudo dd if=$(ISO_FILE) of=$(subst /dev/disk,/dev/rdisk,$(USB_DEVICE)) bs=4m && sync
	$(VERBOSE) diskutil eject $(USB_DEVICE)
else
	$(VERBOSE) sudo dd if=$(ISO_FILE) of=$(USB_DEVICE) bs=4M status=progress conv=fsync && sync
endif
endif

help::
	@printf '  %-14s %s\n' "iso"  "Build a bootable hybrid ISO ($(ISO_FILE))"
	@printf '  %-14s %s\n' "usb"  "Flash that ISO onto USB_DEVICE=<path>"
	@printf '  %-14s %s\n' "*-iso" "Boot the ISO instead of the raw kernel (e.g. qemu-iso)"

.PHONY: iso usb
