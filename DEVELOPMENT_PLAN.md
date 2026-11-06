# M5AtomLiteNeopixel 開発計画

2026-11-06 策定。対象は試作版 `v0.2.0`。

## 目的と対象

M5Stack Atom Lite に接続した外付け WS2812B を、Windows・macOS・Android の Chrome から操作する。近距離では Wi-Fi やインターネットがない状態で初回から BLE 操作できる構成を目指す。設置時はローカル Wi-Fi に接続し、外出先からも操作できるようにする。初版の想定は Atom Lite 1 台、同時に操作する端末は 1 台。GUI には色、明るさ、消灯、固定点灯、点滅、フェード、速度、接続状態を含める。

## 通信方式と起動

| モード | 利点 | 確認が必要な点 |
| --- | --- | --- |
| BLE | 本体の近くでルーターなしに直接操作できる | Chrome の Web Bluetooth はセキュアコンテキストと利用者の接続操作が必要。初回オフラインで GUI を開けるか、3 OS で確認する |
| Wi-Fi | 同じ LAN 内や、VPN で LAN に入った外出先から、同じ操作画面を開ける | Atom Lite が接続できる 2.4 GHz Wi-Fi と、外出先から利用できる VPN が必要 |

両方式を単一ファームウェアに実装し、起動ごとに片方の無線を使う。出荷時の初期値は BLE。起動時に本体ボタン（GPIO39）を押し続けるとモードを切り替えて保存し、通常起動時は保存済みモードを復元する。Wi-Fi モードで設置した機器は、停電からの復旧時にも Wi-Fi で起動する。モードは内蔵 LED（GPIO27）で表示し、外付け LED は起動時に消灯する。起動時の長押しはボタンを放すまで通常時の操作として扱わない。[Atom Lite のピン配置](https://docs.m5stack.com/ja/core/Atom-Lite)

- **BLE**。Atom Lite を GATT サーバーにし、操作の書き込みと状態の読み取り・通知を用意する。GUI の HTML・CSS・JavaScript を外部依存のない形で同梱する。ローカル HTML を Chrome で直接開けるかを先に試す。直接起動できない端末では、端末内の `http://127.0.0.1` で GUI を配信するランチャーを候補とする。Android は補助アプリの導入可否も含め、対応端末での試験結果を見て配布手段を決める。初回ペアリングは本体操作で短時間だけ許可し、暗号化済みの接続で操作できるかも確認する。[Chrome の Web Bluetooth](https://developer.chrome.com/docs/capabilities/bluetooth)・[W3C Secure Contexts](https://w3c.github.io/webappsec-secure-contexts/)・[Chromium のオリジン制約](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/content/browser/bluetooth/web_bluetooth_service_impl.cc)
- **Wi-Fi**。WiFiManager を初期設定と再設定に使い、通常操作用 GUI と API は Atom Lite が同じアドレスから配信する。Wi-Fi 未設定での起動時、または起動後の明示的な長押しで、USB シリアルから一時パスワードの入力を受け付ける。入力後に設定用 AP を時間制限付きで開く。通信断からの自動再接続は保存済み設定で行う。LAN でのアクセス先はルーター側の DHCP 予約を候補にする。外出先では VPN 対応ルーター、または LAN 内の常時稼働機器を経由し、許可した端末からだけ Atom Lite へ到達できるようにする。Atom Lite の Web サーバーはインターネットへの公開ポートを設けずに運用する。VPN 製品と設定手順は設置環境を確認して決める。[WiFiManager](https://github.com/tzapu/WiFiManager)・[VPN ゲートウェイ構成例](https://tailscale.com/docs/features/subnet-routers)

GUI の表示と点灯ロジックは共通化する。色、明るさ、パターン、速度の状態を Atom Lite が保持し、再接続時は機器側の値を画面に反映する。点灯パターンは本体側で時間管理し、通信が切れても動作を継続する。無線の再接続処理と点灯処理は互いを待たせない。Wi-Fi の認証情報は機器の設定に保存し、ソースとログへの出力を避ける。

## LED の配線と USB 給電

外付け LED は WS2812B。Atom Lite 側の Grove は白線の GPIO32 を `DIN` へ、赤線の 5 V を `5V` へ、黒線の GND を `GND` へ接続する。データは GRB 順の 800 kbit/s。信号レベルの適合は使用している LED の仕様と点灯試験で確認する。[Atom Lite](https://docs.m5stack.com/ja/core/Atom-Lite)・[Worldsemi の WS2812B データシート](https://www.world-semi.co.kr/_files/ugd/89cd03_1023b0e9d135431aa1e6491bfc318112.pdf)

外付け LED は USB 給電を前提とし、最大 15 個で固定する。GUI で 1〜15 個を指定でき、初期値は 15 個。指定個数より後ろの LED へは消灯を送る。全パターンに共通の輝度上限 `LED_BRIGHTNESS_MAX = 32`（PWM 0〜255）を適用する。レインボーは指定色を使わず、先頭から指定個数までを同じ色で連続的に変化させる。設定範囲は毎分 1〜60 周で、速度端点は 1 周 60 秒と 1 秒。

## 開発・検証 ToDo

| 段階 | 作業と完了条件 | 状態 |
| --- | --- | --- |
| 0. 計画 | BLE と Wi-Fi の用途、起動方式、LED 数の仕様を記録する。成果物はこのファイル | 2026-11-06 更新。外付け LED は最大 15 個 |
| 1. 基板と LED | Arduino CLI で Atom Lite 向けにビルドし、USB で書き込む。GPIO32 の WS2812B で低輝度の赤・緑・青・消灯を確認する | 2026-11-06 完了。GPIO32 版のビルド・COM13 への書き込み・起動と、外付け LED の赤・緑・青・消灯を確認 |
| 2. BLE 試作 | Windows・macOS・Android の Chrome で、キャッシュや初回 Web アクセスに頼らず、ネット接続なしで画面起動・機器選択・色変更・切断後の再接続まで確認する | 2026-11-06 Windows の BLE API でペアリング後の状態取得・通知登録と、GPIO32 版での点灯操作を確認。Chrome アプリでの実機操作は未確認 |
| 3. Wi-Fi 試作 | 設定用 AP、誤った Wi-Fi 設定からの復旧、LAN 操作、自動再接続を確認する。Android のモバイル回線など、設置 LAN の外側から VPN 経由の操作を確認する | ソース実装済み。GUI の模擬試験合格、実機・VPN 未検証 |
| 4. 統合 | モードの切り替え・電源再投入での復元、全パターン、3 OS の操作を確認する | 未検証 |
| 5. 再現確認 | 初見の手順でビルド・書き込み・接続を再現する。README に使用版、配線、コマンド、復旧方法と検証結果を記録する | 手順記載済み。実機の再現確認は未実施 |
| 6. LED 個数とレインボー | Chrome で 1〜15 個を選び、個数を減らしたときの消灯と、レインボーの色・明るさ・速度を確認する | 2026-11-06 周期の計算と画面の模擬試験に合格。COM13 に書き込み、BLE 起動を確認。新機能の実点灯は未確認 |

開発環境は Arduino CLI と Espressif の Arduino ESP32 コアの M5Atom ボード定義。Arduino 標準の C++ を使用する。ボード設定と依存ライブラリの固定版は `firmware/AtomNeoPixel/sketch.yaml`、ビルド・書き込み・操作手順は `README.md` を参照する。[M5Stack の Atom 書き込みガイド](https://docs.m5stack.com/en/arduino/m5atom/program)・[Arduino CLI のビルド仕様](https://docs.arduino.cc/arduino-cli/sketch-build-process/)

### 2026-11-06 検証記録

- Arduino CLI 1.4.1 の `atom` プロファイルでビルド成功。フラッシュ使用量は 1,791,859 バイト（56%）、静的 RAM は 62,872 バイト（19%）。
- COM13 の ESP32-PICO-D4 に書き込み、転送後のハッシュ検証とリセット後のシリアル出力 `Mode: BLE` を確認。
- 共通 GUI の HTML 書き出し、JavaScript 構文、ローカル配信、BLE と Wi-Fi の状態取得・設定送信を模擬環境で確認。
- Codex の隔離環境では、一時ディレクトリ名の波括弧がビルド時に欠落してコンパイラを参照できず、COM13 のアクセスも拒否された。通常の Windows 一時ディレクトリと COM13 へのアクセス権を使用してビルド・書き込みを確認した。
- 利用者は ChatGPT 内蔵ブラウザの機器選択後に `Connection attempt failed` を確認した。Windows の BLE API で機器検出・GATT 接続・サービス取得までは成功し、ペアリング前の状態読み取りは `Insufficient Authentication`。Windows で明示的にペアリングした後は、6 バイトの状態読み取りと通知登録に成功した。内蔵ブラウザのエラーと認証エラーの因果関係は未確認。
- 外付け LED の型番と配線を WS2812B・GPIO32 に訂正。Arduino CLI 1.4.1 の `atom` プロファイルで再ビルドし、COM13 への書き込みハッシュと BLE モードでの再起動を確認。
- Windows の BLE API で赤・緑・青・消灯のコマンドを順に送り、Atom Lite から読み戻した状態が全て一致した。利用者が外付け WS2812B で赤 → 緑 → 青 → 消灯を確認した。
- `v0.2.0` を Arduino CLI 1.4.1 でビルド。フラッシュ使用量 1,793,951 バイト（57%）、静的 RAM 62,888 バイト（19%）。COM13 の書き込みハッシュ検証と BLE 起動を確認した。レインボーの周期 60 秒・2 秒・1 秒の計算をコンパイラで検証した。BLE と Wi-Fi の GUI 模擬試験では、個数 1〜15、速度端点、色指定の無効化、送信データを確認した。
