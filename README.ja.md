[한국어](README.md) | [English](README.en.md) | [日本語](README.ja.md)

# UmaVR

制作者: [@TBluebox12](https://x.com/TBluebox12)  
支援: [buymeacoffee.com/vrshits](https://buymeacoffee.com/vrshits)

UmaVRは、DMM版『ウマ娘 プリティーダービー』向けの非公式Meta Quest/OpenXR VR Modです。検証済みのLiveでは両眼VRと物理6DoFを使用し、それ以外の画面は元の比率を維持した完全なゲーム画面を平面パネルで表示します。VRコントローラーでゲームUIを操作できます。

Virtual DesktopのVDXR経路で実機検証済みです。現在は最初の正式配布に向けた**v0.1.0リリース候補**で、安定版GitHub Releaseはまだ公開されていません。

## ドキュメント

- [インストール・更新・削除](docs/ja/INSTALLATION.md)
- [使い方と操作](docs/ja/USAGE.md)
- [構成と安全境界](docs/ja/ARCHITECTURE.md)

## 主な機能

- LiveでのOpenXRステレオ、物理HMD 6DoF、ワールドスケール
- Live以外では完全な元ゲーム画面を正面パネルに表示
- 補助パネル、コントローラーレイ、円形カーソル
- 右スティック移動、左スティック30°スナップターン、左右の役割交換
- 描画・カメラ・スケール・移動・対応VFX用の韓国語・英語・日本語設定GUI
- SHA-256、パッケージmanifest、設定保持、他の`externalDlls`との共存を行うインストール/更新設計

## 現在の対応範囲

- 検証済み: Windows 11 x64、ゲーム2.30.0、Unity 2022.3.62f2、Direct3D 11、Virtual Desktop VDXR 1.0.10、Localify 1.50.0ローダー経路
- Liveはimmersive stereo/6DoF/VFX/コントローラーに対応します。
- Home、Story、Race、Training、キャラクタープレビューは安全なステレオ所有経路がないためPANEL表示です。
- SteamVR OpenXRとMeta Quest Link/Air Linkは暫定対応で、本プロジェクトでは未検証です。

現在の候補は互換性のある既存`localify.dll`と`config.json`の`externalDlls`ローダーを必要とします。初期リポジトリはprivateで、再利用可能なGitHub認証情報は配布しないため、安全な配布方針が決まるまで一般ユーザー向けprivate Release自動更新は利用できません。

ゲーム原本、Localifyファイル、ユーザー設定、ログ、ロールバックデータ、ビルド成果物、認証情報はリポジトリに含めません。ソースは[MIT License](LICENSE)で提供します。

> UmaVRは非公式ファンプロジェクトであり、ゲーム開発・配信会社とは関係ありません。正規にインストールしたゲームが必要です。
