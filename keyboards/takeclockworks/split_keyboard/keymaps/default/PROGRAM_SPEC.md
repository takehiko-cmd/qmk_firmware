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

`keymap.c` には6レイヤーが定義されている。

| レイヤー番号 | 定数 | 役割 |
| --- | --- | --- |
| 0 | `_BASE` | 通常入力 |
| 1 | `_FN1` | 記号 / 数字入力 |
| 2 | `_FN2` | ファンクションキー / 記号入力 / Bootloader長押し |
| 3 | `_BASE_MIRROR` | 片手ミラーモード用Base |
| 4 | `_FN1_MIRROR` | 片手ミラーモード用Fn1 |
| 5 | `_FN2_MIRROR` | 片手ミラーモード用Fn2 |

内部的には、実際のレイヤー番号とは別に `active_slot` がある。

| active_slot | 意味 |
| --- | --- |
| `SLOT_BASE` | Base |
| `SLOT_FN1` | Fn1 |
| `SLOT_FN2` | Fn2 |

`is_mirror_mode` がfalseなら `_BASE` / `_FN1` / `_FN2` を使い、trueなら `_BASE_MIRROR` / `_FN1_MIRROR` / `_FN2_MIRROR` を使う。

## 5. 独自キーコード

| キーコード | 役割 |
| --- | --- |
| `FN1_F21` | 押下中だけFn1 / Fn1 Mirrorを有効化 |
| `FN2_F22` | 押下中だけFn2 / Fn2 Mirrorを有効化 |
| `FN3_KEY` | 押下中だけミラーモードを有効化 |
| `FN4_CUSTOM` | CustomMode切替 |
| `BOOT_HOLD` | 長押しでブートローダーへ入る |

## 6. タイミング定数

| 定数 | 値 | 意味 |
| --- | --- | --- |
| `F_KEY_DELAY_MS` | 500 ms | F1-F12を長押し扱いにする時間 |
| `BOOT_KEY_DELAY_MS` | 1000 ms | Bootloader起動に必要な長押し時間 |
| `F_KEY_COUNT` | 12 | F1-F12の数 |
| `RIGHT_SIDE_START_COL` | 7 | 右側キーの開始列 |

## 7. Fnキーの動作

### 7.1 `FN1_F21`

押下時:

- `fn1_hold_count` を加算する。
- `active_slot = SLOT_FN1` にする。ただしFn2が押されている場合はFn2を優先する。
- `sync_layers()` で、現在の `is_mirror_mode` に応じて `_FN1` または `_FN1_MIRROR` へ同期する。

離上時:

- `fn1_hold_count` を0未満にならないよう減算する。
- Fn2が押されていれば `active_slot = SLOT_FN2` にする。
- Fn2が押されておらずFn1も残っていなければ `active_slot = SLOT_BASE` に戻す。
- `FN1_F21` 単体では `KC_F21` を送信しない。

### 7.2 `FN2_F22`

押下時:

- `fn2_hold_count` を加算する。
- `active_slot = SLOT_FN2` にする。
- `sync_layers()` で、現在の `is_mirror_mode` に応じて `_FN2` または `_FN2_MIRROR` へ同期する。

離上時:

- `fn2_hold_count` を0未満にならないよう減算する。
- Fn1が押されていれば `active_slot = SLOT_FN1` に戻す。
- Fn1も押されていなければ `active_slot = SLOT_BASE` に戻す。
- `FN2_F22` 単体では `KC_F22` を送信しない。

### 7.3 Fn1 / Fn2の同時押し

Fn1とFn2が同時に押されている場合はFn2を優先する。

- Fn1押下中にFn2を押すとFn2へ切り替わる。
- Fn2を離してFn1が残っている場合はFn1へ戻る。
- Fn1とFn2の両方を離すとBaseへ戻る。
- 左右に同じFnキーが複数あるため、状態はboolではなく `fn1_hold_count` / `fn2_hold_count` で管理する。

## 8. `FN3_KEY` の動作

`FN3_KEY` はミラーモード用の制御キー。

FN3はトグルではなく、押している間だけミラー側へ切り替えるモーメンタリキーとして動作する。

押下時:

- `fn3_press_count` を増やす。
- 最初のFN3押下なら `mirror_mode_enabled = true`、`is_mirror_mode = true` にする。
- `mirror_overlay_held = true` にする。
- 現在の `active_slot` は維持する。
- 対応するMirrorレイヤーへ同期する。
- Raw HID status reportで `overlay_request = 1` を送る。

解放時:

- `fn3_press_count` を減らす。
- まだ他のFN3が押されている場合は状態を維持する。
- 最後のFN3解放なら `mirror_mode_enabled = false`、`is_mirror_mode = false` にする。
- `mirror_overlay_held = false` にする。
- 現在の `active_slot` は維持したまま通常側レイヤーへ同期する。
- Raw HID status reportで `overlay_request = 0` を送る。

送信されるオーバーレイ要求:

| 状態 | overlay_request |
| --- | --- |
| FN3押下中 | `KLP_OVERLAY_MIRROR_KEYBOARD` |
| FN3非押下中 | `KLP_OVERLAY_NONE` |

`overlay_request` は一回限りの表示要求ではなく、FN3/Mirrorキーが押されている現在状態として扱う。
そのため、FN3解放時は必ず `overlay_request = 0` のstatus reportを送る。

## 9. ミラーモード仕様

ミラーモードは右側の有効キー配置を左側へ写す片手入力用レイヤー。

関連フラグ:

| 変数 | 意味 |
| --- | --- |
| `mirror_mode_enabled` | ミラーレイヤーを有効にしているか |
| `is_mirror_mode` | 現在Mirror側のキー配置を使用中か |
| `mirror_overlay_held` | KeyboardLayerPeekへMirror overlayを表示させるためのFN3押下状態 |

重要な制御:

- 現在の実装ではFN3押下中だけ `mirror_mode_enabled == true` かつ `is_mirror_mode == true` になる。
- FN3解放後は `mirror_mode_enabled == false` へ戻るため、物理右側キーは通常通り入力できる。
- `mirror_mode_enabled == true` かつ `is_mirror_mode == false` のとき、右側キーはすべてブロックされる。
- これは `should_block_right_side_key()` で判定される。
- 条件は `record->event.key.col >= 7`。
- Mirrorレイヤーでは右側列 `7` - `13` は基本的に `KC_NO`。

目的:

- FN3を押している間だけ「左側に写した右側面」を使い、離すと通常面へ戻す。

## 10. F1-F12の遅延出力仕様

`KC_F1` - `KC_F12` は通常の即時出力ではなく、500 msの遅延判定を行う。

押下時:

- 初回押下時にタイマー開始。
- すぐにはキーコードを出力しない。

500 ms以上押し続けた場合:

- 対応するFキーを押下登録する。
- 押している間は保持する。
- 離すと解除する。

500 ms未満で離した場合:

- 何も出力しない。

500 ms以上経過してから、マトリクススキャンで押下登録される前に離した場合:

- 対応するFキーをタップ送信する。

この仕様により、Fキーは短い誤押下では発火せず、明確な長押しでのみ使われる。

## 11. Ctrl + 矢印の変換仕様

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

## 12. Bootloader起動仕様

`BOOT_HOLD` を1秒以上押すと `reset_keyboard()` を実行し、ブートローダーへ入る。

現在の配置では `_FN2` レイヤーのRow0 Col6にある。

## 13. Raw HID レイヤー状態通知仕様

Raw HIDにより、ホスト側アプリへ現在のキーボード状態を32バイトで送る。

有効化:

- `rules.mk` で `RAW_ENABLE = yes`

送信タイミング:

- `keyboard_post_init_user()`
- `layer_state_set_user()`
- `sync_layers()` 後
- `matrix_scan_user()` 内で状態変化があれば送信
- FN3押下 / 解放により `mirror_overlay_held` が変化した時

ただし、前回のstatus reportと完全一致する場合は送信しない。

### 13.1 レポート形式

サイズ: 32 bytes

| Byte | 内容 |
| --- | --- |
| 0 | ASCII `K` |
| 1 | ASCII `L` |
| 2 | ASCII `P` |
| 3 | プロトコルバージョン。現在は `2` |
| 4 | `is_mirror_mode`。通常0、Mirror中1 |
| 5 | `active_slot`。Base=0、Fn1=1、Fn2=2 |
| 6 | アプリ用有効レイヤー番号 |
| 7 | overlay request。`mirror_overlay_held` がtrueなら1、falseなら0 |
| 8 | 押下キー数 |
| 9以降 | 押下キーの row / col ペア |

押下キー情報:

- 最大11キーまで格納する。
- 1キーあたり2バイト。
- `[row, col]` の順。
- Byte 9から開始。

### 13.2 アプリ用有効レイヤー番号

`get_effective_app_layer()` は以下を返す。

```c
(is_mirror_mode ? 3 : 0) + active_slot
```

つまり:

| 状態 | active_slot | app layer |
| --- | --- | --- |
| 通常Base | 0 | 0 |
| 通常Fn1 | 1 | 1 |
| 通常Fn2 | 2 | 2 |
| Mirror Base | 0 | 3 |
| Mirror Fn1 | 1 | 4 |
| Mirror Fn2 | 2 | 5 |

### 13.3 overlay request

| 値 | 定数 | 意味 |
| --- | --- | --- |
| 0 | `KLP_OVERLAY_NONE` | overlayなし。FN3/Mirrorキー非押下 |
| 1 | `KLP_OVERLAY_MIRROR_KEYBOARD` | Mirrorキーボード表示。FN3/Mirrorキー押下中 |
| 2 | `KLP_OVERLAY_LEFT_SIDE_ONLY` | 左側のみ表示要求 |

protocol v2ではByte 7、protocol v3ではByte 8に同じ意味で格納する。
FN3解放後は `KLP_OVERLAY_LEFT_SIDE_ONLY` ではなく `KLP_OVERLAY_NONE` を送る。

### 13.4 Split Keyboard時の注意

`SPLIT_KEYBOARD` が定義されている場合、Raw HID送信はmaster側だけで行う。

```c
#if defined(SPLIT_KEYBOARD)
    if (!is_keyboard_master()) {
        return;
    }
#endif
```

## 14. レイヤー同期仕様

`sync_layers()` はQMKの実レイヤー状態を、内部状態 `active_slot` と `is_mirror_mode` に合わせる。

処理内容:

1. `syncing_layers = true` にする。
2. `_FN1`, `_FN2`, `_BASE_MIRROR`, `_FN1_MIRROR`, `_FN2_MIRROR` をすべてoffにする。
3. `is_mirror_mode` と `active_slot` に応じて必要なレイヤーをonにする。
4. `syncing_layers = false` に戻す。
5. Raw HIDで状態を通知する。

`layer_state_set_user()` では、同期中でない場合だけQMKの実レイヤーから内部状態を復元してRaw HID通知する。

## 15. キーマップ

表の列はQMKマトリクス列 `0` - `13` に対応する。

### 15.1 Layer 0: Base

| Row | Col0 | Col1 | Col2 | Col3 | Col4 | Col5 | Col6 | Col7 | Col8 | Col9 | Col10 | Col11 | Col12 | Col13 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | ZKHK | Esc | Q | W | E | R | T | Y | U | I | O | P | - | Bspc |
| 1 | F2 | Tab | A | S | D | F | G | H | J | K | L | ; | Ins | Del |
| 2 | F8 | LShift | Z | X | C | V | B | N | M | , | . | / | Up | RShift |
| 3 | LCtrl | LGui | LAlt | FN3 | Fn2 Hold | Fn1 Hold | Space | Enter | Fn1 Hold | Fn2 Hold | FN4 | Left | Down | Right |

### 15.2 Layer 1: Fn1

| Row | Col0 | Col1 | Col2 | Col3 | Col4 | Col5 | Col6 | Col7 | Col8 | Col9 | Col10 | Col11 | Col12 | Col13 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | ZKHK | Esc | ! | " | # | @ | - | = | 7 | 8 | 9 | * | / | Bspc |
| 1 | F2 | Tab | % | & | ' | Ctrl+Z | Ctrl+Y | . | 4 | 5 | 6 | + | Ins | Del |
| 2 | F8 | LShift | No | No | Ctrl+X | Ctrl+C | Ctrl+V | 0 | 1 | 2 | 3 | - | Up | RShift |
| 3 | LCtrl | LGui | LAlt | FN3 | Fn2 Hold | Fn1 Hold | Space | Enter | Fn1 Hold | Fn2 Hold | FN4 | Left | Down | Right |

### 15.3 Layer 2: Fn2

| Row | Col0 | Col1 | Col2 | Col3 | Col4 | Col5 | Col6 | Col7 | Col8 | Col9 | Col10 | Col11 | Col12 | Col13 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | ZKHK | Esc | PrintScreen | No | F1 | F2 | BOOT_HOLD | ( | ) | Yen | $ | No | No | Bspc |
| 1 | F2 | Tab | F3 | F4 | F5 | F6 | F7 | [ | ] | No | No | No | Ins | Del |
| 2 | F8 | LShift | F8 | F9 | F10 | F11 | F12 | ; | : | ^ | \| | No | Up | RShift |
| 3 | LCtrl | LGui | LAlt | FN3 | Fn2 Hold | Fn1 Hold | Space | Enter | Fn1 Hold | Fn2 Hold | FN4 | Left | Down | Right |

### 15.4 Layer 3: Base Mirror

| Row | Col0 | Col1 | Col2 | Col3 | Col4 | Col5 | Col6 | Col7-Col13 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | Bspc | - | P | O | I | U | Y | No |
| 1 | Del | Ins | ; | L | K | J | H | No |
| 2 | RShift | Up | / | . | , | M | N | No |
| 3 | Left | Down | Right | FN3 | Fn2 Hold | Fn1 Hold | Enter | No |

### 15.5 Layer 4: Fn1 Mirror

| Row | Col0 | Col1 | Col2 | Col3 | Col4 | Col5 | Col6 | Col7-Col13 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | Bspc | / | * | 7 | 8 | 9 | = | No |
| 1 | Del | Ins | + | 4 | 5 | 6 | . | No |
| 2 | RShift | Up | - | 1 | 2 | 3 | 0 | No |
| 3 | Left | Down | Right | FN3 | Fn2 Hold | Fn1 Hold | Enter | No |

### 15.6 Layer 5: Fn2 Mirror

| Row | Col0 | Col1 | Col2 | Col3 | Col4 | Col5 | Col6 | Col7-Col13 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | Bspc | No | No | $ | Yen | ) | ( | No |
| 1 | Del | Ins | No | No | No | ] | [ | No |
| 2 | RShift | Up | No | \| | ^ | : | ; | No |
| 3 | Left | Down | Right | FN3 | Fn2 Hold | Fn1 Hold | Enter | No |

## 16. 日本語配列キー

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

## 17. 主要関数の役割

| 関数 | 役割 |
| --- | --- |
| `process_record_user()` | 全キー入力の前処理。右側ブロック、Ctrl矢印変換、Fキー遅延、独自キー処理を行う |
| `matrix_scan_user()` | F1-F12長押し判定、Bootloader判定、Raw HID状態通知を行う |
| `layer_state_set_user()` | QMKレイヤー変化から内部状態を同期する |
| `keyboard_post_init_user()` | 起動後にRaw HID状態通知を行う |
| `sync_layers()` | 内部状態に合わせてQMKレイヤーをon/offする |
| `set_status_from_layer()` | QMKの最高レイヤーから `active_slot` とミラー状態を復元する |
| `send_keyboard_layer_status()` | Raw HID状態通知を送る。`overlay_request` は `mirror_overlay_held` から作る |
| `handle_momentary_fn_key()` | FN1/FN2の押下中のみ有効なレイヤー切替処理 |
| `handle_fn3_key()` | FN3の押下中だけミラー側へ切り替える処理 |
| `handle_delayed_f_key()` | F1-F12の遅延出力処理 |
| `handle_ctrl_arrow()` | Ctrl+矢印をPage/Home/End系へ変換 |

## 18. ビルド関連

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

## 19. ChatGPTへ依頼するときの要約文

以下を別のChatGPTに渡すと、現在のプログラムの前提が伝わりやすい。

```text
これはQMK Firmware上のRP2040用4x14分割キーボードです。
左右それぞれMCP23017系I2C GPIOエキスパンダを使い、左0x20、右0x21としてmatrix.cで独自スキャンしています。
左側はQMK列0-6、右側はQMK列7-13です。

keymap.cにはBase/Fn1/Fn2と、それぞれのMirror版の合計6レイヤーがあります。
FN1_F21とFN2_F22は押している間だけFn1/Fn2レイヤーを有効にするモーメンタリキーです。Fn2はFn1より優先され、F21/F22キーイベントは送信しません。
FN3_KEYはミラーモード制御で、押している間だけ左側ミラー面へ切り替え、離すと通常面へ戻ります。
FN3を離した後は物理右側キーも通常通り入力できます。

F1-F12は500ms以上押したときだけ出力する遅延キーになっています。
Ctrl+矢印はCtrlを一時的に外して、Up=PageUp、Down=PageDown、Left=Home、Right=Endへ変換します。
FN2上のBOOT_HOLDは1秒長押しでreset_keyboard()します。

Raw HIDが有効で、32バイトのKLPレポートをホストへ送ります。
レポートは先頭3バイトが'K','L','P'、バージョン2、ミラー状態、active_slot、アプリ用有効レイヤー、overlay request、押下キー数、row/colペアです。
```
