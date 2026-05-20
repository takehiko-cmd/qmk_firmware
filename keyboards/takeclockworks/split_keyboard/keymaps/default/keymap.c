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
    _MIRROR,
};

enum custom_keycodes {
    FN1_F21 = SAFE_RANGE,
    FN2_F22,
    FN3_F23,
};

#define FN_KEY_DELAY_MS 500
#define RIGHT_HAND_MODE_LED_ON_STATE false

enum fn_key_indexes {
    FN_KEY_1 = 0,
    FN_KEY_2,
    FN_KEY_3,
    FN_KEY_COUNT,
};

static const uint16_t fn_output_keycodes[FN_KEY_COUNT] = {
    KC_F21,
    KC_F22,
    KC_F23,
};

static uint16_t fn_timers[FN_KEY_COUNT];
static uint8_t  fn_press_counts[FN_KEY_COUNT];
static bool     fn_layer_active[FN_KEY_COUNT];
static bool     fn_registered[FN_KEY_COUNT];
static bool     fn_output_suppressed[FN_KEY_COUNT];
static bool     fn12_f20_tapped;
static bool     mirror_layer_enabled;

static const pin_t right_hand_mode_led_pins[] = {
    GP17,
    GP25,
    GP16,
};

static void set_right_hand_mode_led(bool enabled) {
    for (uint8_t i = 0; i < ARRAY_SIZE(right_hand_mode_led_pins); i++) {
        gpio_write_pin(right_hand_mode_led_pins[i], enabled ? RIGHT_HAND_MODE_LED_ON_STATE : !RIGHT_HAND_MODE_LED_ON_STATE);
    }
}

void keyboard_post_init_user(void) {
    for (uint8_t i = 0; i < ARRAY_SIZE(right_hand_mode_led_pins); i++) {
        gpio_set_pin_output(right_hand_mode_led_pins[i]);
    }
    set_right_hand_mode_led(mirror_layer_enabled);
}

static void sync_mirror_layer(void) {
    if (mirror_layer_enabled && fn_press_counts[FN_KEY_1] == 0 && fn_press_counts[FN_KEY_2] == 0) {
        layer_on(_MIRROR);
    } else {
        layer_off(_MIRROR);
    }
}

static void release_registered_fn_output(uint8_t fn_index) {
    if (fn_registered[fn_index]) {
        unregister_code16(fn_output_keycodes[fn_index]);
        fn_registered[fn_index] = false;
    }
}

static void activate_fn_layer(uint8_t fn_index) {
    if (fn_layer_active[fn_index]) {
        return;
    }

    if (fn_index == FN_KEY_1) {
        layer_on(_FN1);
    } else if (fn_index == FN_KEY_2) {
        layer_on(_FN2);
    }

    fn_layer_active[fn_index] = true;
    sync_mirror_layer();
}

static void deactivate_fn_layer(uint8_t fn_index) {
    if (!fn_layer_active[fn_index]) {
        return;
    }

    if (fn_index == FN_KEY_1) {
        layer_off(_FN1);
    } else if (fn_index == FN_KEY_2) {
        layer_off(_FN2);
    }

    fn_layer_active[fn_index] = false;
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

static void handle_fn1_key(bool pressed) {
    if (pressed) {
        if (fn_press_counts[FN_KEY_1] == 0) {
            fn_timers[FN_KEY_1]      = timer_read();
            fn_registered[FN_KEY_1]  = false;
            fn_layer_active[FN_KEY_1] = false;
        }
        fn_press_counts[FN_KEY_1]++;
        sync_mirror_layer();
        tap_fn12_f20_if_chorded();
    } else {
        if (fn_press_counts[FN_KEY_1] > 0) {
            fn_press_counts[FN_KEY_1]--;
        }
        if (fn_press_counts[FN_KEY_1] == 0) {
            release_registered_fn_output(FN_KEY_1);
            deactivate_fn_layer(FN_KEY_1);
            fn_output_suppressed[FN_KEY_1] = false;
            fn12_f20_tapped = false;

            sync_mirror_layer();
        }
    }
}

static void handle_delayed_fn_key(uint8_t fn_index, bool pressed) {
    if (pressed) {
        if (fn_press_counts[fn_index] == 0) {
            fn_timers[fn_index]      = timer_read();
            fn_registered[fn_index]  = false;
            fn_layer_active[fn_index] = false;
        }
        fn_press_counts[fn_index]++;
        if (fn_index == FN_KEY_2) {
            activate_fn_layer(fn_index);
        }
        tap_fn12_f20_if_chorded();
    } else {
        if (fn_press_counts[fn_index] > 0) {
            fn_press_counts[fn_index]--;
        }
        if (fn_press_counts[fn_index] == 0) {
            bool short_tap = fn_index == FN_KEY_3 && !fn_layer_active[fn_index] && !fn_output_suppressed[fn_index] && timer_elapsed(fn_timers[fn_index]) < FN_KEY_DELAY_MS;

            release_registered_fn_output(fn_index);
            deactivate_fn_layer(fn_index);
            fn_output_suppressed[fn_index] = false;
            if (fn_index == FN_KEY_2) {
                fn12_f20_tapped = false;
            }
            if (short_tap) {
                mirror_layer_enabled = !mirror_layer_enabled;
                set_right_hand_mode_led(mirror_layer_enabled);
            }
            sync_mirror_layer();
        }
    }
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case FN1_F21:
            handle_fn1_key(record->event.pressed);
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

    if (fn_press_counts[FN_KEY_1] > 0 && !fn_layer_active[FN_KEY_1] && timer_elapsed(fn_timers[FN_KEY_1]) >= FN_KEY_DELAY_MS) {
        activate_fn_layer(FN_KEY_1);
    }

    for (uint8_t i = 0; i < FN_KEY_COUNT; i++) {
        if (fn_press_counts[i] > 0 && !fn_registered[i] && !fn_output_suppressed[i] && timer_elapsed(fn_timers[i]) >= FN_KEY_DELAY_MS) {
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
    { JP_ZKHK, KC_LCTL, KC_LALT, FN3_F23, FN2_F22, FN1_F21, KC_SPC,  KC_ENT,  FN1_F21, FN2_F22, FN3_F23, KC_LEFT, KC_DOWN, KC_RGHT }
},

/*
 * [1] Fn1
 * Function keys
 */
[_FN1] = {
    // Row0
    { _______, _______, KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,    KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10,  KC_F11,  KC_F12  },

    // Row1
    { _______, _______, _______, _______, _______, _______, _______,   _______, _______, _______, _______, _______, _______, _______ },

    // Row2
    { _______, _______, _______, _______, _______, _______, _______,   _______, _______, _______, _______, _______, _______, _______ },

    // Row3
    { _______, _______, _______, _______, _______, _______, KC_SPC,   KC_ENT,  _______, _______, _______, _______, _______, QK_BOOT }
},

/*
 * [2] Fn2
 * Symbols / numbers
 */
[_FN2] = {
    // Row0
    { JP_ZKHK, KC_ESC,  JP_EXLM, JP_DQUO, JP_HASH, JP_DLR,  JP_PERC, KC_7,    KC_8,    KC_9,    JP_ASTR, JP_SLSH, KC_INS,  KC_BSPC },

    // Row1
    { KC_PSCR, KC_TAB,  JP_YEN,  JP_TILD, JP_CIRC, JP_LPRN, JP_RPRN, KC_4,    KC_5,    KC_6,    JP_PLUS, JP_MINS, _______, KC_DEL  },

    // Row2
    { _______, KC_LSFT, JP_PIPE, JP_AMPR, JP_AT,   JP_LBRC, JP_RBRC, KC_1,    KC_2,    KC_3,    JP_DOT,  JP_EQL,  KC_PGUP, KC_RSFT },

    // Row3
    { _______, KC_LCTL, _______, _______, _______, _______, KC_SPC,   KC_ENT,  KC_0,    _______, _______, KC_HOME, KC_PGDN, KC_END  }
},

/*
 * [3] Mirror
 * Toggled by a short Fn3 tap
 */
[_MIRROR] = {
    // Row0
    { KC_BSPC, JP_MINS, KC_P,    KC_O,    KC_I,    KC_U,    KC_Y,     KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   },

    // Row1
    { KC_TAB,  JP_COLN, JP_SCLN, KC_L,    KC_K,    KC_J,    KC_H,     KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   },

    // Row2
    { KC_LSFT, KC_NO,   JP_SLSH, JP_DOT,  JP_COMM, KC_M,    KC_N,     KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   },

    // Row3
    { KC_ENT,  KC_LCTL, KC_LALT, FN3_F23, FN2_F22, FN1_F21, KC_SPC,  KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   }
}

};
