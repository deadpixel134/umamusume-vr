# 構成と安全境界

- `immersive/src`: Localify `externalDlls`で読み込むnative OpenXR/D3D11 runtime。
- `settings/src`: schema 9設定の検証・保存とGitHub Releaseの検証済みstaging。
- `installer/src/UmaVR.Management`: payload integrity、path containment、backup、rollback、process guard。
- `installer/src/UmaVR.Installer`: 多言語UIと自動更新handoff。

更新はインストール外でstagingし、SHA-256とpackage/version検証前には適用しません。再利用可能なGitHub認証情報は配布しません。deny-by-default allowlistによりゲーム原本、Localify、ユーザーデータ、ログ、生成packageを除外します。
