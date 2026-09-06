# UmaVR Configurator

`build/publish/UmaVR.Configurator.exe`를 실행해 Umamusume VR 설정을 관리한다.

- 게임이 종료된 상태에서 저장한다.
- 설정 파일은 game root의 `vrmod/config/settings.json`이며 schema version, 값 범위, invalid fallback을 검증한다.
- 저장은 같은 폴더의 임시 파일에 flush한 뒤 원자적으로 교체하며 기존 파일은 `settings.json.bak`으로 보존한다.
- 기본값, 다시 불러오기, JSON 가져오기/내보내기를 지원한다.
- 현재 Camera Follow control은 runtime에서 지원·검증된 `LIVE`만 표시한다. STORY/RACE는 unsupported/deferred이므로 phantom control을 만들지 않는다.
- 체감 세계 크기는 `0.55–4.00`, 기본 `1.00`이다. 값이 커질수록 physical head translation, locomotion, eye/IPD world offset을 같은 역비율로 줄여 세계와 캐릭터가 더 크게 느껴지게 하며 authored camera pose와 회전은 바꾸지 않는다.
- Eye render scale은 `0.50–1.50`, 기본 `1.00`이며 `1.25` 초과에서는 비용 경고를 표시한다.
- Live post-processing 전체 master와 현재 `CameraData.Parameter`의 22개 effect slot을 모두 나열한다. 이 중 21개는 개별로 끌 수 있고, 독립 enable/strength가 없는 Aura는 안전하지 않은 dictionary 변형을 하지 않도록 개별 끄기 미지원으로 표시한다. Aura까지 억제하려면 전체 master를 끈다. Composite DoF/Diffusion/Bloom은 slot 전체와 세부 효과를 모두 제어한다. 기본 ON은 장면이 지정한 authored 상태를 보존하며, OFF override는 Live generation 이탈 시 원래 current 값을 복원한다.
- Controller는 이동/Snap Turn 사용 여부·속도·각도와 primary/secondary 손 역할 전체 전환을 저장한다. 기본은 오른손 pointer·A/B·이동과 왼손 panel Grip·회전이며, 전환 시 왼손 pointer·X/Y·이동과 오른손 panel Grip·회전이 된다.

빌드와 self-test:

```powershell
& .\vrmod\settings\build.ps1
& .\vrmod\settings\tests\Test-Local.ps1
```
