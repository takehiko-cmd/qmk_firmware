# Take&Clock Works Split Keyboard QMK Program Specification

この文書は、`keyboards/takeclockworks/split_keyboard/keymaps/default/keymap.c` を中心に、現在のQMKプログラムの仕様を別のChatGPTや開発者へ説明するためのものです。

## 1. 対象ファイル

- キーマップ本体: `keyboards/takeclockworks/split_keyboard/keymaps/default/keymap.c`
- キーボード定義: `keyboards/takeclockworks/split_keyboard/keyboard.json`
- ハードウェア設定: `keyboards/takeclockworks/split_keyboard/config.h`
- ビルド設定: `keyboards/takeclockworks/split_keyboard/rules.mk`
- カスタムマトリクス: `keyboards/takeclockworks/split_keyboard/matrix.c`

## 2. キーボード概要

- キーボード名: `Fractus MCP For Take&Clock Works`
- メーカー: `Take&Clock Works`
- MCU: RP2040
- ブートローダー: `rp2040`
- マトリクス: 4行 x 14列
- 左側: QMK列 `0` - `6`
- 右側: QMK列 `7` - `13`
- ダイオード方向: `ROW2COL`
- I2C有効
- Raw HID有効
- Console有効

## 3. ハードウェア / マトリクス仕様

### 3.1 I2C設定

`config.h` でI2C1を使用する。

- I2Cドライバ: `I2CD1`
- SDA: `GP6`
- SCL: `GP7`
- クロック: 100 kHz

### 3.2 MCP23017想定のカスタムスキャン

`matrix.c` はQMK標準のマトリクスピンではなく、I2C接続のGPIOエキスパンダを直接スキャンする。

- 左側MCPアドレス: `0x20`
- 右側MCPアドレス: `0x21`
- 使用行数: 4
- 片側の使用列数: 7
- 列の安定待ち: 200 us
- 右側MCPの再接続試行間隔: 500 ms

スキャン方式:

1. USB接続後にI2C初期化を行う。
2. 左MCPを初期化する。
3. 右MCPをpingし、存在すれば初期化する。
4. 各列を1列ずつLowに駆動する。
5. GPIOAの下位4bitを読み、押下行として扱う。
6. 左側はQMK列 `0` - `6`、右側はQMK列 `7` - `13` に割り当てる。
7. 右側が未検出の場合でも左側は動作し、一定間隔で右側の復帰を試す。

## 4. レイヤー構成

`keymap.c` には5レイヤーが定義されている。

| レイヤー番号 | 定数 | 役割 |
| --- | --- | --- |
| 0 | `_BASE` | 通常入力 |
| 1 | `_FN1` | 数字入力 |
| 2 | `_FN2` | 記号入力 / Bootloader長押し |
| 3 | `_FN3` | F1-F12入力 |
| 4 | `_CUSTOM_MODE` | CustomMode用。Fn4復活時に使うため残している |

内部的には、実際のレイヤー番号とは別に `active_slot` がある。

| active_slot | 意味 |
| --- | --- |
| `SLOT_BASE` | Base |
| `SLOT_FN1` | Fn1 |
| `SLOT_FN2` | Fn2 |
| `SLOT_FN3` | Fn3 |

`active_slot` に応じて `_BASE` / `_FN1` / `_FN2` / `_FN3` を使う。
`_CUSTOM_MODE` は `custom_mode_enabled` によって追加でon/offされる。

## 5. 独自キーコード

| キーコード | 役割 |
| --- | --- |
| `FN1_F21` | 押下中だけFn1を有効化 |
| `FN2_F22` | 押下中だけFn2を有効化 |
| `FN3_KEY` | 押下中だけFn3を有効化 |
| `FN4_CUSTOM` | CustomMode切替 |
| `BOOT_HOLD` | 長押しでブートローダーへ入る |

`FN4_CUSTOM` は現行のBase/Fn1/Fn2配置からは外しているが、将来復活できるようキーコード、処理、CustomModeレイヤーは残している。

## 6. タイミング定数

| 定数 | 値 | 意味 |
| --- | --- | --- |
| `BOOT_KEY_DELAY_MS` | 1000 ms | Bootloader起動に必要な長押し時間 |

## 7. Fnキーの動作

### 7.1 `FN1_F21`

押下時:

- `fn1_hold_count` を加算する。
- `active_slot = SLOT_FN1` にする。ただしFn2またはFn3が押されている場合は、より優先度の高いFnを使う。
- `sync_layers()` で `_FN1` へ同期する。

離上時:

- `fn1_hold_count` を0未満にならないよう減算する。
- Fn3が押されていれば `active_slot = SLOT_FN3` にする。
- Fn3が押されておらずFn2が押されていれば `active_slot = SLOT_FN2` にする。
- 他のFnが押されていなければ `active_slot = SLOT_BASE` に戻す。
- `FN1_F21` 単体では `KC_F21` を送信しない。

### 7.2 `FN2_F22`

押下時:

- `fn2_hold_count` を加算する。
- `active_slot = SLOT_FN2` にする。ただしFn3が押されている場合はFn3を優先する。
- `sync_layers()` で `_FN2` へ同期する。

離上時:

- `fn2_hold_count` を0未満にならないよう減算する。
- Fn3が押されていれば `active_slot = SLOT_FN3` にする。
- Fn3が押されておらずFn1が押されていれば `active_slot = SLOT_FN1` に戻す。
- Fn1も押されていなければ `active_slot = SLOT_BASE` に戻す。
- `FN2_F22` 単体では `KC_F22` を送信しない。

### 7.3 `FN3_KEY`

押下時:

- `fn3_hold_count` を加算する。
- `active_slot = SLOT_FN3` にする。
- `sync_layers()` で `_FN3` へ同期する。

離上時:

- `fn3_hold_count` を0未満にならないよう減算する。
- Fn2が押されていれば `active_slot = SLOT_FN2` に戻す。
- Fn2が押されておらずFn1が押されていれば `active_slot = SLOT_FN1` に戻す。
- どのFnキーも押されていなければ `active_slot = SLOT_BASE` に戻す。

### 7.4 Fnキーの同時押し

Fn1、Fn2、Fn3の押下状態が同時に残っている場合は、内部的にFn3を最優先し、次にFn2、最後にFn1を優先する。

- Fn3を離してFn2が残っている場合はFn2へ戻る。
- Fn2を離してFn1が残っている場合はFn1へ戻る。
- すべてのFnキーを離すとBaseへ戻る。
- 左右に同じFnキーが複数あるため、状態はboolではなく `fn1_hold_count` / `fn2_hold_count` / `fn3_hold_count` で管理する。

## 8. ミラーモード削除

ミラーモードは削除済み。
Raw HIDの互換用フィールドとして、旧ミラー状態Byteとoverlay request Byteは残しているが、どちらも常に0を送る。

## 9. F1-F12の出力仕様

`KC_F1` - `KC_F12` はQMK標準処理で即時出力する。

押下時:

- 対応するFキーを押下登録する。

離した時:

- 対応するFキーを解除する。

## 10. Ctrl + 矢印の変換仕様

`KC_UP` / `KC_DOWN` / `KC_LEFT` / `KC_RGHT` は、Ctrl修飾中だけ別キーに変換される。

| 入力 | Ctrlありの出力 |
| --- | --- |
| Ctrl + Up | Page Up |
| Ctrl + Down | Page Down |
| Ctrl + Left | Home |
| Ctrl + Right | End |

実装詳細:

- 通常のCtrl修飾、One Shot Ctrl修飾の両方を見る。
- 出力前にCtrlを一時的に外す。
- 変換キーをタップ送信する。
- 元の修飾状態を復元する。
- 変換済みの矢印キーのreleaseイベントは消費する。

## 11. Bootloader起動仕様

`BOOT_HOLD` を1秒以上押すと `reset_keyboard()` を実行し、ブートローダーへ入る。

現在の配置では `_FN2` レイヤーのRow2 Col0にある。

## 12. Raw HID レイヤー状態通知仕様

Raw HIDにより、ホスト側アプリへ現在のキーボード状態を32バイトで送る。

有効化:

- `rules.mk` で `RAW_ENABLE = yes`

送信タイミング:

- `keyboard_post_init_user()`
- `layer_state_set_user()`
- `sync_layers()` 後
- `matrix_scan_user()` 内で状態変化があれば送信

ただし、前回のstatus reportと完全一致する場合は送信しない。

### 12.1 レポート形式

サイズ: 32 bytes

| Byte | 内容 |
| --- | --- |
| 0 | ASCII `K` |
| 1 | ASCII `L` |
| 2 | ASCII `P` |
| 3 | プロトコルバージョン。現在は `2` |
| 4 | 旧ミラー状態。現在は常に0 |
| 5 | `active_slot`。Base=0、Fn1=1、Fn2=2、Fn3=3 |
| 6 | アプリ用有効レイヤー番号 |
| 7 | overlay request。現在は常に0 |
| 8 | 押下キー数 |
| 9以降 | 押下キーの row / col ペア |

押下キー情報:

- 最大11キーまで格納する。
- 1キーあたり2バイト。
- `[row, col]` の順。
- Byte 9から開始。

### 12.2 アプリ用有効レイヤー番号

`get_effective_app_layer()` は以下を返す。

```c
active_slot
```

つまり:

| 状態 | active_slot | app layer |
| --- | --- | --- |
| Base | 0 | 0 |
| Fn1 | 1 | 1 |
| Fn2 | 2 | 2 |
| Fn3 | 3 | 3 |

### 12.3 overlay request

| 値 | 定数 | 意味 |
| --- | --- | --- |
| 0 | `KLP_OVERLAY_NONE` | overlayなし |

protocol v2ではByte 7、protocol v3ではByte 8に同じ意味で格納する。
現在は常に `KLP_OVERLAY_NONE` を送る。

### 12.4 Split Keyboard時の注意

`SPLIT_KEYBOARD` が定義されている場合、Raw HID送信はmaster側だけで行う。

```c
#if defined(SPLIT_KEYBOARD)
    if (!is_keyboard_master()) {
        return;
    }
#endif
```

## 13. レイヤー同期仕様

`sync_layers()` はQMKの実レイヤー状態を、内部状態 `active_slot` に合わせる。

処理内容:

1. `syncing_layers = true` にする。
2. `_FN1`, `_FN2`, `_FN3` をすべてoffにする。
3. `active_slot` に応じて必要なレイヤーをonにする。
4. `syncing_layers = false` に戻す。
5. Raw HIDで状態を通知する。

`layer_state_set_user()` では、同期中でない場合だけQMKの実レイヤーから内部状態を復元してRaw HID通知する。

## 14. キーマップ

表の列はQMKマトリクス列 `0` - `13` に対応する。

### 14.1 Layer 0: Base

| Row | Col0 | Col1 | Col2 | Col3 | Col4 | Col5 | Col6 | Col7 | Col8 | Col9 | Col10 | Col11 | Col12 | Col13 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | PrintScreen | Esc | Q | W | E | R | T | Y | U | I | O | P | - | Bspc |
| 1 | F2 | Tab | A | S | D | F | G | H | J | K | L | ; | Ins | Del |
| 2 | F8 | LShift | Z | X | C | V | B | N | M | , | . | / | Up | RShift |
| 3 | LGui | LCtrl | LAlt | Fn1 Hold | FN3 | ZKHK | Space | Enter | LCtrl | Bspc | Fn2 Hold | Left | Down | Right |

### 14.2 Layer 1: Fn1

| Row | Col0 | Col1 | Col2 | Col3 | Col4 | Col5 | Col6 | Col7 | Col8 | Col9 | Col10 | Col11 | Col12 | Col13 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | PrintScreen | Esc | No | No | No | No | No | = | 7 | 8 | 9 | + | - | Bspc |
| 1 | F2 | Tab | No | No | No | No | No | : | 4 | 5 | 6 | * | Ins | Del |
| 2 | F8 | LShift | No | No | No | No | No | . | 1 | 2 | 3 | / | Up | RShift |
| 3 | LGui | LCtrl | LAlt | Fn1 Hold | No | ZKHK | Space | Enter | 0 | Bspc | No | Left | Down | Right |

### 14.3 Layer 2: Fn2

| Row | Col0 | Col1 | Col2 | Col3 | Col4 | Col5 | Col6 | Col7 | Col8 | Col9 | Col10 | Col11 | Col12 | Col13 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | ZKHK | Esc | ! | " | # | @ | _ | No | No | No | No | No | No | Bspc |
| 1 | PrintScreen | Tab | % | & | ' | ( | ) | No | No | No | No | No | Ins | Del |
| 2 | BOOT_HOLD | LShift | Yen | $ | \| | [ | ] | No | No | No | No | No | Up | RShift |
| 3 | LGui | LCtrl | LAlt | No | No | ZKHK | Space | Enter | No | Bspc | Fn2 Hold | Left | Down | Right |

### 14.4 Layer 3: Fn3

| Row | Col0 | Col1 | Col2 | Col3 | Col4 | Col5 | Col6 | Col7 | Col8 | Col9 | Col10 | Col11 | Col12 | Col13 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | PrintScreen | Esc | No | No | No | No | No | F1 | F2 | F3 | F4 | F5 | No | Bspc |
| 1 | No | Tab | No | No | No | No | No | F6 | F7 | F8 | F9 | F10 | Ins | Del |
| 2 | BOOT_HOLD | LShift | No | No | No | No | No | F11 | F12 | No | No | No | Up | RShift |
| 3 | LGui | LCtrl | LAlt | No | FN3 | No | Space | Enter | LCtrl | Bspc | No | Left | Down | Right |

### 14.5 Layer 4: CustomMode

| Row | Col0 | Col1 | Col2 | Col3 | Col4 | Col5 | Col6 | Col7 | Col8 | Col9 | Col10 | Col11 | Col12 | Col13 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | Bspc | Esc | Custom00 | Custom01 | Custom02 | Custom03 | Custom04 | = | 7 | 8 | 9 | * | / | Bspc |
| 1 | Tab | Tab | Custom10 | Custom11 | Custom12 | Custom13 | Custom14 | . | 4 | 5 | 6 | + | Ins | Del |
| 2 | LShift | LShift | Custom20 | Custom21 | Custom22 | Custom23 | Custom24 | 0 | 1 | 2 | 3 | - | Up | RShift |
| 3 | Enter | LGui | LAlt | No | Fn2 Hold | Fn1 Hold | Space | Enter | Fn1 Hold | Fn2 Hold | FN4 | Left | Down | Right |

## 15. 日本語配列キー

`keymap.c` は `keymap_japanese.h` をincludeしており、以下の日本語配列キーコードを使う。

- `JP_ZKHK`: 半角/全角
- `JP_MINS`: -
- `JP_SCLN`: ;
- `JP_COMM`: ,
- `JP_DOT`: .
- `JP_SLSH`: /
- `JP_EQL`: =
- `JP_ASTR`: *
- `JP_PLUS`: +
- `JP_UNDS`: _
- `JP_COLN`: :
- `JP_CIRC`: ^
- `JP_DLR`: $
- `JP_EXLM`: !
- `JP_DQUO`: "
- `JP_QUOT`: '
- `JP_PERC`: %
- `JP_LPRN`: (
- `JP_RPRN`: )
- `JP_AMPR`: &
- `JP_YEN`: Yen
- `JP_HASH`: #
- `JP_AT`: @
- `JP_LBRC`: [
- `JP_RBRC`: ]
- `JP_TILD`: ~
- `JP_PIPE`: |

## 16. 主要関数の役割

| 関数 | 役割 |
| --- | --- |
| `process_record_user()` | 全キー入力の前処理。Ctrl矢印変換、独自キー処理を行う |
| `matrix_scan_user()` | Bootloader判定、Raw HID状態通知を行う |
| `layer_state_set_user()` | QMKレイヤー変化から内部状態を同期する |
| `keyboard_post_init_user()` | 起動後にRaw HID状態通知を行う |
| `sync_layers()` | 内部状態に合わせてQMKレイヤーをon/offする |
| `set_status_from_layer()` | QMKの最高レイヤーから `active_slot` を復元する |
| `send_keyboard_layer_status()` | Raw HID状態通知を送る |
| `handle_momentary_fn_key()` | FN1/FN2/FN3の押下中のみ有効なレイヤー切替処理 |
| `handle_ctrl_arrow()` | Ctrl+矢印をPage/Home/End系へ変換 |

## 17. ビルド関連

`rules.mk` の主な設定:

```make
I2C_DRIVER_REQUIRED = yes
MCU = RP2040
BOOTLOADER = rp2040
SRC += matrix.c
CONSOLE_ENABLE = yes
RAW_ENABLE = yes
```

このキーボードは標準マトリクスではなく `matrix.c` を追加して独自スキャンする。

## 18. ChatGPTへ依頼するときの要約文

以下を別のChatGPTに渡すと、現在のプログラムの前提が伝わりやすい。

```text
これはQMK Firmware上のRP2040用4x14分割キーボードです。
左右それぞれMCP23017系I2C GPIOエキスパンダを使い、左0x20、右0x21としてmatrix.cで独自スキャンしています。
左側はQMK列0-6、右側はQMK列7-13です。

keymap.cにはBase/Fn1/Fn2/Fn3と、将来Fn4復活時に使うCustomModeレイヤーがあります。
FN1_F21、FN2_F22、FN3_KEYは押している間だけ対応するFnレイヤーを有効にするモーメンタリキーです。優先順位はFn3、Fn2、Fn1の順で、F21/F22キーイベントは送信しません。
ミラーモードは削除済みです。

F1-F12はQMK標準処理で即時出力します。
Ctrl+矢印はCtrlを一時的に外して、Up=PageUp、Down=PageDown、Left=Home、Right=Endへ変換します。
FN2上のBOOT_HOLDは1秒長押しでreset_keyboard()します。

Raw HIDが有効で、32バイトのKLPレポートをホストへ送ります。
レポートは先頭3バイトが'K','L','P'、バージョン2、旧ミラー状態、active_slot、アプリ用有効レイヤー、overlay request、押下キー数、row/colペアです。旧ミラー状態とoverlay requestは常に0です。
```
