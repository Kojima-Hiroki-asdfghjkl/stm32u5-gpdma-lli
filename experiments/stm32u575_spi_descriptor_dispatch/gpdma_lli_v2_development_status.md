# GPDMA LLI v2.0.0-beta.2 開発状況スナップショット

## 1. この文書の位置付け

この文書は、`gpdma_lli.hpp` v2.0.0-beta.2公開時点の実装状況、検証結果、
およびRCまでの残作業を固定したスナップショットである。beta.2公開後は
更新せず、それ以後の公開リリース状態は`README_v2.0.0-beta.2.md`を正本とする。

APIの使用方法は`gpdma_lli_v2_program_list_guide.md`、v2系全体の詳細な設計根拠と
将来構想は`gpdma_lli_v2_design_decisions.md`を参照する。この文書に記載された
「現在」および「残作業」は、すべてbeta.2スナップショット作成時点の意味である。

---

## 2. 現在の到達点

従来は実行時に行っていたdescriptor selectorの`CSAR`/`CDAR`接続を廃止し、絶対アドレスを含む初期化式を静的初期化として成立させ、ELFリロケーションとリンカによってROOTイメージを完成できるようになった。

null LLI0から開始する現在の実機サンプルでは、実行時処理は概ね次のとおりである。

```cpp
channel.CCR |= GPDMA_RESET;
channel.CBR1 = 0; // CBR1.BNDT = 0として内部LLI0をnull転送にする

load_lli<Entry>(channel, root);

channel.CCR |= GPDMA_ENABLE;
```

v2.0.0で特に重要だった次の要素は成立している。

- Program/List/Nodeの名前付き位置指定
- descriptor dispatcherの物理レイアウト自動導出
- descriptor ringの`constinit`化
- selectorの`CSAR`/`CDAR`の静的完成
- SRAM2上の書換え可能ROOTの実行
- Reset HandlerによるROMロードイメージの展開
- 実行時selector接続処理なしでの実機動作

---

## 3. 実装済み機能

### 3.1 名前付きProgram/List/Node

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

これにより、Listを追加または並べ替えた場合でも、利用側で物理チェーン番号を数え直す必要がない。

`Program`、`InitialDataLength`、`Dispatch`などは、実体を生成せずに使用するコンパイル時の型タグである。これらの型はProgram/Listを一意に識別するためだけに使用され、ROOTのメモリサイズには影響しない。

### 3.2 論理位置から物理位置への解決

Program/List/Nodeは公開API上の論理階層であり、物理格納形式には既存の`lli_group`、`lli_list`、`LLI_Node`を引き続き使用する。ProgramやListを表す新たな実体オブジェクトがROOTへ追加されるわけではない。

`node_at<ProgramTag, ListTag, NodeIndex>`から、内部で次の情報を解決する。

- ROOT内のList番号
- List内のNode番号
- ROOT先頭からのオフセット
- 対象LLI型
- 対象LLI型に対応するCLLR更新フラグ

この処理には、既存の次の仕組みを利用している。

- `group_chain_offset()`
- `chain_node_offset()`
- `chain_node_type`
- `start_cllr()`
- `patch_jump()`
- `validate_root()`

### 3.3 descriptor stepのテンプレート化

固定型の`DescStep_12BSD_t`ではなく、executor image型を引数に取る構造へ変更した。

```cpp
template<typename ExecutorImage>
struct descriptor_step;
```

v2.0.0の正式サポート対象は`DESC_12BSD_t`に限定する。

現在のstepは次の32バイトで構成される。

```text
next selector CSAR    4 bytes
next selector CDAR    4 bytes
next selector CLLR    4 bytes
executor image       20 bytes
-----------------------------
total                32 bytes
```

executorの`CLLR`は転送範囲外に置き、ROOT側の固定リンクを保持する。

### 3.4 dispatcherレイアウトの自動導出

`descriptor_dispatch_layout`が、ROOT型とselector/executor位置から次の値を自動導出する。

- selectorのROOT内オフセット
- executorのROOT内オフセット
- descriptor転送先の開始位置
- descriptor転送範囲
- selectorからexecutorへのCLLR
- descriptor step型

このため、利用側で次の物理値を定義する必要がなくなった。

```text
DESCRIPTOR_CHAIN_OFFSET
SELECTOR_OFFSET
EXECUTOR_OFFSET
DESCRIPTOR_PATCH_BEGIN
DESCRIPTOR_PATCH_END
SELECTOR_TO_EXECUTOR_CLLR
```

レイアウト不一致は`static_assert`で検出する。

### 3.5 descriptor ringの静的初期化

descriptor ringは`constinit`で構築する。

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

これにより、次の情報が静的イメージとして完成する。

- 次に読み込むdescriptor
- selectorの書換え先
- selectorからexecutorへのリンク
- executorへ転送するレジスタ像

### 3.6 selectorの静的接続

以前は、起動後にselectorの`CSAR`と`CDAR`を接続する必要があった。

現在はROOTの`constinit` initializer内で接続する。

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

絶対アドレスを含む初期化式は静的初期化として受理され、コンパイラがリロケーション付き初期値を生成し、最終アドレスはELFリロケーションとリンカによって完成する。この変更により、実行時の`connectSelectorAddresses()`は不要になった。

### 3.7 descriptor開始位置の指定

selectorが最初に読み込むdescriptorは、`start_at<N>`で指定する。

```cpp
start_at<0>{}
```

`N`を変更することで、`descList[0]`に固定せず、ring内の任意の要素を開始位置に指定できる。

### 3.8 SRAM2ロードイメージ

ROOTはSRAM2上で実行し、ROM上に完成済みのロードイメージを持つ。

```text
ROM load image
    -> Reset_HandlerでSRAM2へコピー
    -> .data/.tdataを初期化
    -> .bss/.tbssをゼロクリア
    -> __libc_init_array()
    -> main()
```

`constructor(101)`を使った暫定コピー処理は廃止済みである。

---

## 4. 実行時APIと責務境界

### 4.1 現在の起動コード

```cpp
GPDMA1_Channel0->CCR |= 0x0000'0002u; // RESET
GPDMA1_Channel0->CBR1 = 0;             // BNDT = 0としてnull LLI0にする

load_lli<SpiGpioProgram::Entry>(
    *GPDMA1_Channel0,
    SPI_GPIO_ROOT
);

GPDMA1_Channel0->CCR |= 0x0000'0001u; // EN
```

### 4.2 `load_lli()`の責務

`load_lli()`の責務は次に限定する。

- `CLBAR`を設定する
- `CLLR`を設定する

### 4.3 呼び出し側の責務

呼び出し側は次を行う。

- チャネルを停止またはRESETする
- 内部レジスタファイルLLI0を準備する
- `CBR1.BNDT = 0`の保証が必要な場合は明示的に設定する
- 必要な`CCR`設定を行う
- チャネルを有効化する

`CCR.RESET`はFIFO、内部状態、`EN`、`SUSP`をリセットするが、`CBR1`、`CSAR`、`CDAR`をゼロには戻さない。

---

## 5. 検証結果

### 5.1 コンパイル時に保証される事項

`constinit`により、ROOTとdescriptor ringの初期化式が定数初期化として成立し、動的初期化へフォールバックしないことをコンパイル時に要求している。アドレス値自体は、ELFリロケーションとリンカによって最終配置に合わせて完成する。

レイアウト、オフセット、転送範囲、対応executor型などの不一致は、`static_assert`またはテンプレート制約で検出する。

### 5.2 静的イメージとして確認済みの事項

Releaseビルドで、ELFの配置と格納値を確認済みである。

- `.gpdma_relocation_probe`はアドレス`0x08000e50`、サイズ16バイトで保持された
- `test_image`はアドレス`0x08000e50`へ配置された
- `csar`には`source`のアドレス`0x08000e58`が格納された
- `cdar`には`destination`のアドレス`0x08000e5c`が格納された
- `source`の初期値は`0x12345678`、`destination`の初期値は`0x00000000`であった

この結果から、同じ静的オブジェクト内のポインタを32bit整数フィールドへ格納し、ELFリロケーションとリンカによって最終アドレスを確定する方式がReleaseビルドで成立することを確認した。

引き続き、ELFまたはオブジェクトファイルでは次を回帰確認の対象とする。

- selectorの`CSAR`/`CDAR`に対するアドレスリロケーション
- descriptor ring内の相互参照
- `.SRAM2`のVMA/LMA
- ROOTのROMロードイメージ
- ROOTおよびdescriptor ring用の動的初期化関数が生成されていないこと
- GCC系ツールチェーンでは、関連する`_GLOBAL__sub_I_*`などが生成されていないこと

なお、Releaseリンク時に`LOAD segment with RWX permissions`警告が発生する。静的リロケーション検証への影響は確認されていないが、リンカスクリプトのメモリ保護設定として別途確認する。

### 5.3 STM32U575実機で確認済みの事項

- SRAM2上の書換え可能ROOT実行
- descriptor ringの循環
- selectorによる次回selector像の更新
- selectorによるexecutor像の更新
- selectorからexecutorへの遷移
- executor終了後の固定CLLR遷移
- selectorの`CSAR`/`CDAR`の静的リロケーション
- 実行時selector接続処理なしでの起動
- Reset HandlerによるSRAM2ロードイメージ展開
- `CBR1.BNDT = 0`によるnull LLI0開始
- 静的リロケーション検証イメージの実機動作
- 電源断後のコールドブート

---

## 6. v2.0.0の正式スコープ

v2.0.0のdescriptor executor imageは`DESC_12BSD_t`に限定する。

更新対象は次のとおりである。

- `CTR1`
- `CTR2`
- `CBR1`
- `CSAR`
- `CDAR`

executorの`CLLR`は固定リンクとして保持する。

v2.0.0では、各descriptorの転送条件が単体で完結し、前回のレジスタ状態へ依存しないことを優先する。

---

## 7. beta.2およびRC判定

この節はbeta.2公開時点の判定内容を記録したものであり、以後更新しない。
現在の公開リリース状態は`README_v2.0.0-beta.2.md`を参照する。

主要機能の実装と主要な回帰確認が完了し、その状態を`v2.0.0-beta.2`として記録した。

beta.2では新しい中核機能を追加せず、beta.1以降に完了したProgram/List API、コンパイル試験、最適化・LTO回帰、ELF検査、実機動作、コールドブートおよび連続動作の結果を固定する。

beta.2公開前に残る必須作業は、次の2項目である。

- README、使用ガイド、設計判断文書を現在の実装と検証結果へ同期する
- beta.2として公開するソース、サンプルおよび検証専用ファイルの範囲を確認する

API凍結、失敗時診断の最終確認、`RWX permissions`警告の判断、API凍結後の最終回帰はRC判定までの作業とする。

### 7.1 完了済みのRC条件

- Listを意味のある連続処理単位へ再編
- Program内の隣接Listを宣言順に自動接続
- `end_list()`によるList末尾の明示終端
- `patch_end<Position>()`による任意Nodeの明示終端
- 同一`(ProgramTag, ListTag)`の重複をコンパイル時に拒否
- 名前付き位置、List自動接続、明示終端、重複拒否のコンパイル試験
- 最適化・LTO条件ごとのビルド、静的初期化および実機回帰
- ELF配置、リロケーション、動的初期化関数不在の確認
- 電源断後のコールドブートおよび30分間の連続動作

確認済みの最適化条件は次のとおりである。

```text
-O0
-Og
-O2
-O3
-Os
-O3 -flto
-Os -flto
```

通常のセクション設定も有効にして確認した。

```text
-ffunction-sections
-fdata-sections
-Wl,--gc-sections
```

### 7.2 beta.2前に残る作業

- README、使用ガイド、設計判断文書を現在の実装と検証結果へ同期する
- `Src/gpdma_static_relocation_probe.cpp`を含む、beta.2へ収録するソース構成を確認する

`Src/gpdma_static_relocation_probe.cpp`は検証専用コードであり、通常のライブラリ利用に必要な機能ではない。beta.2では開発時の検証資料として残せるが、最終リリース成果物へ含めるかはRC前に確定する。

### 7.3 RC前に残る確認

- Program/List公開APIの名称と責務を凍結する
- 不正な`list_tail`など、公開APIの失敗時診断を確認し、必要なら改善する
- API凍結後に負のコンパイル試験を最終確認する
- `Src/gpdma_static_relocation_probe.cpp`を最終リリースへ含めるか、検証専用に分離するかを確定する
- Releaseリンク時の`LOAD segment with RWX permissions`警告を調査し、許容または修正を判断する
- API凍結後に代表的なReleaseビルド、ELF検査、実機動作、コールドブートを再実行する

これらは新しい中核機能の実装ではなく、API凍結、リリース構成、診断および最終回帰の作業である。すべてを確認できた時点を`v2.0.0-rc.1`の判定点とする。

---

## 8. v2.1.0以降の候補

- `BSD_t`などの部分更新executor
- selector自身の次回`CSAR`の状態依存変更
- descriptor tableまたは転送Listの動的選択
- executorの`CLLR`のdescriptorごとの更新
- descriptorごとのProgram/List/Node遷移先
- GC9D01を使った動的SPIシーケンスのサンプル

2Dアドレッシングは、さらに後のMinor release候補とする。

- `CTR3`
- `CBR2`
- `UT3`
- `UB2`
- 2D executor image

既存v2 APIを壊さず対応型を追加できる限り、v3ではなくv2.xのMinor releaseとして扱う。

---

## 9. 環境依存と非目標

現在の実装は、STM32の32bitアドレス空間を前提とする。

現時点の対象はSTM32U575であり、別アドレス幅や別プラットフォームへ対応するためのtraitsまたはpolicyは追加しない。必要性が明確になるまでは、過度な一般化を避ける。

---

## 10. 現在地点チェックリスト

```text
[x] Program/List/Nodeの名前付き位置
[x] node_at<ProgramTag, ListTag, NodeIndex>
[x] descriptor_stepのテンプレート化
[x] v2.0.0はDESC_12BSD_tに限定
[x] descriptor patch範囲の自動計算
[x] selector -> executor CLLRの自動生成
[x] descriptor ringのconstinit化（定数初期化の成立と動的初期化へのフォールバック防止）
[x] selector.CSAR/CDARの静的リロケーション
[x] 実行時connectSelectorAddresses()の削除
[x] Reset HandlerによるSRAM2展開
[x] 実機動作
[x] 最適化/LTO回帰試験
[x] ELF配置・リロケーション・静的初期化の確認
[x] 電源断後のコールドブート
[x] Listの意味単位への再編
[x] List間の標準自動接続
[x] end_list()によるList末尾の明示終端
[x] patch_end<Position>()による任意Nodeの明示終端
[x] ProgramTag/ListTag重複検査
[x] 30分間の連続動作
[ ] beta.2収録ソース構成の確認
[ ] 文書の現状同期
[ ] Program/List公開APIの凍結
[ ] 公開APIの失敗時診断と負のコンパイル試験の最終確認
[ ] 最終リリース用ソース構成の確定
[ ] LOAD segment with RWX permissions警告の判断
[ ] API凍結後の最終回帰
```

中核機能と主要な実機・最適化回帰は完了している。beta.2前に残るのは収録範囲の確認と文書同期であり、API凍結、診断、最終的なリリース構成、`RWX permissions`警告および最終回帰はRC前の作業である。
