// keymaps/default/keymap.c
// Unit 2: MATRIX_ROWS=4, MATRIX_COLS=14
// Left  : QMK Col0 - Col6
// Right : QMK Col7 - Col13

#include QMK_KEYBOARD_H
#include "keymap_japanese.h"
#include "raw_hid.h"
#include <string.h>

enum layers {
    _BASE = 0,
    _FN1,
    _FN2,
    _BASE_MIRROR,
    _FN1_MIRROR,
    _FN2_MIRROR,
};

enum custom_keycodes {
    FN1_F21 = SAFE_RANGE,
    FN2_F22,
    FN3_F23,
    BOOT_HOLD,
};

#define FN_KEY_DELAY_MS 1000
#define BOOT_KEY_DELAY_MS 1000
#define KLP_REPORT_SIZE 32
#define KLP_MAX_PRESSED_KEYS 12
#define KLP_PRESSED_KEYS_OFFSET 8

enum fn_key_indexes {
    FN_KEY_1 = 0,
    FN_KEY_2,
    FN_KEY_3,
    FN_KEY_COUNT,
};

enum layer_slot {
    SLOT_BASE = 0,
    SLOT_FN1,
    SLOT_FN2,
};

static const uint16_t fn_output_keycodes[FN_KEY_COUNT] = {
    KC_F21,
    KC_F22,
    KC_F23,
};

static uint16_t         fn_timers[FN_KEY_COUNT];
static uint8_t          fn_press_counts[FN_KEY_COUNT];
static bool             fn_registered[FN_KEY_COUNT];
static enum layer_slot  active_slot = SLOT_BASE;
static bool             is_mirror_mode;
static bool             syncing_layers;
static uint16_t         boot_timer;
static bool             boot_pressed;
static bool             boot_triggered;
static bool             ctrl_arrow_consumed[4];
static uint8_t          last_layer_status_report[KLP_REPORT_SIZE];
static bool             has_last_layer_status_report;

enum ctrl_arrow_index {
    CTRL_ARROW_UP = 0,
    CTRL_ARROW_DOWN,
    CTRL_ARROW_LEFT,
    CTRL_ARROW_RIGHT,
};

static uint8_t get_effective_app_layer(void) {
    return (is_mirror_mode ? 3 : 0) + active_slot;
}

static void add_pressed_keys_to_report(uint8_t *report) {
    uint8_t pressed_count = 0;

    for (uint8_t row = 0; row < MATRIX_ROWS && row < 4; row++) {
        matrix_row_t row_state = matrix_get_row(row);

        for (uint8_t col = 0; col < MATRIX_COLS && col < 14; col++) {
            if ((row_state & ((matrix_row_t)1u << col)) == 0) {
                continue;
            }

            if (pressed_count >= KLP_MAX_PRESSED_KEYS) {
                return;
            }

            uint8_t offset  = KLP_PRESSED_KEYS_OFFSET + pressed_count * 2;
            report[offset]     = row;
            report[offset + 1] = col;
            pressed_count++;
            report[7] = pressed_count;
        }
    }
}

static void send_keyboard_layer_status(void) {
#if defined(SPLIT_KEYBOARD)
    if (!is_keyboard_master()) {
        return;
    }
#endif

    uint8_t report[KLP_REPORT_SIZE] = {0};
    report[0]          = 'K';
    report[1]          = 'L';
    report[2]          = 'P';
    report[3]          = 1;
    report[4]          = is_mirror_mode ? 1 : 0;
    report[5]          = active_slot;
    report[6]          = get_effective_app_layer();
    add_pressed_keys_to_report(report);

    if (has_last_layer_status_report && memcmp(report, last_layer_status_report, sizeof(report)) == 0) {
        return;
    }

    raw_hid_send(report, sizeof(report));
    memcpy(last_layer_status_report, report, sizeof(report));
    has_last_layer_status_report = true;
}

static void set_status_from_layer(layer_state_t state) {
    switch (get_highest_layer(state)) {
        case _FN2_MIRROR:
            is_mirror_mode = true;
            active_slot    = SLOT_FN2;
            break;
        case _FN1_MIRROR:
            is_mirror_mode = true;
            active_slot    = SLOT_FN1;
            break;
        case _BASE_MIRROR:
            is_mirror_mode = true;
            active_slot    = SLOT_BASE;
            break;
        case _FN2:
            is_mirror_mode = false;
            active_slot    = SLOT_FN2;
            break;
        case _FN1:
            is_mirror_mode = false;
            active_slot    = SLOT_FN1;
            break;
        case _BASE:
        default:
            is_mirror_mode = false;
            active_slot    = SLOT_BASE;
            break;
    }
}

static void sync_layers(void) {
    syncing_layers = true;

    layer_off(_FN1);
    layer_off(_FN2);
    layer_off(_BASE_MIRROR);
    layer_off(_FN1_MIRROR);
    layer_off(_FN2_MIRROR);

    if (is_mirror_mode) {
        switch (active_slot) {
            case SLOT_FN1:
                layer_on(_FN1_MIRROR);
                break;
            case SLOT_FN2:
                layer_on(_FN2_MIRROR);
                break;
            case SLOT_BASE:
            default:
                layer_on(_BASE_MIRROR);
                break;
        }
    } else {
        switch (active_slot) {
            case SLOT_FN1:
                layer_on(_FN1);
                break;
            case SLOT_FN2:
                layer_on(_FN2);
                break;
            case SLOT_BASE:
            default:
                break;
        }
    }

    syncing_layers = false;
    send_keyboard_layer_status();
}

static void release_registered_fn_output(uint8_t fn_index) {
    if (fn_registered[fn_index]) {
        unregister_code16(fn_output_keycodes[fn_index]);
        fn_registered[fn_index] = false;
    }
}

static void toggle_active_slot(enum layer_slot slot) {
    active_slot = active_slot == slot ? SLOT_BASE : slot;
    sync_layers();
}

static void handle_short_fn_tap(uint8_t fn_index) {
    switch (fn_index) {
        case FN_KEY_1:
            toggle_active_slot(SLOT_FN1);
            break;
        case FN_KEY_2:
            toggle_active_slot(SLOT_FN2);
            break;
        case FN_KEY_3:
            is_mirror_mode = !is_mirror_mode;
            sync_layers();
            break;
    }
}

static void handle_fn_key(uint8_t fn_index, bool pressed) {
    if (pressed) {
        if (fn_press_counts[fn_index] == 0) {
            fn_timers[fn_index]     = timer_read();
            fn_registered[fn_index] = false;
        }
        fn_press_counts[fn_index]++;
        return;
    }

    if (fn_press_counts[fn_index] > 0) {
        fn_press_counts[fn_index]--;
    }

    if (fn_press_counts[fn_index] > 0) {
        return;
    }

    uint16_t elapsed    = timer_elapsed(fn_timers[fn_index]);
    bool     long_press = fn_registered[fn_index] || elapsed >= FN_KEY_DELAY_MS;

    if (fn_registered[fn_index]) {
        release_registered_fn_output(fn_index);
    } else if (long_press) {
        tap_code16(fn_output_keycodes[fn_index]);
    }

    if (!long_press) {
        handle_short_fn_tap(fn_index);
    }
}

static bool handle_ctrl_arrow(uint16_t keycode, keyrecord_t *record) {
    uint8_t index;
    uint16_t output_keycode;

    switch (keycode) {
        case KC_UP:
            index          = CTRL_ARROW_UP;
            output_keycode = KC_PGUP;
            break;
        case KC_DOWN:
            index          = CTRL_ARROW_DOWN;
            output_keycode = KC_PGDN;
            break;
        case KC_LEFT:
            index          = CTRL_ARROW_LEFT;
            output_keycode = KC_HOME;
            break;
        case KC_RGHT:
            index          = CTRL_ARROW_RIGHT;
            output_keycode = KC_END;
            break;
        default:
            return true;
    }

    if (!record->event.pressed) {
        if (ctrl_arrow_consumed[index]) {
            ctrl_arrow_consumed[index] = false;
            return false;
        }
        return true;
    }

    uint8_t mods         = get_mods();
    uint8_t oneshot_mods = get_oneshot_mods();

    if (((mods | oneshot_mods) & MOD_MASK_CTRL) == 0) {
        return true;
    }

    ctrl_arrow_consumed[index] = true;
    del_mods(MOD_MASK_CTRL);
    del_oneshot_mods(MOD_MASK_CTRL);
    tap_code16(output_keycode);
    set_mods(mods);
    set_oneshot_mods(oneshot_mods);

    return false;
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (!handle_ctrl_arrow(keycode, record)) {
        return false;
    }

    switch (keycode) {
        case FN1_F21:
            handle_fn_key(FN_KEY_1, record->event.pressed);
            return false;
        case FN2_F22:
            handle_fn_key(FN_KEY_2, record->event.pressed);
            return false;
        case FN3_F23:
            handle_fn_key(FN_KEY_3, record->event.pressed);
            return false;
        case BOOT_HOLD:
            if (record->event.pressed) {
                boot_timer     = timer_read();
                boot_pressed   = true;
                boot_triggered = false;
            } else {
                boot_pressed = false;
            }
            return false;
    }
    return true;
}

void matrix_scan_user(void) {
    if (boot_pressed && !boot_triggered && timer_elapsed(boot_timer) >= BOOT_KEY_DELAY_MS) {
        boot_triggered = true;
        reset_keyboard();
    }

    for (uint8_t i = 0; i < FN_KEY_COUNT; i++) {
        if (fn_press_counts[i] > 0 && !fn_registered[i] && timer_elapsed(fn_timers[i]) >= FN_KEY_DELAY_MS) {
            register_code16(fn_output_keycodes[i]);
            fn_registered[i] = true;
        }
    }

    send_keyboard_layer_status();
}

layer_state_t layer_state_set_user(layer_state_t state) {
    if (!syncing_layers) {
        set_status_from_layer(state);
        send_keyboard_layer_status();
    }

    return state;
}

void keyboard_post_init_user(void) {
    send_keyboard_layer_status();
}

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

/*
 * [0] Base
 *
 * Left  : Col0 - Col6
 * Right : Col7 - Col13
 */
[_BASE] = {
    // Row0
    { JP_ZKHK, KC_ESC,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,     KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    JP_MINS, KC_BSPC },

    // Row1
    { KC_F2,   KC_TAB,  KC_A,    KC_S,    KC_D,    KC_F,    KC_G,     KC_H,    KC_J,    KC_K,    KC_L,    JP_SCLN, KC_INS,  KC_DEL  },

    // Row2
    { KC_F8,   KC_LSFT, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,     KC_N,    KC_M,    JP_COMM, JP_DOT,  JP_SLSH, KC_UP,   KC_RSFT },

    // Row3
    { KC_LCTL, KC_LGUI, KC_LALT, FN3_F23, FN2_F22, FN1_F21, KC_SPC,   KC_ENT,  FN1_F21, FN2_F22, FN3_F23, KC_LEFT, KC_DOWN, KC_RGHT }
},

/*
 * [1] Fn1
 * Function keys / numbers
 */
[_FN1] = {
    // Row0
    { JP_ZKHK, KC_ESC,  KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,    JP_EQL,  KC_7,    KC_8,    KC_9,    JP_ASTR, JP_SLSH, KC_BSPC },

    // Row1
    { KC_F2,   KC_TAB,  KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10,   JP_DOT,  KC_4,    KC_5,    KC_6,    JP_PLUS, KC_INS,  KC_DEL  },

    // Row2
    { KC_F8,   KC_LSFT, KC_F11,  KC_F12,  LCTL(KC_X), LCTL(KC_C), LCTL(KC_V), KC_0,    KC_1,    KC_2,    KC_3,    JP_MINS, KC_UP,   KC_RSFT },

    // Row3
    { KC_LCTL, KC_LGUI, KC_LALT, FN3_F23, FN2_F22, FN1_F21, KC_SPC,   KC_ENT,  FN1_F21, FN2_F22, FN3_F23, KC_LEFT, KC_DOWN, KC_RGHT }
},

/*
 * [2] Fn2
 * Symbols
 */
[_FN2] = {
    // Row0
    { JP_ZKHK, KC_ESC,  KC_NO,   KC_NO,   KC_NO,   KC_NO,   BOOT_HOLD, JP_UNDS, JP_COLN, JP_CIRC, KC_NO,   KC_NO,   KC_NO,   KC_BSPC },

    // Row1
    { KC_PSCR, KC_TAB,  KC_NO,   JP_DLR,  JP_EXLM, JP_DQUO, JP_PERC,  JP_LPRN, JP_RPRN, JP_AMPR, JP_YEN,  KC_NO,   KC_INS,  KC_DEL  },

    // Row2
    { KC_NO,   KC_LSFT, KC_NO,   KC_NO,   KC_NO,   JP_HASH, JP_AT,    JP_LBRC, JP_RBRC, JP_TILD, JP_PIPE, KC_NO,   KC_UP,   KC_RSFT },

    // Row3
    { KC_LCTL, KC_LGUI, KC_LALT, FN3_F23, FN2_F22, FN1_F21, KC_SPC,   KC_ENT,  FN1_F21, FN2_F22, FN3_F23, KC_LEFT, KC_DOWN, KC_RGHT }
},

/*
 * [3] Base Mirror
 * One-hand mode mirrors the effective right side onto the left side.
 */
[_BASE_MIRROR] = {
    // Row0
    { KC_BSPC, JP_MINS, KC_P,    KC_O,    KC_I,    KC_U,    KC_Y,     KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   },

    // Row1
    { KC_DEL,  KC_INS,  JP_SCLN, KC_L,    KC_K,    KC_J,    KC_H,     KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   },

    // Row2
    { KC_RSFT, KC_UP,   JP_SLSH, JP_DOT,  JP_COMM, KC_M,    KC_N,     KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   },

    // Row3
    { KC_LEFT, KC_DOWN, KC_RGHT, FN3_F23, FN2_F22, FN1_F21, KC_ENT,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   }
},

/*
 * [4] Fn1 Mirror
 */
[_FN1_MIRROR] = {
    // Row0
    { KC_BSPC, JP_SLSH, JP_ASTR, KC_7,    KC_8,    KC_9,    JP_EQL,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   },

    // Row1
    { KC_DEL,  KC_INS,  JP_PLUS, KC_4,    KC_5,    KC_6,    JP_DOT,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   },

    // Row2
    { KC_RSFT, KC_UP,   JP_MINS, KC_1,    KC_2,    KC_3,    KC_0,     KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   },

    // Row3
    { KC_LEFT, KC_DOWN, KC_RGHT, FN3_F23, FN2_F22, FN1_F21, KC_ENT,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   }
},

/*
 * [5] Fn2 Mirror
 */
[_FN2_MIRROR] = {
    // Row0
    { KC_BSPC, KC_NO,   KC_NO,   KC_NO,   JP_CIRC, JP_COLN, JP_UNDS,  KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   },

    // Row1
    { KC_DEL,  KC_INS,  KC_NO,   JP_YEN,  JP_AMPR, JP_RPRN, JP_LPRN,  KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   },

    // Row2
    { KC_RSFT, KC_UP,   KC_NO,   JP_PIPE, JP_TILD, JP_RBRC, JP_LBRC,  KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   },

    // Row3
    { KC_LEFT, KC_DOWN, KC_RGHT, FN3_F23, FN2_F22, FN1_F21, KC_ENT,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   }
}

};
