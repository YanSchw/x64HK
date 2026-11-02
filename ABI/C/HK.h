#ifndef HK_ABI_H
#define HK_ABI_H

/* The x64HK user ABI. ABI/SPECIFICATION.md is normative; this is its binding.
 *
 * Valid C11 and C++11, depends on no other header. Compiled as C++ it also
 * declares namespace HK at the bottom. */

#define HK_ABI_VERSION 1

#if defined(__cplusplus)
#define HK_BEGIN_C extern "C" {
#define HK_END_C }
#else
#define HK_BEGIN_C
#define HK_END_C
#endif

#define HK_NORETURN __attribute__((noreturn))

/* The small helpers below are usable in constant expressions when the header
 * is compiled as C++. constexpr already implies inline. */
#if defined(__cplusplus)
#define HK_PURE constexpr
#else
#define HK_PURE static inline
#endif

/* Works in both languages, unlike _Static_assert and static_assert. */
#define HK_STATIC_ASSERT(InCondition, InName) \
    typedef char hk_static_assert_##InName[(InCondition) ? 1 : -1]

HK_BEGIN_C

typedef unsigned char hk_u8;
typedef unsigned short hk_u16;
typedef unsigned int hk_u32;
typedef unsigned long long hk_u64;
typedef long long hk_i64;
typedef hk_u64 hk_size_t;

HK_STATIC_ASSERT(sizeof(hk_u8) == 1, u8);
HK_STATIC_ASSERT(sizeof(hk_u16) == 2, u16);
HK_STATIC_ASSERT(sizeof(hk_u32) == 4, u32);
HK_STATIC_ASSERT(sizeof(hk_u64) == 8, u64);
HK_STATIC_ASSERT(sizeof(void*) == 8, pointer);

/* Negative on failure, the magnitude being an hk_error. */
typedef hk_i64 hk_result_t;

typedef hk_u64 hk_handle_t;

/* Packed as modifiers:scancode:ascii, see hk_key_ascii and friends. */
typedef hk_u32 hk_key_t;

/* --- Address space --------------------------------------------------------
 *
 * The kernel maps a program here. A blob that is linked by hand has to agree
 * with HK_USER_IMAGE_BASE. */

#define HK_USER_MIN 0x0000000000001000ULL
#define HK_USER_IMAGE_BASE 0x0000000000400000ULL
#define HK_USER_STACK_TOP 0x00007FFFFFFFF000ULL
#define HK_USER_STACK_SIZE (64ULL * 1024ULL)
#define HK_USER_MAX 0x0000800000000000ULL

/* --- Segments -------------------------------------------------------------
 *
 * Fixed by the offsets sysret computes off STAR, not freely chosen. */

#define HK_USER_CODE_SELECTOR 0x2b
#define HK_USER_DATA_SELECTOR 0x23

enum hk_syscall {
    HK_SYS_EXIT = 0,
    HK_SYS_WRITE = 1,
    HK_SYS_YIELD = 2,
    HK_SYS_GET_KEY = 3,

    HK_SYS_COUNT
};

enum hk_error {
    HK_OK = 0,
    HK_EPERM = 1,   /* not permitted from ring 3 */
    HK_ENOSYS = 2,  /* no such call number */
    HK_EINVAL = 3,  /* bad argument value */
    HK_EFAULT = 4,  /* pointer outside the user address space */
    HK_EBADF = 5,   /* no such handle */
    HK_EAGAIN = 6,  /* would have blocked */
    HK_ERANGE = 7,  /* argument out of range */

    HK_ERROR_COUNT
};

/* Only destination there is. Handle 0 is invalid on purpose, so a zeroed
 * argument cannot pass for standard output. */
#define HK_STDOUT ((hk_handle_t)1)

/* Per call, not per buffer -- a longer write comes back short. */
#define HK_WRITE_MAX 1024

#define HK_EXIT_SUCCESS 0
#define HK_EXIT_FAULT_BASE 128

/* --- Keys ----------------------------------------------------------------- */

#define HK_MOD_SHIFT 0x01
#define HK_MOD_ALT_LEFT 0x02
#define HK_MOD_ALT_RIGHT 0x04
#define HK_MOD_CTRL_LEFT 0x08
#define HK_MOD_CTRL_RIGHT 0x10
#define HK_MOD_CAPS_LOCK 0x20
#define HK_MOD_NUM_LOCK 0x40
#define HK_MOD_SCROLL_LOCK 0x80

#define HK_MOD_ALT (HK_MOD_ALT_LEFT | HK_MOD_ALT_RIGHT)
#define HK_MOD_CTRL (HK_MOD_CTRL_LEFT | HK_MOD_CTRL_RIGHT)

/* Scan code set 1 make codes, renumbered densely. Generated from
 * Kernel/Device/Key.h and required to stay equal to it. */
enum hk_scancode {
    HK_KEY_INVALID = 0,
    HK_KEY_ESCAPE,
    HK_KEY_DIGIT_1,
    HK_KEY_DIGIT_2,
    HK_KEY_DIGIT_3,
    HK_KEY_DIGIT_4,
    HK_KEY_DIGIT_5,
    HK_KEY_DIGIT_6,
    HK_KEY_DIGIT_7,
    HK_KEY_DIGIT_8,
    HK_KEY_DIGIT_9,
    HK_KEY_DIGIT_0,
    HK_KEY_MINUS,
    HK_KEY_EQUAL,
    HK_KEY_BACKSPACE,
    HK_KEY_TAB,
    HK_KEY_Q,
    HK_KEY_W,
    HK_KEY_E,
    HK_KEY_R,
    HK_KEY_T,
    HK_KEY_Y,
    HK_KEY_U,
    HK_KEY_I,
    HK_KEY_O,
    HK_KEY_P,
    HK_KEY_OPEN_BRACKET,
    HK_KEY_CLOSE_BRACKET,
    HK_KEY_ENTER,
    HK_KEY_LEFT_CTRL,
    HK_KEY_A,
    HK_KEY_S,
    HK_KEY_D,
    HK_KEY_F,
    HK_KEY_G,
    HK_KEY_H,
    HK_KEY_J,
    HK_KEY_K,
    HK_KEY_L,
    HK_KEY_SEMICOLON,
    HK_KEY_APOSTROPHE,
    HK_KEY_GRAVE,
    HK_KEY_LEFT_SHIFT,
    HK_KEY_BACKSLASH,
    HK_KEY_Z,
    HK_KEY_X,
    HK_KEY_C,
    HK_KEY_V,
    HK_KEY_B,
    HK_KEY_N,
    HK_KEY_M,
    HK_KEY_COMMA,
    HK_KEY_PERIOD,
    HK_KEY_SLASH,
    HK_KEY_RIGHT_SHIFT,
    HK_KEY_KP_STAR,
    HK_KEY_LEFT_ALT,
    HK_KEY_SPACE,
    HK_KEY_CAPS_LOCK,
    HK_KEY_F1,
    HK_KEY_F2,
    HK_KEY_F3,
    HK_KEY_F4,
    HK_KEY_F5,
    HK_KEY_F6,
    HK_KEY_F7,
    HK_KEY_F8,
    HK_KEY_F9,
    HK_KEY_F10,
    HK_KEY_NUM_LOCK,
    HK_KEY_SCROLL_LOCK,
    HK_KEY_KP_7,
    HK_KEY_KP_8,
    HK_KEY_KP_9,
    HK_KEY_KP_MINUS,
    HK_KEY_KP_4,
    HK_KEY_KP_5,
    HK_KEY_KP_6,
    HK_KEY_KP_PLUS,
    HK_KEY_KP_1,
    HK_KEY_KP_2,
    HK_KEY_KP_3,
    HK_KEY_KP_0,
    HK_KEY_KP_PERIOD,
    HK_KEY_SYSREQ,
    HK_KEY_EUROPE_2,
    HK_KEY_F11,
    HK_KEY_F12,
    HK_KEY_KP_EQUAL,
    HK_KEY_COUNT,
    HK_KEY_DELETE = HK_KEY_KP_PERIOD,
    HK_KEY_UP = HK_KEY_KP_8,
    HK_KEY_DOWN = HK_KEY_KP_2,
    HK_KEY_LEFT = HK_KEY_KP_4,
    HK_KEY_RIGHT = HK_KEY_KP_6,
};

HK_PURE hk_u8 hk_key_ascii(hk_key_t InKey) {
    return (hk_u8)(InKey & 0xff);
}

HK_PURE hk_u8 hk_key_code(hk_key_t InKey) {
    return (hk_u8)((InKey >> 8) & 0xff);
}

HK_PURE hk_u8 hk_key_modifiers(hk_key_t InKey) {
    return (hk_u8)((InKey >> 16) & 0xff);
}

HK_PURE int hk_key_has(hk_key_t InKey, hk_u8 InModifiers) {
    return (hk_key_modifiers(InKey) & InModifiers) != 0;
}

HK_PURE hk_key_t hk_key_pack(hk_u8 InCode, hk_u8 InAscii, hk_u8 InModifiers) {
    return (hk_key_t)InAscii | ((hk_key_t)InCode << 8) | ((hk_key_t)InModifiers << 16);
}

/* --- Calls ---------------------------------------------------------------- */

HK_PURE int hk_failed(hk_result_t InResult) {
    return InResult < 0;
}

HK_PURE enum hk_error hk_error_of(hk_result_t InResult) {
    return InResult < 0 ? (enum hk_error)(-InResult) : HK_OK;
}

const char* hk_error_name(enum hk_error InError);

/* The raw entry. syscall clobbers rcx and r11 and the kernel returns in rax;
 * everything else survives. Reserved arguments have to be zero. */
static inline hk_result_t hk_syscall(hk_u64 InNumber, hk_u64 InArg1, hk_u64 InArg2, hk_u64 InArg3) {
    hk_result_t result;
    __asm__ volatile("syscall"
                     : "=a"(result)
                     : "a"(InNumber), "D"(InArg1), "S"(InArg2), "d"(InArg3)
                     : "rcx", "r11", "memory");
    return result;
}

HK_NORETURN void hk_exit(int InStatus);

/* Up to HK_WRITE_MAX bytes, returns how many went. */
hk_result_t hk_write(hk_handle_t InHandle, const void* InBuffer, hk_size_t InLength);

/* Loops until the whole buffer is out or the kernel refuses. */
hk_result_t hk_write_all(hk_handle_t InHandle, const void* InBuffer, hk_size_t InLength);

hk_result_t hk_print(const char* InText);

hk_result_t hk_yield(void);

/* Blocks until a key is pressed. */
hk_result_t hk_get_key(hk_key_t* OutKey);

hk_size_t hk_strlen(const char* InText);

/* The entry point HK.c supplies calls this. Declared here because
 * -ffreestanding strips main of its special status, so a C++ program would
 * otherwise mangle it and _start would not find it. Giving it C linkage is the
 * fix, and -Wmain objects to that on principle. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmain"
int main(void);
#pragma GCC diagnostic pop

HK_END_C

#ifdef __cplusplus

/* C++ face of the same ABI. Inline throughout, so nothing is added to HK.c. */
namespace HK {

using Result = hk_result_t;
using Error = hk_error;
using ScanCode = hk_scancode;
using Handle = hk_handle_t;
using Size = hk_size_t;

constexpr Handle StdOut = HK_STDOUT;
constexpr Size WriteMax = HK_WRITE_MAX;

class Key {
public:
    constexpr Key() = default;
    constexpr explicit Key(hk_key_t InRaw) : m_Raw(InRaw) {}

    constexpr hk_key_t Raw() const { return m_Raw; }
    constexpr ScanCode Code() const { return ScanCode((m_Raw >> 8) & 0xff); }
    constexpr char Ascii() const { return char(m_Raw & 0xff); }
    constexpr hk_u8 Modifiers() const { return hk_u8((m_Raw >> 16) & 0xff); }

    constexpr bool Has(hk_u8 InModifiers) const { return (Modifiers() & InModifiers) != 0; }
    constexpr bool Shift() const { return Has(HK_MOD_SHIFT); }
    constexpr bool Alt() const { return Has(HK_MOD_ALT); }
    constexpr bool Ctrl() const { return Has(HK_MOD_CTRL); }

    constexpr bool IsValid() const { return Code() != HK_KEY_INVALID; }
    constexpr bool IsPrintable() const { return Ascii() >= ' ' && Ascii() < 0x7f; }

private:
    hk_key_t m_Raw = 0;
};

inline bool Failed(Result InResult) {
    return InResult < 0;
}

inline Error ErrorOf(Result InResult) {
    return hk_error_of(InResult);
}

inline const char* Name(Error InError) {
    return hk_error_name(InError);
}

[[noreturn]] inline void Exit(int InStatus = HK_EXIT_SUCCESS) {
    hk_exit(InStatus);
}

inline Result Write(const void* InBuffer, Size InLength, Handle InHandle = StdOut) {
    return hk_write(InHandle, InBuffer, InLength);
}

inline Result WriteAll(const void* InBuffer, Size InLength, Handle InHandle = StdOut) {
    return hk_write_all(InHandle, InBuffer, InLength);
}

inline Result Print(const char* InText) {
    return hk_print(InText);
}

inline Result Yield() {
    return hk_yield();
}

inline Result GetKey(Key& OutKey) {
    hk_key_t raw = 0;
    const Result result = hk_get_key(&raw);
    if (result >= 0) {
        OutKey = Key(raw);
    }
    return result;
}

}  // namespace HK

#endif  /* __cplusplus */

#endif  /* HK_ABI_H */
