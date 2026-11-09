# レインボーパターン更新 検証記録

2026-11-09。対象：ファームウェアv0.3.0、Android APK 0.2.0（versionCode 2）。

## 変更と互換性

既存の「レインボー」をLEDごとに色相をずらす動作へ更新した。速度の下に色相角0〜360 deg・10 deg刻みを追加し、レインボー時だけ操作できるようにした。初期値360 deg、0 degなら全LED同色。先頭から末尾を両端込みで均等に割り当てるため、360 degでは先頭と末尾が同色になる。LEDが1個の場合は色相差を付けない。

BLE/HTTPの状態は9バイト。末尾2バイトはlittle-endianの色相角。旧6/7バイトのコマンドは受信できるが応答は9バイトなので、ファームウェアと操作画面を同時に更新する。Android APK 0.1.0は非対応。更新した[APK 0.2.0](../android/releases/AtomNeoPixel-0.2.0.apk)を使用する。

## 対象と依存関係

- M5Stack Atom Lite / ESP32-PICO-D4、WS2812B最大15個、Grove GPIO32、GRB 800 kbit/s。既存の輝度上限32/255を維持。
- Arduino CLI 1.4.1、固定プロファイル`atom`：ESP32 3.3.6、WiFiManager 2.0.17、Adafruit NeoPixel 1.15.2。
- ホスト試験：MSVC 19.50 / C++17、Node.js 24.22.1。Androidビルド：JDK 21.0.10、SDK Platform 36、Build Tools 37.0.0。Android minSdk 26、targetSdk 36。

## 検証結果

| 項目 | 結果・根拠 |
| --- | --- |
| ホストC++ | `tools/test-led-state.cpp`をビルド・実行。全37角度、個数1〜15、毎分1〜60周、両端・1個、通信形式、不正値拒否、角度のみの変更、末尾消灯、既存パターンを確認 |
| IP点滅の回帰 | `tools/test-ip-address-blink.cpp`のコンパイル時検証が通過 |
| BLE/Wi-Fi画面の模擬試験 | `node tools/test-web-ui.mjs`が通過。全37角度、送受信、読戻し、有効/無効、値保持を確認 |
| ファームウェア | 固定プロファイルでビルド成功。フラッシュ1,794,923バイト（57%）、静的RAM62,848バイト（19%） |
| 書込み・起動 | 実機のパーティション表とビルドの一致を確認し、アプリ領域のみ書込み。ハッシュ検証と通常BLE起動を確認 |
| Windows実機BLE | 全37角度、0/180/360 deg×個数1/4/15×速度1/60、旧形式、不正値拒否、通知、再接続と状態保持を確認 |
| 発色・周期・Wi-Fi接続 | 利用者が実機で確認完了を報告。周期の定量測定値・手法は記録されていない |
| Android APK | パッケージ・版数・SDK・起動Activity・DEX・署名v2/v3・zipalignを検証。同梱HTMLが現行WebUi.hと完全一致 |
| Android署名継続 | 旧0.1.0と署名証明書SHA-256が一致 |
| Android 12 / API 31実機 | 新規インストール、起動、アプリ内サーバー開始、Chrome引継ぎ、0/180/360 degのBLE送受信・読戻し、無効化・値保持、消灯、切断、サービス停止・再起動を確認 |
| スマホ単体 | 利用者がUSBを外し、Wi-Fi・モバイル通信オフ、Bluetoothオンで起動・BLE操作できたことを報告 |

ホストC++のHSV→RGB変換は模擬であり、実際の発色は利用者の実機確認を別の根拠として扱う。MSVCの整数縮小変換警告と、AndroidビルドのJava 8向けオプション廃止予定警告は残るが、ビルド・試験は通過した。

## 再現手順

リポジトリ直下から実行する。出力は作業ツリー外の一時ディレクトリ等へ保存する。

```powershell
node tools/test-web-ui.mjs
arduino-cli compile --profile atom --build-path <出力先> firmware/AtomNeoPixel
```

MSVCの開発者コマンドプロンプトでは次のように実行する。IP点滅試験はmainを持たないためコンパイルのみ行う。

```text
cl /std:c++17 /EHsc /utf-8 tools\test-led-state.cpp /Fe:<出力先.exe> /Fo:<出力先.obj>
<出力先.exe>
cl /std:c++17 /EHsc /utf-8 /constexpr:steps10000000 /c tools\test-ip-address-blink.cpp /Fo:<出力先.obj>
```

書込み・接続は[README](../README.md)、Androidのビルド・署名・操作は[Android手順](../android/README.md)を参照する。実機では0/180/360 deg、個数1/4/15、個数減少時の末尾消灯、他パターンでの色相角無効化、再接続時の読戻しを確認する。

## 配布物の識別

- APK 0.2.0：25,109バイト、SHA-256 `7f3d72a504bacba73b45e8a3e80e584a75817cf7428f6ec5b1bfec2503d67de3`。
- Android署名証明書SHA-256：`0ebe6f2c4b4aa57077f282e33d4a49eb7fce38ac24eb02cdc2f731cad04fdbc3`。
- 実機へ書き込んだアプリバイナリ：1,795,072バイト、SHA-256 `b929c06a6c005e3d06553be2851126f1d310249c7e1df1305cc3bc95b1d1b49a`。再ビルドしたバイナリのハッシュが同じになることは保証しない。

## 未検証事項

- 旧APK導入済み端末からの実際の更新インストール。証明書一致とversionCode増加は検証済み。
- Android 8〜11・13以降の実機、通知からの停止、長時間運転、OSによるバックグラウンド終了後の挙動。
- macOS BLE、VPN、通信断や保存先接続失敗からの実機復旧。

暫定の接続スクリプト、機器選択の画面キャプチャ、テスト実行ファイルとテスト用オブジェクトは片付けた。回帰テストのソースと配布物は保持する。
