#include "quantum.h"
#include "i2c_master.h"
#include "usb_util.h"
#include "wait.h"

#ifdef CONSOLE_ENABLE
#    include "print.h"
#    define LOG(...) uprintf(__VA_ARGS__)
#else
#    define LOG(...) \
        do {         \
        } while (0)
#endif

#define MCP_L 0x20
#define MCP_R 0x21

#define IODIRA 0x00
#define IODIRB 0x01
#define GPPUA  0x0C
#define GPPUB  0x0D
#define GPIOA  0x12
#define OLATB  0x15
#define IOCON  0x0A

#define ROWS_USED 4
#define COLS_USED 7
#define COL_SETTLE_US 200
#define RIGHT_RETRY_MS 500

static matrix_row_t matrix[MATRIX_ROWS];

static bool     i2c_scan_enabled = false;
static bool     right_present    = false;
static uint32_t right_retry_time = 0;

static bool mcp_write(uint8_t addr7, uint8_t reg, uint8_t data) {
    uint8_t addr8 = (uint8_t)(addr7 << 1);
    return i2c_write_register(addr8, reg, &data, 1, 50) == I2C_STATUS_SUCCESS;
}

static bool mcp_read(uint8_t addr7, uint8_t reg, uint8_t *out) {
    uint8_t addr8 = (uint8_t)(addr7 << 1);
    return i2c_read_register(addr8, reg, out, 1, 50) == I2C_STATUS_SUCCESS;
}

static bool mcp_ping(uint8_t addr7) {
    uint8_t addr8 = (uint8_t)(addr7 << 1);
    return i2c_ping_address(addr8, 1) == I2C_STATUS_SUCCESS;
}

static void mcp_init_row2col(uint8_t addr7) {
    (void)mcp_write(addr7, IOCON, 0x00);

    (void)mcp_write(addr7, IODIRA, 0xFF);
    (void)mcp_write(addr7, GPPUA, 0xFF);

    (void)mcp_write(addr7, OLATB, 0x7F);
    (void)mcp_write(addr7, GPPUB, 0x00);
    (void)mcp_write(addr7, IODIRB, 0x80);
    (void)mcp_write(addr7, OLATB, 0x7F);
}

static inline void cols_all_high(uint8_t addr7) {
    (void)mcp_write(addr7, OLATB, 0x7F);
}

static inline void drive_col_low(uint8_t addr7, uint8_t col_index) {
    uint8_t mask = (uint8_t)(0x7F & ~(1u << col_index));
    (void)mcp_write(addr7, OLATB, mask);
}

static void clear_matrix(void) {
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        matrix[row] = 0;
    }
}

static void try_enable_i2c_scan(void) {
    if (i2c_scan_enabled || !usb_connected_state()) {
        return;
    }

    i2c_scan_enabled = true;
    i2c_init();

    mcp_init_row2col(MCP_L);

    right_present = mcp_ping(MCP_R);
    if (right_present) {
        mcp_init_row2col(MCP_R);
    }

    cols_all_high(MCP_L);
    if (right_present) {
        cols_all_high(MCP_R);
    }

    right_retry_time = timer_read32();
    LOG("I2C matrix enabled right_present=%d\n", right_present);
}

static void try_init_right(void) {
    if (right_present || timer_elapsed32(right_retry_time) < RIGHT_RETRY_MS) {
        return;
    }

    right_retry_time = timer_read32();
    if (mcp_ping(MCP_R)) {
        mcp_init_row2col(MCP_R);
        cols_all_high(MCP_R);
        right_present = true;
        LOG("right MCP recovered\n");
    }
}

void keyboard_post_init_kb(void) {
    keyboard_post_init_user();

#ifdef CONSOLE_ENABLE
    if (!i2c_scan_enabled) {
        return;
    }

    wait_ms(300);

    bool okL = mcp_ping(MCP_L);
    bool okR = mcp_ping(MCP_R);
    LOG("POST_INIT MCP okL=%d okR=%d\n", okL, okR);

    for (uint8_t addr7 = 0x08; addr7 < 0x78; addr7++) {
        if (mcp_ping(addr7)) {
            LOG("I2C found (7bit): 0x%02X\n", addr7);
        }
    }
#endif
}

void matrix_init(void) {
    clear_matrix();
    try_enable_i2c_scan();
}

uint8_t matrix_scan(void) {
    bool changed = false;

    try_enable_i2c_scan();
    if (!i2c_scan_enabled) {
        return false;
    }

    try_init_right();

    for (uint8_t col = 0; col < COLS_USED; col++) {
        drive_col_low(MCP_L, col);
        if (right_present) {
            drive_col_low(MCP_R, col);
        }

        wait_us(COL_SETTLE_US);

        uint8_t rowA_L = 0xFF;
        uint8_t rowA_R = 0xFF;
        bool    okL    = mcp_read(MCP_L, GPIOA, &rowA_L);
        bool    okR    = right_present ? mcp_read(MCP_R, GPIOA, &rowA_R) : false;

        uint8_t pressed_rows_L = okL ? (uint8_t)((~rowA_L) & 0x0F) : 0;
        uint8_t pressed_rows_R = okR ? (uint8_t)((~rowA_R) & 0x0F) : 0;

        for (uint8_t row = 0; row < ROWS_USED; row++) {
            matrix_row_t old  = matrix[row];
            matrix_row_t newv = old;

            if (pressed_rows_L & (1u << row)) {
                newv |= (matrix_row_t)(1u << col);
            } else {
                newv &= ~(matrix_row_t)(1u << col);
            }

            uint8_t right_bit = (uint8_t)(7 + col);
            if (right_present && (pressed_rows_R & (1u << row))) {
                newv |= (matrix_row_t)(1u << right_bit);
            } else {
                newv &= ~(matrix_row_t)(1u << right_bit);
            }

            if (newv != old) {
#ifdef CONSOLE_ENABLE
                matrix_row_t diff = old ^ newv;

                if (diff & (matrix_row_t)(1u << col)) {
                    bool pressed = (newv & (matrix_row_t)(1u << col)) != 0;
                    LOG("%s L row=%u col=%u rawA=0x%02X pressed_rows=0x%X\n", pressed ? "PRESS" : "RELEASE", row, col, rowA_L, pressed_rows_L);
                }

                if (diff & (matrix_row_t)(1u << right_bit)) {
                    bool pressed = (newv & (matrix_row_t)(1u << right_bit)) != 0;
                    LOG("%s R row=%u col=%u rawA=0x%02X pressed_rows=0x%X\n", pressed ? "PRESS" : "RELEASE", row, col, rowA_R, pressed_rows_R);
                }
#endif
                matrix[row] = newv;
                changed     = true;
            }
        }
    }

    cols_all_high(MCP_L);
    if (right_present) {
        cols_all_high(MCP_R);
    }

    return changed;
}

matrix_row_t matrix_get_row(uint8_t row) {
    return matrix[row];
}

void matrix_print(void) {
#ifdef CONSOLE_ENABLE
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        LOG("row%u=%04X\n", row, (uint16_t)matrix[row]);
    }
#endif
}
