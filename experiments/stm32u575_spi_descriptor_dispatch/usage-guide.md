# GPDMA LLI v2 Usage Guide

[English](#english) | [日本語](#日本語)

This guide explains how to build and use ROOTs with the current v2.0.0-beta.3 API.
Release status, validation results, and known limitations belong in `README_v2.0.0-beta.3.md`.

このガイドは、現行の v2.0.0-beta.3 API を使って ROOT を構築・実行する方法を説明します。
リリース状態、検証結果、既知の制限は `README_v2.0.0-beta.3.md` を正本とします。

---

## English

### 1. Mental model

```text
Group (lli_group: mutable component / finalized_lli_group: finalized ROOT)
└─ List  [tags: GroupTag, ListTag]
   └─ Node  (Chain of GPDMA LLI register images)
```

| Element | Meaning | Runtime storage |
|---|---|---|
| Node | One GPDMA LLI register image | Yes |
| Chain | Ordered Node sequence built by `make_lli()` | Only the Nodes |
| List | One Chain labeled with `GroupTag` and `ListTag` | Only the Chain |
| Group / ROOT | Flattened LLI sequence (`lli_group` / `finalized_lli_group`) | Only the Chains |

Group tags (`GroupTag`) and List tags (`ListTag`) are compile-time types. They do not add strings, pointers, or tag fields to the ROOT image.

### 2. Build Nodes, Chains, and Lists

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

Create a physical Node Chain with `make_lli()`:

```cpp
constexpr auto gpioChain = make_lli(
    GPIO_HIGH_NODE, // Node 0
    GPIO_LOW_NODE   // Node 1
);
```

Attach a Group tag and a List tag with `make_list()`:

```cpp
struct MainGroup;
struct GpioPulse;

constexpr auto gpioList =
    make_list<MainGroup, GpioPulse>(gpioChain);
```

Available forms:

```cpp
make_list<GroupTag, ListTag>(chain); // full named hierarchy
make_list<ListTag>(chain);           // uses default_group_tag
make_list(chain);                    // anonymous, sequential use only
```

Use the anonymous form only when the List is executed in declaration order and does not need a named entry, jump target, dispatcher position, or Group-tag boundary.

### 3. Named positions

Use logical positions instead of physical List indices:

```cpp
using PulseHigh = node_at<MainGroup, GpioPulse, 0>;
using PulseLow  = node_at<MainGroup, GpioPulse, 1>;

using PulseEntry = list_entry<MainGroup, GpioPulse>;
using PulseTail  = list_tail<MainGroup, GpioPulse>;
```

The Node index is relative to the selected List, not to the entire ROOT.

Prefer `list_entry` and `list_tail` for public entry and exit names. They remain correct if Nodes are added to the beginning or end of a List.

The pair `GroupTag + ListTag` must be unique in one ROOT. Anonymous Lists are excluded from named lookup.

### 4. Groups and finalized ROOTs

Build reusable components and full ROOTs with `make_lli_group()`:

```cpp
constexpr auto mainGroup = make_lli_group(
    make_list<MainGroup, Setup>(
        make_lli(SETUP_NODE)
    ),
    make_list<MainGroup, Transfer>(
        make_lli(TX_NODE_0, TX_NODE_1)
    )
);
```

Combine components and finalize the executable ROOT with `finalize_lli_group()`:

```cpp
constexpr auto ROOT = finalize_lli_group(
    make_lli_group(
        mainGroup,
        errorGroup,
        jump<
            list_tail<MainGroup, Transfer>,
            list_entry<ErrorGroup, Stop>
        >{}
    )
);

static_assert(validate_root(ROOT));
```

During Group construction, the library:

- flattens Lists and mutable sub-Groups into one `lli_group`;
- recalculates all ROOT-relative CLLR values;
- connects adjacent Lists that share the same `GroupTag`;
- leaves boundaries between different `GroupTag`s terminal by default;
- applies wiring rules (`jump` / `end_node`) from inner sub-Groups first, then outer rules;
- validates layout, alignment, size, and CLLR targets.

Calling `finalize_lli_group()` strips the compile-time wiring-rule type parameters (with zero runtime size change) and returns a `finalized_lli_group` ready for `descriptor_dispatch_layout` and `load_lli()`, while rejecting further `make_lli_group()`, `append_lli_group()`, or `instantiate()` calls at compile time.

### 5. Automatic links, jumps, and termination

Adjacent Lists with the same `GroupTag` connect automatically unless the preceding List is wrapped with `end_list()`.

```cpp
constexpr auto group = make_lli_group(
    end_list(make_list<MainGroup, Setup>(setupChain)),
    make_list<MainGroup, Transfer>(transferChain)
);
```

Boundaries between different `GroupTag`s do not connect automatically. Declare required transitions (inside a sub-Group or across sub-Groups) at construction time with `jump<From, To>{}`:

```cpp
constexpr auto ROOT = finalize_lli_group(
    make_lli_group(
        mainGroup,
        errorGroup,
        jump<
            list_tail<MainGroup, Transfer>,
            list_entry<ErrorGroup, Stop>
        >{}
    )
);
```

Terminate any specific Node explicitly at construction time with `end_node<Position>{}`:

```cpp
constexpr auto group = make_lli_group(
    make_list<MainGroup, Transfer>(transferChain),
    end_node<node_at<MainGroup, Transfer, 1>>{}
);
```

`end_list()` prevents an automatic List-boundary link. `end_node<Position>{}` sets the `CLLR` of one Node to `0` after list linking. All wiring rules are preserved in the mutable `lli_group` type (with zero runtime size overhead) until `finalize_lli_group()`.

### 6. Reuse and composition

To place the same Group template more than once in one ROOT, assign distinct instance tags with `instantiate<InstanceTag>()`:

```cpp
struct InstanceA;
struct InstanceB;

constexpr auto subGroup = make_lli_group(
    instantiate<InstanceA>(groupTemplate),
    instantiate<InstanceB>(groupTemplate),
    jump<
        list_tail<instanced_tag<InstanceA, OrigGroupTag>, Step>,
        list_entry<instanced_tag<InstanceB, OrigGroupTag>, Step>
    >{}
);
```

The effective Group tag becomes:

```cpp
instanced_tag<InstanceA, OrigGroupTag>
```

`instantiate<InstanceTag>()` takes an un-finalized `lli_group`, rewrites `GroupTag` in both its Lists and its internal `jump` / `end_node` rules to `instanced_tag<InstanceTag, GroupTag>`, and returns a new `lli_group` directly.

Un-finalized Groups can also be combined with `append_lli_group()` before calling `finalize_lli_group()`:

```cpp
constexpr auto ROOT = finalize_lli_group(
    append_lli_group(
        subGroup,
        otherGroup,
        jump<SubGroupExit, OtherGroupEntry>{}
    )
);
```

`append_lli_group()` reconstructs a new `lli_group`, recalculates ROOT-relative links and same-`GroupTag` adjacency, and reapplies inner wiring rules followed by outer wiring rules. Both inputs must be un-finalized `lli_group`s; finalized ROOTs are rejected at compile time.

### 7. Descriptor dispatcher

The dispatcher uses a selector/executor pair:

```text
selector → executor → following List
```

For v2.0.0-beta.3, the supported executor payload is:

```cpp
DESC_12BSD_t
```

Define stable named positions for the dispatcher List:

```cpp
namespace SpiSequence
{
    struct Group;
    struct Dispatch;

    using DispatchSelector = list_entry<Group, Dispatch>;
    using DispatchExecutor = list_tail<Group, Dispatch>;
}
```

Create the layout type after the finalized ROOT type is known:

```cpp
using root_t = std::remove_cv_t<decltype(ROOT)>;

using dispatcher_t = descriptor_dispatch_layout<
    SpiSequence::DispatchSelector,
    SpiSequence::DispatchExecutor,
    DESC_12BSD_t,
    root_t
>;
```

`descriptor_dispatch_layout` requires a finalized ROOT (`finalized_lli_group`) and derives selector/executor offsets, the rewrite range, transfer size, and selector-to-executor CLLR from the ROOT type.

Build a static ring in ROM:

```cpp
extern root_t EXECUTION_ROOT;

constinit const dispatcher_t::step_type steps[4] = {
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
    auto root = ROOT;

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

> **Placement rule (one finalized ROOT per 64 KiB section):**
> Every `CLLR` inside a finalized ROOT is computed as an offset starting from `0` at the beginning of that ROOT, and GPDMA `CLBAR` only holds the upper 16 bits (64 KiB boundary). Therefore, place **at most one finalized ROOT** in each 64 KiB-aligned linker section (e.g., `.SRAM2` or `.lli_block_0`) so its base address has lower 16 bits `0x0000`. If you need multiple independent DMA sequences in the same 64 KiB region, combine them into **one** finalized ROOT under distinct `GroupTag`s and select the starting sequence via `load_lli<EntryPosition>()`.

### 9. API selection reference

| Goal | API |
|---|---|
| Build one physical Node Chain | `make_lli()` |
| Label a Chain with Group/List tags | `make_list()` |
| Stop automatic same-GroupTag linking | `end_list()` |
| Build a reusable Group or mutable ROOT | `make_lli_group()` |
| Reuse an un-finalized Group more than once | `instantiate()` |
| Combine two un-finalized Groups | `append_lli_group()` |
| Declare a node jump at build time | `jump<From, To>{}` |
| Terminate one Node at build time | `end_node<Position>{}` |
| Finalize a ROOT for execution | `finalize_lli_group()` |
| Validate a Group or finalized ROOT | `validate_root()` |
| Define descriptor layout | `descriptor_dispatch_layout` |
| Select initial descriptor | `start_at<N>` |
| Load a named entry | `load_lli<Position>()` |

---

## 日本語

### 1. 基本モデル

```text
Group (lli_group: 部品・未確定ROOT / finalized_lli_group: 確定ROOT)
└─ List  [タグ: GroupTag, ListTag]
   └─ Node  (Chain: GPDMA LLIレジスタ像の並び)
```

| 要素 | 意味 | 実行時にROOTへ格納されるもの |
|---|---|---|
| Node | 1個のGPDMA LLIレジスタ像 | Node本体 |
| Chain | `make_lli()`で作る物理Nodeチェーン | Nodeの並びのみ |
| List | `GroupTag`と`ListTag`を付けたChain | Chainのみ |
| Group / ROOT | 平坦化されたLLIイメージ（`lli_group` / `finalized_lli_group`） | Chainの並びのみ |

グループタグ（`GroupTag`）とリストタグ（`ListTag`）はコンパイル時の型情報です。ROOTへ文字列、ポインタ、タグ用フィールドを追加しません。

### 2. Node、Chain、Listを作る

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

`make_lli()`へ複数Nodeを渡すと、宣言順の物理チェーン（`Chain`）になります。

```cpp
constexpr auto gpioChain = make_lli(
    GPIO_HIGH_NODE, // Node 0
    GPIO_LOW_NODE   // Node 1
);
```

`make_list()`でGroupタグとListタグを付けます。

```cpp
struct MainGroup;
struct GpioPulse;

constexpr auto gpioList =
    make_list<MainGroup, GpioPulse>(gpioChain);
```

使用できる形式は次の3つです。

```cpp
make_list<GroupTag, ListTag>(chain); // Group/Listタグを明示
make_list<ListTag>(chain);           // default_group_tagを使用
make_list(chain);                    // 匿名List、直列用途のみ
```

匿名形式は、宣言順どおりに実行し、名前付きentry、ジャンプ先、dispatcher位置、Groupタグ境界を必要としない場合だけ使用します。

### 3. 名前付き位置

物理List番号ではなく論理位置を指定します。

```cpp
using PulseHigh = node_at<MainGroup, GpioPulse, 0>;
using PulseLow  = node_at<MainGroup, GpioPulse, 1>;

using PulseEntry = list_entry<MainGroup, GpioPulse>;
using PulseTail  = list_tail<MainGroup, GpioPulse>;
```

Node番号はROOT全体の通し番号ではなく、指定したList内の0始まり番号です。

公開entryとexitには`list_entry`と`list_tail`を優先します。Listの先頭や末尾へNodeを追加しても位置名を変更せずに済みます。

同一ROOT内では`GroupTag + ListTag`の組み合わせが一意である必要があります。匿名Listは名前付き検索の対象外です。

### 4. Groupと確定ROOT

部品となるサブGroupも全体のROOTも、すべて`make_lli_group()`で作ります。

```cpp
constexpr auto mainGroup = make_lli_group(
    make_list<MainGroup, Setup>(
        make_lli(SETUP_NODE)
    ),
    make_list<MainGroup, Transfer>(
        make_lli(TX_NODE_0, TX_NODE_1)
    )
);
```

部品を組み合わせて配線し、最後に`finalize_lli_group()`で確定します。

```cpp
constexpr auto ROOT = finalize_lli_group(
    make_lli_group(
        mainGroup,
        errorGroup,
        jump<
            list_tail<MainGroup, Transfer>,
            list_entry<ErrorGroup, Stop>
        >{}
    )
);

static_assert(validate_root(ROOT));
```

Group構築時にライブラリが行う処理は次のとおりです。

- Listと未finalizeのサブGroupを1つの`lli_group`へ平坦化
- 全CLLRを先頭基準で再計算
- 同一`GroupTag`を持つ隣接Listを自動接続
- 異なる`GroupTag`の境界を既定で終端
- 内側部品の配線ルールから外側の配線ルール（`jump` / `end_node`）の順に一括適用
- 配置、アラインメント、サイズ、CLLR遷移先を検証

`finalize_lli_group()`を呼ぶと、実行時サイズを増やすことなくコンパイル時の配線ルール型が外れ、`descriptor_dispatch_layout`や`load_lli()`へ渡せる`finalized_lli_group`になります（finalize後の`make_lli_group()` / `append_lli_group()` / `instantiate()`はコンパイル時に拒否されます）。

### 5. 自動接続、ジャンプ、終端

同じ`GroupTag`を持つ隣接Listは、前のListへ`end_list()`を指定しない限り自動接続されます。

```cpp
constexpr auto group = make_lli_group(
    end_list(make_list<MainGroup, Setup>(setupChain)),
    make_list<MainGroup, Transfer>(transferChain)
);
```

異なる`GroupTag`の境界は自動接続されません。サブGroup内部のループやGroup間の遷移は、構築時に`jump<From, To>{}`で宣言します。

```cpp
constexpr auto ROOT = finalize_lli_group(
    make_lli_group(
        mainGroup,
        errorGroup,
        jump<
            list_tail<MainGroup, Transfer>,
            list_entry<ErrorGroup, Stop>
        >{}
    )
);
```

任意のNodeを終端へ変更する場合は構築時に`end_node<Position>{}`を指定します。

```cpp
constexpr auto group = make_lli_group(
    make_list<MainGroup, Transfer>(transferChain),
    end_node<node_at<MainGroup, Transfer, 1>>{}
);
```

`end_list()`はList境界の自動接続を抑止します。`end_node<Position>{}`はList接続適用後に指定Nodeの`CLLR`を`0`（終端）へ設定します。これらの配線ルールは`finalize_lli_group()`まで型パラメータとして保持され、実行時サイズは増えません。

### 6. 再利用と結合

同じGroupテンプレートを1つのROOT内で複数回使う場合は、`instantiate<InstanceTag>()`で異なるインスタンスタグを付けます。

```cpp
struct InstanceA;
struct InstanceB;

constexpr auto subGroup = make_lli_group(
    instantiate<InstanceA>(groupTemplate),
    instantiate<InstanceB>(groupTemplate),
    jump<
        list_tail<instanced_tag<InstanceA, OrigGroupTag>, Step>,
        list_entry<instanced_tag<InstanceB, OrigGroupTag>, Step>
    >{}
);
```

実効Groupタグは次になります。

```cpp
instanced_tag<InstanceA, OrigGroupTag>
```

`instantiate<InstanceTag>()`は未finalizeの`lli_group`を受け取り、各Listおよび内部`jump` / `end_node`ルールの`GroupTag`を`instanced_tag<InstanceTag, GroupTag>`へ自動変換した新しい`lli_group`を返します。

未finalizeのGroup同士は、内部配線ルールを保持したまま`append_lli_group()`で結合し、その後に`finalize_lli_group()`で確定できます。

```cpp
constexpr auto ROOT = finalize_lli_group(
    append_lli_group(
        subGroup,
        otherGroup,
        jump<SubGroupExit, OtherGroupEntry>{}
    )
);
```

`append_lli_group()`は生バイト列を連結するのではなく、新しい`lli_group`を再構築します。ROOT相対リンクと同一`GroupTag`の隣接接続を再計算したあと、内側の配線ルール→外側の配線ルールの順に再適用します。両方とも未finalizeの`lli_group`である必要があり、finalize済みのROOTは渡せません。

### 7. Descriptor dispatcher

dispatcherはselector/executorの2段で動作します。

```text
selector → executor → 後続List
```

v2.0.0-beta.3時点で対応するexecutor payloadは次です。

```cpp
DESC_12BSD_t
```

dispatcher Listには安定した名前付き位置を定義します。

```cpp
namespace SpiSequence
{
    struct Group;
    struct Dispatch;

    using DispatchSelector = list_entry<Group, Dispatch>;
    using DispatchExecutor = list_tail<Group, Dispatch>;
}
```

finalize済みROOT型が確定した後でlayout型を作ります。

```cpp
using root_t = std::remove_cv_t<decltype(ROOT)>;

using dispatcher_t = descriptor_dispatch_layout<
    SpiSequence::DispatchSelector,
    SpiSequence::DispatchExecutor,
    DESC_12BSD_t,
    root_t
>;
```

`descriptor_dispatch_layout`はfinalize済みROOT（`finalized_lli_group`）を要求し、ROOT型からselector/executorの位置、書き換え範囲、転送サイズ、selectorからexecutorへのCLLRを導出します。

ROM上の静的ringは次のように作ります。

```cpp
extern root_t EXECUTION_ROOT;

constinit const dispatcher_t::step_type steps[4] = {
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
    auto root = ROOT;

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

> **配置ルール（1つの 64 KB セクションにつき 1 確定 ROOT）:**
> 確定 ROOT（`finalized_lli_group`）内の全 `CLLR` はその ROOT 先頭を `0` としたオフセットで計算され、GPDMA の `CLBAR` は上位 16 bit（64 KB 境界）のみを保持します。そのため、64 KB アラインされた 1 つのセクション（`.SRAM2` や `.lli_block_0` 等）に配置できる確定 ROOT は **1 つだけ**（先頭アドレスの下位 16 bit が `0x0000`）です。同じ 64 KB 領域内に複数の独立したシーケンスを同居させたい場合は、確定 ROOT を 2 つ置くのではなく、**1 つの確定 ROOT 内に別々の `GroupTag` でまとめて `load_lli<開始位置>()` で切り替えてください**。

### 9. API早見表

| 目的 | API |
|---|---|
| 1つの物理Nodeチェーンを作る | `make_lli()` |
| チェーンへGroup/Listタグを付ける | `make_list()` |
| 同一GroupTag内の自動接続を止める | `end_list()` |
| 再利用可能なGroup・未確定ROOTを作る | `make_lli_group()` |
| 未finalizeのGroupを複数回使う | `instantiate()` |
| 未finalizeのGroup同士を結合する | `append_lli_group()` |
| 構築時にジャンプ配線を宣言する | `jump<From, To>{}` |
| 構築時にNode終端を宣言する | `end_node<Position>{}` |
| ROOTを実行可能状態へ確定する | `finalize_lli_group()` |
| Group・確定ROOTを検証する | `validate_root()` |
| descriptor layoutを定義する | `descriptor_dispatch_layout` |
| 初期descriptorを選ぶ | `start_at<N>` |
| 名前付きentryをロードする | `load_lli<Position>()` |
