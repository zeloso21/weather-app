# 날씨앱 🌤️

GPS 기반 날씨 PWA. 한국어 UI, 강수확률·강수량, 5일 예보, 구글 캘린더 연동.

## 폰에 앱처럼 설치하기

1. 공개 URL을 폰 브라우저로 엽니다 (Chrome / Safari).
2. **Android Chrome**: 메뉴(⋮) → **앱 설치** 또는 **홈 화면에 추가**
3. **iOS Safari**: 공유(↑) → **홈 화면에 추가**
4. 홈 화면 아이콘으로 실행하면 전체화면 앱처럼 동작합니다.

## 구글 캘린더 연동 설정 (선택)

1. [Google Cloud Console](https://console.cloud.google.com)에서 프로젝트 생성
2. **Calendar API** 활성화
3. OAuth 동의 화면 → 테스트 사용자에 본인 이메일 추가
4. **OAuth 클라이언트 ID(웹)** 발급:
   - 승인된 JavaScript 원본에 GitHub Pages URL 추가
5. `index.html`의 `GOOGLE_CLIENT_ID`에 입력

## 기술

- Open-Meteo API (무료, 키 불필요)
- 브라우저 Geolocation API
- Google Calendar API v3
- Service Worker (오프라인 캐시)
