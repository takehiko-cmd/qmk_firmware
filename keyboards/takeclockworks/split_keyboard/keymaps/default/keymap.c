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
    _CUSTOM_MODE = 6,
};

enum custom_keycodes {
    FN1_F21 = SAFE_RANGE,
    FN2_F22,
    FN3_KEY,
    FN4_CUSTOM,
    BOOT_HOLD,
    CUSTOM_KEY_00,
    CUSTOM_KEY_01,
    CUSTOM_KEY_02,
    CUSTOM_KEY_03,
    CUSTOM_KEY_04,
    CUSTOM_KEY_05,
    CUSTOM_KEY_06,
    CUSTOM_KEY_07,
    CUSTOM_KEY_08,
    CUSTOM_KEY_09,
    CUSTOM_KEY_10,
    CUSTOM_KEY_11,
    CUSTOM_KEY_12,
    CUSTOM_KEY_13,
    CUSTOM_KEY_14,
    CUSTOM_KEY_15,
    CUSTOM_KEY_16,
    CUSTOM_KEY_17,
    CUSTOM_KEY_18,
    CUSTOM_KEY_19,
    CUSTOM_KEY_20,
    CUSTOM_KEY_21,
    CUSTOM_KEY_22,
    CUSTOM_KEY_23,
    CUSTOM_KEY_24,
    CUSTOM_KEY_25,
};

#define F_KEY_DELAY_MS 500
#define F_KEY_COUNT 12
#define BOOT_KEY_DELAY_MS 1000
#define KLP_REPORT_SIZE 32
#define KLP_PROTOCOL_VERSION 2
#define KLP_CUSTOM_PROTOCOL_VERSION 3
#define KLP_MAX_PRESSED_KEYS 11
#define KLP_PRESSED_COUNT_OFFSET 8
#define KLP_PRESSED_KEYS_OFFSET 9
#define KLP_V3_MAX_PRESSED_KEYS 9
#define KLP_V3_PRESSED_COUNT_OFFSET 12
#define KLP_V3_PRESSED_KEYS_OFFSET 13
#define RIGHT_SIDE_START_COL 7

enum klp_overlay_request {
    KLP_OVERLAY_NONE = 0,
    KLP_OVERLAY_MIRROR_KEYBOARD,
    KLP_OVERLAY_LEFT_SIDE_ONLY,
};

enum klp_v3_message_type {
    KLP_V3_MESSAGE_STATUS = 0,
    KLP_V3_MESSAGE_OVERLAY_REQUEST,
    KLP_V3_MESSAGE_CUSTOM_KEY_EVENT,
    KLP_V3_MESSAGE_CUSTOM_MODE_STATE,
};

enum klp_v3_event_type {
    KLP_V3_EVENT_RELEASED = 0,
    KLP_V3_EVENT_PRESSED,
};

enum klp_v3_host_command {
    KLP_V3_COMMAND_CUSTOM_MODE_OFF = 0x81,
    KLP_V3_COMMAND_CUSTOM_MODE_ON,
    KLP_V3_COMMAND_STATUS_REQUEST,
};

enum layer_slot {
    SLOT_BASE = 0,
    SLOT_FN1,
    SLOT_FN2,
};

static uint16_t         f_key_timers[F_KEY_COUNT];
static uint8_t          f_key_press_counts[F_KEY_COUNT];
static bool             f_key_registered[F_KEY_COUNT];
static enum layer_slot  active_slot = SLOT_BASE;
static uint8_t          fn1_hold_count;
static uint8_t          fn2_hold_count;
static bool             mirror_mode_enabled;
static bool             is_mirror_mode;
static bool             custom_mode_enabled;
static bool             syncing_layers;
static uint8_t          fn3_press_count;
static bool             mirror_overlay_held;
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

static uint8_t get_keyboard_layer_overlay_request(void) {
    return mirror_overlay_held ? KLP_OVERLAY_MIRROR_KEYBOARD : KLP_OVERLAY_NONE;
}

static void add_pressed_keys_to_report_at(uint8_t *report, uint8_t count_offset, uint8_t keys_offset, uint8_t max_pressed_keys) {
    uint8_t pressed_count = 0;

    for (uint8_t row = 0; row < MATRIX_ROWS && row < 4; row++) {
        matrix_row_t row_state = matrix_get_row(row);

        for (uint8_t col = 0; col < MATRIX_COLS && col < 14; col++) {
            if ((row_state & ((matrix_row_t)1u << col)) == 0) {
                continue;
            }

            if (pressed_count >= max_pressed_keys) {
                return;
            }

            uint8_t offset      = keys_offset + pressed_count * 2;
            report[offset]     = row;
            report[offset + 1] = col;
            pressed_count++;
            report[count_offset] = pressed_count;
        }
    }
}

static void add_pressed_keys_to_report(uint8_t *report) {
    add_pressed_keys_to_report_at(report, KLP_PRESSED_COUNT_OFFSET, KLP_PRESSED_KEYS_OFFSET, KLP_MAX_PRESSED_KEYS);
}

static void send_keyboard_layer_report(void) {
#if defined(SPLIT_KEYBOARD)
    if (!is_keyboard_master()) {
        return;
    }
#endif

    uint8_t overlay_request = get_keyboard_layer_overlay_request();
    uint8_t report[KLP_REPORT_SIZE] = {0};
    report[0]          = 'K';
    report[1]          = 'L';
    report[2]          = 'P';
    report[3]          = KLP_PROTOCOL_VERSION;
    report[4]          = is_mirror_mode ? 1 : 0;
    report[5]          = active_slot;
    report[6]          = get_effective_app_layer();
    report[7]          = overlay_request;
    add_pressed_keys_to_report(report);

    if (has_last_layer_status_report && memcmp(report, last_layer_status_report, sizeof(report)) == 0) {
        return;
    }

    raw_hid_send(report, sizeof(report));

    memcpy(last_layer_status_report, report, sizeof(report));
    has_last_layer_status_report = true;
}

static void send_keyboard_layer_status(void) {
    send_keyboard_layer_report();
}

static void send_custom_mode_report(uint8_t message_type, uint8_t custom_key_id, uint8_t event_type) {
#if defined(SPLIT_KEYBOARD)
    if (!is_keyboard_master()) {
        return;
    }
#endif

    uint8_t report[KLP_REPORT_SIZE] = {0};
    report[0]  = 'K';
    report[1]  = 'L';
    report[2]  = 'P';
    report[3]  = KLP_CUSTOM_PROTOCOL_VERSION;
    report[4]  = message_type;
    report[5]  = is_mirror_mode ? 1 : 0;
    report[6]  = active_slot;
    report[7]  = get_effective_app_layer();
    report[8]  = get_keyboard_layer_overlay_request();
    report[9]  = custom_mode_enabled ? 1 : 0;
    report[10] = custom_key_id;
    report[11] = event_type;
    add_pressed_keys_to_report_at(report, KLP_V3_PRESSED_COUNT_OFFSET, KLP_V3_PRESSED_KEYS_OFFSET, KLP_V3_MAX_PRESSED_KEYS);

    raw_hid_send(report, sizeof(report));
}

static void send_custom_mode_status(void) {
    send_custom_mode_report(KLP_V3_MESSAGE_STATUS, 0, KLP_V3_EVENT_RELEASED);
}

static void send_custom_mode_state(void) {
    send_custom_mode_report(KLP_V3_MESSAGE_CUSTOM_MODE_STATE, 0, KLP_V3_EVENT_RELEASED);
}

static void send_custom_key_event(uint8_t custom_key_id, bool pressed) {
    send_custom_mode_report(KLP_V3_MESSAGE_CUSTOM_KEY_EVENT, custom_key_id, pressed ? KLP_V3_EVENT_PRESSED : KLP_V3_EVENT_RELEASED);
}

static void set_status_from_layer(layer_state_t state) {
    state &= ~((layer_state_t)1u << _CUSTOM_MODE);

    switch (get_highest_layer(state)) {
        case _FN2_MIRROR:
            mirror_mode_enabled = true;
            is_mirror_mode = true;
            active_slot    = SLOT_FN2;
            break;
        case _FN1_MIRROR:
            mirror_mode_enabled = true;
            is_mirror_mode = true;
            active_slot    = SLOT_FN1;
            break;
        case _BASE_MIRROR:
            mirror_mode_enabled = true;
            is_mirror_mode = true;
            active_slot    = SLOT_BASE;
            break;
        case _FN2:
            mirror_mode_enabled = false;
            is_mirror_mode = false;
            active_slot    = SLOT_FN2;
            break;
        case _FN1:
            mirror_mode_enabled = false;
            is_mirror_mode = false;
            active_slot    = SLOT_FN1;
            break;
        case _BASE:
        default:
            mirror_mode_enabled = false;
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

    if (custom_mode_enabled) {
        layer_on(_CUSTOM_MODE);
    } else {
        layer_off(_CUSTOM_MODE);
    }

    syncing_layers = false;
    send_keyboard_layer_status();
}

static void set_custom_mode_enabled(bool enabled) {
    if (custom_mode_enabled == enabled) {
        sync_layers();
        send_custom_mode_state();
        return;
    }

    custom_mode_enabled = enabled;
    sync_layers();
    send_custom_mode_state();
}

static bool get_f_key_index(uint16_t keycode, uint8_t *index) {
    if (keycode < KC_F1 || keycode > KC_F12) {
        return false;
    }

    *index = keycode - KC_F1;
    return true;
}

static void release_registered_f_key(uint8_t f_key_index) {
    if (f_key_registered[f_key_index]) {
        unregister_code16(KC_F1 + f_key_index);
        f_key_registered[f_key_index] = false;
    }
}

static bool handle_delayed_f_key(uint16_t keycode, keyrecord_t *record) {
    uint8_t f_key_index;

    if (!get_f_key_index(keycode, &f_key_index)) {
        return true;
    }

    if (record->event.pressed) {
        if (f_key_press_counts[f_key_index] == 0) {
            f_key_timers[f_key_index]     = timer_read();
            f_key_registered[f_key_index] = false;
        }
        f_key_press_counts[f_key_index]++;
        return false;
    }

    if (f_key_press_counts[f_key_index] > 0) {
        f_key_press_counts[f_key_index]--;
    }

    if (f_key_press_counts[f_key_index] > 0) {
        return false;
    }

    if (f_key_registered[f_key_index]) {
        release_registered_f_key(f_key_index);
    } else if (timer_elapsed(f_key_timers[f_key_index]) >= F_KEY_DELAY_MS) {
        tap_code16(keycode);
    }

    return false;
}

static void update_momentary_fn_slot(void) {
    enum layer_slot next_slot = SLOT_BASE;

    if (fn2_hold_count > 0) {
        next_slot = SLOT_FN2;
    } else if (fn1_hold_count > 0) {
        next_slot = SLOT_FN1;
    }

    if (active_slot != next_slot) {
        active_slot = next_slot;
        sync_layers();
    }
}

static bool handle_momentary_fn_key(uint16_t keycode, keyrecord_t *record) {
    uint8_t *hold_count = NULL;

    switch (keycode) {
        case FN1_F21:
            hold_count = &fn1_hold_count;
            break;
        case FN2_F22:
            hold_count = &fn2_hold_count;
            break;
        default:
            return true;
    }

    if (record->event.pressed) {
        if (*hold_count < 255) {
            (*hold_count)++;
        }
    } else if (*hold_count > 0) {
        (*hold_count)--;
    }

    update_momentary_fn_slot();
    return false;
}

static void handle_fn3_key(keyrecord_t *record) {
    if (record->event.pressed) {
        if (fn3_press_count < 255) {
            fn3_press_count++;
        }

        if (fn3_press_count == 1) {
            mirror_overlay_held = true;
            mirror_mode_enabled = true;
            is_mirror_mode      = true;
            sync_layers();
        }
        return;
    }

    if (fn3_press_count > 0) {
        fn3_press_count--;
    }

    if (fn3_press_count > 0) {
        return;
    }

    mirror_overlay_held = false;
    mirror_mode_enabled = false;
    is_mirror_mode      = false;
    sync_layers();
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

static bool should_block_right_side_key(keyrecord_t *record) {
    return !custom_mode_enabled && mirror_mode_enabled && !is_mirror_mode && record->event.key.col >= RIGHT_SIDE_START_COL;
}

static bool is_custom_keycode(uint16_t keycode) {
    return keycode >= CUSTOM_KEY_00 && keycode <= CUSTOM_KEY_25;
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (should_block_right_side_key(record)) {
        return false;
    }

    if (is_custom_keycode(keycode)) {
        send_custom_key_event(keycode - CUSTOM_KEY_00, record->event.pressed);
        return false;
    }

    if (custom_mode_enabled) {
        switch (keycode) {
            case FN1_F21:
            case FN2_F22:
                if (record->event.pressed) {
                    bool handled;

                    custom_mode_enabled = false;
                    handled = handle_momentary_fn_key(keycode, record);
                    send_custom_mode_state();
                    return handled;
                }
                return handle_momentary_fn_key(keycode, record);
            case FN3_KEY:
                return false;
            case FN4_CUSTOM:
                if (record->event.pressed) {
                    set_custom_mode_enabled(false);
                }
                return false;
        }
    }

    if (!handle_ctrl_arrow(keycode, record)) {
        return false;
    }

    if (!handle_delayed_f_key(keycode, record)) {
        return false;
    }

    switch (keycode) {
        case FN1_F21:
        case FN2_F22:
            return handle_momentary_fn_key(keycode, record);
        case FN3_KEY:
            handle_fn3_key(record);
            return false;
        case FN4_CUSTOM:
            if (record->event.pressed) {
                set_custom_mode_enabled(!custom_mode_enabled);
            }
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

    for (uint8_t i = 0; i < F_KEY_COUNT; i++) {
        if (f_key_press_counts[i] > 0 && !f_key_registered[i] && timer_elapsed(f_key_timers[i]) >= F_KEY_DELAY_MS) {
            register_code16(KC_F1 + i);
            f_key_registered[i] = true;
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

void raw_hid_receive(uint8_t *data, uint8_t length) {
    if (length < 5 || data[0] != 'K' || data[1] != 'L' || data[2] != 'P' || data[3] != KLP_CUSTOM_PROTOCOL_VERSION) {
        return;
    }

    switch (data[4]) {
        case KLP_V3_COMMAND_CUSTOM_MODE_OFF:
            if (length < 6 || data[5] != 0) {
                break;
            }
            set_custom_mode_enabled(false);
            break;
        case KLP_V3_COMMAND_CUSTOM_MODE_ON:
            set_custom_mode_enabled(true);
            break;
        case KLP_V3_COMMAND_STATUS_REQUEST:
            send_custom_mode_status();
            break;
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
    { JP_ZKHK, KC_ESC,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,     KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    JP_MINS, KC_BSPC },

    // Row1
    { KC_F2,   KC_TAB,  KC_A,    KC_S,    KC_D,    KC_F,    KC_G,     KC_H,    KC_J,    KC_K,    KC_L,    JP_SCLN, KC_INS,  KC_DEL  },

    // Row2
    { KC_F8,   KC_LSFT, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,     KC_N,    KC_M,    JP_COMM, JP_DOT,  JP_SLSH, KC_UP,   KC_RSFT },

    // Row3
    { KC_LCTL, KC_LGUI, KC_LALT, FN3_KEY, FN2_F22, FN1_F21, KC_SPC,   KC_ENT,  FN1_F21, FN2_F22, FN4_CUSTOM, KC_LEFT, KC_DOWN, KC_RGHT }
},

/*
 * [1] Fn1
 * Symbols / numbers
 */
[_FN1] = {
    // Row0
    { JP_ZKHK, KC_ESC,  JP_EXLM, JP_DQUO, JP_HASH, JP_AT,   JP_MINS,  JP_EQL,  KC_7,    KC_8,    KC_9,    JP_ASTR, JP_SLSH, KC_BSPC },

    // Row1
    { KC_F2,   KC_TAB,  JP_PERC, JP_AMPR, JP_QUOT, LCTL(KC_Z), LCTL(KC_Y), JP_DOT,  KC_4,    KC_5,    KC_6,    JP_PLUS, KC_INS,  KC_DEL  },

    // Row2
    { KC_F8,   KC_LSFT, KC_NO,   KC_NO,   LCTL(KC_X), LCTL(KC_C), LCTL(KC_V), KC_0,    KC_1,    KC_2,    KC_3,    JP_MINS, KC_UP,   KC_RSFT },

    // Row3
    { KC_LCTL, KC_LGUI, KC_LALT, FN3_KEY, FN2_F22, FN1_F21, KC_SPC,   KC_ENT,  FN1_F21, FN2_F22, FN4_CUSTOM, KC_LEFT, KC_DOWN, KC_RGHT }
},

/*
 * [2] Fn2
 * Function keys / symbols
 */
[_FN2] = {
    // Row0
    { JP_ZKHK, KC_ESC,  KC_PSCR, KC_NO,   KC_F1,   KC_F2,   BOOT_HOLD, JP_LPRN, JP_RPRN, JP_YEN,  JP_DLR,  KC_NO,   KC_NO,   KC_BSPC },

    // Row1
    { KC_F2,   KC_TAB,  KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,    JP_LBRC, JP_RBRC, KC_NO,   KC_NO,   KC_NO,   KC_INS,  KC_DEL  },

    // Row2
    { KC_F8,   KC_LSFT, KC_F8,   KC_F9,   KC_F10,  KC_F11,  KC_F12,   JP_SCLN, JP_COLN, JP_CIRC, JP_PIPE, KC_NO,   KC_UP,   KC_RSFT },

    // Row3
    { KC_LCTL, KC_LGUI, KC_LALT, FN3_KEY, FN2_F22, FN1_F21, KC_SPC,   KC_ENT,  FN1_F21, FN2_F22, FN4_CUSTOM, KC_LEFT, KC_DOWN, KC_RGHT }
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
    { KC_LEFT, KC_DOWN, KC_RGHT, FN3_KEY, FN2_F22, FN1_F21, KC_ENT,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   }
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
    { KC_LEFT, KC_DOWN, KC_RGHT, FN3_KEY, FN2_F22, FN1_F21, KC_ENT,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   }
},

/*
 * [5] Fn2 Mirror
 */
[_FN2_MIRROR] = {
    // Row0
    { KC_BSPC, KC_NO,   KC_NO,   JP_DLR,  JP_YEN,  JP_RPRN, JP_LPRN,  KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   },

    // Row1
    { KC_DEL,  KC_INS,  KC_NO,   KC_NO,   KC_NO,   JP_RBRC, JP_LBRC,  KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   },

    // Row2
    { KC_RSFT, KC_UP,   KC_NO,   JP_PIPE, JP_CIRC, JP_COLN, JP_SCLN,  KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   },

    // Row3
    { KC_LEFT, KC_DOWN, KC_RGHT, FN3_KEY, FN2_F22, FN1_F21, KC_ENT,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO   }
},

/*
 * [6] CustomMode
 * Left side sends CustomMode key events over Raw HID v3.
 * Right side remains normal keyboard input.
 */
[_CUSTOM_MODE] = {
    // Row0
    { KC_BSPC, KC_ESC,  CUSTOM_KEY_00, CUSTOM_KEY_01, CUSTOM_KEY_02, CUSTOM_KEY_03, CUSTOM_KEY_04, JP_EQL,  KC_7,    KC_8,    KC_9,    JP_ASTR, JP_SLSH, KC_BSPC },

    // Row1
    { KC_TAB,  KC_TAB,  CUSTOM_KEY_10, CUSTOM_KEY_11, CUSTOM_KEY_12, CUSTOM_KEY_13, CUSTOM_KEY_14, JP_DOT,  KC_4,    KC_5,    KC_6,    JP_PLUS, KC_INS,  KC_DEL  },

    // Row2
    { KC_LSFT, KC_LSFT, CUSTOM_KEY_20, CUSTOM_KEY_21, CUSTOM_KEY_22, CUSTOM_KEY_23, CUSTOM_KEY_24, KC_0,    KC_1,    KC_2,    KC_3,    JP_MINS, KC_UP,   KC_RSFT },

    // Row3
    { KC_ENT,  KC_LGUI, KC_LALT, KC_NO,   FN2_F22, FN1_F21, KC_SPC,   KC_ENT,  FN1_F21, FN2_F22, FN4_CUSTOM, KC_LEFT, KC_DOWN, KC_RGHT }
}

};
