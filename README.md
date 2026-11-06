# M5AtomLiteNeopixel

M5Stack Atom Lite と外付け WS2812B を Chrome から操作する試作版 `v0.2.1`。LED 個数、色、明るさ、速度、消灯、固定点灯、点滅、フェード、レインボーに対応します。[開発計画](DEVELOPMENT_PLAN.md)

## 構成と配線

Google Chrome の操作画面から BLE または 2.4 GHz Wi-Fi で Atom Lite に設定を送り、Atom Lite が Grove ポートの **G32（GPIO32）** から外付け NeoPixel **WS2812B** に点灯データを送ります。LED は GRB 順の 800 kbit/s で駆動します。

| Atom Lite の Grove ポート | WS2812B の入力側 | 用途 |
| --- | --- | --- |
| G32（白線） | `DIN` | 点灯データ |
| 5 V（赤線） | `5V` | USB-C 由来の電源 |
| GND（黒線） | `GND` | 共通グラウンド |

線色は Atom Lite 側の Grove ケーブルの配色です。Atom Lite の内蔵 LED（GPIO27）は接続状態の表示に、本体ボタン（GPIO39）は起動モードと接続受付の操作に使います。[M5Stack の回路図とピン配置](https://docs.m5stack.com/ja/core/Atom-Lite)

USB-C で Atom Lite を給電します。GUI の「LED 個数」で実際に接続した個数を1〜15から選びます。起動時は15個・消灯で始まり、個数を減らすと指定個数より後ろの LED は消灯します。GUI の明るさ100%はファームウェアの輝度上限 PWM 32/255 に相当します。

### USB 給電の参考測定

Atom Lite の USB 入力側を、Wi-Fi 接続中・WS2812B 12個・試作版 `v0.2.1` の輝度上限で測定しました。

| 外付け LED の状態 | USB 入力電流の表示値 |
| --- | ---: |
| 全消灯 | 10 mA 以下 |
| 白・明るさ50% | 70 mA |
| 白・明るさ100% | 110 mA |

白100%時の110 mA は、Atom Lite の公称入力 5 V／500 mA の22%です。この測定と実機動作を踏まえ、今回想定した LED 個数・点灯パターン・点灯色では、現行の輝度上限で USB 給電に問題はないと判断しました。瞬時ピークは未測定です。[M5Stack の入力仕様](https://docs.m5stack.com/ja/core/Atom-Lite)・[開発記録の給電評価](DEVELOPMENT_PLAN.md#2026-11-06-usb-給電の評価)

## 点灯パターン

点滅とフェードの速度は 0.1〜10 cycle/sec（1周10秒〜0.1秒）で、0.1 cycle/sec 刻みです。レインボー以外のパターンにはこの範囲を表示します。

「レインボー」を選ぶと、選択した個数の LED が同じ色で赤から緑・青を経て連続的に循環します。色ピッカーは無効になり、明るさと速度を設定できます。速度は 1/60〜1 cycle/sec（1周60秒〜1秒）で、1分当たり1周刻みです。「反映する」を押すと設定が適用されます。

## ビルドと書き込み

Arduino CLI 1.4.1 を使用します。`sketch.yaml` に Arduino ESP32 3.3.6、WiFiManager 2.0.17、Adafruit NeoPixel 1.15.2 を固定しています。FQBN は `esp32:esp32:m5stack_atom:PartitionScheme=huge_app` です。最初のビルドにはパッケージ取得用のインターネット接続とストレージが必要です。

リポジトリ直下で次を実行します。Windows のポートは、Atom Lite を抜き差しして増える COM 番号を確認します。

```powershell
$atomBuildDir = Join-Path ([IO.Path]::GetTempPath()) 'AtomNeoPixel-build'
arduino-cli compile --profile atom --build-path $atomBuildDir firmware/AtomNeoPixel
arduino-cli board list
arduino-cli upload --profile atom --port COMx --input-dir $atomBuildDir firmware/AtomNeoPixel
arduino-cli monitor --port COMx --config baudrate=115200
```

書き込む前にシリアルモニターを閉じ、`COMx` を対象ポートに置き換えます。書き込みには 3 MB のアプリ領域を持つパーティション構成を使います。

## 起動と Chrome からの操作

初回起動は BLE です。通常起動では前回のモードを復元します。モードを変更するには、本体ボタンを押した状態で電源を入れ、約0.5秒以上押し続けてから放します。BLE と Wi-Fi が切り替わり、選択したモードが保存されます。点灯設定は機器の稼働中に保持され、再起動すると既定の消灯状態に戻ります。

### BLE で接続する

1. Atom Lite を BLE モードで起動します。内蔵 LED が青なら接続待機中です。
2. Node.js が入った端末で、リポジトリ直下から `node tools/serve-ui.mjs` を実行します。GUI は端末内のループバックから配信されます。
3. Google Chrome アプリのアドレスバーに `http://127.0.0.1:8080/` を直接入力します。
4. 起動後に Atom Lite の本体ボタンを約1.5秒押し、内蔵 LED を水色にします。60秒間の受付中に、画面の「BLE 接続」から `AtomPixel-XXXXXX` を選びます。OS からペアリングを求められた場合は、表示された機器を確認して承認します。
5. 画面に「接続済み」と表示され、内蔵 LED が緑になったら、LED 個数・パターン・色・明るさ・速度を選び、「反映する」を押します。「状態を取得」で機器側の現在値を読み直せます。

BLE を切断しても、Atom Lite は直前の点灯設定で動作を続けます。再接続するには本体ボタンを長押しして受付を再開します。

Windows で初回接続時に `Connection attempt failed` が出る場合は、本体ボタンを長押しして水色にし、Windows の Bluetooth 設定で `AtomPixel-XXXXXX` をペアリングしてから Chrome で接続を試してください。機器の状態取得と操作には暗号化された BLE 接続が必要です。[Microsoft の Bluetooth 開発者向け FAQ](https://learn.microsoft.com/en-us/windows/uwp/devices-sensors/bluetooth-dev-faq)

単体 HTML は `node tools/serve-ui.mjs --export <新規HTMLファイル>` で取り出せます。Android の Chrome での直接起動とループバック配信、および ChatGPT 内蔵ブラウザでの BLE 操作は未確認です。

ファームウェアの更新後は、`serve-ui.mjs` を再起動して Chrome の画面を再読み込みしてください。単体 HTML を使う場合は最新版を書き出します。

### Wi-Fi で接続する

1. Wi-Fi モードへ切り替えます。接続情報が未設定なら、内蔵 LED が橙色になります。接続情報の変更時は、起動後に本体ボタンを約1.5秒押します。
2. USB シリアル端末を 115200 bps、ローカルエコーと記録をオフに設定します。橙色の間に、設定用 AP 専用の一時パスワードを入力し、改行を送信します。利用できる文字は半角の `!` から `~` までで、8〜32文字です。ファームウェアは入力を表示しません。
3. 内蔵 LED が紫色になったら、端末から `AtomPixel-XXXXXX` の Wi-Fi に一時パスワードで接続します。`http://192.168.4.1/` を開き、接続先の 2.4 GHz Wi-Fi を設定します。設定用 AP は3分で閉じます。
4. 内蔵 LED が緑色になったら、端末を設置先 LAN に戻します。USB シリアルに表示された `http://<Atom LiteのIPv4アドレス>/` を Chrome で開きます。

外出先から使う場合は、設置先の VPN に接続し、同じ IPv4 アドレスを開きます。VPN 側で Atom Lite への到達を許可する端末を設定します。通常操作の HTTP は設置先の信頼できる LAN と VPN 内で使用します。電源投入後に緑色にならない場合は、2.4 GHz Wi-Fi と保存設定を確認し、手順1から設定し直します。

## 制御データと転送

「反映する」は現在の設定一式を送ります。BLE と Wi-Fi は同じ7バイトの状態を使います。Chrome は接続時に機器の状態を読み、変更の送信後は BLE の通知または HTTP の応答から状態を取得できます。

| 位置 | 項目 | 値 |
| ---: | --- | --- |
| 0 | パターン | `0` 消灯、`1` 固定点灯、`2` 点滅、`3` フェード、`4` レインボー |
| 1–3 | 赤・緑・青 | 各 `0–255`。レインボーでは点灯色を本体側で生成 |
| 4 | 明るさ | `0–100`。ファームウェアの輝度上限に対する割合 |
| 5 | 速度 | レインボーは `1–60` 周／分。他は `1–100` で、値の 1/10 が cycle/sec |
| 6 | LED 個数 | `1–15` |

- **BLE**。Chrome が Web Bluetooth で機器を選択し、サービス `251c86a0-3990-4b1e-bb96-5ed47d413b60` に接続します。コマンドの特性 `251c86a1-3990-4b1e-bb96-5ed47d413b60` に応答付きで7バイトを書き込み、状態の特性 `251c86a2-3990-4b1e-bb96-5ed47d413b60` を読み取り・通知登録します。状態の読み取り、コマンドの書き込み、通知登録には暗号化された接続が必要です。
- **Wi-Fi**。Atom Lite が GUI と同じ IPv4 アドレスで `GET /api/state` と `PUT /api/state` を受け付けます。GET は現在の状態を14桁の16進テキストで返します。PUT は同じ形式を本文に入れ、`Content-Type: text/plain` と `X-Atom-Control: 1` を付けます。適用後の状態が同じ形式で返ります。

例として、固定白・明るさ50%・速度0.8 cycle/sec・12個の状態は `01 ff ff ff 32 08 0c` です。BLE では7バイトのまま、HTTP では `01ffffff32080c` を送ります。旧6バイト形式の点灯コマンドを受け取った場合は、現在の LED 個数と、従来の周波数に最も近い 0.1 cycle/sec 刻みの速度で適用します。

## ソースコードの役割

| ファイル | 関数と役割 |
| --- | --- |
| [AtomNeoPixel.ino](firmware/AtomNeoPixel/AtomNeoPixel.ino) | `setup()` は LED・ボタン・描画タスクを初期化し、`heldAtStartup()` で起動モードを判定します。`loop()` は選んだ通信モードを処理し、`checkButton()` は起動後の長押しを検出します。 |
| [AtomNeoPixel.ino の描画](firmware/AtomNeoPixel/AtomNeoPixel.ino#L43) | `applyState()` は受け取った状態を描画キューに渡します。`renderTask()` は時間に応じた色と明るさを計算し、WS2812B と内蔵 LED を更新します。 |
| [AtomNeoPixel.ino の Wi-Fi と HTTP](firmware/AtomNeoPixel/AtomNeoPixel.ino#L84) | `beginWifi()`・`resumeStation()`・`tickWifi()` は Wi-Fi の起動、再接続、Web サーバーの運転を管理します。`requestPortal()`・`readPortalPassword()`・`clearPortalPassword()` は設定用 AP の開始と一時パスワードの扱いを担当します。`configureWeb()`・`stopWeb()` は HTTP 処理を設定・停止し、`requireLocalAddress()`・`hexDigit()`・`parseHttpState()`・`sendState()` が宛先確認と状態の変換を行います。 |
| [BleControl.cpp](firmware/AtomNeoPixel/BleControl.cpp) | `begin()` は GATT サービスと暗号化を設定します。`openWindow()`・`windowOpen()`・`tick()` は60秒の受付と広告の開始・停止を管理します。接続コールバックと `isConnected()` が接続状態を保持します。書き込みコールバックは状態を検証してキューに入れ、`publishState()` で読み取り値を更新して通知します。`takeCommand()` がキューから本体処理へ状態を渡します。 |
| [LedState.h](firmware/AtomNeoPixel/LedState.h) | `encodeState()`・`decodeState()` は状態の変換と入力検証を担当します。`cyclePhase()`・`rainbowHue()`・`frameBrightness()` が各パターンの周期、色相、明るさを計算します。 |
| [WebUi.h](firmware/AtomNeoPixel/WebUi.h) | `updateControls()`・`updatePatternControls()` が接続状態とパターンに応じて入力欄を切り替えます。`selectedState()`・`showState()` は画面と状態データを変換します。`refreshState()`・`requestState()` と接続・送信イベントが BLE または HTTP で転送します。`toHex()`・`fromHex()`・`fromView()` は転送形式を変換し、`withControls()`・`onDisconnect()`・`clearConnection()` が処理中や切断時の表示を管理します。 |
| [serve-ui.mjs](tools/serve-ui.mjs)・[sketch.yaml](firmware/AtomNeoPixel/sketch.yaml) | ランチャーはファームウェアに同梱した GUI を端末内で配信し、単体 HTML にも書き出せます。スケッチのプロファイルは対象ボードと依存関係の版を固定します。 |

## 動作確認の範囲

Windows の Google Chrome から BLE で接続し、WS2812B のレインボー速度と LED 個数の変更を実機で確認済みです。Windows の BLE API から送った赤・緑・青・消灯も、外付け LED の表示と読み戻した状態が一致しました。点滅とフェードの周期端点はコードと画面の模擬試験で検証しました。Wi-Fi 接続中の白点灯は実機で確認されており、Wi-Fi の復旧手順、VPN 経由、macOS・Android での BLE は今後の検証対象です。[検証記録](DEVELOPMENT_PLAN.md#2026-11-06-検証記録)
