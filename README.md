# 날씨앱 ☔

GPS 기반 한국어 날씨 PWA + **아두이노 우산 디스펜서** 연동.

비 올 일정이 30분 안에 잡혀 있고, 앞에 사람이 서면, 아두이노가 우산을 내보낸다.

## 구성

```
[Google Calendar] ─┐
                   ├─→ [날씨앱 (브라우저)] ──USB/BT──→ [Arduino Mega] ──→ [모터/우산]
[Open-Meteo]      ─┘          ↑                          ↑
                            TTS음성               HUSKYLENS(얼굴) + IR(우산 감지)
```

- **앱 (`index.html`)**: 두뇌 — 언제 비가 올지 판단, 아두이노로 신호 전송, 얼굴 감지 시 TTS 멘트
- **아두이노 (`arduino/umbrella_dispenser.ino`)**: 손발 — 비예보 + 우산 있음 + 얼굴 감지 시 모터 구동

## 핵심 동작

### 비 판정 우선순위
1. **30분 안에 시간 지정 일정**이 있으면 → 그 일정의 시각·장소 예보로만 결정
2. 일정이 없을 때만 → 현재 위치 예보 사용
3. 이벤트 시작 ±30분이면 실제 관측값으로 교차 검증
4. 강수확률 ≥ 50% → 비로 판정

### 앱 ↔ 아두이노 통신
- 앱이 **30초마다** `RAIN:1\n` / `RAIN:0\n` 송신 (USB Web Serial 또는 HC-05 BT)
- 아두이노는 USB·BT 양쪽에서 수신
- 6시간 갱신 없으면 비예보 자동 무효화 (안전장치)
- 페이지 열면 `navigator.serial.getPorts()`로 이전 포트 자동 재연결

### 우산 배출 조건 (셋 다 충족)
- `rainExpected` (앱이 보낸 상태)
- `IR` 센서 — 우산이 거치대에 꽂혀 있음
- `HUSKYLENS` — 얼굴 면적 ≥ 3000

→ 모터 1.5초 정방향 → 우산 배출 → `EVENT umbrella_dispensed` 회신

### TTS 음성 안내
- 아두이노가 얼굴 감지 시 `FACE\n` 송신 (15초 쿨다운)
- 앱이 현재 요약(`currentSummary`)을 한국어로 읽어줌
- 자동재생 잠금은 첫 사용자 클릭 시 빈 utterance로 해제

## 하드웨어 배선 (Arduino Mega 2560)

| 부품         | 핀                  | 비고                                    |
|------------|--------------------|---------------------------------------|
| HUSKYLENS  | SDA=20, SCL=21    | I2C (Uno의 A4/A5와 다름)                 |
| HC-05 BT   | TX1=18, RX1=19    | TX1→RXD는 1k+2k 분압 (5V→3.3V)         |
| IR 센서     | D2                | HIGH = 우산 감지                         |
| 모터 드라이버 | IN1=5,IN2=6,IN3=7,IN4=8 / ENA=4,ENB=9 | PWM 255                |

## 폰에 PWA로 설치

1. 공개 URL을 폰 브라우저로 연다 (Chrome / Safari)
2. **Android Chrome**: 메뉴 → **앱 설치** / **홈 화면에 추가**
3. **iOS Safari**: 공유 → **홈 화면에 추가**

## 구글 캘린더 연동 (선택)

1. [Google Cloud Console](https://console.cloud.google.com)에서 프로젝트 생성
2. **Calendar API** 활성화
3. OAuth 동의 화면 → 테스트 사용자에 본인 이메일 추가
4. **OAuth 클라이언트 ID(웹)** 발급, 승인된 JavaScript 원본에 배포 URL 추가
5. `index.html`의 `GOOGLE_CLIENT_ID`에 입력

## 파일

```
날씨앱/
├── index.html              # 앱 본체 (UI, 날씨 판단, Web Serial, TTS)
├── service-worker.js       # 캐시 안 함 (pass-through) — 신선도 우선
├── manifest.json           # PWA 메타데이터
├── icons/                  # PWA 아이콘
├── make_icons.py           # 아이콘 생성 스크립트
├── 서버실행.bat            # 로컬 정적 서버 (Windows)
└── arduino/
    └── umbrella_dispenser.ino
```

## 사용 기술

- **Open-Meteo API** — 무료, 키 불필요
- **Geolocation API** — 현재 위치
- **Google Calendar API v3** — 일정 가져오기
- **Web Serial API** — PC Chrome/Edge에서 아두이노 직결
- **Web Speech API** — 한국어 TTS
- **Service Worker** — PWA 설치 가능

## 주의

- Web Serial은 **데스크탑 Chrome/Edge**에서만 동작 (HTTPS 또는 localhost)
- 폰에서는 HC-05 블루투스 경로로 동작
- `KRX1.xlsx` 같은 별개 프로젝트 파일은 이 앱과 무관
