# Wi-SUN ARIB STD-T108 Sniffer

このプロジェクトは、日本向け Wi-SUN 仕様である ARIB STD-T108 を対象にした Sniffer の試作実装です。USRP から IQ サンプルを取得し、10 MHz 幅の入力信号を複数チャネルへ分割し、バースト検出、FSK/GFSK 復調、簡易的な PHY 解析結果の表示までを行います。

## 現在の実装範囲

現状のコードベースには以下が含まれています。

- UHD を用いた USRP 受信ストリーミング
- FFTW を用いた 50 チャネルの polyphase channelizer
- CFAR によるバースト検出とバーストグルーピング
- FSK/GFSK 復調
- preamble、SFD、PHR、payload の簡易解析
- channelizer と modem loopback のセルフテスト

現時点では、Wi-SUN / IEEE 802.15.4g の完全なデコーダではなく、PHY レベル中心の観測・検証用途の実装です。

<img src="./assets/capture.gif" alt="capture">

## 処理パイプライン

実行時には主に以下の 3 つのワーカースレッドが動作します。

1. `usrp_rx_thread`
   USRP から 10 Msps で複素ベースバンドサンプルを受信します。
2. `channelizer_thread`
   入力帯域を 50 チャネルへ分割し、CFAR によるバースト検出を行い、確定したバーストを後段へ渡します。
3. `demodulator_thread`
   バースト候補を復調し、復元したビット列を解析します。

全体アーキテクチャは以下の通りです。

<img src="./assets/architecture.png" alt="architecture">

## リポジトリ構成

- `main.c`: エントリポイント、CLI オプション、スレッド起動
- `usrp.c`: UHD デバイス初期化とストリーミング処理
- `channelizer.c`: polyphase channelizer、CFAR、バーストグルーピング
- `channelizer_test.c`: 組み込みセルフテスト
- `demodulator.c`: バースト復調処理
- `fsk.c`: FSK/GFSK 関連処理
- `packet.c`: パケット簡易解析と payload 出力
- `brb.c`: channelizer から demodulator への blocking ring buffer
- `lfrb.c`: USRP RX から channelizer への lock-free ring buffer
- `build.sh`: 簡易ビルドスクリプト
- `CMakeLists.txt`: CMake ビルド定義

## 必要環境

### ハードウェア

- MacBook Pro (M1 Pro, 2021, 32GB RAM)
- USRP B210 / LibreSDR B220mini (USRP Clone)
- DIAMOND SRH805S 小型ハンディーアンテナ

上記で動作確認済みです。

### 依存関係

- CMake
- C99 対応の C コンパイラ
- UHD
- FFTW3
- POSIX threads

### macOS での例

```bash
brew install cmake pkgconf uhd fftw
```

## ビルド方法

### Release ビルド

```bash
./build.sh
```

### Debug ビルド

```bash
./build.sh debug
```

生成される実行ファイルは以下です。

```bash
./build/wisun-sniffer
```

## コマンドラインオプション

```text
-a  受信用アンテナ名
-c  受信チャネル番号
-f  受信周波数 [Hz]
-g  受信ゲイン
-w  PCAP 出力先のファイルまたは FIFO
-k  -w 使用時に FCS 不一致フレームも出力
-m  指定チャネルで FSK/GFSK modem loopback self-test を実行して終了
-t  channelizer の single-tone self-test を実行して終了
-h  ヘルプ表示
```

## 実行方法

### 1. Channelizer のセルフテスト

```bash
./build/wisun-sniffer -t
```

合成した単一トーン信号を使って channelizer の動作確認を行い、USRP にはアクセスせず終了します。

### 2. Modem loopback のセルフテスト

```bash
./build/wisun-sniffer -m 10
```

指定チャネル向けの合成変調信号を生成し、channelizer と demodulator の経路を通して pass/fail を確認します。

### 3. USRP でライブ受信する場合

```bash
./build/wisun-sniffer
```

実行時には主に以下のような情報が標準出力へ表示されます。

- 検出したバーストのチャネル情報
- demodulator の処理状況
- SFD の一致結果
- frame length、data whitening、FCS type などの PHR 情報
- payload の 16 進表示

停止は `Ctrl+C` です。

### Wireshark へリアルタイム入力する場合

FIFO を作成し、先に Wireshark で読み込みを開始してから Sniffer を起動します。

```bash
mkfifo /tmp/wisun.pcap
wireshark -k -i /tmp/wisun.pcap
```

別のターミナルで受信を開始します。

```bash
./build/wisun-sniffer -w /tmp/wisun.pcap
```

`-w` は通常の PCAP ファイルにも指定できます。Wireshark でリアルタイム表示する場合は FIFO を使ってください。出力は IEEE 802.15.4 no-FCS (linktype 230) で、FCS を検証してから取り除きます。FCS 不一致フレームは既定で除外され、`-k` を追加すると不一致でも FCS を除去して出力します。通常の解析ログは標準出力に引き続き表示されます。

## ライブ受信時の注意

- 現在の実装は UHD で利用可能な USRP と 10 Msps の受信を前提としています。
- デフォルトの周波数設定は日本向け Wi-SUN 観測を意識したものです。実際の環境に応じて周波数、アンテナ、ゲイン、チャネルを調整してください。
- パケット解析は現状 PHY レベルの情報表示と payload のダンプが中心であり、上位層の完全な解析は未実装です。

## 既知の制限

- README の実行例は、UHD デバイスが標準設定で検出できる前提です。
- TX 関連コードはテスト用途のみで、通常ビルドでは無効です。
- `install` ターゲットやパッケージ化は未整備です。

## 今後の改善候補

- ARIB STD-T108 / IEEE 802.15.4g のより詳細なデコード
