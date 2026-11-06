# M5AtomLiteNeopixel

M5Stack Atom Lite と外付け Unit RGB LED を Chrome から操作する試作版 `v0.1.0`。色、明るさ、速度、消灯、固定点灯、点滅、フェードに対応します。[開発計画](DEVELOPMENT_PLAN.md)

## 配線と設定

Atom Lite の Grove ポートにストリップの信号入力側を接続します。黄線は GPIO26、赤線は 5 V、黒線は GND です。USB 給電を前提とし、外付け LED は最大15個です。実際の接続数を `firmware/AtomNeoPixel/LedState.h` の `LED_COUNT` に設定します。初期値は15で、1〜15以外はビルドエラーになります。GUI の明るさ100%はファームウェアの輝度上限 PWM 32/255 に相当します。

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

## 起動モード

初回起動は BLE です。本体ボタンを押した状態で電源を入れ、約0.5秒以上押し続けると BLE と Wi-Fi が切り替わります。切り替えたモードは再起動後も復元されます。外付け LED は起動時に消灯します。

### BLE

Windows・macOS では、Node.js が入った端末で次を実行し、Google Chrome アプリのアドレスバーに `http://127.0.0.1:8080/` を入力します。配信は端末内のループバックだけで行います。ChatGPT 内蔵ブラウザでの BLE 接続は確認できていません。

```powershell
node tools/serve-ui.mjs
```

本体ボタンを約1.5秒押すと、内蔵 LED が水色になり、60秒間 BLE の接続を受け付けます。画面の「BLE 接続」から `AtomPixel-XXXXXX` を選び、必要に応じて OS のペアリング操作を行います。設定変更後に「反映する」を押します。

Windows で初回接続時に `Connection attempt failed` が出る場合は、本体ボタンを長押しして水色にし、Windows の Bluetooth 設定で `AtomPixel-XXXXXX` をペアリングしてから Chrome で接続を試してください。機器の状態取得と操作には暗号化された BLE 接続が必要です。[Microsoft の Bluetooth 開発者向け FAQ](https://learn.microsoft.com/en-us/windows/uwp/devices-sensors/bluetooth-dev-faq)

単体 HTML は `node tools/serve-ui.mjs --export <新規HTMLファイル>` で取り出せます。Android の Chrome でこのファイルを直接開く方法と、Android 上のループバック配信方法は実機で確認する予定です。

### Wi-Fi

1. Wi-Fi モードへ切り替えます。接続情報が未設定なら、内蔵 LED が橙色になります。接続情報の変更時は、起動後に本体ボタンを約1.5秒押します。
2. USB シリアル端末を 115200 bps、ローカルエコーと記録をオフに設定します。橙色の間に、設定用 AP 専用の一時パスワードを入力し、改行を送信します。利用できる文字は半角の `!` から `~` までで、8〜32文字です。ファームウェアは入力を表示しません。
3. 内蔵 LED が紫色になったら、端末から `AtomPixel-XXXXXX` の Wi-Fi に一時パスワードで接続します。`http://192.168.4.1/` を開き、接続先の 2.4 GHz Wi-Fi を設定します。設定用 AP は3分で閉じます。
4. 内蔵 LED が緑色になったら、端末を設置先 LAN に戻します。USB シリアルに表示された `http://<Atom LiteのIPv4アドレス>/` を Chrome で開きます。

外出先から使う場合は、設置先の VPN に接続し、同じ IPv4 アドレスを開きます。VPN 側で Atom Lite への到達を許可する端末を設定します。通常操作の HTTP は設置先の信頼できる LAN と VPN 内で使用します。電源投入後に緑色にならない場合は、2.4 GHz Wi-Fi と保存設定を確認し、手順1から設定し直します。
