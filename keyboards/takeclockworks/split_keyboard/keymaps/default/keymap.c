// keymaps/default/keymap.c
// Unit 2: MATRIX_ROWS=4, MATRIX_COLS=14
// Left  : QMK Col0 - Col6
// Right : QMK Col7 - Col13

#include QMK_KEYBOARD_H
#include "keymap_japanese.h"

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
};

#define FN_KEY_DELAY_MS 1000

enum fn_key_indexes {
    FN_KEY_1 = 0,
    FN_KEY_2,
    FN_KEY_3,
    FN_KEY_COUNT,
};

enum active_mode {
    MODE_BASE = 0,
    MODE_FN1,
    MODE_FN2,
};

static const uint16_t fn_output_keycodes[FN_KEY_COUNT] = {
    KC_F21,
    KC_F22,
    KC_F23,
};

static uint16_t         fn_timers[FN_KEY_COUNT];
static uint8_t          fn_press_counts[FN_KEY_COUNT];
static bool             fn_registered[FN_KEY_COUNT];
static enum active_mode active_mode = MODE_BASE;
static bool             one_hand_mode;

static void sync_layers(void) {
    layer_off(_FN1);
    layer_off(_FN2);
    layer_off(_BASE_MIRROR);
    layer_off(_FN1_MIRROR);
    layer_off(_FN2_MIRROR);

    if (one_hand_mode) {
        switch (active_mode) {
            case MODE_FN1:
                layer_on(_FN1_MIRROR);
                break;
            case MODE_FN2:
                layer_on(_FN2_MIRROR);
                break;
            case MODE_BASE:
            default:
                layer_on(_BASE_MIRROR);
                break;
        }
    } else {
        switch (active_mode) {
            case MODE_FN1:
                layer_on(_FN1);
                break;
            case MODE_FN2:
                layer_on(_FN2);
                break;
            case MODE_BASE:
            default:
                break;
        }
    }
}

static void release_registered_fn_output(uint8_t fn_index) {
    if (fn_registered[fn_index]) {
        unregister_code16(fn_output_keycodes[fn_index]);
        fn_registered[fn_index] = false;
    }
}

static void toggle_active_mode(enum active_mode mode) {
    active_mode = active_mode == mode ? MODE_BASE : mode;
    sync_layers();
}

static void handle_short_fn_tap(uint8_t fn_index) {
    switch (fn_index) {
        case FN_KEY_1:
            toggle_active_mode(MODE_FN1);
            break;
        case FN_KEY_2:
            toggle_active_mode(MODE_FN2);
            break;
        case FN_KEY_3:
            one_hand_mode = !one_hand_mode;
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

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
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
    }
    return true;
}

void matrix_scan_user(void) {
    for (uint8_t i = 0; i < FN_KEY_COUNT; i++) {
        if (fn_press_counts[i] > 0 && !fn_registered[i] && timer_elapsed(fn_timers[i]) >= FN_KEY_DELAY_MS) {
            register_code16(fn_output_keycodes[i]);
            fn_registered[i] = true;
        }
    }
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
    { KC_BSPC, KC_ESC,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,     KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    JP_MINS, KC_BSPC },

    // Row1
    { KC_DEL,  KC_TAB,  KC_A,    KC_S,    KC_D,    KC_F,    KC_G,     KC_H,    KC_J,    KC_K,    KC_L,    JP_SCLN, JP_COLN, KC_DEL  },

    // Row2
    { KC_LGUI, KC_LSFT, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,     KC_N,    KC_M,    JP_COMM, JP_DOT,  JP_SLSH, KC_UP,   KC_RSFT },

    // Row3
    { JP_ZKHK, KC_LCTL, KC_LALT, FN3_F23, FN2_F22, FN1_F21, KC_SPC,   KC_ENT,  FN1_F21, FN2_F22, FN3_F23, KC_LEFT, KC_DOWN, KC_RGHT }
},

/*
 * [1] Fn1
 * Function keys / numbers
 */
[_FN1] = {
    // Row0
    { KC_NO,   KC_INS,  KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,    KC_7,    KC_8,    KC_9,    JP_ASTR, JP_SLSH, JP_EQL,  KC_BSPC },

    // Row1
    { KC_NO,   KC_NO,   KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10,   KC_4,    KC_5,    KC_6,    JP_PLUS, KC_NO,   KC_NO,   KC_DEL  },

    // Row2
    { KC_NO,   KC_NO,   KC_F11,  KC_F12,  KC_NO,   KC_NO,   KC_NO,    KC_1,    KC_2,    KC_3,    KC_0,    JP_DOT,  KC_PGUP, KC_RSFT },

    // Row3
    { QK_BOOT, KC_NO,   KC_NO,   FN3_F23, FN2_F22, FN1_F21, KC_SPC,   KC_ENT,  FN1_F21, FN2_F22, FN3_F23, KC_HOME, KC_PGDN, KC_END  }
},

/*
 * [2] Fn2
 * Symbols
 */
[_FN2] = {
    // Row0
    { JP_ZKHK, KC_ESC,  KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,    KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_BSPC },

    // Row1
    { KC_PSCR, KC_TAB,  JP_EXLM, JP_DQUO, JP_HASH, JP_DLR,  JP_PERC,  JP_YEN,  JP_TILD, JP_CIRC, JP_LPRN, JP_RPRN, KC_NO,   KC_DEL  },

    // Row2
    { KC_NO,   KC_LSFT, KC_NO,   KC_NO,   KC_NO,   KC_NO,   JP_AT,    JP_LBRC, JP_RBRC, JP_AMPR, JP_PIPE, KC_NO,   KC_NO,   KC_RSFT },

    // Row3
    { KC_NO,   KC_LCTL, KC_NO,   FN3_F23, FN2_F22, FN1_F21, KC_SPC,   KC_NO,   FN1_F21, FN2_F22, FN3_F23, KC_NO,   KC_NO,   KC_NO   }
},

/*
 * [3] Base Mirror
 * One-hand mode mirrors the effective right side onto the left side.
 */
[_BASE_MIRROR] = {
    // Row0
    { KC_BSPC, JP_MINS, KC_P,    KC_O,    KC_I,    KC_U,    KC_Y,     KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   },

    // Row1
    { KC_DEL,  JP_COLN, JP_SCLN, KC_L,    KC_K,    KC_J,    KC_H,     KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   },

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
    { KC_BSPC, JP_EQL,  JP_SLSH, JP_ASTR, KC_7,    KC_8,    KC_9,     KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   },

    // Row1
    { KC_DEL,  KC_NO,   KC_NO,   JP_PLUS, KC_4,    KC_5,    KC_6,     KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   },

    // Row2
    { KC_RSFT, KC_PGUP, JP_DOT,  KC_0,    KC_1,    KC_2,    KC_3,     KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   },

    // Row3
    { KC_END,  KC_PGDN, KC_HOME, FN3_F23, FN2_F22, FN1_F21, KC_ENT,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   }
},

/*
 * [5] Fn2 Mirror
 */
[_FN2_MIRROR] = {
    // Row0
    { KC_BSPC, KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,    KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   },

    // Row1
    { KC_DEL,  KC_NO,   JP_RPRN, JP_LPRN, JP_CIRC, JP_TILD, JP_YEN,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   },

    // Row2
    { KC_RSFT, KC_NO,   KC_NO,   JP_PIPE, JP_AMPR, JP_RBRC, JP_LBRC,  KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   },

    // Row3
    { KC_NO,   KC_NO,   KC_NO,   FN3_F23, FN2_F22, FN1_F21, KC_NO,    KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   }
}

};
