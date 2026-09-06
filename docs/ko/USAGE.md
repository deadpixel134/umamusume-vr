[한국어](USAGE.md) | [English](../en/USAGE.md) | [日本語](../ja/USAGE.md)

# 사용 방법과 조작법

[프로젝트 홈](../../README.md) · [설치](INSTALLATION.md) · [구조](ARCHITECTURE.md)

## 화면 표시

- **Live:** 양안 stereo 세계와 물리 HMD 6DoF를 사용합니다.
- **그 밖의 화면:** 완전한 원본 게임 화면이 시야 정면 평면 패널에 표시됩니다.
- **보조 패널:** Live에서 패널 손 Grip으로 켜고 끕니다. 원본 최종 화면 전체를 사용합니다.

## 기본 조작

| 동작 | 기본 입력 |
|---|---|
| 커서 조준 | 오른손 aim ray |
| 클릭·드래그 | 오른쪽 Trigger 또는 A |
| 뒤로 가기 | B |
| 3D 이동 | 오른쪽 Thumbstick |
| 시야 회전 | 왼쪽 Thumbstick, 30° snap |
| Live 보조 패널 | 왼손 Grip |

설정에서 전체 손 역할을 바꾸면 왼손이 pointer/Trigger/X·Y/이동, 오른손이 panel Grip/snap turn을 담당합니다. 게임 창이 포그라운드일 때만 UI 입력을 주입합니다.

## 설정

`vrmod/tools/UmaVR.Configurator.exe`에서 Camera Follow, 월드 스케일, 눈별 렌더 배율, 이동 속도, snap 각도, 손 역할, post-processing과 지원 VFX를 저장할 수 있습니다. 게임을 종료한 상태에서 저장하십시오.

전체 post-processing OFF는 잔여 흐림 제거가 검증된 완전 fallback입니다. 개별 Blur/DoF 계열은 부분 지원이고 Aura는 개별 OFF를 지원하지 않습니다.

## 알려진 제한

- Live 밖의 Home/Story/Race/Training/캐릭터 미리보기는 PANEL입니다.
- SteamVR와 Meta Quest Link/Air Link는 실기 미검증입니다.
- 직접 터치와 smooth turn은 지원하지 않습니다.
