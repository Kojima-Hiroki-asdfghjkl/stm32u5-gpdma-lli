# STM32U575 Static GPDMA Descriptor Dispatch

[English](#english) | [日本語](#日本語)

> **Status: v2.0.0-beta.2**
>
> This README documents the current API, example, validation results, known
> limitations, and remaining work toward the release candidate.

---

# English

## Overview

This example demonstrates a self-modifying STM32U575 GPDMA linked-list
sequence using the v2.0.0-beta.2 library model.

After the CPU prepares the channel register file, loads the named entry point,
and enables GPDMA1 Channel 0, GPDMA repeatedly performs the following sequence
without further CPU descriptor selection:

1. Reads one descriptor from a descriptor ring.
2. Rewrites the selector state for the next descriptor.
3. Rewrites the executor node with the selected SPI transfer.
4. Executes the SPI transfer.
5. Disables SPI after the transfer completes.
6. Updates the GPIO control signal.
7. Returns to the selector and repeats.

The descriptor sequence reproduced during the beta.2 hardware test is unchanged
from alpha.1:

| Descriptor | Source offset | Length | Observed MOSI data |
|---:|---:|---:|---|
| 0 | `DUMMY + 10` | 10 bytes | `10-19` |
| 1 | `DUMMY + 0` | 10 bytes | `0-9` |
| 2 | `DUMMY + 40` | 10 bytes | `40-49` |
| 3 | `DUMMY + 12` | 10 bytes | `12-21` |

After descriptor 3, the ring returns to descriptor 0.

`DUMMY` is the v2 example's name for the sequential 256-byte test data
corresponding to `TEST_DATA` in the alpha.1 experiment.

## What changed from alpha.1

The DMA behavior and observed SPI sequence are the same as alpha.1. beta.1
changed how the graph is described and initialized; beta.2 extends that API
without changing the transfer sequence.

### Introduced in beta.1 and extended in beta.2

- Named logical positions using:

  ```cpp
  node_at<ProgramTag, ListTag, NodeIndex>
  ```

- List boundary positions using:

  ```cpp
  list_entry<ProgramTag, ListTag>
  list_tail<ProgramTag, ListTag>
  ```

- Compile-time Program/List tags with no runtime storage cost.
- Automatic linking between adjacent Lists that have the same Program tag.
- No automatic link across a Program boundary.
- Explicit link override through `patch_jump<From, To>()`.
- Explicit termination through `patch_end<Position>()`.
- Templated descriptor steps through `descriptor_step<ExecutorImage>`.
- Automatic derivation of selector/executor offsets and the rewrite window.
- Typed initial descriptor selection through `start_at<N>`.
- Static descriptor-ring construction with `constinit`.
- Static selector `CSAR` and `CDAR` initialization through ELF relocation.
- Startup loading of the completed `.SRAM2` image from Flash.

### Removed from the normal runtime path

- Runtime queue construction.
- Runtime node linking.
- Runtime `connectSelectorAddresses()`.
- The provisional `constructor(101)` SRAM2 copy routine.

## Design objective

The v2 design targets:

- strict structural type safety;
- fully static construction of GPDMA node graphs;
- named Program/List/Node positions instead of physical chain numbers;
- no runtime queue construction;
- no runtime node linking;
- runtime execution limited to channel preparation, entry loading, and DMA
  enablement.

Copying the completed `.SRAM2` load image from Flash to its SRAM2 execution
address is treated as image loading, not runtime graph construction.

## Named Program/List/Node model

Program and List names are compile-time type tags. They do not add fields to the
ROOT image.

```cpp
namespace SpiGpioProgram
{
    struct Program;
    struct InitialDataLength;
    struct Dispatch;

    using Entry = list_entry<Program, InitialDataLength>;
    using DispatchSelector = list_entry<Program, Dispatch>;
    using DispatchExecutor = list_tail<Program, Dispatch>;
}
```

The library resolves each logical position to:

- the physical List index;
- the Node index within the List;
- the ROOT-relative byte offset;
- the target LLI type;
- the required CLLR update flags.

This prevents physical List insertion or reordering from renumbering every
public jump target.

## Automatic List linking

Nodes inside each List are linked in declaration order by `make_lli()`.

Adjacent Lists are linked automatically only when both Lists use the same
Program tag.

```text
Program A / List 0 tail -> Program A / List 1 entry
Program A / List 1 tail -> Program A / List 2 entry
Program A / final List tail -> 0

Program A / final List tail -X-> Program B / first List entry
```

Program boundaries are therefore terminal by default. Cross-Program execution
must be requested explicitly:

```cpp
patch_jump<ProgramA::Exit, ProgramB::Entry>(root);
```

The current SPI/GPIO graph requires only two explicit overrides:

1. The initial SPI-start List jumps directly to the dispatcher.
2. The final GPIO-low List jumps back to the normal loop entry.

All other same-Program List transitions are generated automatically.

## Descriptor structure

The selector transfers exactly 32 bytes from the active descriptor into the
SRAM2 ROOT image:

| Descriptor field | Destination |
|---|---|
| next selector source | `selector.CSAR` |
| next selector destination | `selector.CDAR` |
| next selector link | `selector.CLLR` |
| `executor.CTR1` | `executor.CTR1` |
| `executor.CTR2` | `executor.CTR2` |
| `executor.CBR1` | `executor.CBR1` |
| `executor.CSAR` | `executor.CSAR` |
| `executor.CDAR` | `executor.CDAR` |

`executor.CLLR` remains outside the rewrite window. The executor therefore
retains its statically generated link to the following List.

For v2.0.0-beta.2, hardware validation is limited to:

```cpp
DESC_12BSD_t
```

The descriptor implementation is already parameterized by executor-image type,
but additional partial and 2D images are outside the v2.0.0 scope.

## Automatic dispatcher layout

`descriptor_dispatch_layout` derives the following information from the ROOT
type and the named selector/executor positions:

- selector ROOT offset;
- executor ROOT offset;
- rewrite-window beginning;
- rewrite-window end and size;
- selector-to-executor CLLR;
- descriptor-step type.

The application no longer defines physical constants such as:

```text
DESCRIPTOR_CHAIN_OFFSET
SELECTOR_OFFSET
EXECUTOR_OFFSET
DESCRIPTOR_PATCH_BEGIN
DESCRIPTOR_PATCH_END
SELECTOR_TO_EXECUTOR_CLLR
```

The implementation verifies that the derived rewrite-window size exactly
matches `sizeof(descriptor_step<DESC_12BSD_t>)`.

## Static descriptor ring

The descriptor ring is a `constinit` object containing descriptor-to-descriptor
references and references to the SRAM execution ROOT.

```cpp
constinit descriptor_step_t descList[4] = {
    descriptor_dispatch_t::make_step(
        &descList[1], SPI_GPIO_ROOT, test_spi_dat1),
    descriptor_dispatch_t::make_step(
        &descList[2], SPI_GPIO_ROOT, test_spi_dat2),
    descriptor_dispatch_t::make_step(
        &descList[3], SPI_GPIO_ROOT, test_spi_dat3),
    descriptor_dispatch_t::make_step(
        &descList[0], SPI_GPIO_ROOT, test_spi_dat4)
};
```

The initial descriptor is selected with `start_at<N>`.

```cpp
start_at<0>{}
```

Changing `N` selects another descriptor as the initial ring entry without a
runtime selector patch.

## Static selector connection

alpha.1 connected selector `CSAR` and `CDAR` with two startup assignments.
beta.1 completes those fields as part of static initialization.

```cpp
[[gnu::used]]
[[gnu::section(".SRAM2")]]
constinit root_t SPI_GPIO_ROOT = [] {
    auto root = SPI_GPIO_ROOT_TEMPLATE;

    descriptor_dispatch_t::initialize_selector(
        root,
        SPI_GPIO_ROOT,
        descList,
        start_at<0>{}
    );

    return root;
}();
```

The compiler emits statically initialized data and relocation records. The
linker resolves the final addresses after placing the descriptor ring and SRAM2
execution ROOT.

No descriptor or selector connection function is called from `userMain()`.

## SRAM2 execution image

The beta.2 hardware-test application places the writable ROOT in an initialized
`.SRAM2` output section:

```ld
.SRAM2 :
{
    . = ALIGN(8);
    __sram2_start__ = .;

    KEEP(*(.SRAM2))
    KEEP(*(.SRAM2.*))

    . = ALIGN(8);
    __sram2_end__ = .;
} > SRAM2 AT > ROM

__sram2_load__ = LOADADDR(.SRAM2);
```

`AT > ROM` gives the SRAM2 execution image a Flash load image. The application
Reset Handler copies the range beginning at `__sram2_load__` to
`__sram2_start__ .. __sram2_end__` immediately after `SystemInit()` and before
the ordinary C/C++ runtime initialization:

```text
SystemInit()
    -> copy the completed .SRAM2 image from Flash to SRAM2
    -> initialize .data/.tdata
    -> zero .bss/.tbss
    -> __libc_init_array()
    -> main()
```

This linker and startup arrangement is an application-side integration
condition used for the beta.2 hardware test, not a requirement to adopt the
test application's complete linker script. Integrate the initialized SRAM2
section and its startup copy into the target application's existing memory
layout.

## Runtime responsibilities

`load_lli()` only configures the entry link:

- `CLBAR`
- `CLLR`

The caller remains responsible for:

- stopping or resetting the channel;
- preparing the internal channel register file used as LLI0;
- setting `CBR1.BNDT = 0` when a null LLI0 is required;
- configuring CCR options;
- enabling the channel.

The current example uses:

```cpp
GPDMA1_Channel0->CCR |= 0x0000'0002u; // RESET
GPDMA1_Channel0->CBR1 = 0;             // BNDT = 0, null LLI0

load_lli<SpiGpioProgram::Entry>(
    *GPDMA1_Channel0,
    SPI_GPIO_ROOT
);

GPDMA1_Channel0->CCR |= 0x0000'0001u; // EN
```

`CCR.RESET` resets the FIFO, internal state, `EN`, and `SUSP`. It does not clear
`CBR1`, `CSAR`, or `CDAR` to zero.

## Same observed behavior as alpha.1

beta.2 was tested on the STM32U575 hardware and produced the same descriptor
order, MOSI data, and GPIO behavior as alpha.1.

The following remain unchanged:

- GPDMA1 Channel 0;
- SPI1 unidirectional transmit mode;
- four-descriptor ring order;
- 10-byte transfer length per descriptor;
- source offsets `10`, `0`, `40`, and `12`;
- GPIO control waveform;
- executor rewrite behavior;
- descriptor 3 to descriptor 0 wrap;
- CPU-free descriptor selection after DMA startup.

The capture files preserved here were recorded for alpha.1. They document the
expected externally observable sequence, which was reproduced during the beta.2
hardware test. They are not newly recorded beta.2 measurements.

## Verified platform configuration

The reference capture was produced with:

- MCU: STM32U575;
- GPDMA: GPDMA1 Channel 0;
- peripheral: SPI1 in unidirectional transmit mode;
- SPI1 kernel clock: HSI16;
- system clock: 80 MHz;
- SPI1 SCK: PA5;
- SPI1 MOSI: PA7;
- sequence-control output: PC6;
- ROOT execution image: SRAM2;
- initial ROOT image: loaded from Flash during reset startup.

`original_init.hpp` preserves the initialization code from the original test
application. It includes initialization unrelated to this experiment and is not
a minimal reusable platform layer.

## Source snapshot

The beta.2 release snapshot contains:

- `include/gpdma_lli.hpp`
- `include/gpdma_lli_t.hpp`
- `include/stm32u575_spi_t.hpp`
- `v2_0_0_alpha_1_user_main.cpp`
- `v2_0_0_beta_1_user_main.cpp`
- `original_init.hpp`
- `example/startup_stm32u575xx.s`
- `example/STM32U575xx_FLASH.ld`

The versioned `user_main` files preserve the application code used for each
development stage. The beta.2 hardware test uses
`v2_0_0_beta_2_user_main.cpp`.

The files in `example/` show the startup and linker integration required to
load the initialized `.SRAM2` execution image from Flash. They are integration
references rather than a complete standalone example.

The SPI-specific code is an experimental application and is not part of the
generic GPDMA LLI library API.

## Logic capture

The files in `capture/` are the alpha.1 measurements described above. They are
retained as the reference record for the sequence reproduced by beta.2.

### Files

- `capture/v2_0_0_alpha_capture.sal`  
  Original Saleae Logic 2 capture.

- `capture/v2_0_0_alpha_capture.csv`  
  Exported SPI analyzer data.

- `capture/v2_0_0_alpha_capture.png`  
  Representative Logic 2 screenshot.

### Signals

| Logic 2 row | Signal | Purpose |
|---|---|---|
| D1 / RB2 | SPI clock | SPI1 SCK |
| D0 / RB3 | SPI MOSI | SPI1 transmitted data |
| D3 / PC6 | Control output | Active-low transfer window |

### Expected and hardware-reproduced MOSI sequence

```text
10 11 12 13 14 15 16 17 18 19
0  1  2  3  4  5  6  7  8  9
40 41 42 43 44 45 46 47 48 49
12 13 14 15 16 17 18 19 20 21
10 11 12 ...
```

The alpha.1 capture confirms:

- each executor transfer sends 10 bytes;
- descriptor order is preserved;
- descriptor 3 links back to descriptor 0;
- the selector advances the descriptor ring;
- the executor image is rewritten successfully;
- descriptor selection continues without CPU intervention.

## Important hardware-specific limitation

This experiment uses an observed SPI RX DMA request near TXC as the completion
request for the SPI-disable node.

On the tested STM32U575 configuration, one SPI RX DMA request was observed near
the completion of the first unidirectional transmission after enabling SPI.
This behavior is not documented or guaranteed by the reference manual or data
sheet.

Revalidate when changing:

- MCU or silicon revision;
- SPI direction or FIFO configuration;
- SPI initialization sequence;
- DMA request configuration;
- clock configuration.

## Current limitations

- The Program/List/Node public API is implemented but not yet frozen for RC.
- The supported descriptor executor image type for v2.0.0 is limited to
  `DESC_12BSD_t`.
- `executor.CLLR` is not part of the descriptor rewrite window.
- Invalid `list_tail` diagnostics may still produce deep template errors.
- The platform initialization is preserved from the original application and
  is not minimal.
- SPI completion depends on undocumented observed hardware behavior.

Program/List tag duplication checks, automatic adjacent-List connection,
explicit termination, and their compile tests are implemented.

## beta.2 validation status

The beta.2 snapshot records the implemented Program/List API, negative compile
tests, optimization/LTO matrix, ELF inspection, static relocation probe,
hardware operation, cold boot, software reset, Program-to-Program transitions,
explicit termination, descriptor-ring traversal, and a 30-minute continuous
run.

The optimization/LTO matrix covers `-O0`, `-Og`, `-O2`, `-O3`, `-Os`,
`-O3 -flto`, and `-Os -flto`. Acceptance requires successful build and static
image generation, correct `.SRAM2` VMA/LMA and relocation, no dynamic
initializer for the ROOT image, and successful real-hardware operation.

## Related documentation

- [`usage-guide.md`](usage-guide.md) is the usage guide. It intentionally
  contains no release progress or remaining-work tracking and refers here for
  the current public release state.
- `gpdma_lli_v2_design_decisions.md` preserves detailed design rationale,
  alternatives, validation criteria, and release-boundary reasoning. This
  README remains authoritative for public release status.
- `gpdma_lli_v2_development_status.md` is frozen as the beta.2 development
  snapshot. Later release-state changes are recorded here rather than by
  updating that snapshot.

## Remaining work before the release candidate

### v2.0.0-beta.3: finalized ROOT boundary

- Introduce `finalize_lli_group()` and give the finalized ROOT a type distinct
  from the mutable construction-stage ROOT.
- Keep validation, named-position lookup, node access,
  `descriptor_dispatch_layout`, and DMA placement available after finalization.
- Reject `append_lli_group()`, `patch_jump()`, and repeated finalization for a
  finalized ROOT at compile time.
- Add small positive and negative compile tests for the finalized ROOT
  lifecycle.
- Confirm that finalization adds no runtime data and causes no size, layout,
  static-initialization, or generated-code regression.

### v2.0.0-beta.4: descriptor API freeze

- Decide whether the descriptor owns the complete executor image, including
  `CLLR`, and document which fields are controlled by the descriptor and ROOT.
- Fix the public descriptor-step representation and its `sizeof`, `alignof`,
  field offsets, and standard-layout requirements with `static_assert`.
- Derive the selector copy start and size from types rather than handwritten
  constants.
- Reject unsupported executor image formats at compile time and remove
  `DESC_12BSD_t` from public template arguments where it can be inferred.
- Confirm that descriptor-ring and ROOT references remain `constinit` and use
  static relocation.

### v2.0.0-rc.1: release qualification

- Freeze the Program/List, finalized ROOT, and descriptor public APIs.
- Finalize failure diagnostics and positive and negative compile tests.
- Decide whether `Src/gpdma_static_relocation_probe.cpp` is included in the
  final release or kept as validation-only code.
- Investigate the Release-link `LOAD segment with RWX permissions` warning and
  decide whether to accept or fix it.
- Repeat the optimization/LTO matrix, representative Release build, ELF and
  relocation inspection, hardware operation, software reset, cold boot, and
  continuous-run tests after the API freeze.
- Update the README and usage guide to match the frozen API.

## Directory contents

```text
stm32u575_spi_descriptor_dispatch/
├── capture/
│   ├── v2_0_0_alpha_capture.csv
│   ├── v2_0_0_alpha_capture.png
│   └── v2_0_0_alpha_capture.sal
├── example/
│   ├── startup_stm32u575xx.s
│   └── STM32U575xx_FLASH.ld
├── include/
│   ├── gpdma_lli_t.hpp
│   ├── gpdma_lli.hpp
│   └── stm32u575_spi_t.hpp
├── original_init.hpp
├── README.md
├── usage-guide.md
├── v2_0_0_alpha_1_user_main.cpp
├── v2_0_0_beta_1_user_main.cpp
└── v2_0_0_beta_2_user_main.cpp
```

This directory is the v2.0.0-beta.2 release snapshot. The versioned
`user_main` files and Git history preserve the earlier development stages.

---

# 日本語

> **ステータス: v2.0.0-beta.2**
>
> このREADMEは、現行API、サンプル、検証結果、既知の制限、および
> リリース候補版までの残作業を記載しています。

## 概要

**DMAとSPIの観測動作はv2.0.0-alpha.1と同一です。** v2で変更したのは
グラフの構築APIと初期化方式であり、descriptor順序、SPI転送内容、GPIO波形、
自己書き換えシーケンスの動作仕様は変更していません。

CPUがチャネルレジスタと開始位置を準備し、GPDMA1 Channel 0を有効化した後は、
GPDMAがCPUによるdescriptor選択なしで次を繰り返します。

1. descriptor ringからdescriptorを1件読み込む。
2. selectorを次のdescriptorへ進める。
3. 選択されたSPI転送内容でexecutorノードを書き換える。
4. SPI転送を実行する。
5. 転送完了後にSPIを無効化する。
6. GPIO制御信号を更新する。
7. selectorへ戻り、次のdescriptorを処理する。

beta.2の実機試験で再現したdescriptor順序は、alpha.1から変更ありません。

| Descriptor | 送信元オフセット | 長さ | 観測したMOSIデータ |
|---:|---:|---:|---|
| 0 | `DUMMY + 10` | 10バイト | `10-19` |
| 1 | `DUMMY + 0` | 10バイト | `0-9` |
| 2 | `DUMMY + 40` | 10バイト | `40-49` |
| 3 | `DUMMY + 12` | 10バイト | `12-21` |

Descriptor 3の実行後はdescriptor 0へ戻ります。

`DUMMY`は、alpha.1の`TEST_DATA`に相当する、v2サンプルでの0から255までの
連続した256バイトのテストデータです。

## alpha.1からの変更点

DMAの実行結果はalpha.1と同じです。beta.1で構築方法をv2設計へ移行し、
beta.2では転送シーケンスを変えずにAPIを拡張しました。

### beta.1で導入し、beta.2で拡張したもの

- 次の形式による名前付き論理位置：

  ```cpp
  node_at<ProgramTag, ListTag, NodeIndex>
  ```

- List境界位置：

  ```cpp
  list_entry<ProgramTag, ListTag>
  list_tail<ProgramTag, ListTag>
  ```

- 実行時領域を消費しないProgram/List型タグ。
- 同一Programに属する隣接Listの自動接続。
- Program境界では自動接続しない安全な既定動作。
- `patch_jump<From, To>()`による接続上書き。
- `patch_end<Position>()`による明示終端。
- `descriptor_step<ExecutorImage>`によるdescriptor stepのテンプレート化。
- selector/executorオフセットと書き換え範囲の自動導出。
- `start_at<N>`による初期descriptor指定。
- `constinit`によるdescriptor ringの静的構築。
- ELFリロケーションによるselector `CSAR`/`CDAR`の静的完成。
- Reset Handlerによる完成済み`.SRAM2`イメージのロード。

### 通常の実行時経路から削除したもの

- 実行時キュー構築。
- 実行時ノード接続。
- 実行時`connectSelectorAddresses()`。
- 暫定的な`constructor(101)`によるSRAM2コピー。

## 設計目標

v2では次を目標とします。

- 構造に対する厳密な型安全性
- GPDMAノードグラフの静的構築
- 物理チェーン番号に依存しない名前付きProgram/List/Node位置
- 実行時キュー構築なし
- 実行時ノード接続なし
- 実行時処理をチャネル準備、開始位置ロード、DMA有効化に限定

完成済み`.SRAM2`ロードイメージをFlashからSRAM2実行アドレスへコピーする処理は、
グラフの実行時構築ではなく、完成済みイメージのロードとして扱います。

## 名前付きProgram/List/Node

Program名とList名には、実行時データを持たない型タグを使用します。型タグは
ROOTイメージへフィールドを追加しません。

```cpp
namespace SpiGpioProgram
{
    struct Program;
    struct InitialDataLength;
    struct Dispatch;

    using Entry = list_entry<Program, InitialDataLength>;
    using DispatchSelector = list_entry<Program, Dispatch>;
    using DispatchExecutor = list_tail<Program, Dispatch>;
}
```

各論理位置から、ライブラリが次を解決します。

- 物理List番号
- List内Node番号
- ROOT先頭からのバイトオフセット
- 対象LLI型
- 必要なCLLR更新フラグ

Listの追加または並べ替えによって、公開側のジャンプ先番号を数え直す必要が
なくなります。

## Listの自動接続

各List内のNodeは、`make_lli()`により宣言順に接続されます。

隣接Listは、同じProgramタグを持つ場合に限って自動接続されます。

```text
Program A / List 0末尾 -> Program A / List 1先頭
Program A / List 1末尾 -> Program A / List 2先頭
Program A / 最終List末尾 -> 0

Program A / 最終List末尾 -X-> Program B / 最初のList先頭
```

Program境界は既定で終端です。Program間遷移は明示的に指定します。

```cpp
patch_jump<ProgramA::Exit, ProgramB::Entry>(root);
```

現在のSPI/GPIOグラフで必要な明示的上書きは2本だけです。

1. 初回SPI開始Listからdispatcherへ直接移動する。
2. 最後のGPIO Low Listから通常ループの開始位置へ戻る。

その他の同一Program内List遷移は自動生成されます。

## descriptor構造

selectorは、現在のdescriptorからSRAM2上のROOTイメージへ正確に32バイトを
転送します。

| Descriptor側 | 書き込み先 |
|---|---|
| 次selector送信元 | `selector.CSAR` |
| 次selector書き込み先 | `selector.CDAR` |
| 次selectorリンク | `selector.CLLR` |
| `executor.CTR1` | `executor.CTR1` |
| `executor.CTR2` | `executor.CTR2` |
| `executor.CBR1` | `executor.CBR1` |
| `executor.CSAR` | `executor.CSAR` |
| `executor.CDAR` | `executor.CDAR` |

`executor.CLLR`は書き換え範囲外です。このためexecutorは、静的に生成された
後続Listへのリンクを保持します。

v2.0.0-beta.2で検証対象とするdescriptor executor image型は次に限定します。

```cpp
DESC_12BSD_t
```

内部のdescriptor実装はexecutor image型でテンプレート化していますが、部分更新型や
2D型はv2.0.0の対象外です。

## dispatcherレイアウトの自動導出

`descriptor_dispatch_layout`は、ROOT型と名前付きselector/executor位置から次を
自動導出します。

- selectorのROOT内オフセット
- executorのROOT内オフセット
- 書き換え範囲の開始位置
- 書き換え範囲の終了位置とサイズ
- selectorからexecutorへのCLLR
- descriptor step型

利用側では、次の物理定数を定義する必要がありません。

```text
DESCRIPTOR_CHAIN_OFFSET
SELECTOR_OFFSET
EXECUTOR_OFFSET
DESCRIPTOR_PATCH_BEGIN
DESCRIPTOR_PATCH_END
SELECTOR_TO_EXECUTOR_CLLR
```

導出された書き換え範囲と`sizeof(descriptor_step<DESC_12BSD_t>)`の一致は、
`static_assert`で検証します。

## descriptor ringの静的構築

descriptor ringは、descriptor間参照とSRAM実行ROOTへの参照を含む`constinit`
オブジェクトとして構築します。

```cpp
constinit descriptor_step_t descList[4] = {
    descriptor_dispatch_t::make_step(
        &descList[1], SPI_GPIO_ROOT, test_spi_dat1),
    descriptor_dispatch_t::make_step(
        &descList[2], SPI_GPIO_ROOT, test_spi_dat2),
    descriptor_dispatch_t::make_step(
        &descList[3], SPI_GPIO_ROOT, test_spi_dat3),
    descriptor_dispatch_t::make_step(
        &descList[0], SPI_GPIO_ROOT, test_spi_dat4)
};
```

初期descriptorは`start_at<N>`で指定します。

```cpp
start_at<0>{}
```

`N`を変更することで、実行時パッチなしにring内の別descriptorを開始位置に
指定できます。

## selectorの静的接続

alpha.1では、selectorの`CSAR`と`CDAR`を起動時の2代入で接続していました。
beta.1ではROOTの静的初期化式に含めます。

```cpp
[[gnu::used]]
[[gnu::section(".SRAM2")]]
constinit root_t SPI_GPIO_ROOT = [] {
    auto root = SPI_GPIO_ROOT_TEMPLATE;

    descriptor_dispatch_t::initialize_selector(
        root,
        SPI_GPIO_ROOT,
        descList,
        start_at<0>{}
    );

    return root;
}();
```

コンパイラは静的初期値とリロケーション情報を生成し、リンカがdescriptor ringと
SRAM2実行ROOTの配置後に最終アドレスを解決します。

`userMain()`からselector接続関数を呼び出す必要はありません。

## SRAM2実行イメージ

beta.2の実機試験アプリケーションでは、書き換え可能ROOTを初期値付きの`.SRAM2`
出力セクションへ配置します。

```ld
.SRAM2 :
{
    . = ALIGN(8);
    __sram2_start__ = .;

    KEEP(*(.SRAM2))
    KEEP(*(.SRAM2.*))

    . = ALIGN(8);
    __sram2_end__ = .;
} > SRAM2 AT > ROM

__sram2_load__ = LOADADDR(.SRAM2);
```

`AT > ROM`により、SRAM2実行イメージにFlash上のロードイメージを持たせます。
アプリケーションのReset Handlerは、`SystemInit()`の直後、通常のC/C++ランタイム
初期化より前に、`__sram2_load__`から`__sram2_start__ .. __sram2_end__`へコピー
します。

```text
SystemInit()
    -> FlashからSRAM2へ完成済み.SRAM2イメージをコピー
    -> .data/.tdataを初期化
    -> .bss/.tbssをゼロクリア
    -> __libc_init_array()
    -> main()
```

このリンカ設定とstartup処理は、beta.2の実機試験で使用したアプリケーション側の
統合条件です。試験アプリケーションのリンカスクリプト全体を採用する必要は
ありません。対象アプリケーションの既存メモリ配置へ、初期値付きSRAM2セクションと
起動時コピーを統合してください。

## 実行時の責務

`load_lli()`は開始リンクだけを設定します。

- `CLBAR`
- `CLLR`

呼び出し側は次を担当します。

- チャネル停止またはRESET
- 内部レジスタファイルLLI0の準備
- null LLI0が必要な場合の`CBR1.BNDT = 0`
- `CCR`設定
- チャネル有効化

現在のサンプルでは次のように開始します。

```cpp
GPDMA1_Channel0->CCR |= 0x0000'0002u; // RESET
GPDMA1_Channel0->CBR1 = 0;             // BNDT = 0, null LLI0

load_lli<SpiGpioProgram::Entry>(
    *GPDMA1_Channel0,
    SPI_GPIO_ROOT
);

GPDMA1_Channel0->CCR |= 0x0000'0001u; // EN
```

`CCR.RESET`はFIFO、内部状態、`EN`、`SUSP`をリセットしますが、`CBR1`、`CSAR`、
`CDAR`をゼロには戻しません。

## alpha.1と同一の観測動作

beta.2をSTM32U575実機で動作確認し、alpha.1と同じdescriptor順序、MOSIデータ、
GPIO動作を再現することを確認しました。

変更していない項目：

- GPDMA1 Channel 0
- SPI1単方向送信モード
- 4要素descriptor ringの順序
- 各descriptorの10バイト転送
- 送信元オフセット`10`、`0`、`40`、`12`
- GPIO制御波形
- executor書き換え動作
- descriptor 3からdescriptor 0への循環
- DMA開始後のCPU非介在descriptor選択

ここに保存するcaptureファイルはalpha.1で取得したものです。これらはbeta.2でも
再現を確認した外部観測シーケンスの参照記録ですが、beta.2で新たに取得した
測定ファイルではありません。

## 実機確認時の構成

参照capture取得時の構成：

- MCU: STM32U575
- GPDMA: GPDMA1 Channel 0
- 周辺機器: SPI1、単方向送信モード
- SPI1カーネルクロック: HSI16
- システムクロック: 80 MHz
- SPI1 SCK: PA5
- SPI1 MOSI: PA7
- シーケンス制御出力: PC6
- ROOT実行領域: SRAM2
- ROOT初期イメージ: Reset HandlerでFlashからロード

`original_init.hpp`には、実機確認に使用した元プロジェクトの初期化コードを保存して
います。GPDMA実験に関係しない初期化も含まれ、最小構成または再利用可能なplatform
layerではありません。

## ソーススナップショット

beta.2リリーススナップショットには次を含めます。

- `include/gpdma_lli.hpp`
- `include/gpdma_lli_t.hpp`
- `include/stm32u575_spi_t.hpp`
- `v2_0_0_alpha_1_user_main.cpp`
- `v2_0_0_beta_1_user_main.cpp`
- `original_init.hpp`
- `example/startup_stm32u575xx.s`
- `example/STM32U575xx_FLASH.ld`

バージョン名を付けた`user_main`ファイルには、各開発段階で使用した
アプリケーションコードを保存しています。beta.2の実機確認には
`v2_0_0_beta_2_user_main.cpp`を使用しています。

`example/`のファイルは、初期化済み`.SRAM2`実行イメージをFlashからロードする
ために必要なstartupおよびlinker統合例です。完全な単独実行サンプルではなく、
対象アプリケーションへ統合する際の参照コードです。

SPI固有コードは実験用アプリケーションであり、汎用GPDMA LLIライブラリAPIには
含みません。

## Logic capture

`capture/`には、前節で説明したalpha.1の測定ファイルを保存しています。
beta.2で再現確認したシーケンスの参照記録として維持します。

### ファイル

- `capture/v2_0_0_alpha_capture.sal`  
  Saleae Logic 2の元capture。

- `capture/v2_0_0_alpha_capture.csv`  
  SPI Analyzerのエクスポート結果。

- `capture/v2_0_0_alpha_capture.png`  
  Logic 2の代表スクリーンショット。

### 信号

| Logic 2上の行 | 信号 | 用途 |
|---|---|---|
| D1 / RB2 | SPI clock | SPI1 SCK |
| D0 / RB3 | SPI MOSI | SPI1送信データ |
| D3 / PC6 | 制御出力 | Active-Lowの転送期間 |

### 実機で再現確認したMOSIシーケンス

```text
10 11 12 13 14 15 16 17 18 19
0  1  2  3  4  5  6  7  8  9
40 41 42 43 44 45 46 47 48 49
12 13 14 15 16 17 18 19 20 21
10 11 12 ...
```

alpha.1のcaptureから次を確認できます。

- 各executor転送が10バイト
- descriptor順序が維持される
- descriptor 3からdescriptor 0へ戻る
- selectorがdescriptor ringを前進させる
- executor像の書き換えが成立する
- CPU介入なしでdescriptor選択が継続する

## ハードウェア固有の重要な制限

この実験では、SPI送信完了後にSPIを無効化する要求として、TXC付近で観測された
SPI RX DMA要求を使用しています。

検証したSTM32U575構成では、SPIを有効化した直後の最初の単方向送信完了時に、
SPI RX DMA要求が1回発生しました。この挙動はRMまたはデータシートに記載された
保証動作ではありません。

次を変更した場合は再検証が必要です。

- MCUまたはシリコンリビジョン
- SPI方向またはFIFO設定
- SPI初期化順序
- DMA request設定
- クロック設定

## 現在の既知の制限

- Program/List/Node公開APIは実装済みですが、RC前のAPI凍結は未完了です。
- v2.0.0で正式に検証対象とするdescriptor executor image型は
  `DESC_12BSD_t`のみです。
- `executor.CLLR`はdescriptor書き換え範囲に含みません。
- 不正な`list_tail`指定では、深いテンプレートエラーになる場合があります。
- platform初期化は元アプリケーション由来で、最小構成ではありません。
- SPI完了処理は仕様書に記載のない実測挙動に依存します。

Program/Listタグ重複検査、Listの意味単位への再編、隣接Listの自動接続、
`end_list()`および`patch_end()`による明示終端は実装済みです。

## beta.2検証状況

beta.2では、Program/List API、負のコンパイル試験、最適化・LTO回帰、
ELF検査、静的リロケーションprobe、実機動作、コールドブート、
ソフトウェアリセット、Program間遷移、明示終端、descriptor ring周回、
30分間の連続動作を確認済みの結果として記録します。

最適化・LTO回帰試験は、`-O0`、`-Og`、`-O2`、`-O3`、`-Os`、
`-O3 -flto`、`-Os -flto`を対象とします。各条件の合格には、ビルドと静的
イメージ生成の成功、`.SRAM2`のVMA/LMAとリロケーションの正しさ、ROOT用の
動的初期化関数が生成されないこと、および実機動作の成功を必要とします。

## 関連文書

- [`usage-guide.md`](usage-guide.md)は使用方法だけを扱います。リリース進捗と
  残作業は記載せず、現在の公開状態についてこのREADMEを参照します。
- `gpdma_lli_v2_design_decisions.md`は、設計根拠、代替案、詳細な検証条件、
  リリース境界の判断を維持します。公開リリース状態の正本はこのREADMEです。
- `gpdma_lli_v2_development_status.md`はbeta.2時点の開発スナップショットとして
  凍結します。以後のリリース状態の変更は、このREADMEへ記録します。

## RC判定までの残作業

### v2.0.0-beta.3: finalized ROOT境界

- `finalize_lli_group()`を導入し、finalize前の構築中ROOTとは異なる型を返す。
- finalize後も検証、名前付き位置検索、ノード参照、
  `descriptor_dispatch_layout`、DMA配置を使用可能にする。
- finalized ROOTに対する`append_lli_group()`、`patch_jump()`、二重finalizeを
  コンパイル時に拒否する。
- finalized ROOTのライフサイクルについて、小さな正常系・拒否系コンパイル試験を
  追加する。
- finalizeによって実行時データが増えず、サイズ、配置、静的初期化、生成コードに
  退行がないことを確認する。

### v2.0.0-beta.4: descriptor API凍結

- descriptorが`CLLR`を含む完全なexecutor像を所有するかを決定し、
  descriptorとROOTが決定するフィールドを明文化する。
- descriptor stepの公開表現を確定し、`sizeof`、`alignof`、フィールドオフセット、
  standard-layout要件を`static_assert`で固定する。
- selectorのコピー開始位置とサイズを、手書き定数ではなく型から導出する。
- 対応外executor image形式をコンパイル時に拒否し、推論可能な箇所では
  `DESC_12BSD_t`を公開テンプレート引数から除去する。
- descriptor ringとROOTの参照が`constinit`および静的リロケーションのまま
  成立することを確認する。

### v2.0.0-rc.1: リリース判定

- Program/List、finalized ROOT、descriptorの公開APIを凍結する。
- 公開APIの失敗時診断と正常系・拒否系コンパイル試験を最終確認する。
- 静的リロケーションprobeを最終リリースへ含めるか、検証専用に分離するかを
  確定する。
- Releaseリンク時の`LOAD segment with RWX permissions`警告を判断する。
- API凍結後に最適化・LTOマトリクス、代表的なReleaseビルド、ELF・
  リロケーション検査、実機動作、ソフトウェアリセット、コールドブート、
  連続動作試験を再実行する。
- READMEと使用ガイドを凍結後のAPIへ更新する。

最適化・LTO回帰試験は、`-O0`、`-Og`、`-O2`、`-O3`、`-Os`、
`-O3 -flto`、`-Os -flto`で実施済みである。各条件でビルド、静的初期化、
実機動作に加え、`.SRAM2`のVMA/LMA、リロケーション、およびROOT用の
動的初期化関数が生成されないことを確認した。電源断後のコールドブートと
30分間の連続動作も確認済みである。

## ディレクトリ構成

```text
stm32u575_spi_descriptor_dispatch/
├── capture/
│   ├── v2_0_0_alpha_capture.csv
│   ├── v2_0_0_alpha_capture.png
│   └── v2_0_0_alpha_capture.sal
├── example/
│   ├── startup_stm32u575xx.s
│   └── STM32U575xx_FLASH.ld
├── include/
│   ├── gpdma_lli_t.hpp
│   ├── gpdma_lli.hpp
│   └── stm32u575_spi_t.hpp
├── original_init.hpp
├── README.md
├── usage-guide.md
├── v2_0_0_alpha_1_user_main.cpp
├── v2_0_0_beta_1_user_main.cpp
└── v2_0_0_beta_2_user_main.cpp
```

このディレクトリはv2.0.0-beta.2のリリーススナップショットです。
バージョン名を付けた`user_main`ファイルとGit履歴から、以前の開発段階も
参照できます。
