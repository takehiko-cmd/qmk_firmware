#include "quantum.h"
#include "i2c_master.h"
#include "wait.h"

#ifdef CONSOLE_ENABLE
#    include "print.h"
#    define LOG(...) uprintf(__VA_ARGS__)
#else
#    define LOG(...) do {} while (0)
#endif

// ===== MCP23017 I2C 7bit address =====
#define MCP_L 0x20
#define MCP_R 0x21

// ===== MCP23017 regs (BANK=0) =====
#define IODIRA  0x00
#define IODIRB  0x01
#define GPPUA   0x0C
#define GPPUB   0x0D
#define GPIOA   0x12
#define GPIOB   0x13
#define OLATA   0x14
#define OLATB   0x15
#define IOCON   0x0A

// settle time
#define COL_SETTLE_US 200

// 使うピン範囲
#define ROWS_USED 4          // A0-A3
#define COLS_USED 7          // B0-B6

static matrix_row_t matrix[MATRIX_ROWS];

// QMKのi2c_*が8-bitアドレス扱いの環境対策（<<1）
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

static bool right_present = false;

// ROW2COL 前提
// - COL (B0-B6): 出力で1本だけLOW（他はHIGH）
// - ROW (A0-A3): 入力プルアップで読む（LOW=押下）
static void mcp_init_row2col(uint8_t addr7) {
    (void)mcp_write(addr7, IOCON, 0x00);

    // A: rows input (A0-3=1), A4-7 inputのまま
    (void)mcp_write(addr7, IODIRA, 0xFF);
    // A0-3 pullup ON（他は不要なら0でもOKだが安全に全ON）
    (void)mcp_write(addr7, GPPUA, 0xFF);

    // B: cols output (B0-6=0), B7 input(1)でOK
    // IODIR bit: 1=input, 0=output
    // B0-6 = output -> 0, B7 = input -> 1  => 0b1000_0000 = 0x80
    (void)mcp_write(addr7, IODIRB, 0x80);
    // 出力なのでプルアップは不要（入れない）
    (void)mcp_write(addr7, GPPUB, 0x00);

    // cols all HIGH (inactive): B0-6=1
    (void)mcp_write(addr7, OLATB, 0x7F);
}

static inline void cols_all_high(uint8_t addr7) {
    (void)mcp_write(addr7, OLATB, 0x7F);
}

// col_index: 0..6
static inline void drive_col_low(uint8_t addr7, uint8_t col_index) {
    // B0-6: 1=HIGH, 0=LOW. 1本だけLOW。
    uint8_t mask = (uint8_t)(0x7F & ~(1u << col_index));
    (void)mcp_write(addr7, OLATB, mask);
}

void keyboard_post_init_kb(void) {
    keyboard_post_init_user();

#ifdef CONSOLE_ENABLE
    wait_ms(300);
    i2c_init();

    bool okL = mcp_ping(MCP_L);
    bool okR = mcp_ping(MCP_R);
    LOG("POST_INIT MCP okL=%d okR=%d\n", okL, okR);

    // I2C scan（見つけたら出す）
    for (uint8_t a7 = 0x08; a7 < 0x78; a7++) {
        if (mcp_ping(a7)) {
            LOG("I2C found (7bit): 0x%02X\n", a7);
        }
    }
#endif
}

void matrix_init(void) {
    i2c_init();

    for (uint8_t r = 0; r < MATRIX_ROWS; r++) {
        matrix[r] = 0;
    }

    // 左は必須
    mcp_init_row2col(MCP_L);

    // 右は存在する時だけ
    right_present = mcp_ping(MCP_R);
    if (right_present) {
        mcp_init_row2col(MCP_R);
    }

#ifdef CONSOLE_ENABLE
    LOG("matrix_init right_present=%d\n", right_present);
#endif
}

uint8_t matrix_scan(void) {
    bool changed = false;

    // cols (B0-B6) を順にLOWにして、rows(A0-A3)を読む
    for (uint8_t col = 0; col < COLS_USED; col++) {

        // 左/右 それぞれ同じcolをLOWにする
        drive_col_low(MCP_L, col);
        if (right_present) {
            drive_col_low(MCP_R, col);
        }

        wait_us(COL_SETTLE_US);

        // row読み取り：LOW=押下
        uint8_t rowA_L = 0xFF, rowA_R = 0xFF;
        bool okL = mcp_read(MCP_L, GPIOA, &rowA_L);
        bool okR = right_present ? mcp_read(MCP_R, GPIOA, &rowA_R) : false;

        uint8_t pressed_rows_L = okL ? (uint8_t)((~rowA_L) & 0x0F) : 0; // A0-3
        uint8_t pressed_rows_R = okR ? (uint8_t)((~rowA_R) & 0x0F) : 0;

        // matrix[row] の該当ビットを更新する
        // cols: 左=0..6, 右=7..13（<<7）
        for (uint8_t row = 0; row < ROWS_USED; row++) {
            matrix_row_t old = matrix[row];
            matrix_row_t newv = old;

            // 左側 col
            if (pressed_rows_L & (1u << row)) newv |=  (matrix_row_t)(1u << col);
            else                              newv &= ~(matrix_row_t)(1u << col);

            // 右側 col（右未完成なら常に0）
            if (right_present) {
                uint8_t bit = (uint8_t)(7 + col);
                if (pressed_rows_R & (1u << row)) newv |=  (matrix_row_t)(1u << bit);
                else                              newv &= ~(matrix_row_t)(1u << bit);
            } else {
                uint8_t bit = (uint8_t)(7 + col);
                newv &= ~(matrix_row_t)(1u << bit);
            }

            if (newv != old) {
#ifdef CONSOLE_ENABLE
                matrix_row_t diff = old ^ newv;

                // 左側 bit 0..6
                if (diff & (matrix_row_t)(1u << col)) {
                    bool pressed = (newv & (matrix_row_t)(1u << col)) != 0;
                    LOG("%s L row=%u col=%u rawA=0x%02X pressed_rows=0x%X\n",
                        pressed ? "PRESS" : "RELEASE",
                        row, col, rowA_L, pressed_rows_L);
                }

                // 右側 bit 7..13
                if (right_present) {
                    uint8_t bit = (uint8_t)(7 + col);
                    if (diff & (matrix_row_t)(1u << bit)) {
                        bool pressed = (newv & (matrix_row_t)(1u << bit)) != 0;
                        LOG("%s R row=%u col=%u rawA=0x%02X pressed_rows=0x%X\n",
                            pressed ? "PRESS" : "RELEASE",
                            row, col, rowA_R, pressed_rows_R);
                    }
                }
#endif
                matrix[row] = newv;
                changed = true;
            }
        }
    }

    // 解除：colsを全部HIGH
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
    for (uint8_t r = 0; r < MATRIX_ROWS; r++) {
        LOG("row%u=%04X\n", r, (uint16_t)matrix[r]);
    }
#endif
}