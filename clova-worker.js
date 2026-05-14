// =====================================================
// Cloudflare Workers - 네이버 CLOVA Voice TTS 프록시
// =====================================================
// 사용법:
// 1. https://dash.cloudflare.com 가입 (Google 계정으로 가능)
// 2. 좌측 메뉴 → Workers & Pages → Create → "Start with Hello World!"
// 3. 이름: weather-clova → Deploy
// 4. "Edit code" → 이 파일 내용 전체를 복붙 → Save and Deploy
// 5. Settings → Variables and Secrets → Add:
//    - 이름 NCP_CLIENT_ID, 값: 네이버 클로바 Client ID, Type: Secret
//    - 이름 NCP_CLIENT_SECRET, 값: 네이버 클로바 Client Secret, Type: Secret
// 6. 발급된 URL (https://weather-clova.xxx.workers.dev) 복사 → 날씨앱 음성 메뉴에서 "🌐 클로바 프록시 URL 설정" 클릭 후 붙여넣기
// =====================================================

export default {
  async fetch(req, env) {
    const cors = {
      'Access-Control-Allow-Origin': '*',
      'Access-Control-Allow-Methods': 'POST, OPTIONS',
      'Access-Control-Allow-Headers': 'Content-Type'
    };
    if (req.method === 'OPTIONS') return new Response(null, { headers: cors });
    if (req.method !== 'POST') return new Response('POST only', { status: 405, headers: cors });

    let body;
    try { body = await req.json(); }
    catch { return new Response('Invalid JSON', { status: 400, headers: cors }); }

    const { text, speaker = 'nara', speed = 0, pitch = 0, volume = 0 } = body;
    if (!text) return new Response('text required', { status: 400, headers: cors });
    if (!env.NCP_CLIENT_ID || !env.NCP_CLIENT_SECRET) {
      return new Response('Server not configured: missing NCP_CLIENT_ID/SECRET', { status: 500, headers: cors });
    }

    // Premium 화자 (vXXX)는 premium 엔드포인트, 그 외는 standard
    const isPremium = /^v/.test(speaker);
    const endpoint = isPremium
      ? 'https://naveropenapi.apigw.ntruss.com/tts-premium/v1/tts'
      : 'https://naveropenapi.apigw.ntruss.com/tts/v1/tts';

    const params = new URLSearchParams();
    params.append('speaker', speaker);
    params.append('text', String(text).slice(0, 1500));
    params.append('volume', String(volume));
    params.append('speed', String(speed));
    params.append('pitch', String(pitch));
    params.append('format', 'mp3');

    const resp = await fetch(endpoint, {
      method: 'POST',
      headers: {
        'X-NCP-APIGW-API-KEY-ID': env.NCP_CLIENT_ID,
        'X-NCP-APIGW-API-KEY': env.NCP_CLIENT_SECRET,
        'Content-Type': 'application/x-www-form-urlencoded'
      },
      body: params.toString()
    });

    if (!resp.ok) {
      const err = await resp.text();
      return new Response(`CLOVA ${resp.status}: ${err}`, { status: 502, headers: cors });
    }

    return new Response(resp.body, {
      headers: { ...cors, 'Content-Type': 'audio/mpeg', 'Cache-Control': 'no-store' }
    });
  }
};
