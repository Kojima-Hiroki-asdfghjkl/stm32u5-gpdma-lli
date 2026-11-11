# STM32U575 Self-Modifying GPDMA Descriptor Dispatch

[English](#english) | [日本語](#日本語)

> **Status: v2.0.0-alpha.1 development record**
>
> This directory preserves the hardware-verified prototype used to explore
> self-modifying descriptor dispatch for the v2 design. It is not a stable API,
> a minimal example, or a reusable board-support package.

---

# English

## Overview

This experiment demonstrates a self-modifying STM32U575 GPDMA linked-list
sequence built on top of the v1.1.0 infrastructure.

After the CPU loads the entry node and enables GPDMA1 Channel 0, GPDMA performs
the following sequence without further CPU descriptor selection:

1. Reads one descriptor from a descriptor ring.
2. Rewrites the selector state for the next descriptor.
3. Rewrites the executor node with the selected SPI transfer.
4. Executes the SPI transfer.
5. Disables SPI after the transfer completes.
6. Updates the GPIO control signal.
7. Returns to the selector and repeats.

The hardware-verified descriptor sequence is:

| Descriptor | Source offset | Length | Observed MOSI data |
|---:|---:|---:|---|
| 0 | `TEST_DATA + 10` | 10 bytes | `10-19` |
| 1 | `TEST_DATA + 0` | 10 bytes | `0-9` |
| 2 | `TEST_DATA + 40` | 10 bytes | `40-49` |
| 3 | `TEST_DATA + 12` | 10 bytes | `12-21` |

After descriptor 3, the ring returns to descriptor 0.

## Design objective

The long-term design objective of the library is:

- strict structural type safety;
- fully static initialization of GPDMA node graphs;
- no runtime queue construction;
- no runtime node linking;
- runtime execution limited to loading the entry node and enabling DMA.

The alpha.1 prototype still performs two startup assignments for the selector's
`CSAR` and `CDAR`. These assignments are expected to be replaced by link-time
relocation in a later v2 revision.

Copying the completed `.SRAM2` load image from Flash to its SRAM2 execution
address is treated as image loading, not runtime graph construction.

## Prototype structure

The selector transfers exactly 32 bytes from the active descriptor into the
SRAM2 ROOT image:

| Descriptor field | Destination |
|---|---|
| `nextSelectorSource` | `selector.CSAR` |
| `nextSelectorDestination` | `selector.CDAR` |
| `nextSelectorLink` | `selector.CLLR` |
| `executor.CTR1` | `executor.CTR1` |
| `executor.CTR2` | `executor.CTR2` |
| `executor.CBR1` | `executor.CBR1` |
| `executor.CSAR` | `executor.CSAR` |
| `executor.CDAR` | `executor.CDAR` |

`executor.CLLR` is deliberately outside the rewrite window. The executor
therefore retains its statically generated link to the following node.

The implementation checks the relevant object sizes, offsets, alignment, and
rewrite-window size with `static_assert`.

## Verified platform configuration

The captured result was produced with the following configuration:

- MCU: STM32U575;
- GPDMA: GPDMA1 Channel 0;
- peripheral: SPI1 in unidirectional transmit mode;
- SPI1 kernel clock: HSI16;
- system clock: 80 MHz;
- SPI1 SCK: PA5;
- SPI1 MOSI: PA7;
- sequence-control output shown in the capture: PC6;
- ROOT execution image: SRAM2;
- initial ROOT image: loaded from Flash at startup.

`original_init.hpp` preserves the initialization code from the original test
application. It contains initialization unrelated to this GPDMA experiment and
is included to preserve the actual hardware-validation environment. It is not a
minimal initialization example or a reusable platform layer.

## Header snapshot

The files under `include/` are the exact header snapshot used for the
v2.0.0-alpha.1 hardware validation:

- `gpdma_lli.hpp`
- `gpdma_lli_t.hpp`
- `stm32u575_spi_t.hpp`

The snapshot is based on v1.1.0 and contains experimental additions required by
this prototype, including `DESC_12BSD_t` and the STM32U575 SPI-specific type
support.

These files are development-record dependencies. They are not the stable public
headers and are not intended to be installed together with the repository-level
headers.

## Logic capture

The `capture/` directory contains the measurement data used to verify the
prototype.

### Files

- `capture/v2_0_0_alpha_capture.sal`  
  Original Saleae Logic 2 capture. Open this file in Logic 2 to inspect timing,
  zoom into individual transfers, and change the analyzer presentation.

- `capture/v2_0_0_alpha_capture.csv`  
  Exported SPI analyzer data. It records the decoded transfer data and is useful
  for text inspection, scripting, and checking the repeated descriptor order.

- `capture/v2_0_0_alpha_capture.png`  
  Representative Logic 2 screenshot. The waveform pane shows one 10-byte SPI
  transfer while PC6 is low. The Data table on the right shows the wider cyclic
  descriptor sequence.

### Signals visible in the screenshot

| Logic 2 row | Signal | Purpose |
|---|---|---|
| D1 / RB2 | SPI clock | SPI1 SCK |
| D0 / RB3 | SPI MOSI | SPI1 transmitted data |
| D3 / PC6 | Control output | Active-low transfer window shown in the capture |

The other displayed channels are not used to validate this experiment.

### What the capture proves

The analyzer table shows the following decoded MOSI sequence:

```text
10 11 12 13 14 15 16 17 18 19
0  1  2  3  4  5  6  7  8  9
40 41 42 43 44 45 46 47 48 49
12 13 14 15 16 17 18 19 20 21
10 11 12 ...
```

This confirms all of the following:

- each executor transfer sends 10 bytes;
- descriptor order is preserved;
- descriptor 3 links back to descriptor 0;
- the selector advances the descriptor ring;
- the executor node is rewritten successfully;
- the sequence continues after startup without CPU descriptor selection.

The screenshot displays one transfer in detail in the waveform pane. The full
cycle is visible in the Logic 2 Data table and in the CSV export.

## Important hardware-specific limitation

This prototype uses an observed SPI RX DMA request near TXC as the completion
request for the SPI-disable node.

On the tested STM32U575 configuration, one SPI RX DMA request was observed near
the completion of the first unidirectional transmission after enabling SPI.
This behavior is not documented or guaranteed by the reference manual or data
sheet.

Revalidate this behavior when changing any of the following:

- MCU or silicon revision;
- SPI direction or FIFO configuration;
- SPI initialization sequence;
- DMA request configuration;
- clock configuration.

## Alpha.1 limitations

- The implementation is layered on top of the v1.1.0 node/group infrastructure.
- The descriptor-dispatch API is not final.
- `DESC_12BSD_t` is an experimental descriptor type used by this prototype.
- Selector `CSAR` and `CDAR` are connected with two startup assignments.
- `.SRAM2` loading temporarily uses `constructor(101)`.
- The platform initialization is preserved from the original application and
  is not minimal.
- The SPI completion mechanism depends on undocumented observed behavior.

## Directory contents

```text
stm32u575_spi_descriptor_dispatch/
├── README.md
├── v2_0_0_alpha1_user_main.cpp
├── original_init.hpp
├── include/
│   ├── gpdma_lli.hpp
│   ├── gpdma_lli_t.hpp
│   └── stm32u575_spi_t.hpp
└── capture/
    ├── v2_0_0_alpha_capture.sal
    ├── v2_0_0_alpha_capture.csv
    └── v2_0_0_alpha_capture.png
```

This directory is a development record. The implementation and duplicated
header snapshot may be removed from the latest branch after the behavior is
incorporated into the final v2 API. The files remain available from the
`v2.0.0-alpha.1` tag and Git history.

---

# 日本語

## 概要

このディレクトリは、v1.1.0の基盤上で構築したSTM32U575 GPDMAの
自己書き換え型descriptor dispatchについて、実機で成立を確認した時点の
開発記録です。

安定版API、最小実行例、再利用可能なBSPとして公開するものではありません。
v2設計へ取り込む機構の技術検証と、実機確認時のコードおよび測定結果を保存する
ことを目的としています。

CPUが開始ノードをGPDMA1 Channel 0へロードしてチャネルを有効化した後は、
CPUがdescriptorを選択することなく、GPDMAが以下の処理を繰り返します。

1. descriptor ringからdescriptorを1件読み込む。
2. selectorを次のdescriptorへ進める。
3. 選択されたSPI転送内容でexecutorノードを書き換える。
4. SPI転送を実行する。
5. 転送完了後にSPIを無効化する。
6. GPIO制御信号を更新する。
7. selectorへ戻って次のdescriptorを処理する。

実機で確認したdescriptorの順序は以下です。

| Descriptor | 送信元オフセット | 長さ | 観測したMOSIデータ |
|---:|---:|---:|---|
| 0 | `TEST_DATA + 10` | 10バイト | `10-19` |
| 1 | `TEST_DATA + 0` | 10バイト | `0-9` |
| 2 | `TEST_DATA + 40` | 10バイト | `40-49` |
| 3 | `TEST_DATA + 12` | 10バイト | `12-21` |

Descriptor 3の実行後はdescriptor 0へ戻ります。

## 設計目標

このライブラリが最終的に目指す設計方針は以下です。

- 構造に対する徹底した型安全性
- GPDMAノードグラフの徹底した静的初期化
- 実行時のキュー構築を行わない
- 実行時のノード接続を行わない
- 実行時処理を開始ノードのロードとDMA有効化だけにする

alpha.1では、selectorの`CSAR`と`CDAR`に対する2回の代入が起動時に残っています。
以降のv2開発では、これらもリンカリロケーションにより解決し、実行時パッチを
廃止する予定です。

Flash上の完成済み`.SRAM2`ロードイメージをSRAM2の実行アドレスへコピーする
処理は、ノードグラフの実行時構築ではなく、完成済みイメージのロードとして
扱います。

## プロトタイプの構造

selectorは、現在のdescriptorからSRAM2上のROOTイメージへ正確に32バイトを
転送します。

| Descriptor側 | 書き込み先 |
|---|---|
| `nextSelectorSource` | `selector.CSAR` |
| `nextSelectorDestination` | `selector.CDAR` |
| `nextSelectorLink` | `selector.CLLR` |
| `executor.CTR1` | `executor.CTR1` |
| `executor.CTR2` | `executor.CTR2` |
| `executor.CBR1` | `executor.CBR1` |
| `executor.CSAR` | `executor.CSAR` |
| `executor.CDAR` | `executor.CDAR` |

`executor.CLLR`は意図的に書き換え範囲の外にあります。このためexecutorは、
静的に生成された後続ノードへのリンクを保持します。

関連するオブジェクトサイズ、オフセット、アラインメント、書き換え範囲は
`static_assert`で検証しています。

## 実機確認時の構成

波形を取得した際の構成は以下です。

- MCU: STM32U575
- GPDMA: GPDMA1 Channel 0
- 周辺機器: SPI1、単方向送信モード
- SPI1カーネルクロック: HSI16
- システムクロック: 80 MHz
- SPI1 SCK: PA5
- SPI1 MOSI: PA7
- 波形に表示したシーケンス制御出力: PC6
- ROOT実行領域: SRAM2
- ROOT初期イメージ: 起動時にFlashからロード

`original_init.hpp`には、実機確認に使用した元プロジェクトの初期化コードを
保存しています。GPDMA実験に関係しない初期化も含まれますが、実際の検証環境を
記録する目的で収録しています。最小初期化例や再利用可能なplatform layerでは
ありません。

## ヘッダースナップショット

`include/`以下には、v2.0.0-alpha.1の実機検証で使用したヘッダーを保存しています。

- `gpdma_lli.hpp`
- `gpdma_lli_t.hpp`
- `stm32u575_spi_t.hpp`

これらはv1.1.0を基礎として、`DESC_12BSD_t`やSTM32U575 SPI用の型など、
本プロトタイプに必要な実験的追加を含んでいます。

このヘッダー群は開発記録用の依存ファイルです。リポジトリ直下の安定版ヘッダーを
置き換えるものではなく、安定版ヘッダーと同時にインストールすることも意図して
いません。

## Logic capture

`capture/`ディレクトリには、このプロトタイプの確認に使用した測定データを
保存しています。

### ファイル

- `capture/v2_0_0_alpha_capture.sal`  
  Saleae Logic 2の元captureです。Logic 2で開くと、個々の転送の拡大、タイミング
  確認、Analyzer表示の変更ができます。

- `capture/v2_0_0_alpha_capture.csv`  
  SPI Analyzerのエクスポート結果です。デコードされた転送データを記録しており、
  テキスト確認、スクリプト処理、descriptor循環順序の確認に使用できます。

- `capture/v2_0_0_alpha_capture.png`  
  Logic 2の代表スクリーンショットです。左側の波形にはPC6がLowの期間に行われる
  10バイトのSPI転送を1件表示しています。右側のDataテーブルには、より広い範囲の
  descriptor循環が表示されています。

### スクリーンショットに表示されている信号

| Logic 2上の行 | 信号 | 用途 |
|---|---|---|
| D1 / RB2 | SPI clock | SPI1 SCK |
| D0 / RB3 | SPI MOSI | SPI1送信データ |
| D3 / PC6 | 制御出力 | capture上で確認するActive-Lowの転送期間 |

表示されているその他のチャンネルは、この実験の確認には使用していません。

### Captureから確認できること

右側のAnalyzerテーブルには、以下のMOSIデータが順番に表示されています。

```text
10 11 12 13 14 15 16 17 18 19
0  1  2  3  4  5  6  7  8  9
40 41 42 43 44 45 46 47 48 49
12 13 14 15 16 17 18 19 20 21
10 11 12 ...
```

この結果から、以下を確認できます。

- 各executor転送が10バイトである
- descriptorの順序が維持されている
- descriptor 3からdescriptor 0へ戻っている
- selectorがdescriptor ringを前進させている
- executorノードの自己書き換えが成立している
- GPDMA開始後、CPUによるdescriptor選択なしで処理が継続している

スクリーンショット左側の波形表示は1回分の転送を拡大して表示しています。
1周期全体は右側のLogic 2 DataテーブルとCSVで確認できます。

## ハードウェア固有の重要な制限

このプロトタイプでは、SPI送信完了後にSPIを無効化する要求として、TXC付近で
観測されたSPI RX DMA要求を利用しています。

検証したSTM32U575の設定では、SPIを有効化した直後の最初の単方向送信完了時に、
SPI RX DMA要求が1回発生することを実機で確認しました。この挙動はRMまたは
データシートに記載された保証動作ではありません。

以下を変更した場合は再検証が必要です。

- MCUまたはシリコンリビジョン
- SPIの方向またはFIFO設定
- SPIの初期化順序
- DMA request設定
- クロック設定

## alpha.1の既知の制限

- v1.1.0のnode/group基盤上に増築した実装です。
- descriptor dispatchの公開APIは未確定です。
- `DESC_12BSD_t`は本プロトタイプで使用する実験的descriptor型です。
- selectorの`CSAR`と`CDAR`を起動時の2代入で接続します。
- `.SRAM2`のロードには暫定的に`constructor(101)`を使用します。
- platform初期化は元のアプリケーションから保存したもので、最小構成ではありません。
- SPI完了処理は、仕様書に記載のない実測挙動に依存します。

## ディレクトリ構成

```text
stm32u575_spi_descriptor_dispatch/
├── README.md
├── v2_0_0_alpha1_user_main.cpp
├── original_init.hpp
├── include/
│   ├── gpdma_lli.hpp
│   ├── gpdma_lli_t.hpp
│   └── stm32u575_spi_t.hpp
└── capture/
    ├── v2_0_0_alpha_capture.sal
    ├── v2_0_0_alpha_capture.csv
    └── v2_0_0_alpha_capture.png
```

このディレクトリは開発記録です。最終的なv2 APIへ機能を統合した後、実験コードや
重複したヘッダースナップショットを最新ブランチから削除する場合があります。
削除後も、`v2.0.0-alpha.1`タグおよびGit履歴から、この時点のファイルを参照できます。
