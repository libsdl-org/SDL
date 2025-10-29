/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include "SDL_internal.h"

#include "../linux/SDL_evdev_kbd.h"

#ifdef SDL_INPUT_FBSDKBIO

// This logic is adapted from drivers/tty/vt/keyboard.c in the Linux kernel source, slightly modified to work with FreeBSD

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/kbio.h>
#include <sys/consio.h>
#include <termios.h>

#include <signal.h>

#include "../../events/SDL_events_c.h"
#include "SDL_evdev_kbd_default_keyaccmap.h"

#ifndef K_OFF
#define K_OFF 0x04
#endif

typedef void(fn_handler_fn)(SDL_EVDEV_keyboard_state *kbd);


/*
 * Keyboard State
 */

struct SDL_EVDEV_keyboard_state
{
    int console_fd;
    int keyboard_fd;
    bool muted;
    unsigned long old_kbd_mode;
    unsigned short **key_maps;
    keymap_t *key_map;
    keyboard_info_t *kbInfo;
    unsigned char shift_down[4]; // shift state counters..
    bool dead_key_next;
    int npadch; // -1 or number assembled on pad
    accentmap_t *accents;
    unsigned int diacr;
    bool rep; // flag telling character repeat
    unsigned char lockstate;
    unsigned char ledflagstate;
    char shift_state;
    char text[128];
    unsigned int text_len;
    void (*vt_release_callback)(void *);
    void *vt_release_callback_data;
    void (*vt_acquire_callback)(void *);
    void *vt_acquire_callback_data;
};

static bool SDL_EVDEV_kbd_load_keymaps(SDL_EVDEV_keyboard_state *kbd)
{
    return ioctl(kbd->keyboard_fd, GIO_KEYMAP, kbd->key_map) >= 0;
}

static SDL_EVDEV_keyboard_state *kbd_cleanup_state = NULL;
static int kbd_cleanup_sigactions_installed = 0;
static int kbd_cleanup_atexit_installed = 0;

static struct sigaction old_sigaction[NSIG];

static int fatal_signals[] = {
    // Handlers for SIGTERM and SIGINT are installed in SDL_InitQuit.
    SIGHUP, SIGQUIT, SIGILL, SIGABRT,
    SIGFPE, SIGSEGV, SIGPIPE, SIGBUS,
    SIGSYS
};

static void vt_update_mouse(SDL_EVDEV_keyboard_state *kbd, int operation)
{
    struct mouse_info mData;

    SDL_zero(mData);
    mData.operation = operation;
    ioctl(kbd->console_fd, CONS_MOUSECTL, &mData);
}

static void kbd_cleanup(void)
{
    SDL_EVDEV_keyboard_state *kbd = kbd_cleanup_state;
    if (!kbd) {
        return;
    }
    kbd_cleanup_state = NULL;
    ioctl(kbd->keyboard_fd, KDSKBMODE, kbd->old_kbd_mode);
    if (kbd->keyboard_fd != kbd->console_fd) {
        close(kbd->keyboard_fd);
    }
    ioctl(kbd->console_fd, CONS_SETKBD, (unsigned long)(kbd->kbInfo->kb_index));
    vt_update_mouse(kbd, true);
}

void SDL_EVDEV_kbd_reraise_signal(int sig)
{
    raise(sig);
}

siginfo_t *SDL_EVDEV_kdb_cleanup_siginfo = NULL;
void *SDL_EVDEV_kdb_cleanup_ucontext = NULL;

static void kbd_cleanup_signal_action(int signum, siginfo_t *info, void *ucontext)
{
    struct sigaction *old_action_p = &(old_sigaction[signum]);
    sigset_t sigset;

    // Restore original signal handler before going any further.
    sigaction(signum, old_action_p, NULL);

    // Unmask current signal.
    sigemptyset(&sigset);
    sigaddset(&sigset, signum);
    sigprocmask(SIG_UNBLOCK, &sigset, NULL);

    // Save original signal info and context for archeologists.
    SDL_EVDEV_kdb_cleanup_siginfo = info;
    SDL_EVDEV_kdb_cleanup_ucontext = ucontext;

    // Restore keyboard.
    kbd_cleanup();

    // Reraise signal.
    SDL_EVDEV_kbd_reraise_signal(signum);
}

static void kbd_unregister_emerg_cleanup(void)
{
    int tabidx;

    kbd_cleanup_state = NULL;

    if (!kbd_cleanup_sigactions_installed) {
        return;
    }
    kbd_cleanup_sigactions_installed = 0;

    for (tabidx = 0; tabidx < sizeof(fatal_signals) / sizeof(fatal_signals[0]); ++tabidx) {
        struct sigaction *old_action_p;
        struct sigaction cur_action;
        int signum = fatal_signals[tabidx];
        old_action_p = &(old_sigaction[signum]);

        // Examine current signal action
        if (sigaction(signum, NULL, &cur_action)) {
            continue;
        }

        // Check if action installed and not modified
        if (!(cur_action.sa_flags & SA_SIGINFO) || cur_action.sa_sigaction != &kbd_cleanup_signal_action) {
            continue;
        }

        // Restore original action
        sigaction(signum, old_action_p, NULL);
    }
}

static void kbd_cleanup_atexit(void)
{
    // Restore keyboard.
    kbd_cleanup();

    // Try to restore signal handlers in case shared library is being unloaded
    kbd_unregister_emerg_cleanup();
}

static void kbd_register_emerg_cleanup(SDL_EVDEV_keyboard_state *kbd)
{
    int tabidx;

    if (kbd_cleanup_state) {
        return;
    }
    kbd_cleanup_state = kbd;

    if (!kbd_cleanup_atexit_installed) {
        /* Since glibc 2.2.3, atexit() (and on_exit(3)) can be used within a shared library to establish
         * functions that are called when the shared library is unloaded.
         * -- man atexit(3)
         */
        atexit(kbd_cleanup_atexit);
        kbd_cleanup_atexit_installed = 1;
    }

    if (kbd_cleanup_sigactions_installed) {
        return;
    }
    kbd_cleanup_sigactions_installed = 1;

    for (tabidx = 0; tabidx < sizeof(fatal_signals) / sizeof(fatal_signals[0]); ++tabidx) {
        struct sigaction *old_action_p;
        struct sigaction new_action;
        int signum = fatal_signals[tabidx];
        old_action_p = &(old_sigaction[signum]);
        if (sigaction(signum, NULL, old_action_p)) {
            continue;
        }

        /* Skip SIGHUP and SIGPIPE if handler is already installed
         * - assume the handler will do the cleanup
         */
        if ((signum == SIGHUP || signum == SIGPIPE) && (old_action_p->sa_handler != SIG_DFL || (void (*)(int))old_action_p->sa_sigaction != SIG_DFL)) {
            continue;
        }

        new_action = *old_action_p;
        new_action.sa_flags |= SA_SIGINFO;
        new_action.sa_sigaction = &kbd_cleanup_signal_action;
        sigaction(signum, &new_action, NULL);
    }
}

enum {
    VT_SIGNAL_NONE,
    VT_SIGNAL_RELEASE,
    VT_SIGNAL_ACQUIRE,
};
static int vt_release_signal;
static int vt_acquire_signal;
static SDL_AtomicInt vt_signal_pending;
SDL_AtomicInt vt_current;

typedef void (*signal_handler)(int signum);


static void kbd_vt_release_signal_action(int signum)
{
    SDL_SetAtomicInt(&vt_signal_pending, VT_SIGNAL_RELEASE);
    SDL_SetAtomicInt(&vt_current, VT_THEIRS);
}

static void kbd_vt_acquire_signal_action(int signum)
{
    SDL_SetAtomicInt(&vt_signal_pending, VT_SIGNAL_ACQUIRE);
    SDL_SetAtomicInt(&vt_current, VT_OURS);
}

static bool setup_vt_signal(int signum, signal_handler handler)
{
    struct sigaction *old_action_p;
    struct sigaction new_action;
    old_action_p = &(old_sigaction[signum]);
    SDL_zero(new_action);
    new_action.sa_handler = handler;
    new_action.sa_flags = SA_RESTART;
    if (sigaction(signum, &new_action, old_action_p) < 0) {
        return false;
    }
    if (old_action_p->sa_handler != SIG_DFL) {
        // This signal is already in use
        if (signum == SIGUSR1 || signum == SIGUSR2) {
            /*
             * For the moment we have no other options in FreeBSD because
             * the vt(4) will not accept signal numbers over 32.
             */
            return true;
        }
        sigaction(signum, old_action_p, NULL);
        return false;
    }
    return true;
}

static void kbd_vt_quit(int console_fd)
{
    struct vt_mode mode;

    if (vt_release_signal) {
        sigaction(vt_release_signal, &old_sigaction[vt_release_signal], NULL);
        vt_release_signal = 0;
    }
    if (vt_acquire_signal) {
        sigaction(vt_acquire_signal, &old_sigaction[vt_acquire_signal], NULL);
        vt_acquire_signal = 0;
    }

    SDL_zero(mode);
    mode.mode = VT_AUTO;
    ioctl(console_fd, VT_SETMODE, &mode);
}

static bool kbd_vt_init(int console_fd)
{
    struct vt_mode mode;

    if (setup_vt_signal(SIGUSR1, kbd_vt_release_signal_action)) {
        vt_release_signal = SIGUSR1;
    }
    if (setup_vt_signal(SIGUSR2, kbd_vt_acquire_signal_action)) {
        vt_acquire_signal = SIGUSR2;
    }
    if (!vt_release_signal || !vt_acquire_signal ) {
        kbd_vt_quit(console_fd);
        return false;
    }

    SDL_zero(mode);
    mode.mode = VT_PROCESS;
    mode.relsig = vt_release_signal;
    mode.acqsig = vt_acquire_signal;
    mode.frsig = SIGIO;
    if (ioctl(console_fd, VT_SETMODE, &mode) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_INPUT, "Failed VT_SETMODE ioctl: %s", strerror(errno));
        kbd_vt_quit(console_fd);
        return false;
    }
    return true;
}

SDL_EVDEV_keyboard_state *SDL_EVDEV_kbd_init(void)
{
    SDL_EVDEV_keyboard_state *kbd;
    char flag_state;

    kbd = (SDL_EVDEV_keyboard_state *)SDL_calloc(1, sizeof(SDL_EVDEV_keyboard_state));
    if (!kbd) {
        return NULL;
    }

    kbd->npadch = -1;

    // This might fail if we're not connected to a tty (e.g. on the Steam Link)
    kbd->keyboard_fd = kbd->console_fd = open("/dev/tty", O_RDONLY | O_CLOEXEC);

    kbd->shift_state = 0;

    kbd->accents = SDL_calloc(1, sizeof(accentmap_t));
    kbd->key_map = SDL_calloc(1, sizeof(keymap_t));
    kbd->kbInfo = SDL_calloc(1, sizeof(keyboard_info_t));

    ioctl(kbd->console_fd, KDGKBINFO, kbd->kbInfo);

    if (ioctl(kbd->console_fd, KDGKBSTATE, &flag_state) == 0) {
        kbd->ledflagstate = flag_state;
    }

    if (ioctl(kbd->console_fd, GIO_DEADKEYMAP, kbd->accents) < 0) {
        SDL_free(kbd->accents);
        kbd->accents = &accentmap_default_us_acc;
    }

    if (ioctl(kbd->console_fd, KDGKBMODE, &kbd->old_kbd_mode) == 0) {
        // Set the keyboard in XLATE mode and load the keymaps
        ioctl(kbd->console_fd, KDSKBMODE, (unsigned long)(K_XLATE));
        if (!SDL_EVDEV_kbd_load_keymaps(kbd)) {
            SDL_free(kbd->key_map);
            kbd->key_map = &keymap_default_us_acc;
        }

        if (!kbd_vt_init(kbd->console_fd)) {
            SDL_LogInfo(SDL_LOG_CATEGORY_INPUT, "kbd_vt_init failed");
        }

        kbd->keyboard_fd = kbd->console_fd;

        if (!SDL_GetHintBoolean(SDL_HINT_NO_SIGNAL_HANDLERS, false)) {
            kbd_register_emerg_cleanup(kbd);
        }
    }

    vt_update_mouse(kbd, MOUSE_HIDE);

    return kbd;
}

void SDL_EVDEV_kbd_quit(SDL_EVDEV_keyboard_state *kbd)
{
    if (!kbd) {
        return;
    }

    kbd_vt_quit(kbd->console_fd);

    vt_update_mouse(kbd, MOUSE_SHOW);

    kbd_unregister_emerg_cleanup();

    if (kbd->keyboard_fd >= 0) {
        // Restore the original keyboard mode
        ioctl(kbd->keyboard_fd, KDSKBMODE, kbd->old_kbd_mode);

        close(kbd->keyboard_fd);
        if (kbd->console_fd != kbd->keyboard_fd && kbd->console_fd >= 0) {
            // Give back keyboard.
            ioctl(kbd->console_fd, CONS_SETKBD, (unsigned long)(kbd->kbInfo->kb_index));
        }
        kbd->console_fd = kbd->keyboard_fd = -1;
    }

    SDL_free(kbd);
}

void SDL_EVDEV_kbd_set_muted(SDL_EVDEV_keyboard_state *state, bool muted)
{
    struct termios tios;

    SDL_zero(tios);

    if (!state) {
        return;
    }

    if (muted == state->muted) {
        return;
    }

    if (tcgetattr(state->console_fd, &tios) == -1) {
        SDL_LogError(SDL_LOG_CATEGORY_INPUT, "Could not get terminal mode: %s", strerror(errno));
        return;
    }

    if (muted) {
        if (SDL_GetHintBoolean(SDL_HINT_MUTE_CONSOLE_KEYBOARD, true)) {
            ioctl(state->console_fd, KDSKBMODE, K_OFF);
            cfmakeraw(&tios);

            if (!SDL_GetHintBoolean(SDL_HINT_NO_SIGNAL_HANDLERS, false)) {
                kbd_register_emerg_cleanup(state);
            }
        }
    } else {
        kbd_unregister_emerg_cleanup();

        cfmakesane(&tios);
        ioctl(state->console_fd, KDSKBMODE, state->old_kbd_mode);
    }

    if (tcsetattr(state->console_fd, TCSAFLUSH, &tios) == -1) {
        SDL_LogError(SDL_LOG_CATEGORY_INPUT, "Could not set terminal mode to %s: %s", muted ? "raw" : "sane", strerror(errno));
        return;
    }

    state->muted = muted;
}

void SDL_EVDEV_kbd_set_vt_switch_callbacks(SDL_EVDEV_keyboard_state *state, void (*release_callback)(void *), void *release_callback_data, void (*acquire_callback)(void *), void *acquire_callback_data)
{
    if (state == NULL) {
        return;
    }

    state->vt_release_callback = release_callback;
    state->vt_release_callback_data = release_callback_data;
    state->vt_acquire_callback = acquire_callback;
    state->vt_acquire_callback_data = acquire_callback_data;
}

void SDL_EVDEV_kbd_update(SDL_EVDEV_keyboard_state *state)
{
    if (!state) {
        return;
    }

    int signal_pending = SDL_GetAtomicInt(&vt_signal_pending);

    if (signal_pending != VT_SIGNAL_NONE) {
        if (signal_pending == VT_SIGNAL_RELEASE) {
            if (state->vt_release_callback) {
                vt_update_mouse(state, MOUSE_SHOW);
                state->vt_release_callback(state->vt_release_callback_data);
            }
            ioctl(state->console_fd, VT_RELDISP, 1);
        } else {
            if (state->vt_acquire_callback) {
                state->vt_acquire_callback(state->vt_acquire_callback_data);
                vt_update_mouse(state, MOUSE_HIDE);
            }
            ioctl(state->console_fd, VT_RELDISP, VT_ACKACQ);
        }
        SDL_CompareAndSwapAtomicInt(&vt_signal_pending, signal_pending, VT_SIGNAL_NONE);
    }
}

/*
 * Helper Functions.
 */
static void put_queue(SDL_EVDEV_keyboard_state *kbd, uint c)
{
    // c is already part of a UTF-8 sequence and safe to add as a character
    if (kbd->text_len < (sizeof(kbd->text) - 1)) {
        kbd->text[kbd->text_len++] = (char)c;
    }
}

static void put_utf8(SDL_EVDEV_keyboard_state *kbd, uint c)
{
    if (c < 0x80)
        /*  0******* */
        put_queue(kbd, c);
    else if (c < 0x800) {
        /* 110***** 10****** */
        put_queue(kbd, 0xc0 | (c >> 6));
        put_queue(kbd, 0x80 | (c & 0x3f));
    } else if (c < 0x10000) {
        if (c >= 0xD800 && c < 0xE000) {
            return;
        }
        if (c == 0xFFFF) {
            return;
        }
        /* 1110**** 10****** 10****** */
        put_queue(kbd, 0xe0 | (c >> 12));
        put_queue(kbd, 0x80 | ((c >> 6) & 0x3f));
        put_queue(kbd, 0x80 | (c & 0x3f));
    } else if (c < 0x110000) {
        /* 11110*** 10****** 10****** 10****** */
        put_queue(kbd, 0xf0 | (c >> 18));
        put_queue(kbd, 0x80 | ((c >> 12) & 0x3f));
        put_queue(kbd, 0x80 | ((c >> 6) & 0x3f));
        put_queue(kbd, 0x80 | (c & 0x3f));
    }
}

/*
 * We have a combining character DIACR here, followed by the character CH.
 * If the combination occurs in the table, return the corresponding value.
 * Otherwise, if CH is a space or equals DIACR, return DIACR.
 * Otherwise, conclude that DIACR was not combining after all,
 * queue it and return CH.
 */
static unsigned int handle_diacr(SDL_EVDEV_keyboard_state *kbd, unsigned int ch)
{
    unsigned int d = kbd->diacr;
    unsigned int i, j;

    kbd->diacr = 0;

    for (i = 0; i < kbd->accents->n_accs; i++) {
        if (kbd->accents->acc[i].accchar == d) {
            for (j = 0; j < NUM_ACCENTCHARS; ++j) {
                if (kbd->accents->acc[i].map[j][0] == 0) { // end of table
                    break;
                }
                if (kbd->accents->acc[i].map[j][0] == ch) {
                    return kbd->accents->acc[i].map[j][1];
                }
            }
        }
    }

    if (ch == ' ' || ch == d) {
        put_utf8(kbd, d);
        return 0;
    }
    put_utf8(kbd, d);

    return ch;
}

static bool vc_kbd_led(SDL_EVDEV_keyboard_state *kbd, int flag)
{
    return (kbd->ledflagstate & flag) != 0;
}

static void chg_vc_kbd_led(SDL_EVDEV_keyboard_state *kbd, int flag)
{
    kbd->ledflagstate ^= flag;
    ioctl(kbd->keyboard_fd, KDSKBSTATE, (unsigned long)(kbd->ledflagstate));
}

/*
 * Special function handlers
 */

static void k_self(SDL_EVDEV_keyboard_state *kbd, unsigned int value, char up_flag)
{
    if (up_flag) {
        return; // no action, if this is a key release
    }

    if (kbd->diacr) {
        value = handle_diacr(kbd, value);
    }

    if (kbd->dead_key_next) {
        kbd->dead_key_next = false;
        kbd->diacr = value;
        return;
    }
    put_utf8(kbd, value);
}

static void k_deadunicode(SDL_EVDEV_keyboard_state *kbd, unsigned int value, char up_flag)
{
    if (up_flag)
        return;

    kbd->diacr = (kbd->diacr ? handle_diacr(kbd, value) : value);
}

static void k_shift(SDL_EVDEV_keyboard_state *kbd, unsigned char value, char up_flag)
{
    int old_state = kbd->shift_state;

    if (kbd->rep)
        return;

    if (up_flag) {
        /*
         * handle the case that two shift or control
         * keys are depressed simultaneously
         */
        if (kbd->shift_down[value]) {
            kbd->shift_down[value]--;
        }
    } else
        kbd->shift_down[value]++;

    if (kbd->shift_down[value])
        kbd->shift_state |= (1 << value);
    else
        kbd->shift_state &= ~(1 << value);

    // kludge
    if (up_flag && kbd->shift_state != old_state && kbd->npadch != -1) {
        put_utf8(kbd, kbd->npadch);
        kbd->npadch = -1;
    }
}

void SDL_EVDEV_kbd_keycode(SDL_EVDEV_keyboard_state *kbd, unsigned int keycode, int down)
{
    keymap_t key_map;
    struct keyent_t keysym;
    unsigned int final_key_state;
    unsigned int map_from_key_sym;

    if (!kbd) {
        return;
    }

    key_map = *kbd->key_map;

    kbd->rep = (down == 2);

    if (keycode < NUM_KEYS) {
        if (keycode >= 89 && keycode <= 95) {
            // These constitute unprintable language-related keys, so ignore them.
            return;
        }
        if (keycode > 95) {
            keycode -= 7;
        }
        if (vc_kbd_led(kbd, ALKED) || (kbd->shift_state & 0x8)) {
            keycode += ALTGR_OFFSET;
        }
        keysym = key_map.key[keycode];
    } else {
        return;
    }

    final_key_state = kbd->shift_state & 0x7;
    if ((keysym.flgs & FLAG_LOCK_C) && vc_kbd_led(kbd, LED_CAP)) {
        final_key_state ^= 0x1;
    }
    if ((keysym.flgs & FLAG_LOCK_N) && vc_kbd_led(kbd, LED_NUM)) {
        final_key_state ^= 0x1;
    }

    map_from_key_sym = keysym.map[final_key_state];
    if ((keysym.spcl & (0x80 >> final_key_state)) || (map_from_key_sym & SPCLKEY)) {
        // Special function.
        if (map_from_key_sym == 0)
            return; // Nothing to do.
        if (map_from_key_sym & SPCLKEY) {
            map_from_key_sym &= ~SPCLKEY;
        }
        if (map_from_key_sym >= F_ACC && map_from_key_sym <= L_ACC) {
            // Accent function.
            unsigned int accent_index = map_from_key_sym - F_ACC;
            if (kbd->accents->acc[accent_index].accchar != 0) {
                k_deadunicode(kbd, kbd->accents->acc[accent_index].accchar, !down);
            }
        } else {
            switch (map_from_key_sym) {
            case ASH: // alt/meta shift
                k_shift(kbd, 3, down == 0);
                break;
            case LSHA: // left shift + alt lock
            case RSHA: // right shift + alt lock
                if (down == 0) {
                    chg_vc_kbd_led(kbd, ALKED);
                }
                SDL_FALLTHROUGH;
            case LSH: // left shift
            case RSH: // right shift
                k_shift(kbd, 0, down == 0);
                break;
            case LCTRA: // left ctrl + alt lock
            case RCTRA: // right ctrl + alt lock
                if (down == 0) {
                    chg_vc_kbd_led(kbd, ALKED);
                }
                SDL_FALLTHROUGH;
            case LCTR: // left ctrl
            case RCTR: // right ctrl
                k_shift(kbd, 1, down == 0);
                break;
            case LALTA: // left alt + alt lock
            case RALTA: // right alt + alt lock
                if (down == 0) {
                    chg_vc_kbd_led(kbd, ALKED);
                }
                SDL_FALLTHROUGH;
            case LALT: // left alt
            case RALT: // right alt
                k_shift(kbd, 2, down == 0);
                break;
            case ALK: // alt lock
                if (down == 1) {
                    chg_vc_kbd_led(kbd, ALKED);
                }
                break;
            case CLK: // caps lock
                if (down == 1) {
                    chg_vc_kbd_led(kbd, CLKED);
                }
                break;
            case NLK: // num lock
                if (down == 1) {
                    chg_vc_kbd_led(kbd, NLKED);
                }
                break;
            case SLK: // scroll lock
                if (down == 1) {
                    chg_vc_kbd_led(kbd, SLKED);
                }
                break;
            default:
                return;
            }
        }
    } else {
        if (map_from_key_sym == '\n' || map_from_key_sym == '\r') {
            if (kbd->diacr) {
                kbd->diacr = 0;
                return;
            }
        }
        if (map_from_key_sym >= ' ' && map_from_key_sym != 127) {
            k_self(kbd, map_from_key_sym, !down);
        }
    }

    if (kbd->text_len > 0) {
        kbd->text[kbd->text_len] = '\0';
        SDL_SendKeyboardText(kbd->text);
        kbd->text_len = 0;
    }
}

#endif // SDL_INPUT_FBSDKBIO
