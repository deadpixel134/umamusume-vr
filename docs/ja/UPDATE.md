[한국어](../ko/UPDATE.md) | [English](../en/UPDATE.md) | [日本語](../ja/UPDATE.md)

# アップデート

## アップデート · v0.1.1

ゲームを終了して `vrmod/tools/UmaVR.Configurator.exe` を開きます。公開版v0.1.1は起動時に新しい公開版を確認します。再確認するには **更新を確認** を選びます。更新がある場合はZIPを検証してインストーラーを自動起動します。

## 手動更新

1. リリースの **Assets** から同じバージョンの `UmaVR-vX.Y.Z.zip` と `.zip.sha256` を取得します。
2. 手動で更新するには、[Releases](https://github.com/deadpixel134/umamusume-vr/releases) の新しいZIPをゲームフォルダーの外に展開します。同梱の `UmaVR.Installer.exe` を実行し、既存のゲームフォルダーを選んでインストールします。

## 更新が止まった場合

ゲームが起動中と表示されたら、終了して **更新を確認** をもう一度選びます。ダウンロード・検証エラーは設定アプリのステータスで確認できます。インストーラーが起動した後のエラーは `%LOCALAPPDATA%\UmaVR\update.log` の末尾を確認してください。

## 開発ビルド v0.1.12

このビルドでは新しいバージョンが見つかった後に **更新を適用** を選びます。既存の構成や変更済みファイルについて手動確認を求められたら、表示されたパスとファイルを確認してから続けてください。

[v0.1.1をダウンロード](https://github.com/deadpixel134/umamusume-vr/releases/tag/v0.1.1) · [インストール・更新・削除](INSTALLATION.md) · [UmaVR](../../README.ja.md)
