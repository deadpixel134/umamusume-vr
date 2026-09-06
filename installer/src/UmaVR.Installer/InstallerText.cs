using System.Globalization;
using UmaVR.Management;

namespace UmaVR.Installer;

internal enum InstallerLanguage
{
    Korean,
    English,
    Japanese
}

internal static class InstallerText
{
    private static readonly IReadOnlyDictionary<InstallerLanguage, IReadOnlyDictionary<string, string>> Resources =
        new Dictionary<InstallerLanguage, IReadOnlyDictionary<string, string>>
        {
            [InstallerLanguage.Korean] = Korean(),
            [InstallerLanguage.English] = English(),
            [InstallerLanguage.Japanese] = Japanese()
        };

    public static InstallerLanguage CurrentLanguage { get; private set; }

    public static void Initialize()
    {
        ValidateResources();
        CurrentLanguage = LoadLanguage();
    }

    public static string Get(string key) =>
        Resources[CurrentLanguage].TryGetValue(key, out string? value)
            ? value
            : throw new InvalidOperationException($"Missing installer text: {CurrentLanguage}/{key}");

    public static string Format(string key, params object[] arguments) =>
        string.Format(CultureInfo.CurrentCulture, Get(key), arguments);

    public static void SetLanguage(InstallerLanguage language)
    {
        CurrentLanguage = language;
        try
        {
            string directory = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "UmaVR");
            Directory.CreateDirectory(directory);
            File.WriteAllText(Path.Combine(directory, "ui-language.txt"), Code(language));
        }
        catch
        {
            // The selected language remains active for this process.
        }
    }

    public static string Localify(LocalifyStatus status) => Get(status switch
    {
        LocalifyStatus.Installed => "LocalifyInstalled",
        LocalifyStatus.Partial => "LocalifyPartial",
        _ => "LocalifyAbsent"
    });

    public static string ExceptionMessage(Exception exception)
    {
        if (exception is not InstallationException installation)
        {
            return exception.Message;
        }
        return Resources[CurrentLanguage].ContainsKey(installation.Code)
            ? Format(installation.Code, installation.Arguments.ToArray())
            : exception.Message;
    }

    public static string Warning(string warning)
    {
        int separator = warning.IndexOf(':');
        string code = separator >= 0 ? warning[..separator] : warning;
        string path = separator >= 0 ? warning[(separator + 1)..] : warning;
        string key = code switch
        {
            "Missing" => "WarningMissing",
            "Modified" => "WarningModified",
            "BackupMissing" => "WarningBackupMissing",
            _ => "WarningUnknown"
        };
        return Format(key, path);
    }

    private static InstallerLanguage LoadLanguage()
    {
        try
        {
            string path = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "UmaVR",
                "ui-language.txt");
            if (File.Exists(path))
            {
                return File.ReadAllText(path).Trim().ToLowerInvariant() switch
                {
                    "ja" => InstallerLanguage.Japanese,
                    "en" => InstallerLanguage.English,
                    _ => InstallerLanguage.Korean
                };
            }
        }
        catch
        {
            // Fall back to the Windows UI culture.
        }
        return CultureInfo.CurrentUICulture.TwoLetterISOLanguageName switch
        {
            "ja" => InstallerLanguage.Japanese,
            "en" => InstallerLanguage.English,
            _ => InstallerLanguage.Korean
        };
    }

    private static string Code(InstallerLanguage language) => language switch
    {
        InstallerLanguage.Korean => "ko",
        InstallerLanguage.English => "en",
        InstallerLanguage.Japanese => "ja",
        _ => "ko"
    };

    private static void ValidateResources()
    {
        HashSet<string> reference = Resources[InstallerLanguage.Korean].Keys.ToHashSet(StringComparer.Ordinal);
        foreach ((InstallerLanguage language, IReadOnlyDictionary<string, string> resource) in Resources)
        {
            if (!reference.SetEquals(resource.Keys) || resource.Any(pair => string.IsNullOrWhiteSpace(pair.Value)))
            {
                throw new InvalidOperationException($"Installer localization mismatch: {language}");
            }
        }
    }

    private static Dictionary<string, string> Korean() => new()
    {
        ["AppTitle"] = "UmaVR 설치 프로그램",
        ["Heading"] = "UmaVR 설치 및 관리",
        ["Description"] = "한글패치를 보존하며 VR 모드를 안전하게 설치하거나 직전 버전으로 복귀합니다.",
        ["GameFolder"] = "게임 폴더",
        ["Browse"] = "찾기...",
        ["PackageVersion"] = "패키지 버전",
        ["InstalledVersion"] = "설치된 버전",
        ["LocalifyStatus"] = "한글패치 상태",
        ["NotAvailable"] = "사용 불가",
        ["NotInstalled"] = "설치되지 않음",
        ["InvalidGameRoot"] = "올바른 게임 폴더가 아님",
        ["LocalifyInstalled"] = "설치됨 — 완전 보존",
        ["LocalifyPartial"] = "일부 흔적 감지 — 관련 파일 보존",
        ["LocalifyAbsent"] = "설치되지 않음 — VR 의존성만 설치",
        ["Install"] = "설치",
        ["Update"] = "업데이트",
        ["Reinstall"] = "다시 설치",
        ["Uninstall"] = "제거",
        ["Rollback"] = "직전 버전으로 복귀",
        ["OpenSettings"] = "설정 열기",
        ["Refresh"] = "새로고침",
        ["Ready"] = "준비",
        ["InspectFailed"] = "상태 확인 실패: {0}",
        ["ConfirmTitle"] = "UmaVR 확인",
        ["ConfirmUninstall"] = "현재 VR 모드를 제거하시겠습니까? 설정과 로그, 한글패치는 보존됩니다.",
        ["ConfirmRollback"] = "현재 버전을 제거하고 직전 설치 버전으로 복귀하시겠습니까?",
        ["Installing"] = "설치 중 {0}/{1}: {2}",
        ["Uninstalling"] = "제거 중 {0}/{1}: {2}",
        ["ProgressComplete"] = "파일 작업 완료",
        ["InstallComplete"] = "v{0} 설치 완료. 한글패치: {1}",
        ["RollbackComplete"] = "v{0} 제거 완료. v{1}(으)로 복귀했습니다.",
        ["UninstallComplete"] = "v{0} 제거 완료. 사용자 설정과 한글패치는 보존했습니다.",
        ["UninstallWarnings"] = "일부 파일은 변경되었거나 백업이 없어 보존했습니다. 설치 상태를 유지합니다.",
        ["LanguageChanged"] = "표시 언어를 변경했습니다.",
        ["SettingsMissing"] = "설정 프로그램이 아직 설치되지 않았습니다.",
        ["Busy"] = "다른 작업이 진행 중입니다.",
        ["ErrorPrefix"] = "오류: {0}",
        ["PackagePayloadMissing"] = "패키지 payload 폴더가 없습니다: {0}",
        ["PackageFileMissing"] = "패키지 파일이 없습니다: {0}",
        ["PackageHashMismatch"] = "패키지 파일 해시가 일치하지 않습니다: {0}",
        ["PackageRequiredFileMissing"] = "클린 설치 필수 파일이 패키지에 없습니다: {0}",
        ["PackageDuplicatePath"] = "패키지에 중복 경로가 있습니다: {0}",
        ["PackagePolicyInvalid"] = "패키지의 설정 또는 Dobby 보존 정책이 올바르지 않습니다.",
        ["InstalledHashMismatch"] = "설치 후 파일 해시가 일치하지 않습니다: {0}",
        ["InstallStateMissing"] = "설치 상태 파일이 없어 추측으로 제거하지 않습니다.",
        ["PreviousStateMissing"] = "직전 버전 설치 상태 백업이 없습니다.",
        ["PackageManifestMissing"] = "패키지 manifest를 찾지 못했습니다: {0}",
        ["PackageManifestInvalid"] = "패키지 manifest가 손상되었습니다.",
        ["PackageSchemaUnsupported"] = "지원하지 않는 패키지 형식입니다: {0}",
        ["InstallStateInvalid"] = "설치 상태 파일이 손상되었습니다.",
        ["InstallStateSchemaUnsupported"] = "지원하지 않는 설치 상태 형식입니다: {0}",
        ["GameRootInvalid"] = "umamusume.exe, GameAssembly.dll, UnityPlayer.dll이 있는 폴더를 선택하세요.",
        ["LocalifyRequired"] = "현재 릴리스는 localify.dll과 config.json의 externalDlls 로더가 필요합니다.",
        ["BootstrapConfigInvalid"] = "config.json의 externalDlls 항목을 안전하게 읽거나 갱신할 수 없습니다.",
        ["GameRunning"] = "게임이 실행 중입니다. 완전히 종료한 뒤 다시 시도하세요.",
        ["UnsafePath"] = "안전하지 않은 패키지 경로입니다: {0}",
        ["ProtectedLocalifyPath"] = "한글패치 보호 경로는 설치할 수 없습니다: {0}",
        ["WarningMissing"] = "이미 없는 파일: {0}",
        ["WarningModified"] = "사용자 변경 파일 보존: {0}",
        ["WarningBackupMissing"] = "백업 누락으로 파일 보존: {0}",
        ["WarningUnknown"] = "파일 보존: {0}"
    };

    private static Dictionary<string, string> English() => new()
    {
        ["AppTitle"] = "UmaVR Installer",
        ["Heading"] = "Install and manage UmaVR",
        ["Description"] = "Safely install or roll back the VR mod while preserving Localify.",
        ["GameFolder"] = "Game folder",
        ["Browse"] = "Browse...",
        ["PackageVersion"] = "Package version",
        ["InstalledVersion"] = "Installed version",
        ["LocalifyStatus"] = "Localify status",
        ["NotAvailable"] = "Not available",
        ["NotInstalled"] = "Not installed",
        ["InvalidGameRoot"] = "Not a valid game folder",
        ["LocalifyInstalled"] = "Installed — fully preserved",
        ["LocalifyPartial"] = "Partial traces found — files preserved",
        ["LocalifyAbsent"] = "Not installed — VR dependencies only",
        ["Install"] = "Install",
        ["Update"] = "Update",
        ["Reinstall"] = "Reinstall",
        ["Uninstall"] = "Uninstall",
        ["Rollback"] = "Roll back",
        ["OpenSettings"] = "Open settings",
        ["Refresh"] = "Refresh",
        ["Ready"] = "Ready",
        ["InspectFailed"] = "Status check failed: {0}",
        ["ConfirmTitle"] = "Confirm UmaVR action",
        ["ConfirmUninstall"] = "Uninstall the current VR mod? Settings, logs and Localify will be preserved.",
        ["ConfirmRollback"] = "Remove the current version and roll back to the previous installed version?",
        ["Installing"] = "Installing {0}/{1}: {2}",
        ["Uninstalling"] = "Uninstalling {0}/{1}: {2}",
        ["ProgressComplete"] = "File operation complete",
        ["InstallComplete"] = "v{0} installed. Localify: {1}",
        ["RollbackComplete"] = "v{0} removed. Rolled back to v{1}.",
        ["UninstallComplete"] = "v{0} removed. User settings and Localify were preserved.",
        ["UninstallWarnings"] = "Some modified files or files without backups were preserved. Install state remains available.",
        ["LanguageChanged"] = "Display language changed.",
        ["SettingsMissing"] = "The settings application is not installed yet.",
        ["Busy"] = "Another operation is in progress.",
        ["ErrorPrefix"] = "Error: {0}",
        ["PackagePayloadMissing"] = "The package payload folder is missing: {0}",
        ["PackageFileMissing"] = "A package file is missing: {0}",
        ["PackageHashMismatch"] = "A package file hash does not match: {0}",
        ["PackageRequiredFileMissing"] = "A clean-install dependency is missing from the package: {0}",
        ["PackageDuplicatePath"] = "The package contains a duplicate path: {0}",
        ["PackagePolicyInvalid"] = "The package has an invalid settings or Dobby preservation policy.",
        ["InstalledHashMismatch"] = "An installed file hash does not match: {0}",
        ["InstallStateMissing"] = "Install state is missing; no files will be removed by guesswork.",
        ["PreviousStateMissing"] = "The previous-version install-state backup is missing.",
        ["PackageManifestMissing"] = "The package manifest was not found: {0}",
        ["PackageManifestInvalid"] = "The package manifest is damaged.",
        ["PackageSchemaUnsupported"] = "Unsupported package format: {0}",
        ["InstallStateInvalid"] = "The install-state file is damaged.",
        ["InstallStateSchemaUnsupported"] = "Unsupported install-state format: {0}",
        ["GameRootInvalid"] = "Select the folder containing umamusume.exe, GameAssembly.dll and UnityPlayer.dll.",
        ["LocalifyRequired"] = "This release requires localify.dll and its config.json externalDlls loader.",
        ["BootstrapConfigInvalid"] = "The externalDlls entry in config.json cannot be read or updated safely.",
        ["GameRunning"] = "The game is running. Fully close it and try again.",
        ["UnsafePath"] = "Unsafe package path: {0}",
        ["ProtectedLocalifyPath"] = "A protected Localify path cannot be installed: {0}",
        ["WarningMissing"] = "File already missing: {0}",
        ["WarningModified"] = "Preserved user-modified file: {0}",
        ["WarningBackupMissing"] = "Preserved file because its backup is missing: {0}",
        ["WarningUnknown"] = "Preserved file: {0}"
    };

    private static Dictionary<string, string> Japanese() => new()
    {
        ["AppTitle"] = "UmaVR インストーラー",
        ["Heading"] = "UmaVR のインストールと管理",
        ["Description"] = "Localifyを保護したままVRモードを安全にインストール・ロールバックします。",
        ["GameFolder"] = "ゲームフォルダー",
        ["Browse"] = "参照...",
        ["PackageVersion"] = "パッケージバージョン",
        ["InstalledVersion"] = "インストール済みバージョン",
        ["LocalifyStatus"] = "Localifyの状態",
        ["NotAvailable"] = "使用不可",
        ["NotInstalled"] = "未インストール",
        ["InvalidGameRoot"] = "正しいゲームフォルダーではありません",
        ["LocalifyInstalled"] = "インストール済み — 完全に保護",
        ["LocalifyPartial"] = "一部の痕跡を検出 — 関連ファイルを保護",
        ["LocalifyAbsent"] = "未インストール — VR依存ファイルのみ導入",
        ["Install"] = "インストール",
        ["Update"] = "アップデート",
        ["Reinstall"] = "再インストール",
        ["Uninstall"] = "アンインストール",
        ["Rollback"] = "前のバージョンに戻す",
        ["OpenSettings"] = "設定を開く",
        ["Refresh"] = "更新",
        ["Ready"] = "準備完了",
        ["InspectFailed"] = "状態の確認に失敗しました: {0}",
        ["ConfirmTitle"] = "UmaVR の確認",
        ["ConfirmUninstall"] = "現在のVRモードを削除しますか？ 設定、ログ、Localifyは保持されます。",
        ["ConfirmRollback"] = "現在のバージョンを削除して、前のインストール済みバージョンに戻しますか？",
        ["Installing"] = "インストール中 {0}/{1}: {2}",
        ["Uninstalling"] = "削除中 {0}/{1}: {2}",
        ["ProgressComplete"] = "ファイル操作完了",
        ["InstallComplete"] = "v{0}のインストールが完了しました。Localify: {1}",
        ["RollbackComplete"] = "v{0}を削除し、v{1}に戻しました。",
        ["UninstallComplete"] = "v{0}を削除しました。ユーザー設定とLocalifyは保持されました。",
        ["UninstallWarnings"] = "変更済みファイルまたはバックアップのないファイルを保持しました。インストール状態も維持します。",
        ["LanguageChanged"] = "表示言語を変更しました。",
        ["SettingsMissing"] = "設定アプリはまだインストールされていません。",
        ["Busy"] = "別の処理が実行中です。",
        ["ErrorPrefix"] = "エラー: {0}",
        ["PackagePayloadMissing"] = "パッケージのpayloadフォルダーがありません: {0}",
        ["PackageFileMissing"] = "パッケージファイルがありません: {0}",
        ["PackageHashMismatch"] = "パッケージファイルのハッシュが一致しません: {0}",
        ["PackageRequiredFileMissing"] = "クリーンインストールに必要なファイルがパッケージにありません: {0}",
        ["PackageDuplicatePath"] = "パッケージに重複したパスがあります: {0}",
        ["PackagePolicyInvalid"] = "設定またはDobbyの保持ポリシーが正しくありません。",
        ["InstalledHashMismatch"] = "インストール後のファイルハッシュが一致しません: {0}",
        ["InstallStateMissing"] = "インストール状態がないため、推測でファイルを削除しません。",
        ["PreviousStateMissing"] = "前のバージョンのインストール状態バックアップがありません。",
        ["PackageManifestMissing"] = "パッケージmanifestが見つかりません: {0}",
        ["PackageManifestInvalid"] = "パッケージmanifestが破損しています。",
        ["PackageSchemaUnsupported"] = "未対応のパッケージ形式です: {0}",
        ["InstallStateInvalid"] = "インストール状態ファイルが破損しています。",
        ["InstallStateSchemaUnsupported"] = "未対応のインストール状態形式です: {0}",
        ["GameRootInvalid"] = "umamusume.exe、GameAssembly.dll、UnityPlayer.dllがあるフォルダーを選択してください。",
        ["LocalifyRequired"] = "現在のリリースにはlocalify.dllとconfig.jsonのexternalDllsローダーが必要です。",
        ["BootstrapConfigInvalid"] = "config.jsonのexternalDlls項目を安全に読み込み・更新できません。",
        ["GameRunning"] = "ゲームが実行中です。完全に終了してからもう一度お試しください。",
        ["UnsafePath"] = "安全でないパッケージパスです: {0}",
        ["ProtectedLocalifyPath"] = "保護対象のLocalifyパスはインストールできません: {0}",
        ["WarningMissing"] = "すでに存在しないファイル: {0}",
        ["WarningModified"] = "ユーザー変更ファイルを保持: {0}",
        ["WarningBackupMissing"] = "バックアップがないためファイルを保持: {0}",
        ["WarningUnknown"] = "ファイルを保持: {0}"
    };
}
