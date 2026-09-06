[한국어](../ko/INSTALLATION.md) | [English](../en/INSTALLATION.md) | [日本語](INSTALLATION.md)

# インストール・更新・削除

[プロジェクト](../../README.ja.md) · [使い方](USAGE.md) · [構成](ARCHITECTURE.md)

## 必要環境

- Windows 11 x64と正規のDMM版ウマ娘
- PC OpenXR環境
- 現在のリリースではLocalify 1.50.0の`localify.dll`と有効なルート`config.json`。未導入の場合は[公式Localifyインストール案内](https://github.com/Kimjio/umamusume-localify)を先に確認してください。

## インストール

1. `umamusume.exe`を完全に終了します。
2. 安定版Releaseが提供されたら`UmaVR-vX.Y.Z.zip`と同じReleaseの`.sha256`を取得します。
3. ゲームフォルダー外へ展開し、`UmaVR.Installer.exe`を起動します。
4. `umamusume.exe`、`GameAssembly.dll`、`UnityPlayer.dll`があるフォルダーを選択してインストールします。
5. Localifyが未導入または不完全な場合は、インストーラーの**公式Localifyインストール案内を開く**から導入し、状態を更新します。
6. UmaVRをインストールし、`vrmod/tools/UmaVR.Configurator.exe`で設定を確認してDMMからゲームを起動します。

インストーラーは全payload hashを先に検証し、既存設定と他の`externalDlls`を維持してUmaVRを追加します。
UmaVRはLocalifyを自動でダウンロード、導入、置換しません。

展開後の最上位には、実行対象の`UmaVR.Installer.exe`一つと`package-manifest.json`、`payload`、`licenses`のみがあります。インストーラーと導入されるConfiguratorはself-contained単一EXEで、システム全体の.NETインストールは不要です。

## 自動更新

設定アプリは`deadpixel134/umamusume-vr`の安定版Releaseから、正確なバージョンZIPとSHA-256のみを選択します。サイズ・hash・package/versionを検証し、ゲーム停止中だけインストールします。失敗時は既存インストールを変更しません。

リポジトリとReleaseはpublicで、更新metadataとassetには認証なしでアクセスします。GitHub tokenや再利用可能な認証情報は配布しません。

## 削除・ロールバック

ゲームを終了してパッケージのインストーラーを起動します。記録済みの製品所有ファイルのみを処理し、変更済みファイルとユーザー設定は保持します。`config.json`は検証済みバックアップと現在hashが一致する場合のみ元に戻します。
