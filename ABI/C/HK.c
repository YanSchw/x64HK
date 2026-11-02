#include "HK.h"

/* Everything here is a thin layer over hk_syscall. The reserved argument
 * registers are passed as zero, which the kernel checks. */

void hk_exit(int InStatus) {
    hk_syscall(HK_SYS_EXIT, (hk_u64)(hk_u32)InStatus, 0, 0);

    /* Unreachable, but the attribute promises it and the compiler checks. */
    for (;;) {
    }
}

hk_result_t hk_write(hk_handle_t InHandle, const void* InBuffer, hk_size_t InLength) {
    return hk_syscall(HK_SYS_WRITE, InHandle, (hk_u64)InBuffer, InLength);
}

hk_result_t hk_write_all(hk_handle_t InHandle, const void* InBuffer, hk_size_t InLength) {
    const char* cursor = (const char*)InBuffer;
    hk_size_t written = 0;

    while (written < InLength) {
        const hk_result_t step = hk_write(InHandle, cursor + written, InLength - written);
        if (step < 0) {
            return step;
        }
        if (step == 0) {
            break;
        }
        written += (hk_size_t)step;
    }

    return (hk_result_t)written;
}

hk_result_t hk_print(const char* InText) {
    if (InText == 0) {
        return -HK_EFAULT;
    }
    return hk_write_all(HK_STDOUT, InText, hk_strlen(InText));
}

hk_result_t hk_yield(void) {
    return hk_syscall(HK_SYS_YIELD, 0, 0, 0);
}

hk_result_t hk_get_key(hk_key_t* OutKey) {
    const hk_result_t result = hk_syscall(HK_SYS_GET_KEY, 0, 0, 0);
    if (result < 0) {
        return result;
    }

    if (OutKey != 0) {
        *OutKey = (hk_key_t)result;
    }
    return result;
}

hk_size_t hk_strlen(const char* InText) {
    const char* cursor = InText;
    while (*cursor != '\0') {
        cursor++;
    }
    return (hk_size_t)(cursor - InText);
}

const char* hk_error_name(enum hk_error InError) {
    switch (InError) {
        case HK_OK: return "ok";
        case HK_EPERM: return "not permitted";
        case HK_ENOSYS: return "no such call";
        case HK_EINVAL: return "invalid argument";
        case HK_EFAULT: return "bad address";
        case HK_EBADF: return "bad handle";
        case HK_EAGAIN: return "would block";
        case HK_ERANGE: return "out of range";
        default: return "unknown error";
    }
}

#ifndef HK_NO_START

/* The kernel enters here with a 16 byte aligned, empty stack. The alignment is
 * re-established rather than trusted, and the call leaves rsp at 16n+8, which
 * is what the SysV ABI expects at a function's first instruction. */
__asm__(".global _start\n"
        "_start:\n"
        "    xor %ebp, %ebp\n"
        "    and $-16, %rsp\n"
        "    call hk_main_trampoline\n");

void hk_main_trampoline(void);

void hk_main_trampoline(void) {
    hk_exit(main());
}

#endif  /* HK_NO_START */
