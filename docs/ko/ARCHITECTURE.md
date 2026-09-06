# 프로그램 구조와 안전 경계

[프로젝트 홈](../../README.md) · [설치](INSTALLATION.md) · [사용](USAGE.md)

- `immersive/src`: Localify `externalDlls`로 로드되는 native OpenXR/D3D11 runtime입니다.
- `settings/src`: versioned schema 9 설정을 검증·원자 저장하고 GitHub Release 업데이트를 스테이징합니다.
- `installer/src/UmaVR.Management`: payload hash, 경로 containment, 백업·롤백·제거와 실행 중 게임 차단을 소유합니다.
- `installer/src/UmaVR.Installer`: 한국어·영어·일본어 설치 UI와 종료 대기 후 자동 업데이트를 실행합니다.

업데이트는 Release 조회 → 정확한 버전 asset 선택 → 제한된 임시 폴더 다운로드 → SHA-256 검증 → 압축 해제 → package manifest 재검증 → 설치기 실행 순서입니다. 검증 전에 게임 파일을 바꾸지 않으며 재사용 가능한 GitHub 자격 증명을 포함하지 않습니다.

공개 소스 경계는 `.gitignore`의 deny-by-default allowlist입니다. 실제 게임 파일, Localify, 사용자 설정, 로그, 빌드/패키지 산출물과 내부 개발 기록은 추적하지 않습니다.
