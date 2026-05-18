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

#define FN_KEY_DELAY_MS 500

enum fn_key_indexes {
    FN_KEY_1 = 0,
    FN_KEY_2,
    FN_KEY_3,
    FN_KEY_COUNT,
};

static const uint8_t fn_layers[FN_KEY_COUNT] = {
    _FN1,
    _FN2,
    _FN3,
};

static const uint8_t fn_output_keycodes[FN_KEY_COUNT] = {
    KC_F21,
    KC_F22,
    KC_F23,
};

static uint16_t fn_timers[FN_KEY_COUNT];
static uint8_t  fn_press_counts[FN_KEY_COUNT];
static bool     fn_registered[FN_KEY_COUNT];
static bool     fn_output_suppressed[FN_KEY_COUNT];
static bool     fn12_f20_tapped;

static void release_registered_fn_output(uint8_t fn_index) {
    if (fn_registered[fn_index]) {
        unregister_code(fn_output_keycodes[fn_index]);
        fn_registered[fn_index] = false;
    }
}

static void tap_fn12_f20_if_chorded(void) {
    if (fn12_f20_tapped || fn_press_counts[FN_KEY_1] == 0 || fn_press_counts[FN_KEY_2] == 0) {
        return;
    }

    release_registered_fn_output(FN_KEY_1);
    release_registered_fn_output(FN_KEY_2);
    fn_output_suppressed[FN_KEY_1] = true;
    fn_output_suppressed[FN_KEY_2] = true;
    tap_code(KC_F20);
    fn12_f20_tapped = true;
}

static void handle_delayed_fn_key(uint8_t fn_index, bool pressed) {
    if (pressed) {
        if (fn_press_counts[fn_index] == 0) {
            fn_timers[fn_index]     = timer_read();
            fn_registered[fn_index] = false;
        }
        fn_press_counts[fn_index]++;
        layer_on(fn_layers[fn_index]);
        tap_fn12_f20_if_chorded();
    } else {
        if (fn_press_counts[fn_index] > 0) {
            fn_press_counts[fn_index]--;
        }
        if (fn_press_counts[fn_index] == 0) {
            release_registered_fn_output(fn_index);
            fn_output_suppressed[fn_index] = false;
            if (fn_index == FN_KEY_1 || fn_index == FN_KEY_2) {
                fn12_f20_tapped = false;
            }
            layer_off(fn_layers[fn_index]);
        }
    }
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case FN1_F21:
            handle_delayed_fn_key(FN_KEY_1, record->event.pressed);
            return false;
        case FN2_F22:
            handle_delayed_fn_key(FN_KEY_2, record->event.pressed);
            return false;
        case FN3_F23:
            handle_delayed_fn_key(FN_KEY_3, record->event.pressed);
            return false;
    }
    return true;
}

void matrix_scan_user(void) {
    tap_fn12_f20_if_chorded();

    for (uint8_t i = 0; i < FN_KEY_COUNT; i++) {
        if (fn_press_counts[i] > 0 && !fn_registered[i] && !fn_output_suppressed[i] && timer_elapsed(fn_timers[i]) >= FN_KEY_DELAY_MS) {
            register_code(fn_output_keycodes[i]);
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
