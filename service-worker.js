// 캐시 패스스루 SW — 모든 요청을 네트워크로 보내고 캐시하지 않음.
// 기존에 캐시된 옛 파일들도 activate 시 전부 삭제.
const CACHE = 'weather-app-passthrough-v1';

self.addEventListener('install', e => {
  self.skipWaiting();
});

self.addEventListener('activate', e => {
  e.waitUntil((async () => {
    const keys = await caches.keys();
    await Promise.all(keys.map(k => caches.delete(k)));
    await self.clients.claim();
  })());
});

// fetch 핸들러 없음 → 브라우저가 네트워크로 직접 처리
