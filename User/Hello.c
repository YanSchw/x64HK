/* The demo ring 3 program.
 *
 * Nothing here knows anything about the kernel beyond ABI/SPECIFICATION.md: it
 * is linked against ABI/C, starts at _start, and every line it puts on the
 * screen goes through a system call. */

#include "HK.h"

static void Say(const char* InText) {
    if (hk_failed(hk_print(InText))) {
        hk_exit(1);
    }
}

static void Echo(char InCharacter) {
    hk_write_all(HK_STDOUT, &InCharacter, 1);
}

int main(void) {
    Say("hello from ring 3. type something, escape quits.\n");

    /* Nothing depends on this; it is here because a syscall that returns
       normally is worth showing next to the ones that block and the one that
       does not come back at all. */
    hk_yield();

    for (;;) {
        hk_key_t key = 0;
        if (hk_failed(hk_get_key(&key))) {
            return 1;
        }

        if (hk_key_code(key) == HK_KEY_ESCAPE) {
            break;
        }

        if (hk_key_has(key, HK_MOD_CTRL) && hk_key_code(key) == HK_KEY_C) {
            Say("\ninterrupted\n");
            hk_exit(130);
        }

        const char character = (char)hk_key_ascii(key);
        if (character == '\n' || (character >= ' ' && character < 0x7f)) {
            Echo(character);
        }
    }

    Say("\ngoodbye\n");
    return 0;
}
