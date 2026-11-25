# STM32U575 Static GPDMA Descriptor Dispatch

[English](#english) | [日本語](#日本語)

> **Status: v2.0.0-beta.3 public release record**
>
> This README is the canonical source for public information about the current
> v2.0.0-beta.3 release state, supported behavior, validation results, known
> limitations, and remaining release work. The usage guides explain how to use
> the API, the design-decision document preserves detailed rationale, and the
> development-status document is frozen as the beta.2 development snapshot.
>
> beta.3 introduces the ROOT completion boundary (`finalize_lli_group()` /
> `finalized_lli_group`), unifies component construction into `lli_group` /
> `make_lli_group()` (removing `lli_program` / `make_lli_program()` and renaming
> `ProgramTag` to `GroupTag`), and unifies node wiring into declarative
> build-time rules (`jump<From, To>{}` and `end_node<Position>{}`).

---

# English

## Overview

This example demonstrates a self-modifying STM32U575 GPDMA linked-list
sequence using the v2.0.0-beta.3 library model.

After the CPU prepares the channel register file, loads the named entry point,
and enables GPDMA1 Channel 0, GPDMA executes two instantiated `GpioDSequence`
pulses (`Instance1` → `Instance2`) and then repeatedly performs the SPI
descriptor-dispatch loop without further CPU descriptor selection:

1. Reads one descriptor from a ROM descriptor ring.
2. Rewrites the selector state for the next descriptor.
3. Rewrites the executor node with the selected SPI transfer.
4. Executes the SPI transfer.
5. Disables SPI after the transfer completes.
6. Updates the GPIO control signal.
7. Returns to the selector and repeats.

The descriptor sequence reproduced during hardware testing is unchanged from
alpha.1:

| Descriptor | Source offset | Length | Observed MOSI data |
|---:|---:|---:|---|
| 0 | `DUMMY + 10` | 10 bytes | `10-19` |
| 1 | `DUMMY + 0` | 10 bytes | `0-9` |
| 2 | `DUMMY + 40` | 10 bytes | `40-49` |
| 3 | `DUMMY + 12` | 10 bytes | `12-21` |

After descriptor 3, the ring returns to descriptor 0.

## What changed in beta.3

### 1. ROOT completion boundary (`finalize_lli_group()`)

- Separates the mutable construction phase (`lli_group`) from the finalized
  execution phase (`finalized_lli_group`) at the type level.
- Allowed **before** `finalize_lli_group()`:
  `make_lli_group()`, `instantiate()`, and `append_lli_group()`.
- Allowed **after** `finalize_lli_group()`:
  `validate_root()`, tag lookup / node position checks,
  `descriptor_dispatch_layout`, and `load_lli()`.
- Passing a `finalized_lli_group` to `make_lli_group()`, `append_lli_group()`,
  or `instantiate()` is rejected at compile time.
- Runtime size and memory layout of `finalized_lli_group` match `lli_group`
  exactly (`sizeof` and `offsetof(root) == 0` verified by `static_assert`).

### 2. Unification of `Program` into `Group` (`GroupTag`)

- Removed the intermediate `lli_program` container and `make_lli_program()`;
  both reusable sub-components and full ROOT candidates are now built uniformly
  with `make_lli_group()`, and `instantiate<InstanceTag>()` returns a new
  `lli_group` directly.
- Renamed `ProgramTag` to `GroupTag` (`default_program_tag` →
  `default_group_tag`) so the two-level list tag hierarchy matches the
  `Group` → `List` → `Node` container model.

### 3. Unified declarative wiring (`jump<From, To>{}` / `end_node<Position>{}`)

- Replaced imperative `patch_jump()` and `patch_end()` with declarative
  build-time wiring rules passed directly to `make_lli_group()` and
  `append_lli_group()`.
- Wiring rules are stored as empty type parameters on `lli_group` with zero
  runtime memory overhead, and are stripped by `finalize_lli_group()`.
- `instantiate<InstanceTag>()` automatically rewrites `GroupTag` inside
  internal wiring rules to `instanced_tag<InstanceTag, GroupTag>`.
- `make_lli_group()` and `append_lli_group()` recalculate all chains
  (`fixup_group()` → `connect_group_lists()`) and then apply inner component
  rules followed by outer rules, preserving reusable component wiring across
  composition.

## Build-time example (`Src/user_main.cpp`)

```cpp
// Reusable sub-group with two instances and internal jump
namespace GpioDSequence
{
    struct Group;
    struct Pulse;
    struct Instance1;
    struct Instance2;

    constexpr auto pulseTemplate = make_lli_group(
        make_list<Group, Pulse>(
            make_lli(gpioNodes::GPIO_D_HI_NODE, gpioNodes::GPIO_D_LO_NODE)
        )
    );

    constexpr auto group = make_lli_group(
        instantiate<Instance1>(pulseTemplate),
        instantiate<Instance2>(pulseTemplate),
        jump<
            list_tail<instanced_tag<Instance1, Group>, Pulse>,
            list_entry<instanced_tag<Instance2, Group>, Pulse>
        >{}
    );

    using Entry = list_entry<instanced_tag<Instance1, Group>, Pulse>;
    using Exit  = list_tail<instanced_tag<Instance2, Group>, Pulse>;
}

// SPI + GPIO dispatch group with internal loop transitions
namespace SpiGpioSequence
{
    struct Group;
    struct Initial;
    struct LoopSetup;
    struct Dispatch;
    struct Finish;

    using Entry = list_entry<Group, Initial>;
    using DispatchSelector = list_entry<Group, Dispatch>;
    using DispatchExecutor = list_tail<Group, Dispatch>;

    constexpr auto group = make_lli_group(
        make_list<Group, Initial>(...),
        make_list<Group, LoopSetup>(...),
        make_list<Group, Dispatch>(DESC_SEL_EXE),
        make_list<Group, Finish>(...),
        jump<list_tail<Group, Initial>, DispatchSelector>{},
        jump<list_tail<Group, Finish>, list_entry<Group, LoopSetup>>{}
    );
}

// Finalized ROOT
constexpr auto SPI_GPIO_ROOT = finalize_lli_group(
    append_lli_group(
        GpioDSequence::group,
        SpiGpioSequence::group,
        jump<GpioDSequence::Exit, SpiGpioSequence::Entry>{}
    )
);

static_assert(validate_root(SPI_GPIO_ROOT));
static_assert(is_finalized_lli_group_v<decltype(SPI_GPIO_ROOT)>);
```

## beta.3 validation status

- **Compile tests (`ctest --test-dir build/Debug`)**: **19/19 passed**,
  covering positive tag resolution, subgroup composition, automatic same-GroupTag
  linking, explicit `end_list()`, multiple group instances, nested `instantiate()`,
  declarative wiring composition and override order, `finalize_lli_group()`
  lifecycle separation, and negative tests (`FINALIZED_INSTANTIATE_REJECTED`,
  `FINALIZED_APPEND_REJECTED`, `FINALIZED_WIRING_REJECTED`,
  `INVALID_JUMP_WIRING`, `INVALID_GROUP_WIRING`, `DUPLICATE_HEAD`,
  `DUPLICATE_TAIL`).
- **Optimization & LTO Builds**: Verified across `Debug`, `Os-LTO`, and `O3`
  (following the full 11-preset verification), confirming `SRAM2` = 280 B,
  zero `LOAD segment with RWX permissions` warnings, and **no `_GLOBAL__sub_I_*`
  dynamic initializers** via `arm-none-eabi-nm`.
- **Hardware validation (STM32U575)**:
  - Verified full execution of two GPIOD toggles followed by continuous SPI
    descriptor-dispatch transfers, including cold boot from Flash.

## Hardware & runtime integration notes

- **SRAM2 execution image & startup copy**: The writable execution ROOT
  (`SPI_GPIO_EXECUTION_ROOT`) is placed in `.SRAM2` (`> SRAM2 AT > ROM`), and
  `Reset_Handler` copies `__sram2_load__` to `__sram2_start__ .. __sram2_end__`
  immediately after `SystemInit()` and before `.data`/`.bss` initialization.
- **ROM descriptor ring**: `descList` is declared `constinit const` in `.rodata`
  (Flash), linked via static ELF relocations without runtime initialization.
- **Null LLI0 before `load_lli()`**: `CCR.RESET` does not clear `CBR1`, `CSAR`,
  or `CDAR`. Set `CBR1 = 0` explicitly before `load_lli()` and enabling `CCR.EN`.
- **Hardware setup in `Src/user_main.cpp`**: STM32U575 (80 MHz, HSI16 for SPI1),
  GPDMA1 Channel 0, SPI1 unidirectional TX (`PA5` SCK, `PA7` MOSI), `PC6`
  control output, `PD11` GPIOD toggle output, `LPTIM1` 1 kHz trigger.
- **Hardware-specific limitation**: SPI1 disable uses an observed undocumented
  SPI RX DMA request near TXC after enabling unidirectional TX; revalidate if
  MCU, silicon revision, SPI configuration, or clock setup changes.
- **Linker warning resolved**: `STM32U575xx_FLASH.ld` SRAM regions were updated
  from `(xrw)` to `(rw)` and `.gpdma_relocation_probe` was marked `(READONLY)`,
  eliminating `LOAD segment with RWX permissions` (ROM segment flags are `R E`).

## Current limitations & remaining work before RC

- Descriptor API redesign (beta.4 scope): generalize `descriptor_dispatch_layout`
  and `ExecutorImage` support beyond `DESC_12BSD_t` (including optional `CLLR`
  for table-driven branching), derive selector registers without manual
  `DESC_SELECTOR_NODE` constants, and finalize static/dynamic descriptor
  ownership boundaries.
- Decide whether `Src/gpdma_static_relocation_probe.cpp` is included in the
  final release or separated as validation-only code.
- Update `CMakePresets.json` `Release` preset optimization flags.
- Synchronize final documentation and freeze public APIs at `v2.0.0`.

## Logic capture

The beta.3 hardware verification captures (including the initial `GpioDSequence` toggles followed by the continuous SPI descriptor-dispatch loop) are recorded in `capture/`:

- `capture/v2_0_0_beta_3_capture.sal`: Original Saleae Logic 2 capture.
- `capture/v2_0_0_beta_3_capture.csv`: Exported SPI analyzer data.
- `capture/v2_0_0_beta_3_capture_1.png`, `capture/v2_0_0_beta_3_capture_2.png`: Logic 2 waveform screenshots.

The alpha.1 reference captures (`capture/v2_0_0_alpha_capture.*`) are also retained in `capture/`.

## Directory structure & files

```text
stm32u575_spi_descriptor_dispatch/
├── capture/                          # Logic analyzer captures and waveforms
│   ├── v2_0_0_beta_3_capture.sal     # Saleae Logic 2 raw session (beta.3)
│   ├── v2_0_0_beta_3_capture.csv     # Exported SPI decoded data (beta.3)
│   ├── v2_0_0_beta_3_capture_1.png   # Waveform screenshot 1 (overview / startup pulse)
│   ├── v2_0_0_beta_3_capture_2.png   # Waveform screenshot 2 (continuous SPI dispatch loop)
│   ├── v2_0_0_alpha_capture.sal      # Saleae Logic 2 raw session (alpha.1 reference)
│   ├── v2_0_0_alpha_capture.csv      # Exported SPI decoded data (alpha.1 reference)
│   └── v2_0_0_alpha_capture.png      # Waveform screenshot (alpha.1 reference)
├── example/                          # Linker script and startup for STM32U575
│   ├── startup_stm32u575xx.s         # Startup code with SRAM2 image copy in Reset_Handler
│   └── STM32U575xx_FLASH.ld          # Linker script with .SRAM2 execution section definition
├── include/                          # Header-only GPDMA LLI library headers
│   ├── gpdma_lli.hpp                 # Core LLI builder (groups, lists, declarative wiring, finalized_lli_group)
│   ├── gpdma_lli_t.hpp               # Low-level hardware register structures and LLI descriptors
│   └── stm32u575_spi_t.hpp           # Hardware peripheral register definitions for SPI1
├── original_init.hpp                 # Peripheral initialization code (GPIO, SPI, LPTIM, GPDMA)
├── gpdma_lli_v2_design_decisions.md  # Detailed design rationale and release boundary records
├── gpdma_lli_v2_development_status.md# Frozen beta.2 development snapshot
├── README.md                         # Latest release record (v2.0.0-beta.3)
├── README_v2.0.0-beta.2.md           # Preserved snapshot for v2.0.0-beta.2
├── README_v2.0.0-beta.1.md           # Preserved snapshot for v2.0.0-beta.1
├── README_v2.0.0-alpha.1.md          # Preserved snapshot for v2.0.0-alpha.1
├── usage-guide.md                    # API usage guide (updated for v2.0.0-beta.3)
├── v2_0_0_beta_3_user_main.cpp       # Latest implementation example (v2.0.0-beta.3)
├── v2_0_0_beta_2_user_main.cpp       # Previous milestone implementation (beta.2)
├── v2_0_0_beta_1_user_main.cpp       # Previous milestone implementation (beta.1)
└── v2_0_0_alpha_1_user_main.cpp      # Initial milestone implementation (alpha.1)
```

## Related documentation

- [usage-guide.md](usage-guide.md): API usage guide updated for v2.0.0-beta.3.
- [README_v2.0.0-beta.2.md](README_v2.0.0-beta.2.md): Preserved release snapshot for v2.0.0-beta.2.
- [README_v2.0.0-beta.1.md](README_v2.0.0-beta.1.md): Preserved release snapshot for v2.0.0-beta.1.
- [README_v2.0.0-alpha.1.md](README_v2.0.0-alpha.1.md): Preserved release snapshot for v2.0.0-alpha.1.
- [gpdma_lli_v2_design_decisions.md](gpdma_lli_v2_design_decisions.md): Detailed design rationale and release boundary records.
- [gpdma_lli_v2_development_status.md](gpdma_lli_v2_development_status.md): Frozen beta.2 development snapshot.

---

# 日本語

## 概要

このREADMEは、`v2.0.0-beta.3` の公開状態、対応動作、検証結果、既知の制限、
およびRCまでの残作業に関する公開情報の正本です。APIの使用方法は使用ガイド、
詳細な設計根拠は設計判断文書を参照してください（`gpdma_lli_v2_development_status.md`
はbeta.2時点で凍結したスナップショットです）。

beta.3では、**ROOT完成境界（`finalize_lli_group()` / `finalized_lli_group`）** の導入、
**`lli_program` / `make_lli_program()` の廃止と `Group`（`GroupTag`）への統一**、および
**構築時宣言配線（`jump<From, To>{}` / `end_node<Position>{}`）への一本化** を行いました。

CPUがチャネルレジスタと開始位置を設定して `GPDMA1_Channel0` を有効化した後は、
`GpioDSequence`（`Instance1` → `Instance2`）を実行した後、CPUの介在なしでSPIの
descriptor-dispatchループを繰り返します。

1. ROM上のdescriptor ringからdescriptorを1件読み込む。
2. selectorを次のdescriptorへ進める。
3. 選択されたSPI転送内容でexecutorノードを書き換える。
4. SPI転送を実行する。
5. 転送完了後にSPIを無効化する。
6. GPIO制御信号を更新する。
7. selectorへ戻り、次のdescriptorを処理する。

## beta.3での変更点

### 1. ROOT完成境界（`finalize_lli_group()`）

- 構築・配線段階（`lli_group`）と確定済み段階（`finalized_lli_group`）を型で分離しました。
- `finalize_lli_group()` **前**のみ許可：
  `make_lli_group()`、`instantiate()`、`append_lli_group()`。
- `finalize_lli_group()` **後**に許可：
  `validate_root()`、タグ検索・ノード位置判定、`descriptor_dispatch_layout`、`load_lli()`。
- finalize済みROOTを `make_lli_group()`、`append_lli_group()`、`instantiate()` に渡すとコンパイル時に拒否されます。
- 実行時サイズとメモリ配置は `lli_group` と完全一致します（`sizeof` および `offsetof(root) == 0` を `static_assert` で検証）。

### 2. `Program` コンテナの廃止と `GroupTag` への統一

- 中間コンテナだった `lli_program` と `make_lli_program()` を廃止し、再利用部品（サブGroup）もROOT候補もすべて `make_lli_group()` で一貫して構築できるようにしました（`instantiate<InstanceTag>()` も直接新しい `lli_group` を返します）。
- これに合わせて `ProgramTag` を **`GroupTag`**（`default_program_tag` → **`default_group_tag`**）へ改称し、`Group` → `List` → `Node` の3階層とタグ体系を一致させました。

### 3. 宣言的配線（`jump<From, To>{}` / `end_node<Position>{}`）への一本化

- 従来の `patch_jump()` / `patch_end()` を廃止し、`make_lli_group()` / `append_lli_group()` へ渡す宣言的ルールへ一本化しました。
- 配線ルールは `lli_group` の型パラメータとして保持されるため実行時メモリサイズを増やさず、`finalize_lli_group()` 時に型から外れます。
- `instantiate<InstanceTag>()` 実行時、内部配線ルールの `GroupTag` も自動的に `instanced_tag<InstanceTag, GroupTag>` へ変換されます。
- `make_lli_group()` / `append_lli_group()` は `fixup_group()` → `connect_group_lists()` の後に、**内側部品のルール → 外側のルール**の順で一括適用するため、部品単位の内部配線を保ったまま再結合できます。

## beta.3検証状況

- **コンパイル試験（`ctest --test-dir build/Debug`）**: **19/19 件パス**
  - 正常系（タグ一意性、サブGroup合成、同一 `GroupTag` 自動接続、`end_list()`、複数インスタンス化、宣言配線の合成・上書き・階層的 `instantiate`、`finalize_lli_group()` ライフサイクル分離）
  - 拒否系（`FINALIZED_INSTANTIATE_REJECTED`、`FINALIZED_APPEND_REJECTED`、`FINALIZED_WIRING_REJECTED`、`INVALID_JUMP_WIRING`、`INVALID_GROUP_WIRING`、`DUPLICATE_HEAD`、`DUPLICATE_TAIL`）
- **最適化・LTO・ELF検査**:
  - `SRAM2` = 280 B、`LOAD segment with RWX permissions` 警告ゼロ、`arm-none-eabi-nm` により **`_GLOBAL__sub_I_*`（動的初期化子）が存在しない** ことを確認済み。
- **実機確認（STM32U575）**:
  - GPIOD 2回トグル後のSPI転送ループ連続動作および電源断からのコールドブート成功を確認。

## ハードウェア・実行時統合の前提と制限

- **SRAM2実行イメージと起動時コピー**: 書き換え可能ROOT（`SPI_GPIO_EXECUTION_ROOT`）は `.SRAM2`（`> SRAM2 AT > ROM`）へ配置し、`Reset_Handler` が `SystemInit()` 直後・`.data`/`.bss` 初期化前に `__sram2_load__` から `__sram2_start__ .. __sram2_end__` へコピーします。
- **ROM配置の descriptor ring**: `descList` は `constinit const` として `.rodata`（Flash）に配置し、実行時初期化なしにELFリロケーションでアドレス解決しています。
- **`load_lli()` 前の null LLI0**: `CCR.RESET` は `CBR1`、`CSAR`、`CDAR` をゼロクリアしないため、`load_lli()` と `CCR.EN` の前に `CBR1 = 0` を明示する必要があります。
- **実機確認時の構成（`Src/user_main.cpp`）**: STM32U575（80 MHz、SPI1クロック HSI16）、GPDMA1 Channel 0、SPI1単方向送信（`PA5` SCK、`PA7` MOSI）、`PC6` 制御出力、`PD11` GPIODトグル出力、`LPTIM1` 1 kHzトリガー。
- **ハードウェア固有の制限**: SPI無効化トリガーには、単方向送信有効化直後のTXC付近で観測された仕様書未記載のSPI RX DMA要求を利用しているため、MCU・リビジョン・SPI設定・クロック変更時は再検証が必要です。
- **リンカ警告の解消**: `STM32U575xx_FLASH.ld` の SRAM 領域属性を `(xrw)` から `(rw)` に変更し、`.gpdma_relocation_probe` に `(READONLY)` を付けたことで、`LOAD segment with RWX permissions` 警告を解消しました（ROM セグメント属性は `R E`）。

## 現在の既知の制限・RCまでの残作業

- **descriptor API 再設計**: `DESC_12BSD_t` 以外の `DESC_*` / `LLI_*`（`CLLR` 込みでの動的分岐を含む）への汎用化、selector レジスタ設定の自動導出、静的/動的 descriptor 所有モデルの確定。
- `Src/gpdma_static_relocation_probe.cpp` の最終リリース収録範囲の確定。
- `CMakePresets.json` の `Release` プリセットへの最適化フラグ明示。
- 最終文書の同期と `v2.0.0` での公開API凍結。

## Logic capture

`v2.0.0-beta.3` の実機動作確認（`GpioDSequence` のトグル出力に続く SPI descriptor-dispatch ループ連続動作）のロジックアナライザ記録を `capture/` に追加しています。

- `capture/v2_0_0_beta_3_capture.sal`: Saleae Logic 2 の元キャプチャファイル
- `capture/v2_0_0_beta_3_capture.csv`: SPI Analyzer のエクスポート結果
- `capture/v2_0_0_beta_3_capture_1.png`、`capture/v2_0_0_beta_3_capture_2.png`: Logic 2 の波形スクリーンショット

なお、alpha.1 時の参照キャプチャ（`capture/v2_0_0_alpha_capture.*`）も `capture/` に引き続き保存しています。

## ディレクトリ構成とファイル一覧

```text
stm32u575_spi_descriptor_dispatch/
├── capture/                          # ロジックアナライザ波形・キャプチャデータ
│   ├── v2_0_0_beta_3_capture.sal     # Saleae Logic 2 キャプチャデータ（beta.3）
│   ├── v2_0_0_beta_3_capture.csv     # SPI デコードデータ（beta.3）
│   ├── v2_0_0_beta_3_capture_1.png   # 波形スクリーンショット1（全体・GPIODパルス〜開始）
│   ├── v2_0_0_beta_3_capture_2.png   # 波形スクリーンショット2（SPI descriptor-dispatch ループ連続波形）
│   ├── v2_0_0_alpha_capture.sal      # Saleae Logic 2 キャプチャデータ（alpha.1 参照用）
│   ├── v2_0_0_alpha_capture.csv      # SPI デコードデータ（alpha.1 参照用）
│   └── v2_0_0_alpha_capture.png      # 波形スクリーンショット（alpha.1 参照用）
├── example/                          # STM32U575 向けスタートアップおよびリンカ設定
│   ├── startup_stm32u575xx.s         # Reset_Handler 内で SRAM2 実行イメージをコピーするスタートアップコード
│   └── STM32U575xx_FLASH.ld          # .SRAM2 実行領域を定義したリンカスクリプト
├── include/                          # GPDMA LLI ライブラリ本体（ヘッダオンリー）
│   ├── gpdma_lli.hpp                 # コアビルダー（Group/List、宣言的配線、finalized_lli_group 等）
│   ├── gpdma_lli_t.hpp               # レジスタ構造体・各種 LLI ノードディスクリプタ定義
│   └── stm32u575_spi_t.hpp           # SPI1 ペリフェラルレジスタ定義
├── original_init.hpp                 # ペリフェラル初期化コード（GPIO, SPI, LPTIM, GPDMA）
├── gpdma_lli_v2_design_decisions.md  # 設計根拠とリリース境界の記録
├── gpdma_lli_v2_development_status.md# beta.2 時点の凍結開発スナップショット
├── README.md                         # 最新リリース記録（v2.0.0-beta.3）
├── README_v2.0.0-beta.2.md           # v2.0.0-beta.2 時点の保存スナップショット
├── README_v2.0.0-beta.1.md           # v2.0.0-beta.1 時点の保存スナップショット
├── README_v2.0.0-alpha.1.md          # v2.0.0-alpha.1 時点の保存スナップショット
├── usage-guide.md                    # 利用手順・API解説ガイド（v2.0.0-beta.3 仕様へ更新済み）
├── v2_0_0_beta_3_user_main.cpp       # 最新実装コード例（v2.0.0-beta.3）
├── v2_0_0_beta_2_user_main.cpp       # 過去マイルストーン実装（beta.2）
├── v2_0_0_beta_1_user_main.cpp       # 過去マイルストーン実装（beta.1）
└── v2_0_0_alpha_1_user_main.cpp      # 初期マイルストーン実装（alpha.1）
```

## 関連文書

- [usage-guide.md](usage-guide.md): `v2.0.0-beta.3` 仕様へ更新済みのAPI使用ガイド
- [README_v2.0.0-beta.2.md](README_v2.0.0-beta.2.md): v2.0.0-beta.2 時点の保存スナップショット
- [README_v2.0.0-beta.1.md](README_v2.0.0-beta.1.md): v2.0.0-beta.1 時点の保存スナップショット
- [README_v2.0.0-alpha.1.md](README_v2.0.0-alpha.1.md): v2.0.0-alpha.1 時点の保存スナップショット
- [gpdma_lli_v2_design_decisions.md](gpdma_lli_v2_design_decisions.md): 設計根拠とリリース境界の記録
- [gpdma_lli_v2_development_status.md](gpdma_lli_v2_development_status.md): `beta.2` 時点の凍結スナップショット
