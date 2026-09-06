[한국어](INSTALLATION.md) | [English](../en/INSTALLATION.md) | [日本語](../ja/INSTALLATION.md)

# 설치·업데이트·제거

[프로젝트 홈](../../README.md) · [사용 방법](USAGE.md) · [구조](ARCHITECTURE.md)

## 준비물

- Windows 11 x64와 DMM판 우마무스메의 정상 설치본
- OpenXR를 사용할 수 있는 PC VR 환경
- 현재 릴리스에서는 Localify 1.50.0의 `localify.dll`과 유효한 루트 `config.json`. 설치되어 있지 않다면 [공식 Localify 저장소의 설치 안내](https://github.com/Kimjio/umamusume-localify)를 먼저 확인하십시오.

## 설치

1. 게임과 DMM 런처에서 실행 중인 `umamusume.exe`를 완전히 종료합니다.
2. 정식 Release가 제공되면 `UmaVR-vX.Y.Z.zip`과 같은 Release의 `.sha256`을 내려받습니다.
3. ZIP을 게임 폴더 밖에 풀고 `UmaVR.Installer.exe`를 실행합니다.
4. `umamusume.exe`, `GameAssembly.dll`, `UnityPlayer.dll`이 있는 폴더를 선택합니다.
5. Localify가 없거나 불완전하면 인스톨러에 나타나는 **공식 Localify 설치 안내 열기**를 눌러 설치를 마친 뒤 **새로고침**합니다.
6. 설치를 누릅니다. 설치기는 전체 payload hash와 manifest를 먼저 검증한 뒤 파일을 백업·설치하고 `config.json.externalDlls`에 UmaVR DLL을 병합합니다.
7. `vrmod/tools/UmaVR.Configurator.exe`에서 설정을 확인한 뒤 DMM 런처로 게임을 시작합니다.

기존 `externalDlls` 항목과 `vrmod/config/settings.json`은 보존합니다. 현재 후보는 Localify 자체를 배포하거나 교체하지 않습니다.

## 자동 업데이트

설정 프로그램은 시작 시와 **업데이트 확인** 버튼을 눌렀을 때 `deadpixel134/umamusume-vr`의 draft/prerelease가 아닌 최신 Release를 확인합니다. 정확히 `UmaVR-vX.Y.Z.zip`과 대응 SHA-256만 선택하고, 다운로드 크기·해시·패키지 버전·manifest·설치기 존재 여부를 검증한 뒤 게임이 종료된 상태에서 설치합니다. API·다운로드·검증 실패는 기존 설치를 변경하지 않습니다.

저장소와 Release는 public이며 인증 없이 업데이트 metadata와 asset을 조회합니다. 프로그램에는 GitHub 토큰이나 재사용 가능한 자격 증명을 포함하지 않습니다.

## 제거와 롤백

게임을 종료하고 사용한 패키지의 `UmaVR.Installer.exe`를 실행해 **제거** 또는 **롤백**을 선택합니다. 설치기가 기록한 product-owned 파일만 처리하며, 설치 후 수정된 파일은 지우지 않고 경고합니다. 원래 `config.json`은 현재 설치본의 hash가 일치할 때만 byte-exact 백업으로 복원하며 사용자 설정은 남깁니다.

## 문제 해결

- Localify 미설치 또는 불완전 상태라면 인스톨러의 공식 설치 안내 링크를 열고 설치를 마친 뒤 새로고침하십시오. UmaVR는 Localify를 자동으로 내려받거나 교체하지 않습니다.
- VR가 시작되지 않으면 활성 OpenXR runtime과 DMM 런처 경로를 확인하십시오.
- 업데이트가 실패하면 `%LOCALAPPDATA%\UmaVR\update.log`를 확인하십시오. 기존 설치는 그대로 유지됩니다.
- 로그를 공유하기 전에 계정 식별자·토큰·인증 정보가 없는지 확인하십시오.
