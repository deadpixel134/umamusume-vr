[한국어](README.md) | [English](README.en.md) | [日本語](README.ja.md)

# UmaVR

제작자: [@TBluebox12](https://x.com/TBluebox12)  
아카라이브 가상현실 채널: [가상현실 채널](https://arca.live/b/vrshits)  
후원: [buymeacoffee.com/vrshits](https://buymeacoffee.com/vrshits)

우마무스메 프리티 더비 DMM판을 위한 비공식 Meta Quest/OpenXR VR 모드입니다. 검증된 Live 장면은 양안 VR과 물리 6DoF로 표시하고, 그 밖의 화면은 원본 비율을 보존한 평면 패널로 표시합니다. VR 컨트롤러로 게임 UI를 조작할 수 있습니다.

Virtual Desktop의 VDXR 경로로 실기 검증했습니다. 현재 공식 버전은 **v0.1.0**입니다.

## 문서

- [설치·업데이트·제거](docs/ko/INSTALLATION.md)
- [사용 방법과 조작법](docs/ko/USAGE.md)
- [프로그램 구조와 안전 경계](docs/ko/ARCHITECTURE.md)
- [v0.1.0 릴리스 정보](docs/releases/v0.1.0.md)
- [English installation](docs/en/INSTALLATION.md) · [English usage](docs/en/USAGE.md)
- [日本語インストール](docs/ja/INSTALLATION.md) · [日本語の使い方](docs/ja/USAGE.md)

## 핵심 기능

- Live의 OpenXR 양안 스테레오, 물리 HMD 회전·이동과 월드 스케일
- Live 밖에서는 완전한 원본 게임 화면을 시야 정면 패널로 표시
- Immersive에서 Grip으로 켜는 보조 패널과 컨트롤러 레이·원형 커서
- 기본 오른손 이동, 왼손 30° 스냅 턴, 전체 손 역할 전환
- 렌더 배율, Camera Follow, 월드 스케일, 이동, VFX를 저장하는 한국어·영어·일본어 설정 GUI
- SHA-256과 패키지 manifest를 검증하고 기존 설정·다른 `externalDlls` 항목을 보존하는 설치/업데이트 구조

## 현재 지원 경계

- 검증됨: Windows 11 x64, 게임 2.30.0, Unity 2022.3.62f2, Direct3D 11, Virtual Desktop VDXR 1.0.10, Localify 1.50.0 로더 경로
- Live: immersive stereo/6DoF/VFX/컨트롤러 지원
- Home, Story, Race, Training, 캐릭터 미리보기: 안전한 스테레오 소유권 경로가 없어 현재 PANEL 표시
- SteamVR OpenXR와 Meta Quest Link/Air Link: 예비 지원, 이 프로젝트에서 실기 미검증
- 전체 post-processing OFF는 잔여 흐림 제거가 검증되었습니다. 일부 효과는 개별 제어가 제한되며 Aura는 개별 비활성화를 지원하지 않습니다.

## 주의 사항

- 현재 릴리스 후보는 기존 `localify.dll`과 `config.json`의 `externalDlls` 로더를 사용합니다. clean-game standalone loader는 아직 완료되지 않았습니다.
- 설치기는 게임 실행 중 파일을 교체하지 않으며, payload 전체를 먼저 검증합니다. 실패하면 현재 설치를 변경하지 않는 것이 목표입니다.
- 공개 GitHub Release는 인증 정보 없이 조회합니다. 프로그램과 패키지에는 GitHub 토큰이나 재사용 가능한 자격 증명을 포함하지 않습니다.

이 저장소에는 게임 원본 파일, Localify 파일, 사용자 설정, 로그, 롤백 데이터, 빌드 산출물과 인증 정보를 포함하지 않습니다. 프로젝트 소스는 [MIT License](LICENSE)로 배포되며 외부 구성 요소는 각자의 라이선스를 따릅니다.

> UmaVR은 비공식 팬 프로젝트이며 게임 개발사·배급사와 관련이 없습니다. 게임과 관련 상표·저작물의 권리는 각 권리자에게 있습니다. 사용하려면 정식으로 설치한 게임이 필요합니다.
