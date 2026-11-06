# Atom NeoPixel Android アプリ

試作版 `0.1.0`。Android 8.0 以降で、スマホ内の HTTP サーバーが BLE 操作画面を配信します。Google Chrome が Web Bluetooth で Atom Lite に接続します。インストール後は Windows PC、USB デバッグ、インターネット接続なしで操作できる構成です。

## インストールと使い方

1. [署名済み APK（AtomNeoPixel 0.1.0）](releases/AtomNeoPixel-0.1.0.apk) をスマホに保存し、「ファイル」アプリなどから開きます。Android に求められたら、APK を開いたアプリに対して「この提供元からのインストールを許可」をオンにします。
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

2026-11-06 に署名済み `AtomNeoPixel-0.1.0.apk` を生成しました。APK 署名 v2/v3、パッケージ情報、組み込まれた DEX、配布 HTML とファームウェア内 HTML の一致を確認しました。PC 上の Java 試験では画面の取得、無効な HTTP リクエスト、ポート競合、停止時の接続終了、再起動を確認しました。Android 実機へのインストール、サービス開始から Chrome 起動までの一連の操作、オフラインでの BLE 点灯変更は未検証です。

配布 APK の SHA-256 は `5c425b4d9c5ccd402c020187904859a37a34e39ccd7b28f20b634b028106fe89` です。
