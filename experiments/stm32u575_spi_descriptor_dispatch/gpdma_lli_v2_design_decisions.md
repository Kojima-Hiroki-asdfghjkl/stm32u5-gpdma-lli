# GPDMA LLI Library v2.x Design Decisions

## 1. Purpose

This document preserves the detailed rationale, alternatives, validation
criteria, and release-boundary decisions for the STM32U575 GPDMA linked-list
library. `README_v2.0.0-beta.3.md` is the canonical source for current public
release information; this document provides the supporting design record and
does not supersede the README's release status.

The central objective is to preserve direct control over GPDMA LLI register images while moving fragile offset calculation, selector/executor wiring, and linker-address construction into the library.

The public API should describe:

- ROOTs
- Programs
- Lists
- Nodes
- Descriptor dispatchers
- Entry points
- Transitions
- Explicit termination

The library should generate the corresponding GPDMA memory image statically wherever possible.

---

## 2. Current alpha implementation

The current implementation has already demonstrated the following:

- Compile-time construction of variable-length GPDMA LLI chains
- Compile-time CLLR generation
- Multiple chains collected into one ROOT
- A selector/executor arrangement that rewrites future LLI images in SRAM
- A descriptor ring that updates:
  - the next selector source address
  - the selector destination address
  - the selector link
  - the executor transfer image
- Execution of the writable ROOT from SRAM2
- ROM load image generation using `constinit`
- Startup-time ROM-to-SRAM2 copying before `__libc_init_array()`
- Removal of the provisional `constructor(101)` SRAM2 initializer

The current runtime connection function still performs two assignments:

```cpp
selector.CSAR = reinterpret_cast<std::uintptr_t>(&descList[0]);
selector.CDAR = reinterpret_cast<std::uintptr_t>(&SPI_GPIO_ROOT)
              + DESCRIPTOR_PATCH_BEGIN;
```

Removing these two runtime assignments is a primary v2.0.0 objective.

---

## 3. Startup and SRAM2 initialization

### 3.1 Linker arrangement

The writable ROOT is placed in SRAM2 with a ROM load image.

```ld
.SRAM2 :
{
    . = ALIGN(8);
    __sram2_start__ = .;

    KEEP(*(.SRAM2))
    KEEP(*(.SRAM2.*))

    . = ALIGN(8);
    __sram2_end__ = .;
} > SRAM2 AT> ROM

__sram2_load__ = LOADADDR(.SRAM2);
__sram2_size__ = SIZEOF(.SRAM2);
```

The linker script must verify:

- The ROOT begins on the required 64 KiB CLBAR boundary
- The ROOT remains within one 64 KiB window
- The load address and runtime address are word aligned
- The copied image size is a multiple of four bytes

### 3.2 Reset handler arrangement

The Reset Handler copies the initialized `.SRAM2` image before the ordinary `.data` initialization and before `__libc_init_array()`.

Conceptual order:

```text
SystemInit()
    -> ROM to SRAM2 copy
    -> ROM to SRAM3 .data/.tdata copy
    -> .bss/.tbss zero fill
    -> __libc_init_array()
    -> main()
```

The provisional `constructor(101)` initializer is no longer required.

### 3.3 Responsibility boundaries

`load_lli()` remains a narrow operation. It configures only the linked-list entry information, currently CLBAR and CLLR.

The caller remains responsible for:

- Ensuring that the GPDMA channel is in a valid stopped/reset state
- Setting CCR mode and interrupt options
- Preparing the internal register file used as LLI0
- Enabling the channel

If the internal LLI0 is intended to be a null transfer, the caller must ensure:

```cpp
channel.CBR1 = 0; // BNDT = 0
```

`CCR.RESET` resets the FIFO, internal channel state, EN, and SUSP. It does not restore CBR1, CSAR, or CDAR to zero.

The following restart sequence was confirmed to work:

```cpp
channel.CCR |= 0x0000'0002u; // RESET
channel.CBR1 = 0;             // null LLI0

load_lli<Entry>(channel, root);

channel.CCR |= 0x0000'0001u; // EN
```

This remains caller responsibility rather than `load_lli()` behavior.

---

## 4. Dynamic selector/executor model

The selector/executor mechanism is treated as a valid use of the GPDMA linked-list model rather than an accidental undocumented behavior.

The operating sequence is:

```text
selector is loaded into the channel register file
    -> selector data transfer rewrites future SRAM LLI images
    -> selector transfer completes
    -> executor is loaded from SRAM
    -> executor transfer runs
    -> subsequent LLI is loaded
```

The currently executing selector is held in the channel linked-list register file. Rewriting the SRAM image of the next selector or executor does not overwrite the currently executing register-file state.

This is consistent with the reference manual's dynamic LLI insertion/update model. The present implementation performs the update and the subsequent load serially within one channel, avoiding CPU intervention or a second DMA channel.

The application must not concurrently modify a descriptor while the selector is transferring that descriptor.

---

## 5. v2 logical hierarchy

The current physical addressing form:

```cpp
node_at<ChainIndex, NodeIndex>
```

is difficult to maintain because inserting a chain renumbers all later chains.

v2 introduces a logical three-level position:

```cpp
node_at<ProgramTag, ListTag, NodeIndex>
```

Meaning:

```text
ROOT
    -> Program
        -> List
            -> Node
```

Example:

```cpp
struct DisplayProgram;
struct SetupList;

using SetupEntry = node_at<DisplayProgram, SetupList, 0>;
```

Program and List identifiers are type tags rather than numeric indices.

Internally, the resolver converts the logical position into:

- Program offset
- List offset
- Node offset
- LLI type
- CLLR update flags
- Final ROOT-relative location

The existing offset and CLLR machinery should be reused beneath this resolver.

---

## 6. Namespace usage

The v2 model must continue to work naturally with C++ namespaces.

Recommended pattern:

```cpp
namespace Display
{
    struct Program;
    struct SetupList;
    struct FinishList;

    using Entry = node_at<Program, SetupList, 0>;
    using FinishEntry = node_at<Program, FinishList, 0>;
}
```

Use:

```cpp
load_lli<Display::Entry>(channel, root);
```

```cpp
patch_jump<Display::FinishTail, Display::Entry>(root);
```

Program names, List names, and commonly used entries should be exposed through aliases so callers do not need to count physical chains or nodes.

---

## 7. Program, List, and Node connection rules

### 7.1 Nodes inside a List

Nodes inside a List remain automatically connected in declaration order.

```text
Node 0 -> Node 1 -> Node 2 -> ...
```

This reuses the current `make_lli()` behavior.

### 7.2 Lists inside a Program

Lists inside a Program may be automatically connected in declaration order.

```text
List 1 tail -> List 2 entry
List 2 tail -> List 3 entry
```

The final List terminates by default unless explicitly connected elsewhere.

### 7.3 Explicit override operations

The following operations remain available:

```cpp
patch_jump<From, To>(root);
```

for loops, branches, List overrides, and Program-to-Program transitions.

An explicit terminal operation is added:

```cpp
patch_end<Position>(root);
```

or an equivalent `terminate<Position>(root)` spelling.

The chosen public spelling should be frozen before the v2.0.0 RC.

### 7.4 Program boundaries

Program boundaries are logical organization only. Hardware sees one ROOT and one CLBAR address space.

Therefore this is valid:

```cpp
patch_jump<ProgramA::Tail, ProgramB::Entry>(root);
```

The behavior is best described as a ROOT-wide goto. It is not `longjmp`, because no call stack or saved execution context exists.

---

## 8. Descriptor ownership

Descriptors should belong to Programs rather than to a special ROOT-global infrastructure area.

A Program may contain:

- Multiple Lists
- Multiple descriptor dispatchers
- Independent descriptor rings
- Multiple entry paths

Conceptual structure:

```text
ROOT
├─ Program A
│  ├─ List A
│  ├─ List B
│  ├─ Dispatch A
│  │  ├─ Selector
│  │  ├─ Executor
│  │  └─ Descriptor list
│  └─ Dispatch B
│     ├─ Selector
│     ├─ Executor
│     └─ Descriptor list
└─ Program B
   ├─ List A
   └─ Dispatch A
```

A descriptor dispatcher should expose named logical entries while hiding the selector/executor physical chain indices.

---

## 9. Descriptor dispatch API

The intended API shape is approximately:

```cpp
make_dispatch<DispatchTag>(
    descriptor_list{
        descriptor(transfer0),
        descriptor(transfer1),
        descriptor(transfer2)
    },
    start_at<3>
);
```

`start_at<N>` controls the initial descriptor index. It replaces the current hard-coded `descList[0]` start.

Default behavior may use `start_at<0>` when no explicit start index is supplied.

The dispatcher implementation must calculate and hide:

- Selector offset
- Executor offset
- Descriptor patch beginning
- Descriptor patch end
- Descriptor patch size
- Selector-to-executor CLLR
- Initial selector CSAR
- Selector CDAR
- Next descriptor CSAR values

The caller should not need to declare:

```cpp
DESCRIPTOR_CHAIN_OFFSET
SELECTOR_OFFSET
EXECUTOR_OFFSET
DESCRIPTOR_PATCH_BEGIN
DESCRIPTOR_PATCH_END
SELECTOR_TO_EXECUTOR_CLLR
```

---

## 10. Static address completion

The primary v2.0.0 feature is removal of runtime selector address connection.

The model is:

```text
compile time:
    build ROOT-relative typed graph from offset zero

link time:
    resolve final absolute CSAR/CDAR addresses through ELF relocations

startup:
    copy completed ROM load image to SRAM2

runtime:
    no selector connection patching
```

ROOT and descriptors should be grouped into a final statically initialized image where appropriate.

The final initializer must expose expressions such as:

```cpp
&program.descriptors[index]
```

and:

```cpp
reinterpret_cast<std::uintptr_t>(&program.root) + patchOffset
```

in a form that GCC can emit as static relocations.

`constinit` is required so the compiler must either produce static initialization or reject the program. Falling back to dynamic initialization is not acceptable.

The implementation must be checked for absence of generated `_GLOBAL__sub_I_*` initialization associated with the ROOT/program image.

---

## 11. `load_lli()` direction

The normal v2 call should use a named logical entry:

```cpp
load_lli<Display::Entry>(channel, root);
```

A default ROOT entry may later allow:

```cpp
load_lli(channel, root);
```

Explicit entry selection must remain available for restart, debugging, recovery, or alternate Program entry points.

The ROOT/program object remains passed explicitly. Program/List/Node identity is carried in the position type.

---

## 12. v2.0.0 feature scope

v2.0.0 should focus on one complete, self-contained one-dimensional executor image:

```cpp
LLI_12BSD_t
```

The v2.0.0 descriptor payload updates:

- CTR1
- CTR2
- CBR1
- CSAR
- CDAR

The executor CLLR remains fixed in v2.0.0.

Advantages:

- Descriptor state is self-contained
- No dependency on a previous executor state
- Restarting from another entry is easier to reason about
- Program-to-Program transitions do not inherit partial transfer configuration
- Debugging can inspect one descriptor and understand the complete transfer

### Internal extensibility requirement

Although only `12BSD` is supported publicly in v2.0.0, the internal representation must not hard-code a `DescStep_12BSD_t`-only architecture.

Use an extensible form such as:

```cpp
template<typename ExecutorImage>
struct descriptor_step;
```

The v2.0.0 implementation may constrain `ExecutorImage` to `DESC_12BSD_t`, but the patch range and payload type must be derived from the executor image type.

---

## 13. Planned v2.1.0 features

v2.1.0 is intended for data-driven dynamic dispatch and partial executor updates.

Planned features include:

- Partial update executors such as `BSD_t`
- Selector self-update of the next CSAR
- State/index-driven selection of the next descriptor or descriptor list
- Dynamic transfer-list replacement without CPU intervention
- Optional executor CLLR rewriting
- Descriptor-specific transition targets
- GC9D01 display sample demonstrating dynamic SPI sequences

A `BSD_t` executor can update:

- CBR1
- CSAR
- CDAR
- optionally CLLR in a routed descriptor form

while preserving common CTR1/CTR2 settings.

The current `12BSD` dispatch remains valid and unchanged.

This is a minor-version feature as long as the v2.0.0 API and behavior remain source-compatible.

---

## 14. Planned later 2D support

Two-dimensional addressing should not be forced into v2.0.0.

Later support may include:

- CTR3
- CBR2
- UT3
- UB2
- 2D executor image types
- Validation of 1D/2D register retention
- A dedicated 2D hardware sample

A suggested roadmap is:

```text
v2.0.0: named hierarchy and static 12BSD descriptor dispatch
v2.1.0: partial updates, state-driven dispatch, routed executor links
v2.2.0: 2D executor and addressing support
```

These remain minor releases unless existing v2 APIs or semantics are broken.

---

## 15. Multiple Programs versus multiple ROOTs

Use multiple Programs inside one ROOT when:

- The sequences share one GPDMA channel
- The sequences share one 64 KiB CLBAR address space
- Program-to-Program transitions are useful
- Descriptor dispatchers should share one execution image

Use multiple ROOTs when:

- Independent GPDMA channels require independent images
- Different CLBAR windows are needed
- Different memory placement or security attributes are required
- Images must be replaced or managed independently

The existing issue for a multiple-ROOT sample should be redefined as:

> Document and demonstrate the distinction between multiple Programs and multiple ROOTs.

This issue may be closed when the v2 beta ships with the Program model, documentation, and an appropriate sample.

---

## 16. Validation

### 16.1 Compile-time validation

The library should reject:

- Duplicate Program tags
- Duplicate List tags within one Program
- Unknown Program/List tags
- Out-of-range Node indices
- Out-of-range `start_at<N>` values
- Unsupported executor image types
- Descriptor/executor layout mismatches
- Invalid update-flag combinations
- ROOT images larger than one 64 KiB window
- Misaligned ROOT or node positions
- Invalid jump targets

### 16.2 Build-condition validation

The optimization/LTO regression has been completed with the following compiler conditions:

```text
-O0
-Og
-O2
-O3
-Os
-O3 -flto
-Os -flto
```

The normal section and garbage-collection options were enabled:

```text
-ffunction-sections
-fdata-sections
-Wl,--gc-sections
```

Each condition passed when all of the following criteria were satisfied:

- The firmware built and linked successfully.
- ROOT and descriptor-ring objects remained statically initialized.
- No ROOT or descriptor-ring dynamic initialization function was emitted.
- The ELF image placed the `.SRAM2` section at the intended VMA and retained the intended ROM load image at its LMA.
- Address relocations completed the selector `CSAR`/`CDAR` fields and descriptor-to-descriptor references with the final linked addresses.
- The ROOT image remained within one 64 KiB GPDMA link-address window.
- The resulting image started and executed the representative sequence on the STM32U575 hardware.

Passing only compilation or linking was not treated as sufficient. A condition passed the matrix only after its static image and hardware operation had also been confirmed.

The representative Release build must be repeated after the public API freeze as part of the RC regression.

### 16.3 ELF inspection

ELF inspection passed when the linked image satisfied all of the following criteria:

- `.SRAM2` had the intended runtime VMA and ROM load-image LMA.
- The ROOT image was present in the ROM load image and fit within one 64 KiB window.
- Relocations resolved selector `CSAR`/`CDAR`, descriptor-to-descriptor references, and selector destination addresses to their final linked values.
- Completed `CSAR`/`CDAR` values pointed to the intended source and destination objects.
- No ROOT or descriptor-ring dynamic initialization function, including a related `_GLOBAL__sub_I_*` function, was emitted.

The static CSAR/CDAR relocation was also verified with `Src/gpdma_static_relocation_probe.cpp`. In the verified Release image, the 16-byte probe image was placed at `0x08000e50`; its `csar` and `cdar` fields contained the final addresses `0x08000e58` and `0x08000e5c` of the source and destination fields. This was accepted as evidence that a pointer within the same static object can be represented in a 32-bit GPDMA address field and completed by ELF relocation and the linker.

The inspection used:

```text
arm-none-eabi-nm
arm-none-eabi-readelf
arm-none-eabi-objdump
linker map files
```

The Release-link `LOAD segment with RWX permissions` warning remains known. It did not invalidate the static-relocation result, but the RC decision must either document why the linker layout is accepted or change the layout so that the warning is removed.

After the public API freeze, repeat the representative Release ELF inspection against the same acceptance criteria.

### 16.4 Hardware regression

Hardware regression passed when the STM32U575 sample demonstrated:

- Startup after a full power interruption, proving that the Reset Handler restored the `.SRAM2` load image without runtime selector connection.
- Startup after a software reset.
- Descriptor-ring wrap and continued execution.
- Startup from an initial descriptor index other than zero.
- Same-Program loops and Program-to-Program jumps.
- Explicit termination without an unintended following transfer.
- Restart from another named entry.
- Correct representative SPI/GPIO sequence execution for every condition in the optimization/LTO matrix.
- Continuous operation for 30 minutes without a sequence stop or observed transfer failure.

The optimization/LTO matrix, cold boot, representative hardware operation, and 30-minute continuous run have passed these criteria. After the public API freeze, repeat the representative Release hardware test and cold boot for the RC decision.

---

## 17. Beta, RC, and Release boundaries

### beta.2

The following feature design and implementation is complete and is recorded by beta.2:

- Program/List/Node hierarchy
- Named resolution
- Automatic List connection
- Explicit termination
- Program-to-Program transitions
- Program-owned descriptor dispatchers
- `start_at<N>`
- Static CSAR/CDAR completion
- No runtime selector connection
- 12BSD executor support
- Updated real-hardware sample
- Negative compile tests
- Optimization/LTO matrix
- ELF inspection
- Hardware operation, cold boot, and continuous-run validation

No new core feature is added for beta.2. Before publishing beta.2:

- Synchronize the README, usage guides, and this design-decision document with the implemented behavior and validation results.
- Confirm which source, sample, and validation-only files, including `Src/gpdma_static_relocation_probe.cpp`, are included in the beta.2 snapshot.

### Release candidate

RC adds no major feature. Before the RC decision:

- Freeze the Program/List public API names and responsibilities.
- Finalize public-API failure diagnostics, including invalid `list_tail` use.
- Reconfirm the negative compile tests after the API freeze.
- Decide whether `Src/gpdma_static_relocation_probe.cpp` is included in the final release or kept as validation-only code.
- Decide whether to accept or fix the Release-link `LOAD segment with RWX permissions` warning.
- Repeat the representative Release build, ELF inspection, hardware test, and cold boot after the API freeze.
- Complete the final documentation review and sample cleanup.

The optimization/LTO matrix, ELF inspection, hardware validation, and static-relocation probe are already complete; RC repeats only the representative final regression after the API freeze.

`validate_reachability()` is optional for v2.0.0. It should not delay release if the static type and structural validations are complete.

### Release

The release may use the same commit as the final RC if no release-blocking issue is found.

A suggested sequence is:

```text
v2.0.0-beta.1
v2.0.0-beta.2
v2.0.0-rc.1
v2.0.0
```

---

## 18. Implementation record

The implementation proceeded incrementally in the following order:

1. Froze the working alpha implementation and artifacts
2. Added Program/List tags and the logical position resolver
3. Added `node_at<ProgramTag, ListTag, NodeIndex>`
4. Added Program construction and automatic List connections
5. Added explicit termination
6. Templated descriptor-step storage by executor image type
7. Hid selector/executor physical layout and patch calculations
8. Added `start_at<N>`
9. Proved static CSAR/CDAR relocation with `Src/gpdma_static_relocation_probe.cpp`
10. Integrated static relocation into the full dispatcher
11. Removed runtime selector connection
12. Migrated the SPI/GPIO sample
13. Completed the optimization/LTO, ELF, and hardware regression matrix

This sequence is retained as the implementation record rather than as pending work. The remaining work is limited to the beta.2 snapshot decisions and the RC activities in section 17.

---

## 19. Summary of frozen direction

The agreed direction is:

- Keep direct GPDMA semantics rather than hiding register behavior
- Hide fragile mechanical layout and address construction
- Use named Program/List tags instead of physical chain numbers
- Preserve namespaces as the primary organizational mechanism
- Automatically connect nodes inside Lists
- Automatically connect normal adjacent Lists inside Programs
- Support explicit jump overrides and explicit termination
- Allow ROOT-wide Program-to-Program transitions
- Let Programs own one or more independent descriptor dispatchers
- Resolve ROOT-relative structure at compile time
- Complete CSAR/CDAR through linker relocations
- Copy a completed ROM image into SRAM2 during startup
- Keep `load_lli()` narrow and caller-controlled
- Limit v2.0.0 descriptor execution to complete `12BSD` images
- Design the internals for later `BSD`, routed CLLR, state-driven selection, and 2D support

The expected result is a library whose internal implementation is highly specialized, while its public API reads as a typed DMA program and transition description.


//ここから追加

# GPDMA LLI v2.0.0 開発共有メモ

## 概要

`gpdma_lli.hpp` の v2 系改造について、現在までに実装・確認できた内容を共有する。

今回の主な成果は、従来は実行時に行っていた descriptor selector の `CSAR/CDAR` 接続を廃止し、コンパイラの静的初期化とリンカのリロケーションだけで ROOT イメージを完成できたことにある。

現在、実行時に必要な処理は概ね次だけになった。

```cpp
channel.CCR |= GPDMA_RESET;
channel.CBR1 = 0; // 内部レジスタファイル LLI0 を null 転送にする

load_lli<Entry>(channel, root);

channel.CCR |= GPDMA_ENABLE;
```

---

## 現在までに実現できたこと

### 1. 名前付き Program / List / Node

従来の物理番号指定、

```cpp
node_at<6, 1>
```

から、論理名を使った3階層指定へ移行した。

```cpp
node_at<ProgramTag, ListTag, NodeIndex>
```

例：

```cpp
namespace SpiGpioProgram
{
    struct Program;
    struct InitialDataLength;
    struct Dispatch;

    using Entry = node_at<Program, InitialDataLength, 0>;
    using DispatchSelector = node_at<Program, Dispatch, 0>;
    using DispatchExecutor = node_at<Program, Dispatch, 1>;
}
```

これにより、Listを追加・並べ替えしても、利用側で物理チェーン番号を数え直す必要がなくなる。

---

### 2. Program/Listタグから物理位置を解決

`node_at<ProgramTag, ListTag, NodeIndex>` は、内部で次へ解決される。

- ROOT内のList番号
- List内のNode番号
- ROOT先頭からのオフセット
- 対象LLI型
- 対象LLI型に対応するCLLR更新フラグ

既存の以下の仕組みは引き続き流用している。

- `group_chain_offset()`
- `chain_node_offset()`
- `chain_node_type`
- `start_cllr()`
- `patch_jump()`
- `validate_root()`

---

### 3. descriptor stepのテンプレート化

従来の固定された `DescStep_12BSD_t` ではなく、executor image型を引数に取る構造へ変更した。

```cpp
template<typename ExecutorImage>
struct descriptor_step;
```

v2.0.0では正式サポート対象を次に限定する。

```cpp
DESC_12BSD_t
```

現在のstep構成は次の32バイトである。

```text
next selector CSAR    4 bytes
next selector CDAR    4 bytes
next selector CLLR    4 bytes
executor image       20 bytes
-----------------------------
total                32 bytes
```

executorの`CLLR`は転送範囲外に置き、v2.0.0ではROOT側の固定リンクを保持する。

---

### 4. descriptor dispatcherの物理計算をライブラリ内部へ移動

`descriptor_dispatch_layout` が、ROOT型とselector/executor位置から以下を自動導出する。

- selectorのROOT内オフセット
- executorのROOT内オフセット
- descriptor転送先の開始位置
- descriptor転送範囲
- selectorからexecutorへのCLLR
- descriptor step型

利用側では、従来必要だった以下の値を定義しなくてよくなった。

```cpp
DESCRIPTOR_CHAIN_OFFSET
SELECTOR_OFFSET
EXECUTOR_OFFSET
DESCRIPTOR_PATCH_BEGIN
DESCRIPTOR_PATCH_END
SELECTOR_TO_EXECUTOR_CLLR
```

レイアウト不一致は `static_assert` で検出する。

```cpp
static_assert(
    patch_end - patch_begin == sizeof(step_type),
    "descriptor patch range must match descriptor_step"
);
```

---

### 5. descriptor ringの相互参照を静的初期化

descriptor ringは `constinit` で構築される。

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

これにより、以下が静的イメージとして完成する。

- 次に読み込むdescriptor
- selectorの書換え先
- selectorからexecutorへのリンク
- executorへ転送するレジスタ像

---

### 6. selectorのCSAR/CDARを静的に完成

以前は起動後に次の2代入が必要だった。

```cpp
selector.CSAR = address_of(descList[0]);
selector.CDAR = address_of(SPI_GPIO_ROOT) + patch_begin;
```

現在は `SPI_GPIO_ROOT` の `constinit` initializer内で接続する。

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

絶対アドレスはコンパイラ／リンカの静的初期化とELFリロケーションで解決される。

この変更により、実行時の `connectSelectorAddresses()` は不要になった。

---

### 7. 初期descriptor番号を型で指定

selectorが最初に読むdescriptorは、`start_at<N>`で指定できる。

```cpp
start_at<0>{}
```

将来的には、例えば以下も可能になる。

```cpp
start_at<3>{}
```

これにより、`descList[0]`固定ではなく、任意のdescriptorから開始できる。

---

### 8. SRAM2実行イメージ

ROOTはSRAM2上で実行し、ROM上に完成済みロードイメージを持つ。

```text
ROM load image
    -> Reset_HandlerでSRAM2へコピー
    -> __libc_init_array()
    -> main()
```

`constructor(101)`による暫定コピー処理は廃止済み。

現在の初期化順序は次のとおり。

```text
SystemInit()
    -> ROMからSRAM2へROOTイメージをコピー
    -> .data/.tdataを初期化
    -> .bss/.tbssをゼロクリア
    -> __libc_init_array()
    -> main()
```

---

## 現在の実行コード

```cpp
GPDMA1_Channel0->CCR |= 0x0000'0002u; // RESET
GPDMA1_Channel0->CBR1 = 0;             // null LLI0

load_lli<SpiGpioProgram::Entry>(
    *GPDMA1_Channel0,
    SPI_GPIO_ROOT
);

GPDMA1_Channel0->CCR |= 0x0000'0001u; // EN
```

`load_lli()`の責務は引き続き限定する。

```text
load_lli():
- CLBARを設定
- CLLRを設定

呼び出し側:
- チャネル停止／RESET
- 内部レジスタファイルLLI0の準備
- CBR1.BNDT = 0の保証が必要なら明示設定
- CCR設定
- チャネル有効化
```

`CCR.RESET`はFIFO、内部状態、EN、SUSPをリセットするが、CBR1、CSAR、CDARをゼロには戻さない。

---

## 動作確認済み事項

以下はSTM32U575実機で動作確認済み。

- SRAM2上の書換え可能ROOT実行
- descriptor ringの循環
- selectorによる次回selector像の更新
- selectorによるexecutor像の更新
- selectorからexecutorへの遷移
- executor終了後の固定CLLR遷移
- selectorのCSAR/CDARを静的リロケーションで完成
- 実行時selector接続処理なしで起動
- Reset HandlerによるSRAM2ロードイメージ展開
- `CBR1.BNDT = 0`によるnull LLI0開始

---

## beta.2およびRCまでの境界

主要機能の設計、実装、および主要な回帰確認は完了している。beta.2では新しい中核機能を追加せず、beta.1以降に完了した次の内容を実装済み事項として記録する。

- Program/List/Node階層と名前付き位置指定
- Named resolution
- List内部およびProgram内の隣接List間の自動接続
- `end_list()`と`patch_end()`による明示的終端
- Program間遷移
- Programが所有するdescriptor dispatcher
- `start_at<N>`
- selectorの`CSAR`/`CDAR`の静的完成
- 実行時selector接続処理の廃止
- `DESC_12BSD_t` executorのサポート
- 負のコンパイル試験
- 最適化・LTO回帰
- ELF検査
- 実機動作、コールドブート、および30分間の連続動作

### beta.2公開前

beta.2公開前に残る作業は、実装済みの機能と検証結果を公開文書および収録物へ反映することである。

- README、使用ガイド、および設計判断文書を現在の実装と検証結果へ同期する
- `Src/gpdma_static_relocation_probe.cpp`を含め、beta.2へ収録するソース、サンプル、および検証専用ファイルの範囲を確認する

`Src/gpdma_static_relocation_probe.cpp`を使った静的リロケーション検証は実施済みである。beta.2では開発時の検証資料として収録できるが、最終リリース成果物へ含めるかはRC前に確定する。

### RC判定前

RCでは主要機能を追加しない。RC判定前に、次を完了する。

- Program/List公開APIの名称と責務を凍結する
- 不正な`list_tail`の使用を含む、公開APIの失敗時診断を最終確認し、必要なら改善する
- API凍結後に負のコンパイル試験を最終確認する
- `Src/gpdma_static_relocation_probe.cpp`を最終リリースへ含めるか、検証専用コードとして分離するかを確定する
- Releaseリンク時の`LOAD segment with RWX permissions`警告を許容するか、リンカ配置を修正するかを判断する
- API凍結後に代表的なReleaseビルド、ELF検査、実機動作、およびコールドブートを再実行する
- 文書の最終確認とサンプルの整理を完了する

最適化・LTO回帰、ELF検査、実機検証、および静的リロケーションprobeによる検証は実施済みである。RC前に行うのは、API凍結後の代表構成による最終回帰である。

`validate_reachability()`はv2.0.0の任意機能とする。静的な型検査と構造検査が成立している場合、未実装であることをリリース延期の理由にはしない。

---

## 最適化・LTOおよび実機確認

次のコンパイラ条件で回帰試験を実施済みである。

```text
-O0
-Og
-O2
-O3
-Os
-O3 -flto
-Os -flto
```

各条件では、通常のセクション分割と未使用セクション除去を有効にした。

```text
-ffunction-sections
-fdata-sections
-Wl,--gc-sections
```

各条件は、次の項目をすべて満たした場合に合格とした。

- ファームウェアのビルドとリンクが成功する
- ROOTとdescriptor ringが静的初期化として成立する
- ROOTおよびdescriptor ring用の動的初期化関数が生成されない
- `.SRAM2`が意図したVMAに配置され、ROM側のロードイメージが意図したLMAに保持される
- selectorの`CSAR`/`CDAR`およびdescriptor ringの相互参照が、リロケーションによって最終リンクアドレスへ解決される
- ROOTがGPDMAの1つの64 KiBリンクアドレス窓内に収まる
- STM32U575実機で代表シーケンスが起動し、意図どおり動作する

コンパイルまたはリンクの成功だけでは合格としていない。静的イメージの検査と実機動作まで確認できた条件を、最適化・LTO回帰の合格条件としている。

ELF検査では、`arm-none-eabi-nm`、`arm-none-eabi-readelf`、`arm-none-eabi-objdump`、およびリンカmapを使用し、次を合格条件とした。

- `.SRAM2`のVMA/LMAが意図した配置である
- ROOTのROMロードイメージが存在し、ROOT全体が64 KiB窓内に収まる
- selectorの`CSAR`/`CDAR`、descriptor間参照、およびselectorの転送先アドレスが最終リンク値へ解決される
- 関連する`_GLOBAL__sub_I_*`を含め、ROOTおよびdescriptor ring用の動的初期化関数が生成されない

`Src/gpdma_static_relocation_probe.cpp`によるRelease検証では、16バイトのprobe imageが`0x08000e50`へ配置され、`csar`と`cdar`にsourceとdestinationの最終アドレス`0x08000e58`と`0x08000e5c`が格納された。同一静的オブジェクト内のポインタを32bitのGPDMAアドレスフィールドへ保持し、ELFリロケーションとリンカによって完成できることを、この結果の合格条件として確認した。

実機検証では、次を合格条件とした。

- 電源断後の起動でReset Handlerが`.SRAM2`のロードイメージを展開し、実行時selector接続処理なしで開始できる
- ソフトウェアリセット後に開始できる
- descriptor ringが周回して動作を継続する
- 0以外の初期descriptor位置から開始できる
- 同一Program内のループとProgram間遷移が成立する
- 明示的終端後に意図しない転送を実行しない
- 別の名前付きentryから再開できる
- 全最適化・LTO条件で代表SPI/GPIOシーケンスが意図どおり動作する
- 30分間、シーケンス停止または確認可能な転送失敗なく連続動作する

Releaseリンク時の`LOAD segment with RWX permissions`警告は既知であり、静的リロケーションの検証結果は無効にならない。ただしRC判定では、現在のリンカ配置を許容する根拠を記録するか、警告が解消するよう配置を修正する。

API凍結後は、同じ合格条件を使って代表Release構成のビルド、ELF検査、実機動作、およびコールドブートを再実行する。

---

## v2.0.0の正式スコープ

v2.0.0では、descriptor executor imageを次に限定する。

```cpp
DESC_12BSD_t
```

更新対象：

- CTR1
- CTR2
- CBR1
- CSAR
- CDAR

executor.CLLRは固定リンクとして保持する。

v2.0.0では、descriptor単体で転送条件が完結し、前回状態へ依存しないことを優先する。

---

## v2.1.0以降の予定

v2.1.0では、次を追加候補とする。

- `BSD_t`など部分更新executor
- selector自身の次回CSARを状態に応じて変更
- descriptor tableまたは転送Listの動的選択
- executor.CLLRのdescriptorごとの更新
- descriptorごとのProgram/List/Node遷移先
- GC9D01を使った動的SPIシーケンスのサンプル

2Dアドレッシングは、さらに後のMinor release候補とする。

- CTR3
- CBR2
- UT3
- UB2
- 2D executor image

既存v2 APIを壊さず対応型を追加できる限り、v3ではなくv2.xのMinor releaseとする。

---

## 環境依存部分について

現在の実装はSTM32の32bitアドレス空間を前提としている。

現時点では対象がSTM32に限定されているため、この前提は実用上問題ない。

将来、別環境や別アドレス幅への対応が必要になった場合は、ポインタ表現とGPDMAレジスタ用32bitアドレス表現を切り替えられるtraitsまたはpolicyを追加する可能性がある。

ただし、現段階では過度な一般化は行わない。

---

## 現在地点の評価

v2.0.0の中核機能と主要な検証は成立している。

```text
[x] Program/List/Nodeの名前付き位置
[x] node_at<ProgramTag, ListTag, NodeIndex>
[x] descriptor_stepのテンプレート化
[x] v2.0.0はDESC_12BSD_tに限定
[x] descriptor patch範囲の自動計算
[x] selector -> executor CLLRの自動生成
[x] descriptor ringのconstinit化
[x] selector.CSAR/CDARの静的リロケーション
[x] 実行時connectSelectorAddresses()の削除
[x] Reset HandlerによるSRAM2展開
[x] Listの意味単位への再編
[x] List間の標準自動接続
[x] 明示終端API
[x] ProgramTag/ListTagの重複検査
[x] 負のコンパイル試験
[x] 最適化・LTO回帰
[x] ELF検査
[x] 実機動作
[x] コールドブート
[x] 30分間の連続動作
```

beta.2までは、実装済み事項と検証結果の文書同期、および収録範囲の確認を行う。RCまでは、公開APIの凍結、失敗時診断と負のコンパイル試験の最終確認、probeとRWX警告の扱いの決定、およびAPI凍結後の最終回帰を行う。
