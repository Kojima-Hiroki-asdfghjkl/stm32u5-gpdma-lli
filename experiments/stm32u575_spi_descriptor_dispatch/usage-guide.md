# GPDMA LLI v2 Usage Guide

[English](#english) | [日本語](#日本語)

This guide explains how to build and use ROOTs with the current v2.0.0-beta.2 API.
Release status, validation results, and known limitations belong in `README.md`.

このガイドは、現行の v2.0.0-beta.2 API を使って ROOT を構築・実行する方法を説明します。
リリース状態、検証結果、既知の制限は `README.md` を正本とします。

---

## English

### 1. Mental model

```text
ROOT
└─ Program
   └─ List
      └─ Node
```

| Element | Meaning | Runtime storage |
|---|---|---|
| Node | One GPDMA LLI register image | Yes |
| List | One named Node chain | Only the Node chain |
| Program | A logical group of Lists | No extra fields |
| ROOT | The flattened, executable LLI image | Yes |

Program and List tags are compile-time types. They do not add strings, pointers, or tag fields to the ROOT image.

### 2. Build Nodes and Lists

A Node is a register image such as `LLI_12BSD_t`.

```cpp
constexpr LLI_12BSD_t GPIO_HIGH_NODE = {
    .CTR1 = 0x0002'4002,
    .CTR2 = 0x0000'0200,
    .CBR1 = 4,
    .CSAR = static_cast<uint32_t>(
        reinterpret_cast<std::uintptr_t>(&GPIO_HIGH_VALUE)),
    .CDAR = static_cast<uint32_t>(
        reinterpret_cast<std::uintptr_t>(&(GPIOC->BSRR)))
};
```

Create a physical Node chain with `make_lli()`:

```cpp
constexpr auto gpioChain = make_lli(
    GPIO_HIGH_NODE, // Node 0
    GPIO_LOW_NODE   // Node 1
);
```

Attach Program and List tags with `make_list()`:

```cpp
struct MainProgram;
struct GpioPulse;

constexpr auto gpioList =
    make_list<MainProgram, GpioPulse>(gpioChain);
```

Available forms:

```cpp
make_list<ProgramTag, ListTag>(chain); // full named hierarchy
make_list<ListTag>(chain);             // default_program_tag
make_list(chain);                      // anonymous, sequential use only
```

Use the anonymous form only when the List is executed in declaration order and does not need a named entry, jump target, dispatcher position, or Program boundary.

### 3. Named positions

Use logical positions instead of physical List indices:

```cpp
using PulseHigh = node_at<MainProgram, GpioPulse, 0>;
using PulseLow  = node_at<MainProgram, GpioPulse, 1>;

using PulseEntry = list_entry<MainProgram, GpioPulse>;
using PulseTail  = list_tail<MainProgram, GpioPulse>;
```

The Node index is relative to the selected List, not to the entire ROOT.

Prefer `list_entry` and `list_tail` for public entry and exit names. They remain correct if Nodes are added to the beginning or end of a List.

The pair `ProgramTag + ListTag` must be unique in one ROOT. Anonymous Lists are excluded from named lookup.

### 4. Programs and ROOTs

Build a reusable Program from Lists:

```cpp
constexpr auto mainProgram = make_lli_program(
    make_list<MainProgram, Setup>(
        make_lli(SETUP_NODE)
    ),
    make_list<MainProgram, Transfer>(
        make_lli(TX_NODE_0, TX_NODE_1)
    )
);
```

`make_lli_program()` stores a reusable List sequence. It does not complete ROOT-relative CLLR fixups or build the final executable ROOT.

Build the final ROOT with `make_lli_group()`:

```cpp
constexpr auto rootTemplate = make_lli_group(
    mainProgram,
    errorProgram
);

static_assert(validate_root(rootTemplate));
```

During ROOT construction, the library:

- flattens Programs and Groups into one ROOT;
- recalculates all ROOT-relative CLLR values;
- connects adjacent Lists with the same Program tag;
- leaves Program boundaries terminal by default;
- validates layout, alignment, size, and CLLR targets.

### 5. Automatic links, jumps, and termination

Adjacent Lists in the same Program connect automatically unless the preceding List is wrapped with `end_list()`.

```cpp
constexpr auto program = make_lli_program(
    end_list(make_list<MainProgram, Setup>(setupChain)),
    make_list<MainProgram, Transfer>(transferChain)
);
```

Program boundaries do not connect automatically. Add required transitions after ROOT construction:

```cpp
constexpr auto rootTemplate = [] {
    auto root = make_lli_group(mainProgram, errorProgram);

    patch_jump<
        list_tail<MainProgram, Transfer>,
        list_entry<ErrorProgram, Stop>
    >(root);

    return root;
}();
```

Terminate any position explicitly with:

```cpp
patch_end<node_at<MainProgram, Transfer, 1>>(root);
```

`end_list()` prevents an automatic List-boundary link during construction. `patch_end()` overwrites the CLLR of one Node in an already constructed ROOT.

### 6. Reuse and composition

To place the same Program template more than once in one ROOT, assign distinct instance tags:

```cpp
struct InstanceA;
struct InstanceB;

constexpr auto root = make_lli_group(
    instantiate<InstanceA>(programTemplate),
    instantiate<InstanceB>(programTemplate)
);
```

The effective Program tag becomes:

```cpp
instanced_tag<InstanceA, OriginalProgramTag>
```

A previously instantiated Program cannot be instantiated again directly. Reuse the original template or rebuild a ROOT first.

Completed Groups can also be combined:

```cpp
constexpr auto root = append_lli_group(groupA, groupB);
```

`append_lli_group()` reconstructs a new ROOT. It does not concatenate raw bytes. ROOT-relative links and same-Program adjacency are recalculated.

### 7. Descriptor dispatcher

The dispatcher uses a selector/executor pair:

```text
selector → executor → following List
```

For v2.0.0, the supported executor payload is:

```cpp
DESC_12BSD_t
```

Define stable named positions for the dispatcher List:

```cpp
namespace SpiProgram
{
    struct Program;
    struct Dispatch;

    using DispatchSelector = list_entry<Program, Dispatch>;
    using DispatchExecutor = list_tail<Program, Dispatch>;
}
```

Create the layout type after the ROOT type is known:

```cpp
using root_t = std::remove_cv_t<decltype(ROOT_TEMPLATE)>;

using dispatcher_t = descriptor_dispatch_layout<
    SpiProgram::DispatchSelector,
    SpiProgram::DispatchExecutor,
    DESC_12BSD_t,
    root_t
>;
```

`descriptor_dispatch_layout` derives selector/executor offsets, the rewrite range, transfer size, and selector-to-executor CLLR from the ROOT type.

Build a static ring:

```cpp
extern root_t EXECUTION_ROOT;

constinit dispatcher_t::step_type steps[4] = {
    dispatcher_t::make_step(&steps[1], EXECUTION_ROOT, transfer0),
    dispatcher_t::make_step(&steps[2], EXECUTION_ROOT, transfer1),
    dispatcher_t::make_step(&steps[3], EXECUTION_ROOT, transfer2),
    dispatcher_t::make_step(&steps[0], EXECUTION_ROOT, transfer3)
};
```

Complete the writable execution ROOT statically:

```cpp
[[gnu::used]]
[[gnu::section(".SRAM2")]]
constinit root_t EXECUTION_ROOT = [] {
    auto root = ROOT_TEMPLATE;

    dispatcher_t::initialize_selector(
        root,
        EXECUTION_ROOT,
        steps,
        start_at<0>{}
    );

    return root;
}();
```

`start_at<N>` selects the first descriptor. `N` must be smaller than the ring size.

Use `initialize_selector()` for a statically completed ROOT. `connect_selector()` is available only when the selector must be connected to an already instantiated ROOT at runtime.

### 8. Load and start a ROOT

`load_lli()` writes only CLBAR and CLLR. It does not reset, stop, or enable the channel.

```cpp
channel.CCR |= 0x0000'0002u; // RESET
channel.CBR1 = 0;             // null LLI0

load_lli<Entry>(channel, EXECUTION_ROOT);

channel.CCR |= 0x0000'0001u; // EN
```

Call `load_lli()` while the channel is disabled. CLBAR and CLLR are locked while `CCR.EN` is set.

`CCR.RESET` does not clear CBR1, CSAR, or CDAR. Set `CBR1 = 0` explicitly when the channel-register LLI0 must perform no transfer before loading the first linked-list item.

### 9. API selection reference

| Goal | API |
|---|---|
| Build one Node chain | `make_lli()` |
| Name a chain | `make_list()` |
| Stop automatic same-Program linking | `end_list()` |
| Build reusable Lists | `make_lli_program()` |
| Build the final ROOT | `make_lli_group()` |
| Reuse a Program more than once | `instantiate()` |
| Combine completed Groups | `append_lli_group()` |
| Override a transition | `patch_jump()` |
| Terminate one Node | `patch_end()` |
| Validate the completed ROOT | `validate_root()` |
| Define descriptor layout | `descriptor_dispatch_layout` |
| Select initial descriptor | `start_at<N>` |
| Load a named entry | `load_lli<Position>()` |

---

## 日本語

### 1. 基本モデル

```text
ROOT
└─ Program
   └─ List
      └─ Node
```

| 要素 | 意味 | 実行時にROOTへ格納されるもの |
|---|---|---|
| Node | 1個のGPDMA LLIレジスタ像 | Node本体 |
| List | 名前付きNodeチェーン | Nodeチェーンのみ |
| Program | 複数Listの論理的なまとまり | 追加フィールドなし |
| ROOT | 平坦化された実行用LLIイメージ | ROOT本体 |

ProgramタグとListタグはコンパイル時の型情報です。ROOTへ文字列、ポインタ、タグ用フィールドを追加しません。

### 2. NodeとListを作る

Nodeは`LLI_12BSD_t`などのレジスタ像です。

```cpp
constexpr LLI_12BSD_t GPIO_HIGH_NODE = {
    .CTR1 = 0x0002'4002,
    .CTR2 = 0x0000'0200,
    .CBR1 = 4,
    .CSAR = static_cast<uint32_t>(
        reinterpret_cast<std::uintptr_t>(&GPIO_HIGH_VALUE)),
    .CDAR = static_cast<uint32_t>(
        reinterpret_cast<std::uintptr_t>(&(GPIOC->BSRR)))
};
```

`make_lli()`へ複数Nodeを渡すと、宣言順の物理チェーンになります。

```cpp
constexpr auto gpioChain = make_lli(
    GPIO_HIGH_NODE, // Node 0
    GPIO_LOW_NODE   // Node 1
);
```

`make_list()`でProgramタグとListタグを付けます。

```cpp
struct MainProgram;
struct GpioPulse;

constexpr auto gpioList =
    make_list<MainProgram, GpioPulse>(gpioChain);
```

使用できる形式は次の3つです。

```cpp
make_list<ProgramTag, ListTag>(chain); // Program/Listを明示
make_list<ListTag>(chain);             // default_program_tagを使用
make_list(chain);                      // 匿名List、直列用途のみ
```

匿名形式は、宣言順どおりに実行し、名前付きentry、ジャンプ先、dispatcher位置、Program境界を必要としない場合だけ使用します。

### 3. 名前付き位置

物理List番号ではなく論理位置を指定します。

```cpp
using PulseHigh = node_at<MainProgram, GpioPulse, 0>;
using PulseLow  = node_at<MainProgram, GpioPulse, 1>;

using PulseEntry = list_entry<MainProgram, GpioPulse>;
using PulseTail  = list_tail<MainProgram, GpioPulse>;
```

Node番号はROOT全体の通し番号ではなく、指定したList内の0始まり番号です。

公開entryとexitには`list_entry`と`list_tail`を優先します。Listの先頭や末尾へNodeを追加しても位置名を変更せずに済みます。

同一ROOT内では`ProgramTag + ListTag`の組み合わせが一意である必要があります。匿名Listは名前付き検索の対象外です。

### 4. ProgramとROOT

再利用するList列はProgramとして作ります。

```cpp
constexpr auto mainProgram = make_lli_program(
    make_list<MainProgram, Setup>(
        make_lli(SETUP_NODE)
    ),
    make_list<MainProgram, Transfer>(
        make_lli(TX_NODE_0, TX_NODE_1)
    )
);
```

`make_lli_program()`は再利用可能なList列を保持します。ROOT相対CLLRの確定や最終ROOT構築は行いません。

最終ROOTは`make_lli_group()`で作ります。

```cpp
constexpr auto rootTemplate = make_lli_group(
    mainProgram,
    errorProgram
);

static_assert(validate_root(rootTemplate));
```

ROOT構築時にライブラリが行う処理は次のとおりです。

- ProgramとGroupを1つのROOTへ平坦化
- 全CLLRをROOT先頭基準で再計算
- 同一Programの隣接Listを自動接続
- Program境界を既定で終端
- 配置、アラインメント、サイズ、CLLR遷移先を検証

### 5. 自動接続、ジャンプ、終端

同じProgram内の隣接Listは、前のListへ`end_list()`を指定しない限り自動接続されます。

```cpp
constexpr auto program = make_lli_program(
    end_list(make_list<MainProgram, Setup>(setupChain)),
    make_list<MainProgram, Transfer>(transferChain)
);
```

Program境界は自動接続されません。必要な遷移だけROOT構築後に追加します。

```cpp
constexpr auto rootTemplate = [] {
    auto root = make_lli_group(mainProgram, errorProgram);

    patch_jump<
        list_tail<MainProgram, Transfer>,
        list_entry<ErrorProgram, Stop>
    >(root);

    return root;
}();
```

任意のNodeを終端へ変更する場合は`patch_end()`を使います。

```cpp
patch_end<node_at<MainProgram, Transfer, 1>>(root);
```

`end_list()`はROOT構築時のList境界自動接続を止めます。`patch_end()`は完成ROOT内の1つのNodeのCLLRを終端へ上書きします。

### 6. 再利用と結合

同じProgramテンプレートを1つのROOT内で複数回使う場合は、異なるインスタンスタグを付けます。

```cpp
struct InstanceA;
struct InstanceB;

constexpr auto root = make_lli_group(
    instantiate<InstanceA>(programTemplate),
    instantiate<InstanceB>(programTemplate)
);
```

実効Programタグは次になります。

```cpp
instanced_tag<InstanceA, OriginalProgramTag>
```

インスタンス化済みProgramを直接もう一度`instantiate()`することはできません。元テンプレートを再利用するか、一度ROOTとして再構築します。

完成済みGroupも結合できます。

```cpp
constexpr auto root = append_lli_group(groupA, groupB);
```

`append_lli_group()`は生バイト列を連結するのではなく、新しいROOTを再構築します。ROOT相対リンクと同一Programの隣接接続も再計算されます。

### 7. Descriptor dispatcher

dispatcherはselector/executorの2段で動作します。

```text
selector → executor → 後続List
```

v2.0.0で対応するexecutor payloadは次です。

```cpp
DESC_12BSD_t
```

dispatcher Listには安定した名前付き位置を定義します。

```cpp
namespace SpiProgram
{
    struct Program;
    struct Dispatch;

    using DispatchSelector = list_entry<Program, Dispatch>;
    using DispatchExecutor = list_tail<Program, Dispatch>;
}
```

ROOT型が確定した後でlayout型を作ります。

```cpp
using root_t = std::remove_cv_t<decltype(ROOT_TEMPLATE)>;

using dispatcher_t = descriptor_dispatch_layout<
    SpiProgram::DispatchSelector,
    SpiProgram::DispatchExecutor,
    DESC_12BSD_t,
    root_t
>;
```

`descriptor_dispatch_layout`は、ROOT型からselector/executorの位置、書き換え範囲、転送サイズ、selectorからexecutorへのCLLRを導出します。

静的ringは次のように作ります。

```cpp
extern root_t EXECUTION_ROOT;

constinit dispatcher_t::step_type steps[4] = {
    dispatcher_t::make_step(&steps[1], EXECUTION_ROOT, transfer0),
    dispatcher_t::make_step(&steps[2], EXECUTION_ROOT, transfer1),
    dispatcher_t::make_step(&steps[3], EXECUTION_ROOT, transfer2),
    dispatcher_t::make_step(&steps[0], EXECUTION_ROOT, transfer3)
};
```

書き換え可能な実行ROOTも静的に完成させます。

```cpp
[[gnu::used]]
[[gnu::section(".SRAM2")]]
constinit root_t EXECUTION_ROOT = [] {
    auto root = ROOT_TEMPLATE;

    dispatcher_t::initialize_selector(
        root,
        EXECUTION_ROOT,
        steps,
        start_at<0>{}
    );

    return root;
}();
```

`start_at<N>`で最初に読むdescriptor番号を指定します。`N`はring要素数未満でなければなりません。

静的完成するROOTでは`initialize_selector()`を使用します。`connect_selector()`は、生成済みROOTへ実行時にselectorを接続する必要がある場合だけ使用します。

### 8. ROOTをロードして開始する

`load_lli()`が設定するのはCLBARとCLLRだけです。チャネルのRESET、停止、開始は行いません。

```cpp
channel.CCR |= 0x0000'0002u; // RESET
channel.CBR1 = 0;             // null LLI0

load_lli<Entry>(channel, EXECUTION_ROOT);

channel.CCR |= 0x0000'0001u; // EN
```

`load_lli()`はチャネル無効中に呼びます。`CCR.EN`が立っている間はCLBARとCLLRがロックされます。

`CCR.RESET`はCBR1、CSAR、CDARをゼロクリアしません。チャネルレジスタ上のLLI0で転送を行わず、直ちにリンク先LLIへ進めたい場合は`CBR1 = 0`を明示します。

### 9. API早見表

| 目的 | API |
|---|---|
| 1つのNodeチェーンを作る | `make_lli()` |
| チェーンへ名前を付ける | `make_list()` |
| 同一Program内の自動接続を止める | `end_list()` |
| 再利用可能なList列を作る | `make_lli_program()` |
| 最終ROOTを作る | `make_lli_group()` |
| 同じProgramを複数回使う | `instantiate()` |
| 完成済みGroupを結合する | `append_lli_group()` |
| 遷移先を上書きする | `patch_jump()` |
| Nodeを終端にする | `patch_end()` |
| 完成ROOTを検証する | `validate_root()` |
| descriptor layoutを定義する | `descriptor_dispatch_layout` |
| 初期descriptorを選ぶ | `start_at<N>` |
| 名前付きentryをロードする | `load_lli<Position>()` |
