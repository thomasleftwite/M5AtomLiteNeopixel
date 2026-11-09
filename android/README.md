# Atom NeoPixel Android アプリ

試作版 `0.2.0`（versionCode 2）。Android 8.0 以降で、スマホ内の HTTP サーバーが BLE 操作画面を配信します。Google Chrome が Web Bluetooth で Atom Lite に接続します。インストール後は Windows PC、USB デバッグ、インターネット接続なしで操作できる構成です。

`0.2.0` はファームウェア `v0.3.0` 向けです。9バイト形式と色相角0〜360 deg・10 deg刻みのスライダーを同梱しました。旧0.1.0と同じ署名鍵を使用しています。旧APKは7バイト形式のため、v0.3.0には0.2.0を使用してください。

## インストールと使い方

1. [署名済み APK（AtomNeoPixel 0.2.0）](releases/AtomNeoPixel-0.2.0.apk) をスマホに保存し、「ファイル」アプリなどから開きます。Android に求められたら、APK を開いたアプリに対して「この提供元からのインストールを許可」をオンにします。
2. インストールした「Atom NeoPixel」を開き、**サーバーを起動して Chrome を開く**を押します。通知を許可すると通知からサーバーを停止できます。Chrome が開くアドレスは `http://127.0.0.1:8080/` です。
3. Atom Lite はボタンを押さずに起動します。起動後に本体ボタンを約1.5秒押し、内蔵 LED が水色になったら、Chrome の **BLE 接続**から `AtomPixel-…` を選びます。設定を選んで **反映する**を押します。
4. 操作を終えるときは、Chrome で **BLE 切断**を押し、アプリまたは通知からサーバーを停止します。サーバー停止は HTML の配信を終了する操作です。すでに開いている Chrome の BLE 接続は、Chrome 側で切断できます。

## ビルドと署名

Node.js、JDK 21、Android SDK Platform 36、Android Build Tools 37.0.0 を使います。`build.ps1` はファームウェアと共通の HTML を `assets/index.html` に書き出し、ビルド・署名・署名検証を行います。署名用のキーストアは作業ツリーの外に保管します。将来の上書きインストールには同じ署名鍵を使います。

PowerShell で `ATOM_APK_STORE_PASSWORD` を設定してから、リポジトリ直下で実行します。出力先のフォルダは先に作成します。

```powershell
./android/build.ps1 -PlatformJar <Android36のandroid.jar> `
  -BuildTools <BuildToolsのフォルダ> -JavaHome <JDKのフォルダ> `
  -Keystore <署名用キーストア> -Output <新規APKのパス>
```

`LauncherActivity` はサービス開始・停止と Chrome の起動、`LoopbackService` は通知付きサービスと HTML の読み込み、`LoopbackServer` は `127.0.0.1:8080` の HTTP 配信を担当します。Android 14 以降向けのフォアグラウンドサービス種別は `specialUse` です。対象 SDK は API 36 です。[サービス種別の仕様](https://developer.android.com/develop/background-work/services/fgs/service-types#special-use)

## 検証記録

2026-11-09にAPK 0.2.0を生成。versionCode 2、minSdk 26、targetSdk 36、起動Activity、DEX、署名v2/v3、zipalign、旧0.1.0との署名証明書SHA-256の一致を検証しました。同梱HTMLは現行ファームウェアのWebUi.hと完全一致し、BLE/Wi-Fi画面模擬試験も通過しました。

Android 12（API 31）端末へ新規インストールし、起動、アプリ内サーバー開始、Chromeへの引継ぎ、スマホ内HTTP配信内容の一致を確認しました。ChromeからAtom LiteへBLE接続し、0/180/360 degの送受信と読戻し、レインボー以外でのグレイアウト、角度の保持、消灯を確認。BLE切断、アプリからのサーバー停止・再起動・再配信も通過しました。PCからスマホへ画面を配信するadb reverseは使用していません。検証用adb forwardは解除済みです。2026-11-09に利用者がUSBを外し、Wi-Fi・モバイル通信オフのスマホ単体で起動・BLE操作できたことを報告しました。APK更新の基本検証は完了です。ほかのAndroid版・長時間運転は未検証です。

APKのSHA-256：`7f3d72a504bacba73b45e8a3e80e584a75817cf7428f6ec5b1bfec2503d67de3`。成果物は [APK 0.2.0](releases/AtomNeoPixel-0.2.0.apk)、詳細は[検証記録](../docs/VALIDATION_2026-11-09.md)を参照してください。旧版からの実際の更新インストールと、Android 12以外の実機検証は未実施です。

### 旧0.1.0の記録

2026-11-06 に署名済み `AtomNeoPixel-0.1.0.apk` を生成しました。APK 署名 v2/v3、パッケージ情報、組み込まれた DEX、配布 HTML とファームウェア内 HTML の一致を確認しました。PC 上の Java 試験では画面の取得、無効な HTTP リクエスト、ポート競合、停止時の接続終了、再起動を確認しました。Android 実機へのインストール、サービス開始から Chrome 起動までの一連の操作、オフラインでの BLE 点灯変更は未検証です。

配布 APK の SHA-256 は `5c425b4d9c5ccd402c020187904859a37a34e39ccd7b28f20b634b028106fe89` です。
