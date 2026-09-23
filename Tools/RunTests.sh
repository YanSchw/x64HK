#!/bin/sh
#
# Boots the test kernel headless and turns the guest's own verdict into an exit
# status.
#
# QEMU's isa-debug-exit device exits the emulator with (value << 1) | 1, so the
# kernel's SUCCESS (0x10) arrives here as 33 and FAILURE (0x11) as 35. Every
# other status -- a triple fault reboot, a timeout, QEMU refusing to start --
# counts as a failure, which is the point: a kernel that never reaches its
# report must not pass.

set -u

usage() {
    cat <<USAGE
Usage: Tools/RunTests.sh QEMU IMAGE [CORES] [MEMORY_MIB] [TIMEOUT_SECONDS]

IMAGE is either a kernel ELF (loaded by QEMU itself) or a .iso (booted
through GRUB, which is the handover real hardware uses).
USAGE
    exit 2
}

[ $# -ge 2 ] || usage

QEMU=$1
IMAGE=$2
CORES=${3:-4}
MEMORY=${4:-512}
TIMEOUT=${5:-120}

# When set, the guest is expected to die, and its serial log to contain this.
EXPECT=${6:-}

EXIT_PASS=33
EXIT_FAIL=35

[ -x "$(command -v "$QEMU" 2>/dev/null)" ] || {
    printf 'RunTests: %s not found\n' "$QEMU" >&2
    exit 2
}
[ -f "$IMAGE" ] || {
    printf 'RunTests: %s not found\n' "$IMAGE" >&2
    exit 2
}

case $IMAGE in
    *.iso) BOOT="-cdrom $IMAGE"; LABEL="iso" ;;
    *)     BOOT="-kernel $IMAGE"; LABEL="elf" ;;
esac

# Serial goes to a file rather than stdout: QEMU's stdio chardev wants a
# terminal it can put into raw mode, which is exactly what a CI runner has not
# got. The log is printed below either way, so a timeout still shows how far
# the kernel got.
LOG=$(mktemp "${TMPDIR:-/tmp}/x64hk-test.XXXXXX") || exit 2
trap 'rm -f "$LOG"' EXIT INT TERM

# coreutils is not a given on macOS, so the timeout is best effort.
TIMEOUT_CMD=""
if command -v timeout >/dev/null 2>&1; then
    TIMEOUT_CMD="timeout -k 5 $TIMEOUT"
elif command -v gtimeout >/dev/null 2>&1; then
    TIMEOUT_CMD="gtimeout -k 5 $TIMEOUT"
else
    printf 'RunTests: no timeout command, a hung kernel will hang this run\n' >&2
fi

printf 'TEST  %s, %s cores, %s MiB\n' "$LABEL" "$CORES" "$MEMORY"

# Deliberately not the QEMU_FLAGS from Tools/Qemu.mk: those pull in an audio
# backend for the PC speaker, and there is no sound daemon on a build runner.
# shellcheck disable=SC2086
$TIMEOUT_CMD "$QEMU" \
    $BOOT \
    -display none \
    -serial "file:$LOG" \
    -m "$MEMORY" \
    -smp "$CORES" \
    -accel tcg,thread=multi \
    -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
    -no-reboot \
    -d guest_errors \
    </dev/null
status=$?

[ -s "$LOG" ] && cat "$LOG"

if [ -n "$EXPECT" ]; then
    if [ "$status" -eq "$EXIT_FAIL" ] && grep -q "$EXPECT" "$LOG"; then
        printf 'PASS  %s, %s cores, %s MiB: died as expected\n\n' "$LABEL" "$CORES" "$MEMORY"
        exit 0
    fi
    printf 'FAIL  %s, %s cores, %s MiB: expected a panic matching "%s", got status %s\n\n' \
        "$LABEL" "$CORES" "$MEMORY" "$EXPECT" "$status" >&2
    exit 1
fi

case $status in
    "$EXIT_PASS")
        printf 'PASS  %s, %s cores, %s MiB\n\n' "$LABEL" "$CORES" "$MEMORY"
        exit 0
        ;;
    "$EXIT_FAIL")
        printf 'FAIL  %s, %s cores, %s MiB: failed checks\n\n' "$LABEL" "$CORES" "$MEMORY" >&2
        ;;
    124 | 137)
        printf 'FAIL  %s, %s cores, %s MiB: no verdict within %ss, killed\n\n' "$LABEL" "$CORES" "$MEMORY" "$TIMEOUT" >&2
        ;;
    0)
        # QEMU exited on its own, so the kernel never reached the exit device.
        printf 'FAIL  %s, %s cores, %s MiB: QEMU exited without a verdict\n\n' "$LABEL" "$CORES" "$MEMORY" >&2
        ;;
    *)
        printf 'FAIL  %s, %s cores, %s MiB: QEMU exit status %s\n\n' "$LABEL" "$CORES" "$MEMORY" "$status" >&2
        ;;
esac

exit 1
