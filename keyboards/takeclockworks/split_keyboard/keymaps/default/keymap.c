// keymaps/default/keymap.c
// 2号機: MATRIX_ROWS=4, MATRIX_COLS=14 前提
// Left  : QMK Col0 - Col6
// Right : QMK Col7 - Col13

#include QMK_KEYBOARD_H
#include "keymap_japanese.h"

enum layers {
    _BASE = 0,
    _FN1,
    _FN2,
    _FN3,
};

enum custom_keycodes {
    FN1_F21 = SAFE_RANGE,
    FN2_F22,
    FN3_F23,
};

#define FN1_F21_DELAY_MS 500

static uint16_t fn1_timer;
static uint8_t fn1_press_count;
static bool fn1_f21_registered;

static void handle_fn_key(uint16_t keycode, bool pressed) {
    switch (keycode) {
        case FN1_F21:
            if (pressed) {
                if (fn1_press_count == 0) {
                    fn1_timer = timer_read();
                    fn1_f21_registered = false;
                }
                fn1_press_count++;
                layer_on(_FN1);
            } else {
                if (fn1_press_count > 0) {
                    fn1_press_count--;
                }
                if (fn1_press_count == 0) {
                    if (fn1_f21_registered) {
                        unregister_code(KC_F21);
                        fn1_f21_registered = false;
                    }
                    layer_off(_FN1);
                }
            }
            break;
        case FN2_F22:
            if (pressed) {
                tap_code(KC_F22);
                layer_on(_FN2);
            } else {
                layer_off(_FN2);
            }
            break;
        case FN3_F23:
            if (pressed) {
                tap_code(KC_F23);
                layer_on(_FN3);
            } else {
                layer_off(_FN3);
            }
            break;
    }
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case FN1_F21:
        case FN2_F22:
        case FN3_F23:
            handle_fn_key(keycode, record->event.pressed);
            return false;
    }
    return true;
}

void matrix_scan_user(void) {
    if (fn1_press_count > 0 && !fn1_f21_registered && timer_elapsed(fn1_timer) >= FN1_F21_DELAY_MS) {
        register_code(KC_F21);
        fn1_f21_registered = true;
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
    { KC_ESC,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    JP_ZKHK,  KC_BSPC,  KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    JP_SLSH },

    // Row1
    { KC_TAB,  KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_LGUI,  KC_INS, KC_H,    KC_J,    KC_K,    KC_L,    JP_MINS, JP_PLUS },

    // Row2
    { KC_LSFT, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_DEL,   KC_PSCR, KC_N,    KC_M,    JP_COMM, JP_DOT,  JP_UNDS, KC_RSFT },

    // Row3
    { KC_LCTL, KC_LALT, FN3_F23,  FN2_F22,  FN1_F21,  KC_SPC, KC_ENT,  KC_ENT,  KC_SPC,  FN1_F21,  FN2_F22,  FN3_F23,  KC_RALT, KC_RCTL }
},

/*
 * [1] Fn1
 * Navigation / Boot layer
 */
[_FN1] = {
    // Row0: Boot
    { QK_BOOT, _______, _______, _______, _______, _______, _______,  _______, _______, _______, _______, _______, _______, QK_BOOT },

    // Row1: Ins / Del / Up / Back
    { _______, _______, KC_INS,  KC_DEL,  KC_UP,   KC_BSPC, _______,  KC_INS,  KC_DEL,  KC_UP,   KC_BSPC, _______, _______, _______ },

    // Row2: Home / Left / Down / Right / End
    { _______, _______, KC_HOME, KC_LEFT, KC_DOWN, KC_RGHT, KC_END,   KC_HOME, KC_LEFT, KC_DOWN, KC_RGHT, KC_END,  _______, _______ },

    // Row3: Ctrl / Alt / Space / Enter
    { KC_LCTL, KC_LALT, _______, _______, _______, KC_SPC,  KC_ENT,   KC_ENT,  KC_SPC,  _______, _______, _______, KC_RALT, KC_RCTL }
},

/*
 * [2] Fn2
 * Symbol / Numpad layer
 */
[_FN2] = {
    // Row0
    { _______, JP_EXLM, JP_DQUO, JP_HASH, JP_DLR,  JP_PERC, _______,  KC_NUM,  KC_P7,   KC_P8,   KC_P9,   KC_PAST, KC_PSLS, _______ },

    // Row1
    { _______, JP_AMPR, JP_QUOT, JP_TILD, JP_PIPE, JP_CIRC, _______,  _______, KC_P4,   KC_P5,   KC_P6,   KC_PPLS, KC_PMNS, _______ },

    // Row2
    { _______, JP_LBRC, JP_RBRC, JP_SCLN, JP_COLN, JP_AT,   _______,  _______, KC_P1,   KC_P2,   KC_P3,   KC_PEQL, _______, _______ },

    // Row3
    { KC_LCTL, KC_LALT, _______, _______, _______, KC_SPC,  KC_ENT,   _______, KC_P0,   _______, KC_PDOT, _______, _______, _______ }
},

/*
 * [3] Fn3
 * Function key layer
 */
[_FN3] = {
    // Row0
    { KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   _______,  _______, KC_F7,   KC_F8,   KC_F9,   KC_F10,  KC_F11,  KC_F12  },

    // Row1
    { _______, _______, _______, _______, _______, _______, _______,  _______, _______, _______, _______, _______, _______, _______ },

    // Row2
    { _______, _______, _______, _______, _______, _______, _______,  _______, _______, _______, _______, _______, _______, _______ },

    // Row3
    { _______, _______, _______, _______, _______, _______, _______,  _______, _______, _______, _______, _______, _______, _______ }
}

};
